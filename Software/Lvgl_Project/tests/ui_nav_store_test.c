#include "services/ui_nav_store.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static ui_nav_request_t dispatched;
static bool dispatch(const ui_nav_request_t *r,void *context){(void)context;dispatched=*r;return true;}
int main(void)
{
 ui_nav_session_init();ui_navigation_service_init();ui_nav_store_set_capabilities(UI_NAV_CAP_ROUTES);
 ui_nav_session_t session={.version=1,.id=31,.state=UI_NAV_SESSION_ACTIVE,
  .route={.id=14,.revision=3,.point={34.3,108.9,UI_NAV_COORD_WGS84}}};
 assert(ui_nav_session_submit(&session));ui_nav_session_tick(10);
 nav_model_t guide={.session_bound=true,.session_id=31,.route_id=14,.route_revision=2,.step_id=8,
  .coordinate_system=UI_NAV_COORD_WGS84,.guidance_valid=true,.position_valid=true,.heading_valid=true,
  .speed_valid=true,.speed=24,.distance_valid=true,.distance=1.8,.eta_valid=true,.eta_hour=11,.eta_min=5,
  .latitude=34.3,.longitude=108.9,.ui_state=NAV_UI_APPROACH,.sampled_at_ms=10,.stale_after_ms=100};
 ui_navigation_service_submit(&guide);assert(ui_navigation_service_process());ui_nav_snapshot_t snapshot;
 assert(ui_nav_store_get(&snapshot)&&!snapshot.guidance_matches&&!snapshot.guidance.guidance_valid);
 guide.route_revision=3;ui_navigation_service_submit(&guide);assert(ui_navigation_service_process());
 assert(ui_nav_store_get(&snapshot)&&snapshot.guidance_matches&&snapshot.phase==UI_NAV_PHASE_APPROACH);
 guide.step_id=7;ui_navigation_service_submit(&guide);assert(!ui_navigation_service_process());guide.step_id=8;
 guide.exit_position_valid=true;guide.exit_latitude=NAN;ui_navigation_service_submit(&guide);assert(!ui_navigation_service_process());
 guide.exit_position_valid=false;guide.ui_state=NAV_UI_PASSED;ui_navigation_service_submit(&guide);assert(ui_navigation_service_process());
 assert(ui_nav_store_get(&snapshot)&&snapshot.phase==UI_NAV_PHASE_PASSED);
 assert(ui_navigation_service_expire(111));ui_nav_store_get(&snapshot);
 assert(!snapshot.guidance.position_valid&&!snapshot.guidance.guidance_valid&&!snapshot.guidance.distance_valid&&!snapshot.guidance.eta_valid);
 assert(snapshot.capabilities==UI_NAV_CAP_ROUTES);
 ui_nav_backend_t backend={.dispatch=dispatch};assert(ui_nav_session_register_backend(&backend));
 uint32_t id=ui_nav_store_command(&(ui_nav_request_t){.kind=UI_NAV_REQ_LIST,.list=UI_NAV_LIST_RECENT_ROUTES},120);
 assert(id==dispatched.id&&!ui_nav_session_cancel_request(id+1)&&ui_nav_session_busy());
 ui_nav_result_t result={.request_id=id,.code=UI_NAV_OK,.catalog={.kind=UI_NAV_LIST_RECENT_ROUTES}};
 assert(ui_nav_session_complete(&result));ui_nav_session_tick(130);ui_nav_catalog_t catalog;
 assert(ui_nav_session_get_catalog(&catalog,NULL)&&catalog.request_id==id&&!ui_nav_session_busy());
 printf("ui_nav_store_test: PASS (snapshot=%zu, catalog=%zu bytes)\n",sizeof(snapshot),sizeof(catalog));return 0;
}
