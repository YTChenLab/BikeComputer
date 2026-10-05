#include "ui_nav_store.h"
#include "ui/core/ui_concurrency.h"
#include <string.h>
static uint32_t capabilities;
void ui_nav_store_set_capabilities(uint32_t value)
{ui_concurrency_lock();capabilities=value;ui_concurrency_unlock();}
uint32_t ui_nav_store_capabilities(void)
{ui_concurrency_lock();uint32_t value=capabilities;ui_concurrency_unlock();return value;}
uint32_t ui_nav_store_command(const ui_nav_request_t *r,uint32_t now)
{return r?ui_nav_session_request(r,now):0;}
bool ui_nav_store_get(ui_nav_snapshot_t *out)
{
    if(!out)return false;
    memset(out,0,sizeof(*out));ui_nav_session_get(&out->session);
    out->capabilities=ui_nav_store_capabilities();out->busy=ui_nav_session_busy();
    nav_model_t model;uint32_t version=0;
    bool active=out->session.state==UI_NAV_SESSION_ACTIVE||out->session.state==UI_NAV_SESSION_ARRIVED||out->session.state==UI_NAV_SESSION_STOPPING;
    if(ui_navigation_service_get(&model,&version)&&active&&model.session_bound&&
       model.session_id==out->session.id&&model.route_id==out->session.route.id&&
       model.route_revision==out->session.route.revision&&model.coordinate_system==out->session.route.point.system){
        out->guidance=model;out->guidance_matches=true;
    }else{
        out->guidance=(nav_model_t){.session_bound=true,.session_id=out->session.id,
            .route_id=out->session.route.id,.route_revision=out->session.route.revision,
            .coordinate_system=out->session.route.point.system,.zoom=14};
        if(out->session.position_valid){out->guidance.position_valid=true;
            out->guidance.latitude=out->session.position.latitude;out->guidance.longitude=out->session.position.longitude;
            out->guidance.coordinate_system=out->session.position.system;}
    }
    if(!out->guidance.position_valid){out->guidance.distance_valid=false;out->guidance.eta_valid=false;out->guidance.guidance_valid=false;}
    switch(out->guidance.ui_state){
        case NAV_UI_APPROACH:out->phase=UI_NAV_PHASE_APPROACH;break;
        case NAV_UI_IMMINENT:out->phase=UI_NAV_PHASE_NEAR;break;
        case NAV_UI_NOW:out->phase=UI_NAV_PHASE_NOW;break;
        case NAV_UI_PASSED:out->phase=UI_NAV_PHASE_PASSED;break;
        default:out->phase=UI_NAV_PHASE_FAR;break;
    }
    out->revision=out->session.version*2654435761u^version^out->capabilities^(out->busy?0x80000000u:0);
    return true;
}
