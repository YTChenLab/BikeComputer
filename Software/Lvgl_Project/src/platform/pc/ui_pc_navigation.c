#include "ui_pc_navigation.h"
#include "services/ui_nav_session.h"
#include "services/ui_nav_store.h"
#include "services/ui_nav_map_provider.h"
#include "services/ui_navigation_service.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Synthetic geometry for UI acceptance, NOT a Xi'an map or routing engine. */
static struct {
    ui_nav_request_t request;
    ui_nav_item_t route;
    ui_nav_map_request_t map_request;
    uint32_t remaining, elapsed, session, model_elapsed, step;
    uint8_t scenario;
    bool pending, map_pending, deleted[3],used[3];
} s;
static ui_nav_point_t origin(void)
{ return (ui_nav_point_t){34.3416,108.9398,UI_NAV_COORD_WGS84}; }
static ui_nav_point_t offset(ui_nav_point_t p,double x,double y)
{ p.longitude+=x/(111319.49*cos(p.latitude*0.0174532925199433)); p.latitude+=y/111319.49; return p; }
static ui_nav_item_t item(unsigned i)
{
    ui_nav_item_t result={.id=i+1,.revision=1,.point=offset(origin(),400+i*180,600+i*120),
        .ascent_m=96+i*42,.ascent_valid=true,.distance_m=1400+i*600,.duration_s=320+i*130,.distance_valid=true,.duration_valid=true,.start_allowed=true,.demo=true};
    const char *names[]={"城市骑行","连续转弯","环岛路线"};
    snprintf(result.name,sizeof(result.name),"%s",names[i%3]);
    snprintf(result.detail,sizeof(result.detail),"骑行路线");
    result.thumbnail_count=5;result.thumbnail[0]=origin();result.thumbnail[1]=offset(origin(),0,240);
    result.thumbnail[2]=offset(origin(),220,240);result.thumbnail[3]=offset(origin(),220,500);result.thumbnail[4]=result.point;
    return result;
}
static bool dispatch(const ui_nav_request_t *request,void *context)
{
    (void)context;
    s.request=*request; s.pending=true;
    s.remaining=(request->kind==UI_NAV_REQ_PLAN || request->kind==UI_NAV_REQ_LOAD_ROUTE)?600:240;
    if(request->kind==UI_NAV_REQ_REROUTE){s.remaining=700;s.model_elapsed=0;}
    if(request->kind==UI_NAV_REQ_REROUTE){nav_model_t m;uint32_t v;if(ui_navigation_service_get(&m,&v)&&v){m.off_route=true;m.reroute_failed=false;m.guidance_valid=false;m.ui_state=NAV_UI_REROUTING;ui_navigation_service_submit(&m);}}
    return true;
}
static void cancel(uint32_t id,void *context)
{ (void)context; if(s.request.id==id) s.pending=false; }
static bool request_map(const ui_nav_map_request_t *request,void *context)
{ (void)context; s.map_request=*request; s.map_pending=true; return true; }
static void cancel_map(uint32_t id,void *context)
{ (void)context; if(s.map_request.id==id) s.map_pending=false; }
static ui_nav_local_point_t local_point(ui_nav_point_t o,ui_nav_point_t p)
{return (ui_nav_local_point_t){(p.longitude-o.longitude)*111319.49*cos(o.latitude*.01745329252),(p.latitude-o.latitude)*111319.49};}
static void area_rect(ui_nav_map_frame_t *f,float east,float north,float width,float height,ui_nav_area_kind_t kind)
{
    if(f->area_count>=UI_NAV_MAP_AREA_MAX)return;ui_nav_area_t *a=&f->areas[f->area_count++];a->kind=kind;a->count=4;
    ui_nav_local_point_t base=local_point(f->origin,origin());
    a->points[0]=(ui_nav_local_point_t){base.east_m+east,base.north_m+north};
    a->points[1]=(ui_nav_local_point_t){base.east_m+east+width,base.north_m+north+12};
    a->points[2]=(ui_nav_local_point_t){base.east_m+east+width+8,base.north_m+north+height};
    a->points[3]=(ui_nav_local_point_t){base.east_m+east+8,base.north_m+north+height-12};
}
static void map_frame(void)
{
    ui_nav_map_request_t *r=&s.map_request;
    ui_nav_map_frame_t f={.request_id=r->id,.route_id=r->route_id,.route_revision=r->route_revision,.map_revision=2,.origin=r->center,.meters_per_pixel=r->meters_per_pixel,.status=UI_NAV_OK,.demo=true};
    ui_nav_point_t start=origin(),end=s.route.id?s.route.point:item(0).point;
    double dx=(end.longitude-start.longitude)*111319.49*cos(start.latitude*.01745329252),dy=(end.latitude-start.latitude)*111319.49;
    if(r->route_id){f.destination_valid=true;f.destination=end;f.point_count=8;
        f.points[0]=start;f.points[1]=offset(start,0,dy*.40);f.points[2]=offset(start,dx*.35,dy*.40);f.points[3]=offset(start,dx*.35,dy*.65);
        f.points[4]=offset(start,dx*.65,dy*.69);f.points[5]=offset(start,dx*.65,dy*.86);f.points[6]=offset(start,dx,dy*.86);f.points[7]=end;
        if(r->purpose==UI_NAV_MAP_OVERVIEW){f.origin=offset(start,dx*.5,dy*.5);f.meters_per_pixel=fmax(.75,fmax(fabs(dx)/fmax(100,r->width-110),fabs(dy)/fmax(100,r->height-90)));}}
    else {f.point_count=1;f.points[0]=start;}
    for(unsigned i=1;i<f.point_count;i++)f.roads[f.road_count++]=(ui_nav_road_t){f.points[i-1],f.points[i],0};
    /* Irregular synthetic city blocks. No geographic or lane accuracy is implied. */
    const int streets[][4]={
        {-850,230,1000,290},{-780,520,1050,600},{-900,-130,900,-40},
        {-460,-650,-290,1200},{450,-620,570,1140},{770,-700,560,1300},
        {-740,-520,800,880},{-340,880,700,1080}
    };
    for(unsigned i=0;i<8;i++)f.roads[f.road_count++]=(ui_nav_road_t){offset(start,streets[i][0],streets[i][1]),offset(start,streets[i][2],streets[i][3]),i==0||i==4?0:1};
    const int river[][2]={{-420,-650},{-250,-200},{-190,110},{-110,300},{-130,570},{-260,850},{-350,1300}};
    ui_nav_local_point_t base=local_point(f.origin,start);
    for(unsigned i=1;i<7;i++){
        ui_nav_area_t *a=&f.areas[f.area_count++];a->kind=UI_NAV_AREA_WATER;a->count=4;
        a->points[0]=(ui_nav_local_point_t){base.east_m+river[i-1][0]-45,base.north_m+river[i-1][1]};
        a->points[1]=(ui_nav_local_point_t){base.east_m+river[i-1][0]+45,base.north_m+river[i-1][1]};
        a->points[2]=(ui_nav_local_point_t){base.east_m+river[i][0]+45,base.north_m+river[i][1]};
        a->points[3]=(ui_nav_local_point_t){base.east_m+river[i][0]-45,base.north_m+river[i][1]};
        f.roads[f.road_count++]=(ui_nav_road_t){offset(start,river[i-1][0]+72,river[i-1][1]),offset(start,river[i][0]+72,river[i][1]),2};
    }
    area_rect(&f,-440,300,190,240,UI_NAV_AREA_PARK);area_rect(&f,390,70,180,160,UI_NAV_AREA_PARK);
    for(int row=0;row<8;row++)for(int col=0;col<9;col++){
        float east=-530+col*130,north=-280+row*140;
        if((east>-300&&east<-60)||(east<-240&&north>240&&north<540)||(east>340&&east<610&&north>20&&north<230))continue;
        if(f.area_count>=UI_NAV_MAP_AREA_MAX)break;
        area_rect(&f,east+((row+col)%3)*8,north+((col*3+row)%4)*7,42+((row+col)%4)*12,60+((row*3+col)%3)*14,UI_NAV_AREA_BUILDING);
    }
    for(int i=0;i<8&&f.road_count<UI_NAV_MAP_ROAD_MAX;i++){
        float east=-500+i*145,north=-360+(i%3)*160;
        f.roads[f.road_count++]=(ui_nav_road_t){offset(start,east,north),offset(start,east+55,north+760),2};
    }
    const char *names[]={"公园","河道","主路","绿地","支路"};
    float px[]={-340,-155,80,480,560},py[]={420,160,80,150,780};
    for(unsigned i=0;i<5;i++){ui_nav_map_label_t *label=&f.labels[f.label_count++];label->point=(ui_nav_local_point_t){base.east_m+px[i],base.north_m+py[i]};label->kind=i==0||i==3?1:i==1?2:0;snprintf(label->text,sizeof(label->text),"%s",names[i]);}
    ui_nav_map_provider_submit(&f);s.map_pending=false;
}
static void complete(void)
{
    ui_nav_result_t result={.request_id=s.request.id,.code=UI_NAV_OK};
    switch(s.request.kind) {
        case UI_NAV_REQ_LIST:
            if(s.request.list>UI_NAV_LIST_MAPS && s.request.list!=UI_NAV_LIST_RECENT_ROUTES){result.code=UI_NAV_UNSUPPORTED;break;}
            result.catalog.kind=s.request.list; result.catalog.offset=s.request.offset;
            if(s.request.list==UI_NAV_LIST_MAPS) {
                result.catalog.count=1; result.catalog.total=1;
                result.catalog.items[0]=(ui_nav_item_t){.id=1,.revision=1,.demo=true,.map_status=UI_NAV_MAP_MISSING};
                snprintf(result.catalog.items[0].name,64,"本地地图");
                snprintf(result.catalog.items[0].detail,96,"尚未导入地图文件");
            } else {
                for(unsigned i=0;i<3;i++) if(!s.deleted[i] && (s.request.list!=UI_NAV_LIST_RECENT_ROUTES || s.used[i])) {
                    uint32_t ordinal=result.catalog.total++;
                    if(ordinal<s.request.offset) continue;
                    result.catalog.items[result.catalog.count]=item(i);
                    result.catalog.items[result.catalog.count++].saved=s.request.list==UI_NAV_LIST_ROUTES;
                }
            }
            break;
        case UI_NAV_REQ_PLAN: case UI_NAV_REQ_LOAD_ROUTE:
            if(s.request.kind==UI_NAV_REQ_LOAD_ROUTE && (s.request.route_id<1 || s.request.route_id>3 || s.deleted[s.request.route_id-1])) {
                result.code=UI_NAV_NO_ROUTE; break;
            }
            s.route=s.request.kind==UI_NAV_REQ_LOAD_ROUTE?item((s.request.route_id-1)%3):s.request.destination;
            s.route.id=s.request.kind==UI_NAV_REQ_PLAN?100+s.route.id:s.route.id;
            s.route.saved=s.request.kind==UI_NAV_REQ_LOAD_ROUTE;
            s.route.revision=1; s.route.start_allowed=s.route.demo=true;
            s.route.distance_m=1800; s.route.duration_s=420; s.route.distance_valid=s.route.duration_valid=true;
            result.route=s.route; break;
        case UI_NAV_REQ_START: case UI_NAV_REQ_RESTORE:
            if(s.route.id>=1&&s.route.id<=3)s.used[s.route.id-1]=true;
            result.session_id=++s.session; s.step=1; s.scenario=0; s.model_elapsed=900;
            break;
        case UI_NAV_REQ_STOP: break;
        case UI_NAV_REQ_DELETE_ROUTE:
            if(s.request.route_id>=1 && s.request.route_id<=3) s.deleted[s.request.route_id-1]=true;
            else result.code=UI_NAV_NO_ROUTE;
            break;
        case UI_NAV_REQ_MAP_CHECK:
            result.catalog.kind=UI_NAV_LIST_MAPS; result.catalog.count=result.catalog.total=1;
            result.catalog.items[0]=(ui_nav_item_t){.id=1,.revision=1,.demo=true,.map_status=UI_NAV_MAP_MISSING};
            snprintf(result.catalog.items[0].name,64,"本地地图");
            snprintf(result.catalog.items[0].detail,96,"地图文件尚未导入");
            break;
        case UI_NAV_REQ_IMPORT_ROUTE: case UI_NAV_REQ_IMPORT_MAP: case UI_NAV_REQ_MAP_UPDATE: result.code=UI_NAV_UNSUPPORTED;break;
        case UI_NAV_REQ_REROUTE: {ui_nav_session_t current;ui_nav_session_get(&current);
            current.route.revision++;s.route=current.route;current.version+=2;ui_nav_session_submit(&current);}
            s.scenario=0; s.step++; s.model_elapsed=900; break;
    }
    ui_nav_session_complete(&result); s.pending=false;
}
static void publish_model(void)
{
    ui_nav_session_t session; ui_nav_session_get(&session);
    if(session.state!=UI_NAV_SESSION_ACTIVE && session.state!=UI_NAV_SESSION_ARRIVED) return;
    static const nav_ui_state_t phases[]={NAV_UI_CRUISE,NAV_UI_APPROACH,NAV_UI_IMMINENT,NAV_UI_NOW,
        NAV_UI_CRUISE,NAV_UI_APPROACH,NAV_UI_REROUTING,NAV_UI_GPS_WEAK,NAV_UI_CRUISE,NAV_UI_ARRIVED};
    nav_model_t model={.demo=true,.session_bound=true,.session_id=session.id,
        .route_id=session.route.id,.route_revision=session.route.revision,.step_id=s.step,
        .speed=25,.speed_valid=true,.distance=1.8f,.distance_valid=true,.eta_valid=true,.eta_hour=11,.eta_min=5,
        .latitude=origin().latitude,.longitude=origin().longitude,.coordinate_system=UI_NAV_COORD_WGS84,
        .heading=0,.heading_valid=true,.position_valid=true,.stale_after_ms=5000,.sampled_at_ms=ui_nav_session_now(),
        .zoom=14,.direction=NAV_MANEUVER_RIGHT,.ui_state=phases[s.scenario%10],.guidance_valid=true,
        .direction_distance=420,.junction_valid=true,.next_valid=true,.next_direction=NAV_MANEUVER_RIGHT,.next_distance_m=85};
    ui_nav_point_t junction=offset(origin(),0,240);
    ui_nav_point_t exit=offset(junction,140,0);model.exit_position_valid=true;model.exit_latitude=exit.latitude;model.exit_longitude=exit.longitude;
    model.junction_latitude=junction.latitude; model.junction_longitude=junction.longitude;
    snprintf(model.road_name,sizeof(model.road_name),"主路");
    if(model.ui_state==NAV_UI_APPROACH) model.direction_distance=120;
    if(model.ui_state==NAV_UI_IMMINENT) model.direction_distance=35;
    if(model.ui_state==NAV_UI_NOW) model.direction_distance=0;
    if(s.scenario==5) { model.direction=NAV_MANEUVER_ROUNDABOUT; model.exit_valid=true; model.roundabout_exit=3; }
    if(s.scenario==6) { model.off_route=true; model.reroute_failed=true; model.guidance_valid=false; }
    if(s.scenario==7) { model.position_valid=false; model.guidance_valid=false; model.heading_valid=false; }
    if(s.pending&&s.request.kind==UI_NAV_REQ_REROUTE){model.off_route=true;model.reroute_failed=false;model.guidance_valid=false;model.ui_state=NAV_UI_REROUTING;}
    if(s.scenario==9) {
        model.direction=NAV_MANEUVER_ARRIVE; model.distance=0; model.direction_distance=0;
        session.state=UI_NAV_SESSION_ARRIVED;session.elapsed_valid=session.travelled_valid=session.ascent_valid=true;session.elapsed_s=420;session.travelled_m=1800;session.ascent_m=96;session.version+=2; ui_nav_session_submit(&session);
    }
    ui_navigation_service_submit(&model);
}
static void tick(uint32_t elapsed,void *context)
{
    (void)context; s.elapsed+=elapsed;
    if(s.pending) { if(elapsed>=s.remaining) complete(); else s.remaining-=elapsed; }
    if(s.map_pending) map_frame();
    s.model_elapsed+=elapsed;
    if(s.model_elapsed>=900 && ui_nav_session_active()) { s.model_elapsed=0; publish_model(); }
    ui_nav_session_t session; ui_nav_session_get(&session);
    if(!session.position_valid && session.state==UI_NAV_SESSION_IDLE && session.source!=UI_NAV_SOURCE_DEMO) {
        session.position_valid=true; session.position=origin(); session.source=UI_NAV_SOURCE_DEMO;
        session.version+=2; ui_nav_session_submit(&session);
    }
}
void ui_pc_navigation_register(void)
{
    memset(&s,0,sizeof(s));
    ui_nav_store_set_capabilities(UI_NAV_CAP_ROUTES|UI_NAV_CAP_MAPS|UI_NAV_CAP_REROUTE);
    ui_navigation_service_register_demo_step(ui_pc_navigation_next_scenario);
    ui_nav_backend_t backend={dispatch,cancel,tick,NULL}; ui_nav_session_register_backend(&backend);
    ui_nav_map_backend_t maps={request_map,cancel_map,NULL}; ui_nav_map_provider_register(&maps);
}
void ui_pc_navigation_next_scenario(void)
{ if(!ui_nav_session_active()) return; s.scenario=(s.scenario+1)%10; if(s.scenario==4||s.scenario==5) s.step++; publish_model(); }
