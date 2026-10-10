#include "ui_offline_tiles.h"
#include <math.h>
#include <string.h>
#include <limits.h>
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint16_t u16(const uint8_t *p){return (uint16_t)p[0]|(uint16_t)p[1]<<8;}
static float f32(const uint8_t *p){uint32_t u=u32(p);float v;memcpy(&v,&u,4);return v;}
uint32_t ui_offline_crc32(uint32_t c,const void *data,size_t count)
{
    static const uint32_t table[16]={0,0x1DB71064,0x3B6E20C8,0x26D930AC,0x76DC4190,0x6B6B51F4,0x4DB26158,0x5005713C,0xEDB88320,0xF00F9344,0xD6D6A3E8,0xCB61B38C,0x9B64C2B0,0x86D3D2D4,0xA00AE278,0xBDBDF21C};
    const uint8_t *p=data;
    while(count--){c^=*p++;c=(c>>4)^table[c&15];c=(c>>4)^table[c&15];}
    return c;
}
ui_nav_result_code_t ui_offline_tiles_open(ui_offline_tiles_t *t,const ui_offline_map_io_t *io)
{
    if(!t||!io||!io->read_at)return UI_NAV_NO_MAP;
    memset(t,0,sizeof(*t));uint8_t h[128];
    if(io->size<128||!io->read_at(io->context,0,h,128))return UI_NAV_NO_MAP;
    if(memcmp(h,"BTIL0002",8)||u32(h+8)!=2||u32(h+16)!=256)return UI_NAV_UNSUPPORTED;
    uint32_t c=ui_offline_crc32(0xffffffff,h,68);c=ui_offline_crc32(c,h+72,56)^0xffffffff;
    if(c!=u32(h+68))return UI_NAV_CORRUPT_MAP;
    t->io=*io;t->revision=u32(h+12);t->level_count=u32(h+20);
    t->west=(int32_t)u32(h+24);t->south=(int32_t)u32(h+28);t->east=(int32_t)u32(h+32);t->north=(int32_t)u32(h+36);
    t->latitude_ref=(int32_t)u32(h+40)/1e7f;t->meters_lon=f32(h+44);t->meters_lat=f32(h+48);t->total=u32(h+52);t->directory=u32(h+60);
    if(!t->revision||!t->level_count||t->level_count>UI_OFFLINE_TILE_LEVEL_MAX||t->west>=t->east||t->south>=t->north||
       t->west< -1800000000||t->east>1800000000||t->south< -850000000||t->north>850000000||
       !isfinite(t->meters_lon)||!isfinite(t->meters_lat)||t->meters_lon<1000||t->meters_lon>112000||t->meters_lat<110000||t->meters_lat>112000||
       !isfinite(t->latitude_ref)||fabsf(t->latitude_ref)>85||
       t->directory!=128+t->level_count*16||t->directory>io->size||t->total>(io->size-t->directory)/16)return UI_NAV_CORRUPT_MAP;
    uint32_t first=0;
    for(unsigned i=0;i<t->level_count;i++){
        uint8_t b[16];if(!io->read_at(io->context,128+i*16,b,16))return UI_NAV_CORRUPT_MAP;
        ui_offline_tile_level_t *l=&t->levels[i];*l=(ui_offline_tile_level_t){f32(b),u32(b+4),u32(b+8),u32(b+12)};
        if(!isfinite(l->mpp)||l->mpp<.1f||l->mpp>1000||!l->cols||!l->rows||l->first!=first||l->rows>t->total/l->cols||
           (i&&l->mpp<=t->levels[i-1].mpp))return UI_NAV_CORRUPT_MAP;
        first+=l->cols*l->rows;
    }
    if(first!=t->total)return UI_NAV_CORRUPT_MAP;
    uint8_t *buffer=t->validation_workspace;uint32_t remaining=t->directory+t->total*16-128,offset=128;c=0xffffffff;
    while(remaining){size_t n=remaining>sizeof(t->validation_workspace)?sizeof(t->validation_workspace):remaining;
        if(!io->read_at(io->context,offset,buffer,n))return UI_NAV_CORRUPT_MAP;
        c=ui_offline_crc32(c,buffer,n);remaining-=n;offset+=n;}
    if((c^0xffffffff)!=u32(h+64))return UI_NAV_CORRUPT_MAP;
    t->label_offset=u32(h+72);t->label_bytes=u32(h+76);
    if(t->label_offset){
        if(t->label_offset<t->directory+t->total*16||t->label_offset>io->size||t->label_bytes>io->size-t->label_offset||t->label_bytes<t->total*8)return UI_NAV_CORRUPT_MAP;
        offset=t->label_offset;remaining=t->label_bytes;c=0xffffffff;
        while(remaining){size_t n=remaining>sizeof(t->validation_workspace)?sizeof(t->validation_workspace):remaining;
            if(!io->read_at(io->context,offset,buffer,n))return UI_NAV_CORRUPT_MAP;c=ui_offline_crc32(c,buffer,n);remaining-=n;offset+=n;}
        if((c^0xffffffff)!=u32(h+80))return UI_NAV_CORRUPT_MAP;
    }
    return UI_NAV_OK;
}
void ui_offline_tile_cache_init(ui_offline_tile_cache_t *cache,uint16_t *pixels,unsigned slots)
{if(cache){memset(cache,0,sizeof(*cache));cache->pixels=pixels;cache->slots=slots>32?32:slots;}}
typedef struct {const ui_offline_tiles_t *tiles;ui_offline_tiles_stats_t *stats;uint32_t offset,left,crc;size_t index,count;uint8_t *data;} stream_t;
static bool fill(stream_t *s)
{
    if(s->index<s->count)return true;if(!s->left)return false;
    s->count=s->left>4096U?4096U:s->left;s->index=0;
    if(!s->tiles->io.read_at(s->tiles->io.context,s->offset,s->data,s->count))return false;
    s->crc=ui_offline_crc32(s->crc,s->data,s->count);s->stats->bytes+=s->count;s->offset+=s->count;s->left-=s->count;return true;
}
static bool byte(stream_t *s,uint8_t *value){if(!fill(s))return false;*value=s->data[s->index++];return true;}
static bool length(stream_t *s,uint32_t *value)
{uint8_t extra;do{if(!byte(s,&extra)||*value>131072-extra)return false;*value+=extra;}while(extra==255);return true;}
/* Bounded streaming LZ4 block decoder, following the published block format.
 * No allocator or board-side compression library; at most 4 KiB input scratch. */
static bool lz4_block(stream_t *s,uint8_t *output)
{
    uint32_t written=0;
    while(s->left||s->index<s->count){
        uint8_t token;if(!byte(s,&token))return false;uint32_t literal=token>>4;
        if(literal==15&&!length(s,&literal))return false;if(literal>131072-written)return false;
        while(literal){if(!fill(s))return false;size_t n=s->count-s->index;if(n>literal)n=literal;
            memcpy(output+written,s->data+s->index,n);written+=n;s->index+=n;literal-=n;}
        if(!s->left&&s->index==s->count)return written==131072;
        uint8_t a,b;if(!byte(s,&a)||!byte(s,&b))return false;uint32_t offset=(uint32_t)a|(uint32_t)b<<8;
        if(!offset||offset>written)return false;uint32_t match=(token&15)+4;
        if((token&15)==15&&!length(s,&match))return false;if(match>131072-written)return false;
        if(offset>=8)while(match>=8){memcpy(output+written,output+written-offset,8);written+=8;match-=8;}
        while(match--){output[written]=output[written-offset];written++;}
    }
    return false;
}
static bool read_labels(const ui_offline_tiles_t *t,uint32_t key,ui_offline_tile_cache_entry_t *entry,ui_offline_tiles_stats_t *stats)
{
    entry->label_count=0;if(!t->label_offset)return true;uint8_t b[60];
    if(!t->io.read_at(t->io.context,t->label_offset+key*8,b,8))return false;stats->bytes+=8;
    uint32_t offset=u32(b),count=u32(b+4);if(!count)return offset==0;
    if(count>16||offset<t->total*8||offset>t->label_bytes||count>(t->label_bytes-offset)/60)return false;
    for(unsigned i=0;i<count;i++){
        if(!t->io.read_at(t->io.context,t->label_offset+offset+i*60,b,60))return false;stats->bytes+=60;
        float x=f32(b),y=f32(b+4);uint32_t kind=u32(b+8);
        if(!isfinite(x)||!isfinite(y)||fabsf(x)>1000000||fabsf(y)>1000000||kind>7||!memchr(b+12,0,48))return false;
        entry->labels[i].x=x;entry->labels[i].y=y;entry->labels[i].kind=kind;memcpy(entry->labels[i].text,b+12,48);
    }
    entry->label_count=count;return true;
}
static ui_nav_result_code_t tile(const ui_offline_tiles_t *t,ui_offline_tile_cache_t *cache,uint32_t key,const uint16_t **out,ui_offline_tiles_stats_t *stats)
{
    for(unsigned i=0;i<cache->slots;i++)if(cache->entries[i].valid&&cache->entries[i].key==key){
        cache->entries[i].age=++cache->clock;*out=cache->pixels+i*65536;stats->hits++;return UI_NAV_OK;}
    uint8_t entry[16];uint32_t directory_end=t->directory+t->total*16;
    if(key>=t->total||!t->io.read_at(t->io.context,t->directory+key*16,entry,16))return UI_NAV_CORRUPT_MAP;
    stats->bytes+=16;uint32_t offset=u32(entry),length=u32(entry+4),expected=u32(entry+8),encoding=u32(entry+12);
    if(!length)return UI_NAV_NO_MAP;
    if(offset<directory_end||offset>t->io.size||length>t->io.size-offset||length>262144||encoding>2||
       (encoding==0&&length!=131072)||(encoding==1&&length%4))return UI_NAV_CORRUPT_MAP;
    unsigned slot=0;for(unsigned i=0;i<cache->slots;i++)if(!cache->entries[i].valid){slot=i;break;}
        else if(cache->entries[i].age<cache->entries[slot].age)slot=i;
    cache->entries[slot].valid=false;uint16_t *pixels=cache->pixels+slot*65536;uint32_t c=0xffffffff,written=0;uint8_t *buffer=cache->decode_workspace;
    if(encoding==2){
        stream_t stream={.data=cache->decode_workspace,.tiles=t,.stats=stats,.offset=offset,.left=length,.crc=0xffffffff};
        if(!lz4_block(&stream,(uint8_t *)pixels)||(stream.crc^0xffffffff)!=expected)return UI_NAV_CORRUPT_MAP;
        for(unsigned i=0;i<65536;i++)pixels[i]=u16((const uint8_t *)pixels+i*2);
        cache->entries[slot].key=key;cache->entries[slot].age=++cache->clock;
        if(!read_labels(t,key,&cache->entries[slot],stats))return UI_NAV_CORRUPT_MAP;
        cache->entries[slot].valid=true;*out=pixels;stats->misses++;return UI_NAV_OK;
    }
    while(length){size_t n=length>sizeof(cache->decode_workspace)?sizeof(cache->decode_workspace):length;
        if(!t->io.read_at(t->io.context,offset,buffer,n))return UI_NAV_CORRUPT_MAP;
        stats->bytes+=n;c=ui_offline_crc32(c,buffer,n);
        if(!encoding){for(size_t i=0;i<n;i+=2)pixels[written++]=u16(buffer+i);}
        else for(size_t i=0;i<n;i+=4){uint32_t count=u16(buffer+i);uint16_t color=u16(buffer+i+2);
            if(!count||count>65536-written)return UI_NAV_CORRUPT_MAP;
            for(uint32_t j=0;j<count;j++)pixels[written++]=color;}
        offset+=n;length-=n;
    }
    if(written!=65536||(c^0xffffffff)!=expected)return UI_NAV_CORRUPT_MAP;
    cache->entries[slot].key=key;cache->entries[slot].age=++cache->clock;
    if(!read_labels(t,key,&cache->entries[slot],stats))return UI_NAV_CORRUPT_MAP;
    cache->entries[slot].valid=true;*out=pixels;stats->misses++;return UI_NAV_OK;
}
ui_nav_result_code_t ui_offline_tiles_window(const ui_offline_tiles_t *t,ui_offline_tile_cache_t *cache,
    const ui_nav_map_request_t *r,uint16_t *pixels,ui_nav_map_frame_t *f,ui_offline_tiles_stats_t *stats)
{
    if(!t||!cache||!cache->pixels||!cache->slots||!r||!pixels||!f)return UI_NAV_FAILED;
    if(r->center.system!=UI_NAV_COORD_WGS84)return UI_NAV_UNSUPPORTED;
    if(!isfinite(r->center.latitude)||!isfinite(r->center.longitude)||!isfinite(r->meters_per_pixel)||r->meters_per_pixel<.05f)return UI_NAV_FAILED;
    if(r->center.longitude*1e7<t->west||r->center.longitude*1e7>t->east||r->center.latitude*1e7<t->south||r->center.latitude*1e7>t->north)return UI_NAV_NO_MAP;
    ui_offline_tiles_stats_t local={0};if(!stats)stats=&local;memset(stats,0,sizeof(*stats));
    unsigned level=0;float best=1e30;
    for(unsigned i=0;i<t->level_count;i++){float d=fabsf(logf(t->levels[i].mpp/r->meters_per_pixel));if(d<best){best=d;level=i;}}
    /* A selected LOD must cover the rotated viewport during zoom-out. */
    while(level+1<t->level_count && t->levels[level].mpp*768 <
          fmaxf(r->width,r->height)*r->meters_per_pixel*1.08f)level++;
    for(;;){
        const ui_offline_tile_level_t *l=&t->levels[level];
        double x=(r->center.longitude-t->west/1e7)*t->meters_lon/l->mpp,y=(t->north/1e7-r->center.latitude)*t->meters_lat/l->mpp;
        /* 768-square overscan: no file read is needed for small pans/heading changes. */
        int left=(int)floor((x-384)/64)*64,top=(int)floor((y-384)/64)*64;
        bool missing=false;uint16_t background=(uint16_t)(((0x1B>>3)<<11)|((0x27>>2)<<5)|(0x2C>>3));
        for(unsigned i=0;i<768*768;i++)pixels[i]=background;
        int tx0=(int)floor(left/256.),ty0=(int)floor(top/256.);
        int tx1=(int)floor((left+767)/256.),ty1=(int)floor((top+767)/256.);
        for(int ty=ty0;ty<=ty1;ty++)for(int tx=tx0;tx<=tx1;tx++){
            if(tx<0||ty<0||tx>=(int)l->cols||ty>=(int)l->rows)continue;
            const uint16_t *source;ui_nav_result_code_t code=tile(t,cache,l->first+ty*l->cols+tx,&source,stats);
            if(code==UI_NAV_NO_MAP){missing=true;break;}if(code!=UI_NAV_OK)return code;
            int dx=tx*256-left,dy=ty*256-top,sx=dx<0?-dx:0,sy=dy<0?-dy:0;
            int count=256-sx;if(dx+sx+count>768)count=768-dx-sx;
            for(int yy=sy;yy<256&&dy+yy<768;yy++)memcpy(pixels+(dy+yy)*768+dx+sx,source+yy*256+sx,count*2);
            stats->tiles++;
        }
        if(missing&&level+1<t->level_count){level++;continue;}if(missing)return UI_NAV_NO_MAP;
        memset(f,0,sizeof(*f));f->request_id=r->id;f->route_id=r->route_id;f->route_revision=r->route_revision;f->map_revision=t->revision;
        f->origin=(ui_nav_point_t){t->north/1e7-(top+384)*l->mpp/t->meters_lat,t->west/1e7+(left+384)*l->mpp/t->meters_lon,UI_NAV_COORD_WGS84};
        f->meters_per_pixel=l->mpp;f->status=UI_NAV_OK;f->raster_id=r->id;f->raster_latitude_ref=t->latitude_ref;stats->mpp=l->mpp;
        float scores[UI_NAV_MAP_LABEL_MAX];
        double ox=(left+384)*l->mpp,oy=(top+384)*l->mpp;
        /* Label order must not depend on cache-slot history or worker timing. */
        for(int ty=ty0;ty<=ty1;ty++)for(int tx=tx0;tx<=tx1;tx++){
            if(tx<0||ty<0||tx>=(int)l->cols||ty>=(int)l->rows)continue;
            uint32_t key=l->first+ty*l->cols+tx;const ui_offline_tile_cache_entry_t *entry=NULL;
            for(unsigned ci=0;ci<cache->slots;ci++)if(cache->entries[ci].valid&&cache->entries[ci].key==key){entry=&cache->entries[ci];break;}
            if(!entry)continue;
            for(unsigned j=0;j<entry->label_count;j++){
                const ui_offline_tile_label_t *p=&entry->labels[j];
                float dx=(p->x-ox)/l->mpp,dy=(p->y-oy)/l->mpp;
                if(fabsf(dx)>384||fabsf(dy)>384)continue;
                float score=(p->kind>=3?.1f:p->kind?.2f:.3f)+hypotf(dx,dy)*.0001f;unsigned at=0;
                while(at<f->label_count&&(scores[at]<score||(scores[at]==score&&strcmp(f->labels[at].text,p->text)<=0)))at++;
                if(at>=UI_NAV_MAP_LABEL_MAX)continue;
                unsigned end=f->label_count<UI_NAV_MAP_LABEL_MAX?f->label_count++:UI_NAV_MAP_LABEL_MAX-1;
                for(unsigned k=end;k>at;k--){f->labels[k]=f->labels[k-1];scores[k]=scores[k-1];}
                f->labels[at].point=(ui_nav_local_point_t){(p->x-ox)*cos(f->origin.latitude*.01745329252)/cos(t->latitude_ref*.01745329252),oy-p->y};
                f->labels[at].kind=p->kind;memcpy(f->labels[at].text,p->text,48);scores[at]=score;
            }
        }
        return UI_NAV_OK;
    }
}
