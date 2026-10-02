#include "ui_preferences.h"
#include "ui/core/ui_concurrency.h"
#include <string.h>

static ui_preferences_port_t s_port;
static ui_preferences_t s_current, s_pending;
static bool s_pending_valid, s_dirty, s_working, s_save_ok = true;
static bool s_loaded;
static uint32_t s_next_attempt;
static uint32_t crc(const uint8_t *data, unsigned size)
{
    uint32_t value = ~UINT32_C(0);
    for(unsigned i = 0; i < size; ++i) {
        value ^= data[i];
        for(unsigned b = 0; b < 8; ++b)
            value = (value >> 1) ^ (UINT32_C(0xEDB88320) & (0U - (value & 1)));
    }
    return ~value;
}
static void put32(uint8_t *p, uint32_t v) { for(unsigned i=0;i<4;++i) p[i]=(uint8_t)(v>>(i*8)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static bool valid(const ui_preferences_t *p)
{
    if(!p || p->brightness > 100 || p->brightness < 1 || p->sound > 100 ||
       p->user_name[0] == 0 || !memchr(p->user_name, 0, sizeof(p->user_name)) ||
       !memchr(p->tagline, 0, sizeof(p->tagline)) ||
       (p->idle_sleep_ms && (p->idle_sleep_ms < 10000 || p->idle_sleep_ms > 3600000))) return false;
    for(unsigned i=0;i<4;++i) {
        if(!p->heart_zone_bounds[i] || p->heart_zone_bounds[i] > 250 ||
           !p->power_zone_bounds[i] || p->power_zone_bounds[i] > 3000) return false;
        if(i && (p->heart_zone_bounds[i] <= p->heart_zone_bounds[i-1] ||
                 p->power_zone_bounds[i] <= p->power_zone_bounds[i-1])) return false;
    }
    return ui_dashboard_layout_valid(p->dashboard_preset, p->dashboard_fields);
}
static void encode(uint8_t *blob, const ui_preferences_t *p)
{
    memset(blob,0,UI_PREFERENCES_BLOB_SIZE);
    memcpy(blob,"BKPF",4); blob[4]=2;
    blob[6]=(p->auto_pause ? 1 : 0) | (p->reduced_motion ? 2 : 0);
    blob[7]=p->brightness; blob[8]=p->sound;
    memcpy(blob+16,p->user_name,32); memcpy(blob+48,p->tagline,48);
    for(unsigned i=0;i<4;++i) {
        unsigned h=96+i*2, w=104+i*2;
        blob[h]=(uint8_t)p->heart_zone_bounds[i]; blob[h+1]=(uint8_t)(p->heart_zone_bounds[i]>>8);
        blob[w]=(uint8_t)p->power_zone_bounds[i]; blob[w+1]=(uint8_t)(p->power_zone_bounds[i]>>8);
    }
    put32(blob+112,p->idle_sleep_ms);
    memcpy(blob+116,p->dashboard_fields,UI_DASHBOARD_SLOT_COUNT);
    blob[122]=p->dashboard_preset;
    put32(blob+124,crc(blob,124));
}
bool ui_preferences_decode(const uint8_t *blob, ui_preferences_t *p)
{
    if(!blob || !p || memcmp(blob,"BKPF",4) || (blob[4]!=1 && blob[4]!=2) || (blob[6]&~3U) ||
       get32(blob+124)!=crc(blob,124)) return false;
    memset(p,0,sizeof(*p)); p->revision=1;
    p->auto_pause=(blob[6]&1)!=0; p->reduced_motion=(blob[6]&2)!=0;
    p->brightness=blob[7]; p->sound=blob[8];
    memcpy(p->user_name,blob+16,32); memcpy(p->tagline,blob+48,48);
    for(unsigned i=0;i<4;++i) {
        unsigned h=96+i*2,w=104+i*2;
        p->heart_zone_bounds[i]=blob[h] | (uint16_t)blob[h+1]<<8;
        p->power_zone_bounds[i]=blob[w] | (uint16_t)blob[w+1]<<8;
    }
    p->idle_sleep_ms=get32(blob+112);
    if(blob[4] == 1) {
        p->dashboard_preset=UI_DASHBOARD_CRUISE;
        ui_dashboard_preset_fields(UI_DASHBOARD_CRUISE,p->dashboard_fields);
    } else {
        memcpy(p->dashboard_fields,blob+116,UI_DASHBOARD_SLOT_COUNT);
        p->dashboard_preset=blob[122];
        if(blob[123] != 0) return false;
    }
    return valid(p);
}
bool ui_preferences_register(const ui_preferences_port_t *port)
{
    if(!port || !port->load || !port->store || !port->request_worker || s_working) return false;
    s_port=*port; return true;
}
void ui_preferences_init(void)
{
    s_current=(ui_preferences_t){.user_name="骑行者", .tagline="Good Roads, Better Days.",
        .heart_zone_bounds={120,140,155,170}, .power_zone_bounds={150,220,300,400},
        .brightness=75,.sound=62,.auto_pause=true,.revision=1};
    ui_dashboard_preset_fields(UI_DASHBOARD_CRUISE,s_current.dashboard_fields);
    uint8_t blob[UI_PREFERENCES_BLOB_SIZE];
    ui_preferences_t loaded;
    s_loaded=false;
    if(s_port.load && s_port.load(blob,s_port.context) && ui_preferences_decode(blob,&loaded))
        { s_current=loaded; s_loaded=true; }
    s_pending_valid=s_dirty=s_working=false; s_save_ok=true; s_next_attempt=0;
}
bool ui_preferences_get(ui_preferences_t *p)
{
    if(!p) return false;
    ui_concurrency_lock(); *p=s_pending_valid ? s_pending : s_current; ui_concurrency_unlock(); return true;
}
bool ui_preferences_submit(const ui_preferences_t *p)
{
    if(p == NULL) return false;
    ui_preferences_t normalized=*p;
    bool unspecified=true;
    for(unsigned i=0;i<UI_DASHBOARD_SLOT_COUNT;++i) if(p->dashboard_fields[i]) unspecified=false;
    if(unspecified) {
        /* Old callers omit the new fields: preserve the current layout. */
        ui_concurrency_lock();
        const ui_preferences_t *latest=s_pending_valid ? &s_pending : &s_current;
        memcpy(normalized.dashboard_fields,latest->dashboard_fields,UI_DASHBOARD_SLOT_COUNT);
        normalized.dashboard_preset=latest->dashboard_preset;
        ui_concurrency_unlock();
    }
    if(!valid(&normalized)) return false;
    ui_concurrency_lock(); s_pending=normalized; s_pending_valid=true; ui_concurrency_unlock(); return true;
}
bool ui_preferences_process(uint32_t now, ui_preferences_t *p)
{
    ui_concurrency_lock();
    bool changed=s_pending_valid;
    if(changed) { uint32_t revision=s_current.revision+1; s_current=s_pending;
        s_current.revision=revision; s_pending_valid=false; s_dirty=true; }
    if(p) *p=s_current;
    bool request=s_dirty && !s_working && s_port.request_worker && (int32_t)(now-s_next_attempt)>=0;
    if(request) { s_working=true; s_next_attempt=now+5000; }
    ui_concurrency_unlock();
    if(request && !s_port.request_worker(s_port.context)) {
        ui_concurrency_lock(); s_working=false; s_save_ok=false; ui_concurrency_unlock();
    }
    return changed;
}
void ui_preferences_worker(void)
{
    ui_preferences_t snapshot;
    ui_concurrency_lock();
    if(!s_working) { ui_concurrency_unlock(); return; }
    snapshot=s_current;
    ui_concurrency_unlock();
    uint8_t blob[UI_PREFERENCES_BLOB_SIZE]; encode(blob,&snapshot);
    bool success=s_port.store && s_port.store(blob,s_port.context);
    ui_concurrency_lock(); s_save_ok=success;
    if(success) s_loaded=true;
    if(success && snapshot.revision==s_current.revision) s_dirty=false;
    s_working=false; ui_concurrency_unlock();
}
bool ui_preferences_dirty(void) { ui_concurrency_lock(); bool value=s_dirty; ui_concurrency_unlock(); return value; }
bool ui_preferences_last_save_ok(void) { ui_concurrency_lock(); bool value=s_save_ok; ui_concurrency_unlock(); return value; }
ui_preferences_storage_state_t ui_preferences_storage_state(void)
{
    ui_concurrency_lock();
    ui_preferences_storage_state_t state=!s_port.store || !s_port.request_worker ? UI_PREF_STORAGE_UNCONFIGURED :
        s_dirty ? (s_save_ok ? UI_PREF_STORAGE_PENDING : UI_PREF_STORAGE_FAILED) :
        s_loaded ? UI_PREF_STORAGE_READY : UI_PREF_STORAGE_DEFAULT;
    ui_concurrency_unlock(); return state;
}
