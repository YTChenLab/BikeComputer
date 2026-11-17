#include "resource_install.h"
#include "resource_storage.h"
#include <string.h>

/* 固定容量、单一所有者；ISR 不接触协议和 Flash。32 位 mode 发布给 UiTask。 */
static resource_install_port_t s_port;
/* AC6/GCC 原生原子内建，不依赖 Keil 默认未暴露的 stdatomic.h 路径。 */
_Static_assert(__atomic_always_lock_free(4,0), "mode requires lock-free 32-bit atomics");
static uint32_t s_mode;
static uint8_t s_frame[RI_HEADER+RI_PAYLOAD_MAX], s_last[RI_HEADER+RI_PAYLOAD_MAX];
static uint8_t s_reply[RI_HEADER+RI_REPLY_SIZE], s_cached[RI_HEADER+RI_REPLY_SIZE];
static uint32_t s_used, s_last_size, s_start, s_byte_time, s_activity;
static uint32_t s_session, s_next, s_total, s_target, s_error, s_detail;
static bool s_running, s_committed, s_have_cache;

static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static void put(uint8_t *p,uint32_t v)
{ for(unsigned i=0;i<4;++i) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t crc(const uint8_t *p,uint32_t n,uint32_t state)
{ return ResourcePackage_Crc32Update(state,p,n); }
static bool locked(void) { return s_port.locked && s_port.locked(s_port.context); }

static void snapshot(uint8_t *p,uint32_t code,uint32_t detail)
{
    resource_storage_info_t info;
    (void)ResourceStorage_GetInfo(&info);
    memset(p,0,RI_REPLY_SIZE);
    put(p,code); put(p+4,detail); put(p+8,ResourceInstall_Mode());
    put(p+12,info.active_slot); put(p+16,ResourceStorage_GetReceived());
    put(p+20,s_total); put(p+24,info.package_version); put(p+28,info.package_size);
    memcpy(p+32,info.payload_sha256,32); memcpy(p+64,s_port.uid,12);
    /* 非诊断回复：尾部报告 session/next/target、最后异步错误和锁定状态。 */
    put(p+76,s_session); put(p+80,s_next); put(p+84,s_target);
    put(p+88,s_error); put(p+92,s_detail); put(p+96,locked());
    put(p+100,1U); /* 协议能力版本 */
    put(p+104,s_committed);
    put(p+108,s_running); put(p+112,0x42494B45U); /* 产品标识 BIKE */
    put(p+116,1U); put(p+120,256U); put(p+124,RI_SESSION_MS);
}

static void abort_session(uint32_t reason)
{
    if(s_running) (void)ResourceStorage_Abort();
    s_running=false; s_session=0; s_have_cache=false; s_error=reason;
}

static void dispatch(uint32_t bytes,uint32_t now)
{
    uint8_t cmd=s_frame[5]; uint32_t length=bytes-RI_HEADER;
    uint32_t session=u32(s_frame+8),seq=u32(s_frame+12);
    uint32_t code=RI_OK,detail=0;
    bool mutation=cmd>=RI_BEGIN && cmd<=RI_ABORT;
    bool reset=false,maintenance=false;
    uint8_t *data=s_frame+RI_HEADER;
    /* ACK 丢失后按完全相同的请求重放；比较完整帧，不能仅凭序号或 CRC 猜重复。 */
    if(mutation && s_have_cache && bytes==s_last_size && memcmp(s_frame,s_last,bytes)==0) {
        (void)s_port.send(s_cached,sizeof(s_cached),s_port.context); return;
    }
    if(cmd==RI_HELLO || cmd==RI_STATUS) {
        if(length || session) code=RI_FORMAT;
    } else if(cmd==RI_MAINTENANCE || cmd==RI_REBOOT) {
        if(length || session) code=RI_FORMAT;
        else { reset=true; maintenance=cmd==RI_MAINTENANCE; }
    } else if(cmd==RI_DIAG) {
        if(length!=4 || session) code=RI_LENGTH;
        else if(u32(data)&~1U) code=RI_FORMAT;
        else if(s_running || ResourceInstall_Mode()==RI_UI_PENDING) code=RI_BUSY;
    } else if(!mutation) code=RI_UNSUPPORTED;
    else if(locked()) code=RI_LOCKED;
    else if(ResourceInstall_Mode()!=RI_INSTALL) code=RI_STATE;
    else if(cmd==RI_BEGIN) {
        if(length!=4 || !session || seq) code=RI_FORMAT;
        else if(s_running) code=RI_BUSY;
        else {
            resource_package_slot_t target;
            detail=ResourceStorage_Begin(u32(data),&target);
            if(detail) code=RI_STORAGE;
            else {
                s_session=session; s_next=1; s_total=u32(data); s_target=(uint32_t)target;
                s_running=true; s_committed=false; s_activity=now; s_error=0;
            }
        }
    } else if(!s_running || session!=s_session) code=RI_SESSION;
    else if(seq!=s_next) code=seq<s_next ? RI_CONFLICT : RI_SEQUENCE;
    else if(cmd==RI_DATA) {
        if(length<5 || length>260) code=RI_LENGTH;
        else if(u32(data)!=ResourceStorage_GetReceived()) code=RI_SEQUENCE;
        else {
            detail=ResourceStorage_Write(u32(data),data+4,length-4);
            if(detail) code=RI_STORAGE;
            else { ++s_next; s_activity=now; }
        }
    } else if(cmd==RI_COMMIT) {
        if(length) code=RI_LENGTH;
        else {
            /* 原 storage 内部完整验证后原子提交 metadata，不按“字节收齐”直接切槽。 */
            detail=ResourceStorage_Commit();
            if(detail) { code=RI_STORAGE; abort_session(code); }
            else { s_running=false; s_committed=true; ++s_next; s_activity=now; }
        }
    } else if(cmd==RI_ABORT) {
        if(length) code=RI_LENGTH;
        else { detail=ResourceStorage_Abort(); if(detail) code=RI_STORAGE;
               else { s_running=false; s_session=0; s_next=0; s_total=0; } }
    }
    if(code) { s_error=code; s_detail=detail; }
    memcpy(s_reply,s_frame,RI_HEADER); s_reply[5]=(uint8_t)(cmd|0x80U);
    s_reply[6]=RI_REPLY_SIZE; s_reply[7]=0;
    snapshot(s_reply+RI_HEADER,code,detail);
    if(cmd==RI_DIAG && code==RI_OK) {
        uint32_t result[13]={0};
        if(s_port.diagnose) s_port.diagnose(u32(data),result,s_port.context);
        else result[0]=RI_UNSUPPORTED;
        put(s_reply+RI_HEADER,result[0]);
        for(unsigned i=0;i<13;++i) put(s_reply+RI_HEADER+76+4*i,result[i]);
    }
    uint32_t c=crc(s_reply,16,UINT32_MAX);
    c=crc(s_reply+RI_HEADER,RI_REPLY_SIZE,c)^UINT32_MAX; put(s_reply+16,c);
    if(mutation && code==RI_OK) {
        memcpy(s_last,s_frame,bytes); s_last_size=bytes;
        memcpy(s_cached,s_reply,sizeof(s_reply)); s_have_cache=true;
    }
    bool sent=s_port.send(s_reply,sizeof(s_reply),s_port.context);
    /* 回复已同步发完才复位；失败不假装 PC 已收到，重传仍可重试。 */
    if(reset && sent && s_port.reset) {
        abort_session(RI_OK); s_port.reset(maintenance,s_port.context);
    }
}

void ResourceInstall_Init(const resource_install_port_t *port,bool maintenance,uint32_t now)
{
    s_port=*port; s_used=0; s_start=now; s_running=false; s_committed=false;
    s_session=0; s_next=0; s_total=0; s_target=UINT32_MAX; s_have_cache=false;
    s_detail=ResourceStorage_Init(1U);
    s_error=s_detail==RESOURCE_STORAGE_OK || s_detail==RESOURCE_STORAGE_EMPTY ? 0 : RI_STORAGE;
    __atomic_store_n(&s_mode,(!maintenance && s_detail==RESOURCE_STORAGE_OK) ? RI_BOOT : RI_INSTALL,__ATOMIC_RELEASE);
}

void ResourceInstall_Feed(const uint8_t *data,uint32_t bytes,uint32_t now)
{
    for(uint32_t i=0;i<bytes;++i) {
        if(s_used && now-s_byte_time>RI_PARTIAL_MS) s_used=0;
        s_byte_time=now;
        s_frame[s_used++]=data[i];
        /* 丢弃噪声，滑动寻找 magic；非法长度不等待其声称的数据量。 */
        while(s_used>=4 && memcmp(s_frame,"BCRI",4)!=0) {
            memmove(s_frame,s_frame+1,--s_used);
        }
        if(s_used<RI_HEADER) continue;
        uint32_t n=(uint32_t)s_frame[6]|((uint32_t)s_frame[7]<<8);
        if(s_frame[4]!=1 || n>RI_PAYLOAD_MAX || (s_frame[5]&0x80U)) {
            s_used=0; s_error=RI_FORMAT; continue;
        }
        if(s_used==RI_HEADER+n) {
            uint32_t c=crc(s_frame,16,UINT32_MAX);
            c=crc(s_frame+RI_HEADER,n,c)^UINT32_MAX;
            if(c==u32(s_frame+16)) dispatch(s_used,now);
            else s_error=RI_CRC; /* 不信任损坏帧的 session/序号，丢弃后由 PC 超时重传。 */
            s_used=0;
        }
    }
}

void ResourceInstall_Poll(uint32_t now)
{
    if(s_used && now-s_byte_time>RI_PARTIAL_MS) { s_used=0; s_error=RI_TIMEOUT; }
    if(s_running && now-s_activity>RI_SESSION_MS) abort_session(RI_TIMEOUT);
    if(ResourceInstall_Mode()==RI_BOOT && now-s_start>=RI_BOOT_GRACE_MS)
        __atomic_store_n(&s_mode,RI_UI_PENDING,__ATOMIC_RELEASE);
}
void ResourceInstall_TransportLost(void) { s_used=0; abort_session(RI_TRANSPORT); }
uint32_t ResourceInstall_Mode(void) { return __atomic_load_n(&s_mode,__ATOMIC_ACQUIRE); }
void ResourceInstall_UiResult(bool success)
{ __atomic_store_n(&s_mode,success ? RI_UI_ACTIVE : RI_INSTALL,__ATOMIC_RELEASE); }
