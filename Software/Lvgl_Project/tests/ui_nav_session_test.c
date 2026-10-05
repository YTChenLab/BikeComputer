#include "services/ui_nav_session.h"
#include "services/ui_nav_map_provider.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static ui_nav_request_t sent;
static ui_nav_map_request_t map_sent;
static unsigned cancelled;
static bool dispatch(const ui_nav_request_t *r,void *c) { (void)c; sent=*r; return true; }
static bool map_request(const ui_nav_map_request_t *r,void *c) { (void)c; map_sent=*r; return true; }
static void cancel(uint32_t id,void *c) { (void)c; assert(id); cancelled++; }
static ui_nav_item_t route(void)
{ return (ui_nav_item_t){.id=23,.revision=2,.start_allowed=true,.point={34.3,108.9,UI_NAV_COORD_WGS84}}; }
int main(void)
{
    ui_nav_session_init();
    ui_nav_backend_t backend={dispatch,cancel,NULL,NULL}; assert(ui_nav_session_register_backend(&backend));
    ui_nav_request_t plan={.kind=UI_NAV_REQ_PLAN,.destination=route(),.timeout_ms=100};
    uint32_t first=ui_nav_session_request(&plan,10); assert(first && sent.id==first);
    uint32_t second=ui_nav_session_request(&plan,20); assert(second!=first && cancelled==1);
    ui_nav_result_t result={.request_id=first,.code=UI_NAV_OK,.route=route()};
    assert(!ui_nav_session_complete(&result));
    result.request_id=second; assert(ui_nav_session_complete(&result)); assert(!ui_nav_session_complete(&result));
    ui_nav_session_tick(30); ui_nav_session_t session; ui_nav_session_get(&session);
    assert(session.state==UI_NAV_SESSION_PREVIEW && !session.id && session.route.id==23);
    ui_nav_request_t start={.kind=UI_NAV_REQ_START,.route_id=23};
    uint32_t start_id=ui_nav_session_request(&start,31); ui_nav_session_get(&session);
    assert(session.state==UI_NAV_SESSION_STARTING && !ui_nav_session_active());
    result=(ui_nav_result_t){.request_id=start_id,.code=UI_NAV_OK,.session_id=80};
    assert(ui_nav_session_complete(&result)); ui_nav_session_tick(40);
    assert(ui_nav_session_active());
    uint32_t bad_plan=ui_nav_session_request(&plan,41); assert(bad_plan); ui_nav_session_tick(42);
    ui_nav_session_get(&session); assert(session.state==UI_NAV_SESSION_ACTIVE && session.route.id==23);
    ui_nav_result_t last; ui_nav_session_get_result(&last,NULL); assert(last.code==UI_NAV_NO_ROUTE);
    ui_nav_session_get(&session); session.version=100; session.state=UI_NAV_SESSION_ARRIVED;
    assert(ui_nav_session_submit(&session)); assert(!ui_nav_session_submit(&session)); ui_nav_session_tick(43);
    ui_nav_request_t stop={.kind=UI_NAV_REQ_STOP}; uint32_t stop_id=ui_nav_session_request(&stop,44);
    result=(ui_nav_result_t){.request_id=stop_id,.code=UI_NAV_FAILED}; assert(ui_nav_session_complete(&result));
    ui_nav_session_tick(45); assert(ui_nav_session_active());
    stop_id=ui_nav_session_request(&stop,46);
    result=(ui_nav_result_t){.request_id=stop_id,.code=UI_NAV_OK}; assert(ui_nav_session_complete(&result)); ui_nav_session_tick(47);
    assert(!ui_nav_session_active()); session.version=101; assert(!ui_nav_session_submit(&session));
    uint32_t timeout=ui_nav_session_request(&plan,UINT32_MAX-50);
    ui_nav_session_tick(60); ui_nav_session_get_result(&last,NULL); assert(last.code==UI_NAV_TIMEOUT);
    result=(ui_nav_result_t){.request_id=timeout,.code=UI_NAV_OK,.route=route()}; assert(!ui_nav_session_complete(&result));
    assert(!ui_nav_session_busy());
    ui_nav_catalog_t catalog={.kind=UI_NAV_LIST_MAPS,.count=1,.total=1};
    catalog.items[0].progress_valid=true; catalog.items[0].progress_percent=40;
    assert(ui_nav_session_submit_catalog(&catalog,1)); assert(!ui_nav_session_submit_catalog(&catalog,1));
    ui_nav_session_tick(70); uint32_t version; ui_nav_session_get_catalog(&catalog,&version);
    assert(version && catalog.items[0].progress_percent==40);
    catalog.items[0].progress_percent=101; assert(!ui_nav_session_submit_catalog(&catalog,2));
    ui_nav_request_t update={.kind=UI_NAV_REQ_MAP_UPDATE,.list=UI_NAV_LIST_MAPS,.timeout_ms=100};
    uint32_t update_id=ui_nav_session_request(&update,80);
    catalog.request_id=update_id; catalog.items[0].progress_percent=50;
    assert(ui_nav_session_submit_catalog(&catalog,2)); ui_nav_session_tick(180); assert(ui_nav_session_busy());
    ui_nav_session_tick(230); assert(ui_nav_session_busy());
    ui_nav_session_cancel(); catalog.items[0].progress_percent=100;
    assert(!ui_nav_session_submit_catalog(&catalog,3));
    ui_nav_session_register_backend(NULL); uint32_t missing=ui_nav_session_request(&plan,80);
    ui_nav_session_tick(90); ui_nav_session_get_result(&last,NULL); assert(last.request_id==missing && last.code==UI_NAV_UNAVAILABLE);
    assert(ui_nav_session_register_backend(&backend));
    uint32_t stale=ui_nav_session_request(&plan,100);
    session=(ui_nav_session_t){.id=99,.version=102,.source=UI_NAV_SOURCE_PHONE,.state=UI_NAV_SESSION_ACTIVE,.route=route()};
    assert(ui_nav_session_submit(&session)); ui_nav_session_tick(110);
    ui_nav_session_get(&session); assert(session.source==UI_NAV_SOURCE_PHONE && ui_nav_session_active() && !ui_nav_session_busy());
    result=(ui_nav_result_t){.request_id=stale,.code=UI_NAV_OK,.route=route()}; assert(!ui_nav_session_complete(&result));
    ui_nav_map_provider_init(); ui_nav_map_backend_t maps={map_request,cancel,NULL}; ui_nav_map_provider_register(&maps);
    ui_nav_map_request_t mr={.route_id=23,.route_revision=2,.center={34.3,108.9,UI_NAV_COORD_WGS84},.meters_per_pixel=2,.width=480,.height=500};
    uint32_t m1=ui_nav_map_provider_request(&mr,100); uint32_t m2=ui_nav_map_provider_request(&mr,110);
    ui_nav_map_frame_t frame={.request_id=m1,.route_id=23,.route_revision=2,.map_revision=1,.status=UI_NAV_OK,
        .origin=mr.center,.meters_per_pixel=2,.point_count=1}; frame.points[0]=mr.center;
    assert(!ui_nav_map_provider_submit(&frame)); frame.request_id=m2; frame.route_revision=1;
    assert(!ui_nav_map_provider_submit(&frame)); frame.route_revision=2; frame.points[0].system=UI_NAV_COORD_GCJ02;
    assert(!ui_nav_map_provider_submit(&frame)); frame.points[0]=mr.center;
    assert(ui_nav_map_provider_submit(&frame)); assert(ui_nav_map_provider_process(120));
    ui_nav_map_provider_get(&frame,&version); assert(version==1 && frame.request_id==m2);
    ui_nav_map_provider_request(&mr,130); assert(ui_nav_map_provider_busy()); assert(ui_nav_map_provider_process(5130));
    ui_nav_map_provider_get(&frame,NULL); assert(frame.status==UI_NAV_TIMEOUT && !ui_nav_map_provider_busy());
    frame.point_count=UI_NAV_WINDOW_POINT_MAX+1; assert(!ui_nav_map_provider_submit(&frame));
    uint32_t route_only=ui_nav_map_provider_request(&mr,5200);
    frame=(ui_nav_map_frame_t){.request_id=route_only,.route_id=23,.route_revision=2,.status=UI_NAV_NO_MAP,
        .origin=mr.center,.meters_per_pixel=2,.point_count=2}; frame.points[0]=frame.points[1]=mr.center;
    assert(ui_nav_map_provider_submit(&frame)); assert(ui_nav_map_provider_process(5210));
    mr.center.latitude=NAN; assert(ui_nav_map_provider_request(&mr,0)==0);
    mr.center.latitude=34.3;
    uint32_t geometry=ui_nav_map_provider_request(&mr,5300);
    frame=(ui_nav_map_frame_t){.request_id=geometry,.route_id=23,.route_revision=2,.map_revision=3,.status=UI_NAV_OK,
        .origin=mr.center,.meters_per_pixel=2,.area_count=UI_NAV_MAP_AREA_MAX+1};
    assert(!ui_nav_map_provider_submit(&frame));frame.area_count=1;
    frame.areas[0]=(ui_nav_area_t){.kind=UI_NAV_AREA_BUILDING,.count=4,.points={{0,0},{100,0},{100,100},{0,100}}};
    frame.areas[0].points[0].east_m=NAN;assert(!ui_nav_map_provider_submit(&frame));frame.areas[0].points[0].east_m=0;
    frame.areas[0].points[2]=(ui_nav_local_point_t){20,20};assert(!ui_nav_map_provider_submit(&frame));
    frame.areas[0].points[2]=(ui_nav_local_point_t){100,100};frame.label_count=1;
    for(unsigned i=0;i<sizeof(frame.labels[0].text);i++)frame.labels[0].text[i]='x';assert(!ui_nav_map_provider_submit(&frame));
    frame.labels[0].text[0]=0;frame.map_revision=0;assert(!ui_nav_map_provider_submit(&frame));frame.map_revision=3;
    assert(ui_nav_map_provider_submit(&frame));assert(ui_nav_map_provider_process(5310));
    ui_nav_session_init();
    uint32_t import=ui_nav_session_request(&(ui_nav_request_t){.kind=UI_NAV_REQ_IMPORT_ROUTE},5400);
    result=(ui_nav_result_t){.request_id=import,.code=UI_NAV_OK,.route=route()};assert(ui_nav_session_complete(&result));ui_nav_session_tick(5410);
    ui_nav_session_get(&session);assert(session.state==UI_NAV_SESSION_PREVIEW&&session.route.id==23);
    uint32_t storage=ui_nav_session_request(&(ui_nav_request_t){.kind=UI_NAV_REQ_LIST,.list=UI_NAV_LIST_STORAGE},5420);
    result=(ui_nav_result_t){.request_id=storage,.code=UI_NAV_OK,.catalog={.kind=UI_NAV_LIST_STORAGE,.storage_valid=true,.storage_total_kib=100,.storage_free_kib=101}};
    assert(!ui_nav_session_complete(&result));result.catalog.storage_free_kib=99;assert(ui_nav_session_complete(&result));ui_nav_session_tick(5430);
    assert(ui_nav_session_get_catalog(&catalog,NULL)&&catalog.storage_valid&&catalog.storage_free_kib==99&&catalog.request_id==storage);
    assert(ui_nav_session_discard_preview());ui_nav_session_get(&session);assert(session.state==UI_NAV_SESSION_IDLE&&!session.route.id);
    assert(!ui_nav_session_discard_preview());
    printf("ui_nav_session_test: PASS (session=%zu, frame=%zu, catalog=%zu bytes)\n",sizeof(session),sizeof(frame),sizeof(catalog));
    return 0;
}
