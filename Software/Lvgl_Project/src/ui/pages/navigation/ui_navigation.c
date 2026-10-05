#include "ui_navigation.h"
#include "ui_nav_workspace.h"
#include "services/ui_nav_store.h"
#include "services/ui_nav_map_provider.h"
#include "../../components/navigation/ui_nav_layout.h"
#include "../../components/navigation/ui_nav_visual.h"
#include "../../components/navigation/ui_nav_search_panel.h"
#include "../../core/ui_motion.h"
#include "../../core/ui_bottom_surface.h"
#include "../../core/ui_event_bus.h"
#include "../../core/ui_text.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define NAV_EVENT_SOURCE 0x4E415600UL
static struct {
 lv_obj_t *screen,*map_status,*capsule,*capsule_title,*capsule_detail,*attribution;
 ui_nav_map_t map;nav_model_t model;bool active,created,live,was_preview,card_entered;
 ui_navigation_card_t selected_card;
 uint32_t model_revision,map_version,requested_at,route_id,route_revision,success_until;
 uint32_t notification_key,notification_keys[12];uint8_t notification_index;
 ui_nav_map_request_t last_request;bool request_cached;
 int32_t search_progress,candidates_progress;ui_nav_point_t search_previous_center;nav_state_t search_previous_state;bool search_restore;
 void (*exit_callback)(void);void (*routes_callback)(ui_nav_list_t);
} s_navigation;
static void select_card(ui_navigation_card_t card,bool entered)
{
 s_navigation.selected_card=card;s_navigation.card_entered=entered;
 bool map=card==UI_NAV_CARD_MAP;
 lv_obj_set_style_border_color(s_navigation.map.root,lv_color_hex(map&&!entered?0xFFD900:0x4B4B4B),0);
 lv_obj_set_style_border_width(s_navigation.map.root,map&&!entered?2:1,0);
 ui_bottom_surface_set_dynamic_focus(!map&&!entered);
 ui_nav_workspace_set_controls_active(!map&&entered);
 ui_nav_map_set_focus(&s_navigation.map,s_navigation.map.focused_control,map&&entered);
}
ui_navigation_card_t ui_navigation_selected_card(void){return s_navigation.selected_card;}
bool ui_navigation_card_entered(void){return s_navigation.card_entered;}
static void exit_page(void){if(s_navigation.exit_callback)s_navigation.exit_callback();}
static void map_picked(ui_nav_point_t point,void *context)
{
 (void)context;ui_nav_item_t place={.id=3,.revision=1,.point=point};
 snprintf(place.name,sizeof(place.name),"地图选点");
 snprintf(place.detail,sizeof(place.detail),"%.5f, %.5f",point.latitude,point.longitude);
 ui_nav_workspace_preview_destination(&place);select_card(UI_NAV_CARD_DOCK,true);
 s_navigation.request_cached=false;s_navigation.requested_at=0;
}
static const char *direction_name(nav_direction_t direction)
{
    switch(direction) {
        case NAV_MANEUVER_LEFT: return "左转";
        case NAV_MANEUVER_RIGHT: return "右转";
        case NAV_MANEUVER_SLIGHT_LEFT: return "轻微左转";
        case NAV_MANEUVER_SLIGHT_RIGHT: return "轻微右转";
        case NAV_MANEUVER_SHARP_LEFT: return "急左转";
        case NAV_MANEUVER_SHARP_RIGHT: return "急右转";
        case NAV_MANEUVER_U_TURN: return "掉头";
        case NAV_MANEUVER_ROUNDABOUT: return "进入环岛";
        case NAV_MANEUVER_ARRIVE: return "到达目的地";
        case NAV_MANEUVER_KEEP_LEFT:return "靠左";
        case NAV_MANEUVER_KEEP_RIGHT:return "靠右";
        case NAV_MANEUVER_FORK_LEFT:return "左侧分叉";
        case NAV_MANEUVER_FORK_RIGHT:return "右侧分叉";
        case NAV_MANEUVER_EXIT_ROUNDABOUT:return "出环岛";
        default: return "直行";
    }
}

static ui_navigation_turn_t event_direction(nav_direction_t direction)
{
    if(direction == NAV_MANEUVER_LEFT || direction==NAV_MANEUVER_SLIGHT_LEFT || direction==NAV_MANEUVER_SHARP_LEFT || direction==NAV_MANEUVER_KEEP_LEFT || direction==NAV_MANEUVER_FORK_LEFT) return UI_NAVIGATION_TURN_LEFT;
    if(direction == NAV_MANEUVER_RIGHT || direction==NAV_MANEUVER_SLIGHT_RIGHT || direction==NAV_MANEUVER_SHARP_RIGHT || direction==NAV_MANEUVER_KEEP_RIGHT || direction==NAV_MANEUVER_FORK_RIGHT) return UI_NAVIGATION_TURN_RIGHT;
    if(direction == NAV_DIRECTION_U_TURN) return UI_NAVIGATION_TURN_U_TURN;
    if(direction==NAV_MANEUVER_ROUNDABOUT||direction==NAV_MANEUVER_EXIT_ROUNDABOUT)return UI_NAVIGATION_TURN_ROUNDABOUT;
    return UI_NAVIGATION_TURN_STRAIGHT;
}

static void publish_turn_event(const nav_model_t *model)
{
    if(model == NULL) return;
    ui_event_t event = {0};
    event.type = UI_EVENT_NAVIGATION_TURN;
    event.priority = UI_EVENT_PRIORITY_P1_RIDING_SAFETY;
    event.source_id = NAV_EVENT_SOURCE;
    event.dedupe_key = model->session_bound?s_navigation.notification_key:NAV_EVENT_SOURCE;
    event.timeout_ms = 5000U;
    event.value = (int32_t)event_direction(model->direction);
    snprintf(event.title, sizeof(event.title), "前方转向");
    snprintf(event.message, sizeof(event.message), "前方 %lu m %s",
             (unsigned long)model->direction_distance,
             direction_name(model->direction));
    snprintf(event.primary_action, sizeof(event.primary_action), "查看");
    (void)ui_event_bus_publish(&event);
}

static void navigation_model_changed(const nav_model_t *model,
                                     uint32_t version, void *user_data)
{
    (void)version;
    (void)user_data;
    if(model == NULL) return;
    ui_nav_session_t session; ui_nav_session_get(&session);
    if(!model->session_bound || !ui_nav_session_active() || model->session_id!=session.id ||
        model->route_id!=session.route.id || model->route_revision!=session.route.revision) return;
    uint32_t key=model->session_id*2654435761u ^ model->route_revision*2246822519u ^
        model->step_id*3266489917u ^ (uint32_t)model->ui_state;
    bool seen=false;
    for(unsigned i=0;i<12;i++) if(s_navigation.notification_keys[i]==key) seen=true;
    bool remind=model->guidance_valid && model->step_id &&
        (model->ui_state==NAV_UI_APPROACH||model->ui_state==NAV_UI_IMMINENT||model->ui_state==NAV_UI_NOW) && !seen;
    if(remind) {
        s_navigation.notification_key=key;
        s_navigation.notification_keys[s_navigation.notification_index++%12]=key;
    }
    if(s_navigation.active) { /* Render on the UI timer from the store. */ }
    else {
        s_navigation.model = *model;
        if(remind) publish_turn_event(model);
    }
}


static void visual_status(void)
{
 const nav_model_t *m=&s_navigation.model;const char *title=NULL,*detail="";
 if(s_navigation.live){
  if(m->off_route||m->ui_state==NAV_UI_REROUTING){title="已偏离路线";detail=m->reroute_failed?"重新规划失败":
    (ui_nav_store_capabilities()&UI_NAV_CAP_REROUTE)?"正在重新规划…":"当前不支持独立重新规划";}
  else if(!m->position_valid){title="定位暂时不可用";detail="保留最后有效位置";}
  else if(m->ui_state==NAV_UI_GPS_WEAK){title="GNSS 信号较弱";detail="定位可能存在偏差";}
  else if(m->ui_state!=NAV_UI_ARRIVED&&(int32_t)(s_navigation.success_until-lv_tick_get())>0){title="路线已重新规划";detail="新路线已生效";}
 }
 if(title&&ui_nav_workspace_map_visible()){
  lv_obj_clear_flag(s_navigation.capsule,LV_OBJ_FLAG_HIDDEN);
  ui_text_set_label(s_navigation.capsule_title,title,24);ui_text_set_label(s_navigation.capsule_detail,detail,20);
 }else lv_obj_add_flag(s_navigation.capsule,LV_OBJ_FLAG_HIDDEN);
}
static void map_tick(lv_timer_t *timer)
{
 (void)timer;if(!s_navigation.active)return;
 if(ui_nav_search_panel_active()||s_navigation.search_progress)return; /* Freeze map camera while entering text. */
 ui_nav_workspace_refresh();ui_nav_snapshot_t snapshot;ui_nav_store_get(&snapshot);
 const ui_nav_session_t *session=&snapshot.session;s_navigation.live=ui_nav_session_active();
 if(!ui_nav_workspace_map_visible()){lv_obj_add_flag(s_navigation.map.root,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(s_navigation.map_status,LV_OBJ_FLAG_HIDDEN);visual_status();return;}
 lv_obj_clear_flag(s_navigation.map.root,LV_OBJ_FLAG_HIDDEN);
 bool preview=session->state==UI_NAV_SESSION_PREVIEW||session->state==UI_NAV_SESSION_STARTING;
 if(s_navigation.was_preview&&!preview&&s_navigation.live){ui_nav_map_set_overview(&s_navigation.map,false);ui_nav_workspace_rebuild();s_navigation.requested_at=0;}
 s_navigation.was_preview=preview;
 uint32_t route=(preview||s_navigation.live)?session->route.id:0,revision=route?session->route.revision:0;
 if(route!=s_navigation.route_id||revision!=s_navigation.route_revision){
  bool new_version=s_navigation.route_id==route&&s_navigation.route_revision&&revision>s_navigation.route_revision;
  if(new_version)s_navigation.success_until=lv_tick_get()+1800;
  s_navigation.route_id=route;s_navigation.route_revision=revision;s_navigation.requested_at=0;s_navigation.request_cached=false;
  s_navigation.map.route_point_count=s_navigation.map.road_count=0;s_navigation.map.provider_ready=false;
  if(preview)ui_nav_map_set_overview(&s_navigation.map,true);
  else if(s_navigation.map.state==NAV_OVERVIEW)ui_nav_map_set_overview(&s_navigation.map,false);
 }
 if(snapshot.revision!=s_navigation.model_revision){s_navigation.model_revision=snapshot.revision;
  s_navigation.model=snapshot.guidance;ui_nav_map_apply_model(&s_navigation.map,&s_navigation.model);}
 ui_nav_map_camera(&s_navigation.map,&s_navigation.model,lv_tick_get());
 /* Keep the bounded map snapshot out of the small embedded UI task stack. */
 static ui_nav_map_frame_t frame;uint32_t version;ui_nav_map_provider_get(&frame,&version);
 /* Do not rebase the raster/local coordinate frame under a held finger.
  * Guidance and the rider marker continue updating; apply geometry on release. */
 if(!s_navigation.map.pointer_down&&version!=s_navigation.map_version&&frame.route_id==route&&frame.route_revision==revision){
  s_navigation.map_version=version;ui_nav_map_apply_frame(&s_navigation.map,&frame);ui_nav_map_apply_model(&s_navigation.map,&s_navigation.model);
  if(frame.status==UI_NAV_OK&&!frame.demo)lv_obj_clear_flag(s_navigation.attribution,LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_navigation.attribution,LV_OBJ_FLAG_HIDDEN);
  const char *t=ui_nav_result_text(frame.status);
  if(frame.status==UI_NAV_OK)lv_obj_add_flag(s_navigation.map_status,LV_OBJ_FLAG_HIDDEN);
  else {if(frame.status==UI_NAV_NO_MAP&&frame.point_count>=2)t="暂无底图";
   ui_text_set_label(s_navigation.map_status,t,16);lv_obj_clear_flag(s_navigation.map_status,LV_OBJ_FLAG_HIDDEN);}
 }
 uint32_t now=lv_tick_get();
 if(!s_navigation.map.pointer_down&&!ui_nav_map_provider_busy()&&(!s_navigation.requested_at||now-s_navigation.requested_at>=140)){
  ui_nav_point_t center=s_navigation.map.origin_valid?ui_nav_map_view_center(&s_navigation.map):session->position;
  bool valid=s_navigation.map.origin_valid||session->position_valid;
  if(s_navigation.map.state!=NAV_EXPLORE&&snapshot.guidance.position_valid){center=(ui_nav_point_t){snapshot.guidance.latitude,snapshot.guidance.longitude,snapshot.guidance.coordinate_system};valid=true;}
  if(!valid&&route){center=session->route.point;valid=true;}
  if(valid){ui_nav_map_request_t request={.route_id=route,.route_revision=revision,.center=center,
    .meters_per_pixel=s_navigation.map.raster_valid?s_navigation.map.meters_per_pixel*256.f/s_navigation.map.render_scale:3,.width=464,.height=s_navigation.map.safe_height,.heading=s_navigation.map.heading,
    .purpose=(s_navigation.map.state==NAV_OVERVIEW||(preview&&s_navigation.map.state!=NAV_EXPLORE))?UI_NAV_MAP_OVERVIEW:s_navigation.map.state==NAV_EXPLORE?UI_NAV_MAP_BROWSE:UI_NAV_MAP_FOLLOW};
   if(request.purpose!=UI_NAV_MAP_OVERVIEW)ui_nav_map_request_coverage(&s_navigation.map,&request);
   const ui_nav_map_request_t *old=&s_navigation.last_request;
   double east=(request.center.longitude-old->center.longitude)*92000,north=(request.center.latitude-old->center.latitude)*111319;
   bool same=s_navigation.request_cached&&request.route_id==old->route_id&&request.route_revision==old->route_revision&&request.purpose==old->purpose&&
    (s_navigation.map.raster_valid||(request.width==old->width&&request.height==old->height))&&request.center.system==old->center.system&&
    fabs(request.meters_per_pixel-old->meters_per_pixel)<old->meters_per_pixel*.2f&&
    hypot(east,north)<fmax(12,fmin(request.width,request.height)*request.meters_per_pixel*.08);
   if(!same||!s_navigation.map.provider_ready){ui_nav_map_provider_request(&request,now);s_navigation.last_request=request;s_navigation.request_cached=true;}
   s_navigation.requested_at=now?now:1;
  }else {ui_text_set_label(s_navigation.map_status,"等待定位或离线地图数据",16);lv_obj_clear_flag(s_navigation.map_status,LV_OBJ_FLAG_HIDDEN);}
 }
 visual_status();
}
void ui_navigation_create(lv_obj_t *screen)
{
 if(!screen||s_navigation.created)return;memset(&s_navigation,0,sizeof(s_navigation));s_navigation.screen=screen;
 if(!ui_bottom_surface_is_created())ui_bottom_surface_create(screen);
 ui_nav_map_create(&s_navigation.map,screen,8,62);s_navigation.map.strict_geometry=true;
 s_navigation.map.pick_callback=map_picked;
 s_navigation.attribution=lv_label_create(s_navigation.map.map_hud);
 lv_label_set_text(s_navigation.attribution,"(c) OpenStreetMap contributors");
 lv_obj_set_pos(s_navigation.attribution,152,524);lv_obj_set_width(s_navigation.attribution,280);
 lv_obj_set_style_text_font(s_navigation.attribution,&lv_font_montserrat_12,0);
 lv_obj_set_style_text_color(s_navigation.attribution,lv_color_hex(0xA9ABAE),0);
 lv_obj_set_style_text_align(s_navigation.attribution,LV_TEXT_ALIGN_RIGHT,0);
 lv_obj_set_style_bg_color(s_navigation.attribution,lv_color_black(),0);lv_obj_set_style_bg_opa(s_navigation.attribution,LV_OPA_60,0);
 lv_obj_add_flag(s_navigation.attribution,LV_OBJ_FLAG_HIDDEN);
 s_navigation.map_status=ui_nav_label(s_navigation.map.root,12,10,306,"正在加载地图…",16,true);
 lv_obj_set_style_bg_color(s_navigation.map_status,lv_color_black(),0);lv_obj_set_style_bg_opa(s_navigation.map_status,LV_OPA_60,0);
 s_navigation.capsule=ui_nav_surface(s_navigation.map.root,60,18,306,76,true);lv_obj_set_style_radius(s_navigation.capsule,38,0);
 lv_obj_set_style_bg_color(s_navigation.capsule,lv_color_hex(0xFFD900),0);lv_obj_set_style_border_color(s_navigation.capsule,lv_color_hex(0xFFD900),0);
 ui_nav_icon(s_navigation.capsule,18,20,36,NAV_ICON_WARNING,0x000000);
 s_navigation.capsule_title=ui_nav_label(s_navigation.capsule,62,10,230,"",24,false);s_navigation.capsule_detail=ui_nav_label(s_navigation.capsule,62,43,230,"",20,false);
 lv_obj_set_style_text_color(s_navigation.capsule_title,lv_color_black(),0);lv_obj_set_style_text_color(s_navigation.capsule_detail,lv_color_black(),0);
 lv_label_set_long_mode(s_navigation.capsule_detail,LV_LABEL_LONG_DOT);lv_obj_set_height(s_navigation.capsule_detail,26);lv_obj_add_flag(s_navigation.capsule,LV_OBJ_FLAG_HIDDEN);
 ui_nav_workspace_create(screen,NULL,exit_page);ui_nav_workspace_bind_map(&s_navigation.map);
 ui_nav_search_panel_create(screen);
 select_card(UI_NAV_CARD_MAP,false);
 ui_navigation_service_subscribe(navigation_model_changed,NULL);lv_timer_create(map_tick,100,NULL);s_navigation.created=true;
}
void ui_navigation_set_active(bool active)
{
 if(!s_navigation.created)return;if(!active&&ui_nav_search_panel_active())ui_navigation_close_search();s_navigation.active=active;ui_nav_workspace_set_active(active);
 if(active){select_card(UI_NAV_CARD_MAP,false);s_navigation.requested_at=0;map_tick(NULL);}
 else ui_bottom_surface_set_dynamic_focus(false);
}
bool ui_navigation_contains_point(int32_t x,int32_t y){if(ui_nav_search_panel_active())return x>=0&&x<480&&y>=62&&y<838;return s_navigation.created&&(ui_nav_workspace_contains_point(x,y)||(ui_nav_workspace_map_visible()&&ui_nav_map_contains_point(&s_navigation.map,x,y)));}
void ui_navigation_handle_pointer(const ui_input_pointer_event_t *event)
{
 if(ui_nav_search_panel_active()||s_navigation.search_progress)return;
 if(event->type==UI_INPUT_TOUCH_BEGIN&&ui_nav_workspace_map_visible()){
  if(ui_nav_workspace_contains_point(event->x,event->y))select_card(UI_NAV_CARD_DOCK,true);
  else if(ui_nav_map_contains_point(&s_navigation.map,event->x,event->y))select_card(UI_NAV_CARD_MAP,true);
 }
 ui_nav_workspace_handle_pointer(event);
}
void ui_navigation_cancel_pointer(void){ui_nav_workspace_cancel_pointer();if(s_navigation.created)lv_obj_send_event(s_navigation.map.root,LV_EVENT_PRESS_LOST,NULL);}
bool ui_navigation_handle_input(ui_input_event_t input)
{
 if(!s_navigation.created)return false;
 if(ui_nav_search_panel_active())return ui_nav_search_panel_input(input);
 if(!ui_nav_workspace_map_visible())return ui_nav_workspace_handle_input(input);
 bool rotate=input==UI_INPUT_ENCODER_CW||input==UI_INPUT_ENCODER_CCW;
 if(!rotate&&input!=UI_INPUT_ENCODER_PRESS)return false;
 if(!s_navigation.card_entered){
  if(rotate)select_card(s_navigation.selected_card==UI_NAV_CARD_MAP?UI_NAV_CARD_DOCK:UI_NAV_CARD_MAP,false);
  else select_card(s_navigation.selected_card,true);
  return true;
 }
 if(s_navigation.selected_card==UI_NAV_CARD_MAP){
  if(rotate){int delta=input==UI_INPUT_ENCODER_CW?1:-1;
   ui_nav_map_set_focus(&s_navigation.map,(s_navigation.map.focused_control+UI_NAV_FOCUS_COUNT+delta)%UI_NAV_FOCUS_COUNT,true);
  }else ui_nav_map_activate_focus(&s_navigation.map);
  return true;
 }
 if(ui_nav_workspace_handle_input(input))return true;
 if(ui_nav_workspace_view()==UI_NAV_VIEW_LIVE){
  if(rotate)select_card(UI_NAV_CARD_MAP,false);else ui_navigation_open_menu();
  return true;
 }
 return false;
}
bool ui_navigation_handle_back(void)
{
 if(ui_nav_search_panel_active())return ui_nav_search_panel_back();
 ui_nav_view_t view=ui_nav_workspace_view();
 if(view==UI_NAV_VIEW_MENU){ui_nav_workspace_back();select_card(UI_NAV_CARD_DOCK,false);return true;}
 if(view==UI_NAV_VIEW_STOP||view==UI_NAV_VIEW_DELETE){ui_nav_workspace_back();select_card(UI_NAV_CARD_DOCK,true);return true;}
 if(!ui_nav_workspace_map_visible()){bool handled=ui_nav_workspace_back();if(handled)select_card(UI_NAV_CARD_DOCK,true);return handled;}
 if(view==UI_NAV_VIEW_LIVE&&ui_nav_session_active()){ui_navigation_open_menu();return true;}
 if(s_navigation.card_entered&&s_navigation.selected_card==UI_NAV_CARD_MAP){select_card(UI_NAV_CARD_MAP,false);return true;}
 if(ui_nav_workspace_back())return true;
 if(s_navigation.card_entered){select_card(s_navigation.selected_card,false);return true;}
 return false;
}
void ui_navigation_set_exit_callback(void (*cb)(void)){s_navigation.exit_callback=cb;}
void ui_navigation_set_routes_callback(void (*cb)(ui_nav_list_t)){s_navigation.routes_callback=cb;}
void ui_navigation_open_routes_kind(ui_nav_list_t kind){if(s_navigation.routes_callback)s_navigation.routes_callback(kind);else ui_nav_workspace_open_routes();}
void ui_navigation_open_routes(void){ui_navigation_open_routes_kind(UI_NAV_LIST_ROUTES);}
void ui_navigation_open_menu(void){ui_nav_workspace_open_menu();select_card(UI_NAV_CARD_DOCK,true);}
void ui_navigation_preview_destination(const ui_nav_item_t *place){ui_nav_workspace_preview_destination(place);select_card(UI_NAV_CARD_DOCK,true);}
void ui_navigation_preview_route(const ui_nav_item_t *route){ui_nav_workspace_preview_route(route);select_card(UI_NAV_CARD_DOCK,true);}
void ui_navigation_set_overview(bool value){ui_nav_map_set_overview(&s_navigation.map,value);s_navigation.requested_at=0;ui_nav_workspace_rebuild();}
bool ui_navigation_overview(void){return s_navigation.map.state==NAV_OVERVIEW;}
void ui_navigation_toggle_direction(void){ui_nav_map_set_north_up(&s_navigation.map,!s_navigation.map.north_up);}
bool ui_navigation_north_up(void){return s_navigation.map.north_up;}
void ui_navigation_toggle_prompts(void){ui_navigation_service_set_prompts_enabled(!ui_navigation_service_prompts_enabled());}
bool ui_navigation_prompts_enabled(void){return ui_navigation_service_prompts_enabled();}
void ui_navigation_open_phone(void){ui_nav_workspace_open_phone();}
void ui_navigation_import_route(void){ui_nav_workspace_import_route();}
void ui_navigation_open_maps(void){ui_nav_workspace_open_maps();}
void ui_navigation_demo_next_state(void){if(ui_nav_session_active())(void)ui_navigation_service_demo_step();}
const ui_nav_map_t *ui_navigation_map(void){return &s_navigation.map;}
static void search_geometry(void *unused,int32_t value)
{
 (void)unused;value=LV_CLAMP(0,value,1000);s_navigation.search_progress=value;ui_nav_map_t *m=&s_navigation.map;
 int search_height=UI_NAV_SEARCH_MAP_IDLE_HEIGHT-
  (UI_NAV_SEARCH_MAP_IDLE_HEIGHT-UI_NAV_SEARCH_MAP_HEIGHT)*s_navigation.candidates_progress/1000;
 ui_nav_map_resize_view(m,UI_NAV_MAP_TOP+(UI_NAV_SEARCH_MAP_TOP-UI_NAV_MAP_TOP)*value/1000,
  UI_NAV_MAP_HEIGHT-(UI_NAV_MAP_HEIGHT-search_height)*value/1000);
 lv_obj_set_y(s_navigation.attribution,m->safe_height-16);
 const lv_obj_t *controls[]={m->compass.root,m->zoom_in.root,m->zoom_out.root,m->locate.root,m->scale_root};
 for(unsigned i=0;i<5;i++){if(value)lv_obj_add_flag((lv_obj_t *)controls[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag((lv_obj_t *)controls[i],LV_OBJ_FLAG_HIDDEN);}
 m->read_only=value!=0;if(!value){s_navigation.requested_at=0;if(s_navigation.search_restore){s_navigation.search_restore=false;m->state=s_navigation.search_previous_state;if(m->state==NAV_OVERVIEW)ui_nav_map_set_overview(m,true);}}
}
void ui_navigation_search_candidates(int32_t progress)
{
 s_navigation.candidates_progress=LV_CLAMP(0,progress,1000);
 if(s_navigation.created)search_geometry(NULL,s_navigation.search_progress);
}
void ui_navigation_search_layout(bool open)
{
 ui_nav_map_t *m=&s_navigation.map;m->read_only=true;
 ui_motion_value(m->root,search_geometry,s_navigation.search_progress,open?1000:0,320,lv_anim_path_ease_in_out,NULL,NULL);
}
void ui_navigation_open_search(void)
{
 if(!s_navigation.created||ui_nav_session_active()||ui_nav_search_panel_active())return;
 ui_navigation_cancel_pointer();s_navigation.search_previous_center=ui_nav_map_view_center(&s_navigation.map);
 if(!s_navigation.search_restore||!s_navigation.search_progress)s_navigation.search_previous_state=s_navigation.map.state;
 s_navigation.search_restore=false;
 lv_anim_delete(&s_navigation.map,NULL);
 s_navigation.map.state=NAV_EXPLORE;ui_nav_map_set_focus(&s_navigation.map,s_navigation.map.focused_control,false);ui_nav_workspace_suspend(true);
 ui_nav_search_panel_open(s_navigation.search_previous_center,s_navigation.map.origin_valid);
}
void ui_navigation_close_search(void)
{
 if(!ui_nav_search_panel_active())return;ui_nav_search_panel_close(true);ui_nav_workspace_suspend(false);s_navigation.search_restore=true;ui_navigation_search_layout(false);
 select_card(UI_NAV_CARD_DOCK,true);
}
void ui_navigation_search_selected(const ui_place_t *place)
{
 if(!place||!ui_nav_search_panel_active()||ui_nav_session_active())return;
 ui_nav_item_t item={.id=place->id,.revision=1,.point=place->point};snprintf(item.name,sizeof(item.name),"%s",place->name);snprintf(item.detail,sizeof(item.detail),"%s",place->detail);
 ui_nav_search_panel_close(true);ui_nav_workspace_suspend(false);s_navigation.search_restore=false;ui_navigation_search_layout(false);
 ui_nav_map_center_point(&s_navigation.map,place->point);ui_nav_workspace_plan_search_destination(&item);select_card(UI_NAV_CARD_DOCK,true);
 s_navigation.request_cached=false;s_navigation.requested_at=0;
}
