#include "services/ui_preferences.h"
#include "services/ui_device_info.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t stored[UI_PREFERENCES_BLOB_SIZE];
static bool available, fail_store, inject_change;
static unsigned requests;
static bool load(uint8_t *blob,void *context) { (void)context; memcpy(blob,stored,sizeof(stored)); return available; }
static bool store(const uint8_t *blob,void *context)
{
    (void)context;
    if(fail_store) return false;
    memcpy(stored,blob,sizeof(stored)); available=true;
    if(inject_change) {
        inject_change=false;
        ui_preferences_t next; assert(ui_preferences_get(&next)); next.sound=35;
        assert(ui_preferences_submit(&next)); assert(ui_preferences_process(20,NULL));
    }
    return true;
}
static bool request(void *context) { (void)context; ++requests; return true; }
static void checksum(uint8_t *blob)
{
    uint32_t value=UINT32_MAX;
    for(unsigned i=0;i<124;++i) {
        value^=blob[i];
        for(unsigned j=0;j<8;++j) value=(value>>1)^(UINT32_C(0xEDB88320)&(0U-(value&1)));
    }
    value=~value;
    for(unsigned i=0;i<4;++i) blob[124+i]=(uint8_t)(value>>(8*i));
}
int main(void)
{
    ui_preferences_port_t port={.load=load,.store=store,.request_worker=request};
    assert(ui_preferences_register(&port)); ui_preferences_init();
    assert(ui_preferences_storage_state()==UI_PREF_STORAGE_DEFAULT);
    ui_preferences_t p; assert(ui_preferences_get(&p)); assert(p.brightness==75);
    strcpy(p.user_name,"王磊"); p.sound=50; p.reduced_motion=true;
    assert(ui_preferences_submit(&p)); assert(ui_preferences_process(10,&p));
    assert(requests==1 && ui_preferences_dirty());
    assert(ui_preferences_storage_state()==UI_PREF_STORAGE_PENDING);
    inject_change=true; ui_preferences_worker(); assert(ui_preferences_dirty());
    assert(!ui_preferences_process(5011,&p)); assert(requests==2); ui_preferences_worker();
    assert(!ui_preferences_dirty() && ui_preferences_last_save_ok());
    assert(ui_preferences_storage_state()==UI_PREF_STORAGE_READY);
    ui_preferences_init(); assert(ui_preferences_get(&p));
    assert(strcmp(p.user_name,"王磊")==0 && p.sound==35 && p.reduced_motion);
    uint8_t legacy[UI_PREFERENCES_BLOB_SIZE];
    memcpy(legacy,stored,sizeof(legacy)); legacy[4]=1;
    memset(legacy+116,0,8); checksum(legacy);
    ui_preferences_t migrated;
    assert(ui_preferences_decode(legacy,&migrated));
    assert(migrated.sound==35 && migrated.brightness==75 && migrated.reduced_motion);
    assert(strcmp(migrated.user_name,"王磊")==0);
    assert(memcmp(migrated.heart_zone_bounds,p.heart_zone_bounds,sizeof(p.heart_zone_bounds))==0);
    assert(migrated.dashboard_preset==UI_DASHBOARD_CRUISE);
    assert(ui_dashboard_layout_valid(migrated.dashboard_preset,migrated.dashboard_fields));
    p.dashboard_preset=UI_DASHBOARD_CLIMB;
    assert(ui_dashboard_preset_fields(p.dashboard_preset,p.dashboard_fields));
    assert(ui_preferences_submit(&p)); assert(ui_preferences_process(100,&p));
    ui_preferences_worker(); assert(!ui_preferences_dirty() && stored[4]==2);
    ui_preferences_init(); assert(ui_preferences_get(&p));
    assert(p.dashboard_preset==UI_DASHBOARD_CLIMB);
    assert(ui_dashboard_layout_valid(p.dashboard_preset,p.dashboard_fields));
    ui_preferences_t omitted=p;
    memset(omitted.dashboard_fields,0,sizeof(omitted.dashboard_fields)); omitted.dashboard_preset=0;
    assert(ui_preferences_submit(&omitted)); assert(ui_preferences_process(101,&p));
    assert(p.dashboard_preset==UI_DASHBOARD_CLIMB); ui_preferences_worker();
    ui_preferences_t invalid=p; invalid.dashboard_fields[1]=invalid.dashboard_fields[0];
    assert(!ui_preferences_submit(&invalid));
    invalid=p; invalid.dashboard_fields[0]=UI_RIDE_FIELD_BATTERY;
    assert(!ui_preferences_submit(&invalid));
    invalid=p; invalid.dashboard_preset=UI_DASHBOARD_TRAINING;
    assert(!ui_preferences_submit(&invalid));
    uint8_t broken[UI_PREFERENCES_BLOB_SIZE]; memcpy(broken,stored,sizeof(broken));
    broken[117]=broken[116]; checksum(broken);
    assert(!ui_preferences_decode(broken,&invalid));
    memcpy(broken,stored,sizeof(broken)); broken[123]=1; checksum(broken);
    assert(!ui_preferences_decode(broken,&invalid));
    p.heart_zone_bounds[1]=p.heart_zone_bounds[0]; assert(!ui_preferences_submit(&p));
    assert(ui_preferences_get(&p)); p.idle_sleep_ms=1; assert(!ui_preferences_submit(&p));
    assert(ui_preferences_get(&p)); p.sound=60; fail_store=true;
    assert(ui_preferences_submit(&p)); assert(ui_preferences_process(6000,NULL)); ui_preferences_worker();
    assert(ui_preferences_dirty() && !ui_preferences_last_save_ok());
    assert(ui_preferences_storage_state()==UI_PREF_STORAGE_FAILED);
    fail_store=false; ui_preferences_process(11001,NULL); ui_preferences_worker(); assert(!ui_preferences_dirty());
    stored[30]^=1; ui_preferences_init(); assert(ui_preferences_get(&p)); assert(p.sound==62);
    ui_device_info_t device={.device_model="STM32 Bike",.health="OK",.storage_ready=true};
    assert(ui_device_info_submit(&device)); strcpy(device.device_model,"mutated");
    assert(ui_device_info_process(&device)); assert(strcmp(device.device_model,"STM32 Bike")==0);
    assert(!ui_device_info_process(&device));
    puts("ui_preferences_test: PASS"); return 0;
}
