#include "ui/core/ui_memory.h"
#include "ui_nav_workspace.h"
#include "ui_navigation.h"
#include "services/ui_nav_store.h"
#include "../../core/ui_bottom_surface.h"
#include "../../core/ui_motion.h"
#include "../../core/ui_text.h"
#include "../../core/ui_theme.h"
#include "../../core/ui_colors.h"
#include "../../components/navigation/ui_nav_visual.h"
#include "../../components/navigation/ui_nav_map.h"
#include "../../components/navigation/ui_nav_layout.h"
#include "../../components/navigation/ui_nav_command_dock.h"
#include "../../components/navigation/ui_nav_data_strip.h"
#include "../../components/navigation/ui_nav_capsule_row.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef enum { A_BACK,A_ROUTES,A_RECENT,A_MAPS,A_ITEM,A_START,A_MENU,A_FOLLOW,
 A_OVERVIEW,A_SOUND,A_STOP,A_STOP_YES,A_RETRY,A_IMPORT,A_PHONE,A_PLACES,A_UPDATE,
 A_VERSIONS,A_STORAGE,A_DELETE,A_DELETE_YES,A_MAP_IMPORT,A_PLAN,A_PICK,A_PICK_CONFIRM,A_SEARCH } action_t;
static UI_LARGE_RAM struct {
 lv_obj_t *root,*content,*aux,*rows,*fallback_slot,*focus_capsule,*choice_track,*compact,*spinner;
 ui_nav_capsule_row_t aux_choices[2];lv_obj_t *aux_footer;
 lv_obj_t *focus[24];uint32_t focus_ids[24];unsigned count,selected;
 struct {uint32_t id;int32_t scroll;ui_nav_list_t list;uint32_t offset;} saved[16];
 ui_nav_view_t view,return_view;ui_nav_list_t list;ui_nav_snapshot_t snapshot;
 ui_nav_item_t selected_place,selected_map;ui_nav_catalog_t catalog;ui_nav_result_t result;ui_nav_request_t retry;
 uint32_t request_id,result_version,catalog_version,render_revision;
 bool active,arrival_map,created,pointer_owned,pointer_dragged,controls_active,search_origin,search_entrance,search_manual,search_online,suspended;int height,pointer_x,pointer_y;ui_nav_view_t pointer_view;
 ui_nav_map_t *map;ui_nav_data_strip_t trip;ui_nav_command_dock_t maneuver;
 void (*exit_page)(void);
 void (*start_callback)(void);
 bool (*finish_callback)(void);
} s;
static void build(ui_nav_view_t view);
static bool auxiliary(ui_nav_view_t view);
static void perform(action_t a,unsigned index);
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,const char *text,int size,bool muted)
{return ui_nav_label(parent,x,y,w,text,size,muted);}
static void center(lv_obj_t *parent,int y,const char *text,int size,bool muted)
{lv_obj_t *o=label(parent,16,y,432,text,size,muted);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
 lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);lv_obj_set_height(o,size>=24?32:24);}
static void center_region(lv_obj_t *parent,int y,int height,const char *text,int size,bool muted)
{lv_obj_t *o=label(parent,16,0,432,text,size,muted);const lv_font_t *font=lv_obj_get_style_text_font(o,0);
 lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);lv_obj_set_height(o,font->line_height);
 lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_y(o,y+(height-font->line_height)/2);}
static void capsule_x(void *o,int32_t v){lv_obj_set_x(o,v);}
static void capsule_y(void *o,int32_t v){lv_obj_set_y(o,v);}
static void capsule_w(void *o,int32_t v){lv_obj_set_width(o,v);}
static void capsule_h(void *o,int32_t v){lv_obj_set_height(o,v);}
/* Independently damped leading/trailing edges give the filled segment a liquid
 * stretch while retaining velocity during a reversal. Text stays outside it. */
static void capsule_left(void *object,int32_t value)
{
 lv_obj_t *o=object;int right=lv_obj_get_style_x(o,0)+lv_obj_get_style_width(o,0);
 int left=LV_CLAMP(16,value,238);lv_obj_set_x(o,left);lv_obj_set_width(o,LV_MAX(40,right-left));
}
static void capsule_right(void *object,int32_t value)
{
 lv_obj_t *o=object;int left=lv_obj_get_style_x(o,0);
 int right=LV_CLAMP(left+40,value,448);lv_obj_set_width(o,right-left);
}
static void focus_update(bool animate)
{
 bool segmented=s.choice_track!=NULL&&s.count&&lv_obj_get_style_y(s.focus[s.selected],0)==142;
 bool controls=s.controls_active||auxiliary(s.view);
 for(unsigned j=0;j<2;j++)ui_nav_capsule_row_focus(&s.aux_choices[j],s.count?s.focus[s.selected]:NULL,
  controls&&s.count&&ui_nav_capsule_row_contains(&s.aux_choices[j],s.focus[s.selected]),animate);
 for(unsigned i=0;i<s.count;i++){
  lv_obj_t *o=s.focus[i];bool chosen=controls&&i==s.selected;
  bool row_segment=s.choice_track&&lv_obj_get_style_y(o,0)==142;
  if(ui_nav_capsule_row_contains(&s.aux_choices[0],o)||ui_nav_capsule_row_contains(&s.aux_choices[1],o))continue;
  if(lv_obj_get_parent(o)!=s.content){ui_nav_button_focus(o,chosen);continue;}
  lv_obj_set_style_border_width(o,0,0);
  lv_obj_set_style_bg_opa(o,row_segment||chosen?LV_OPA_TRANSP:LV_OPA_COVER,0);
  lv_obj_set_style_bg_color(o,lv_color_hex(lv_obj_has_flag(o,LV_OBJ_FLAG_USER_2)?0x291919:0x282828),0);
  for(unsigned j=0;j<lv_obj_get_child_count(o);j++){
   lv_obj_t *child=lv_obj_get_child(o,j);
   lv_obj_set_style_text_color(child,lv_color_hex(row_segment?0x101010:chosen?0x000000:lv_obj_has_flag(o,LV_OBJ_FLAG_USER_2)?0xFF4D4F:0xFFFFFF),0);
  }
 }
 if(!controls){lv_obj_add_flag(s.focus_capsule,LV_OBJ_FLAG_HIDDEN);return;}
 if(!s.count){lv_obj_add_flag(s.focus_capsule,LV_OBJ_FLAG_HIDDEN);return;}
 lv_obj_t *o=s.focus[s.selected];
 if(s.rows&&lv_obj_get_parent(o)==s.rows)lv_obj_scroll_to_view(o,animate?ui_motion_scroll_mode():LV_ANIM_OFF);
 if(lv_obj_get_parent(o)!=s.content){lv_obj_add_flag(s.focus_capsule,LV_OBJ_FLAG_HIDDEN);return;}
 lv_obj_clear_flag(s.focus_capsule,LV_OBJ_FLAG_HIDDEN);lv_obj_update_layout(o);
 int x=lv_obj_get_x(o),y=lv_obj_get_y(o),w=lv_obj_get_width(o),h=lv_obj_get_height(o);
 if(animate){
  if(segmented){
   int left=lv_obj_get_style_x(s.focus_capsule,0),right=left+lv_obj_get_style_width(s.focus_capsule,0);bool forward=x>=left;
   ui_motion_spring(s.focus_capsule,capsule_left,left,x,forward,NULL,NULL);
   ui_motion_spring(s.focus_capsule,capsule_right,right,x+w,!forward,NULL,NULL);
  }else ui_motion_spring(s.focus_capsule,capsule_x,lv_obj_get_x(s.focus_capsule),x,true,NULL,NULL);
  ui_motion_spring(s.focus_capsule,capsule_y,lv_obj_get_y(s.focus_capsule),y,true,NULL,NULL);
  if(!segmented)ui_motion_spring(s.focus_capsule,capsule_w,lv_obj_get_width(s.focus_capsule),w,true,NULL,NULL);
  ui_motion_spring(s.focus_capsule,capsule_h,lv_obj_get_height(s.focus_capsule),h,true,NULL,NULL);
 }else{ui_motion_cancel(s.focus_capsule,capsule_left);ui_motion_cancel(s.focus_capsule,capsule_right);ui_motion_cancel(s.focus_capsule,capsule_x);ui_motion_cancel(s.focus_capsule,capsule_y);
  ui_motion_cancel(s.focus_capsule,capsule_w);ui_motion_cancel(s.focus_capsule,capsule_h);
  lv_obj_set_pos(s.focus_capsule,x,y);lv_obj_set_size(s.focus_capsule,w,h);}
}
static void live_click(lv_event_t *e){if(lv_event_get_code(e)==LV_EVENT_CLICKED&&s.view==UI_NAV_VIEW_LIVE)perform(A_MENU,0);}
static void click(lv_event_t *e)
{if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;uintptr_t v=(uintptr_t)lv_event_get_user_data(e);
 for(unsigned i=0;i<s.count;i++)if(s.focus[i]==lv_event_get_target(e))s.selected=i;
 focus_update(true);perform(v&255,v>>8);}
static lv_obj_t *button(lv_obj_t *parent,int x,int y,int w,int h,const char *text,action_t a,unsigned i,bool primary)
{
 lv_obj_t *o=ui_nav_button(parent,x,y,w,h,text,primary,a==A_STOP||(a==A_STOP_YES&&!primary));
 lv_obj_add_event_cb(o,click,LV_EVENT_CLICKED,(void *)(uintptr_t)(a|(i<<8)));
 if(parent==s.content){
  lv_obj_set_style_radius(o,36,0);lv_obj_set_style_bg_color(o,lv_color_hex(0x282828),0);
  lv_obj_set_style_border_width(o,0,0);
  lv_obj_t *t=lv_obj_get_child(o,0);lv_obj_set_style_text_color(t,lv_color_white(),0);
 }
 if(s.count<24){s.focus[s.count]=o;s.focus_ids[s.count++]=(a==A_ITEM&&i<s.catalog.count)?s.catalog.items[i].id+0x10000u:a;}
 return o;
}
static void divider(lv_obj_t *p,int y)
{lv_obj_t *o=ui_nav_surface(p,16,y,432,1,false);lv_obj_set_style_bg_color(o,lv_color_hex(0x484848),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);}
static void request(const ui_nav_request_t *r)
{s.retry=*r;memset(&s.result,0,sizeof(s.result));s.request_id=ui_nav_store_command(r,lv_tick_get());ui_nav_store_get(&s.snapshot);}
static void request_kind(ui_nav_request_kind_t kind,ui_nav_list_t list,uint32_t offset,const ui_nav_item_t *item)
{
 static UI_LARGE_RAM ui_nav_request_t r;
 memset(&r,0,sizeof(r));r.kind=kind;r.list=list;r.offset=offset;
 if(item){r.destination=*item;if(kind==UI_NAV_REQ_LOAD_ROUTE){r.route_id=item->id;r.route_revision=item->revision;}}
 request(&r);
}
static void load(ui_nav_list_t list,uint32_t offset)
{s.list=list;s.catalog=(ui_nav_catalog_t){.kind=list,.offset=offset};request_kind(UI_NAV_REQ_LIST,list,offset,NULL);build(list==UI_NAV_LIST_MAPS?UI_NAV_VIEW_MAPS:list==UI_NAV_LIST_MAP_VERSIONS?UI_NAV_VIEW_MAP_VERSIONS:list==UI_NAV_LIST_STORAGE?UI_NAV_VIEW_STORAGE:UI_NAV_VIEW_LIST);}
static void statistics(lv_obj_t *p,int y,bool arrived)
{
 const ui_nav_session_t *q=&s.snapshot.session;char a[32],b[32],c[32];
 if(arrived){if(q->elapsed_valid)snprintf(a,32,"%lu min",(unsigned long)(q->elapsed_s/60));else strcpy(a,"-- min");
  if(q->travelled_valid)snprintf(b,32,"%.1f km",q->travelled_m/1000.);else strcpy(b,"-- km");
  if(q->ascent_valid)snprintf(c,32,"+%lu m",(unsigned long)q->ascent_m);else strcpy(c,"-- m");
 }else{if(q->route.distance_valid)snprintf(a,32,"%.1f km",q->route.distance_m/1000.);else strcpy(a,"-- km");
  if(q->route.duration_valid)snprintf(b,32,"%lu min",(unsigned long)(q->route.duration_s/60));else strcpy(b,"-- min");
  if(q->route.ascent_valid)snprintf(c,32,"+%lu m",(unsigned long)q->route.ascent_m);else strcpy(c,"-- m");}
 ui_nav_metric_triplet(p,y,464,a,b,c,arrived?"用时":"总距离",arrived?"距离":"预计用时","爬升");
}
static void compact_update(void)
{
 if(!s.compact)return;const nav_model_t *m=&s.snapshot.guidance;char t[144];
 if(m->guidance_valid)snprintf(t,sizeof(t),"%lu m · %s · %s",(unsigned long)m->direction_distance,
  m->direction==NAV_MANEUVER_LEFT?"左转":m->direction==NAV_MANEUVER_RIGHT?"右转":m->direction==NAV_MANEUVER_ROUNDABOUT?"进入环岛":m->direction==NAV_MANEUVER_U_TURN?"掉头":"沿路线",m->road_name);
 else snprintf(t,sizeof(t),m->off_route?"已偏离路线":"等待有效引导");
 ui_text_set_label(s.compact,t,20);
}
static bool auxiliary(ui_nav_view_t v)
{return v==UI_NAV_VIEW_LIST||v==UI_NAV_VIEW_MAPS||v==UI_NAV_VIEW_MAP_DETAIL||v==UI_NAV_VIEW_PHONE||v==UI_NAV_VIEW_MAP_VERSIONS||v==UI_NAV_VIEW_STORAGE;}
static void remember(void)
{
 if(s.count)s.saved[s.view].id=s.focus_ids[s.selected];
 s.saved[s.view].scroll=s.rows?lv_obj_get_scroll_y(s.rows):0;
 s.saved[s.view].list=s.list;s.saved[s.view].offset=s.catalog.offset;
}
static void restore_focus(void)
{s.selected=0;for(unsigned i=0;i<s.count;i++)if(s.focus_ids[i]==s.saved[s.view].id)s.selected=i;
 if(s.view==UI_NAV_VIEW_PREVIEW&&s.count>1&&!s.saved[s.view].id)s.selected=s.count-1;
 if(s.view!=UI_NAV_VIEW_LIVE)focus_update(false);if(s.rows)lv_obj_scroll_to_y(s.rows,s.saved[s.view].scroll,LV_ANIM_OFF);}
static void build(ui_nav_view_t v)
{
 if(!s.created)return;remember();bool changed=s.view!=v;s.view=v;s.count=0;s.rows=s.compact=s.spinner=s.choice_track=s.aux_footer=NULL;
 ui_motion_cancel(s.content,capsule_y);lv_obj_set_pos(s.content,0,0);
 lv_obj_clear_flag(s.content,LV_OBJ_FLAG_CLICKABLE);lv_obj_clean(s.content);lv_obj_clean(s.aux);memset(s.aux_choices,0,sizeof(s.aux_choices));
 s.focus_capsule=ui_nav_surface(s.content,12,12,40,40,false);
 lv_obj_set_style_radius(s.focus_capsule,36,0);lv_obj_set_style_border_width(s.focus_capsule,0,0);
 lv_obj_set_style_bg_color(s.focus_capsule,lv_color_hex(0xFFD900),0);lv_obj_set_style_bg_opa(s.focus_capsule,LV_OPA_COVER,0);lv_obj_add_flag(s.focus_capsule,LV_OBJ_FLAG_HIDDEN);
 lv_obj_add_flag(s.trip.root,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(s.maneuver.root,LV_OBJ_FLAG_HIDDEN);
 bool aux=auxiliary(v);if(aux)lv_obj_clear_flag(s.aux,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(s.aux,LV_OBJ_FLAG_HIDDEN);
 ui_nav_store_get(&s.snapshot);const ui_nav_session_t *q=&s.snapshot.session;const nav_model_t *m=&s.snapshot.guidance;
 if(s.map){ui_nav_map_set_pick_mode(s.map,v==UI_NAV_VIEW_PICK);if(v==UI_NAV_VIEW_LIVE)s.map->pick_point_valid=false;}
 if(v==UI_NAV_VIEW_CENTER){
  lv_obj_t *search=button(s.content,16,35,432,72,"搜索地点",A_SEARCH,0,false);
  lv_obj_t *search_icon=ui_nav_icon(search,24,24,24,NAV_ICON_SEARCH,0xFFFFFF);lv_obj_align(search_icon,LV_ALIGN_LEFT_MID,24,0);
  button(s.content,16,142,210,72,"我的路线",A_ROUTES,0,false);button(s.content,238,142,210,72,"最近使用",A_RECENT,0,false);
 }else if(v==UI_NAV_VIEW_PICK){
  ui_nav_icon_label(s.content,16,16,432,110,"移动地图选择位置",24,false,NAV_ICON_PIN,46,0xFFD900);
  button(s.content,16,142,210,72,"返回",A_BACK,0,false);button(s.content,238,142,210,72,"选这里",A_PICK_CONFIRM,0,true);
 }else if(v==UI_NAV_VIEW_DESTINATION){
  lv_obj_t *b;
  if(s.search_origin){
   lv_obj_t *name=label(s.content,16,8,432,s.selected_place.name,24,false);lv_obj_set_height(name,60);
   lv_obj_t *detail=label(s.content,16,72,432,s.selected_place.detail,20,true);lv_label_set_long_mode(detail,LV_LABEL_LONG_DOT);lv_obj_set_height(detail,24);
   label(s.content,16,104,432,s.search_manual?"手动位置 · 请确认入口":s.search_online?"联网坐标 · 请确认落点":s.search_entrance?"地图入口 · 请确认通行":"地图位置 · 可调整到入口",20,true);
   button(s.content,16,142,136,72,"返回",A_BACK,0,false);button(s.content,164,142,136,72,"调整位置",A_PICK,0,false);
   b=button(s.content,312,142,136,72,"规划路线",A_PLAN,0,true);
  }else{
   ui_nav_icon_label(s.content,16,16,432,110,s.selected_place.name,24,false,NAV_ICON_PIN,48,0xFFD900);
   button(s.content,16,142,210,72,"返回地图",A_BACK,0,false);b=button(s.content,238,142,210,72,"规划路线",A_PLAN,0,true);
  }
  if(!(s.snapshot.capabilities&UI_NAV_CAP_PLAN))lv_obj_add_state(b,LV_STATE_DISABLED);
 }else if(v==UI_NAV_VIEW_PREVIEW){
  bool preparing=s.snapshot.busy&&q->state!=UI_NAV_SESSION_STARTING;
  if(preparing){center_region(s.content,26,32,q->destination.name[0]?q->destination.name:"导航路线",24,false);
   s.spinner=lv_spinner_create(s.content);lv_obj_set_size(s.spinner,40,40);lv_spinner_set_anim_params(s.spinner,900,220);
   lv_obj_set_style_arc_color(s.spinner,UI_TOKEN_COLOR_ACCENT,LV_PART_INDICATOR);lv_obj_set_style_arc_color(s.spinner,lv_color_hex(0x3B350B),LV_PART_MAIN);
   lv_obj_t *preparing_label=label(s.content,0,0,352,"正在准备导航路线",24,false);
   ui_nav_center_pair(s.spinner,preparing_label,16,72,432,40,16);
   button(s.content,16,142,432,72,"取消",A_BACK,0,false);
  }else if(q->state==UI_NAV_SESSION_PREVIEW||q->state==UI_NAV_SESSION_STARTING){
   ui_nav_icon_label(s.content,16,18,432,32,q->route.name,24,false,NAV_ICON_PIN,28,0xFFD900);
   statistics(s.content,54,false);
   button(s.content,16,142,210,72,"返回",A_BACK,0,false);
   lv_obj_t *b=button(s.content,238,142,210,72,q->state==UI_NAV_SESSION_STARTING?"正在开始…":"开始导航",A_START,0,true);
   if(!q->route.start_allowed||q->state==UI_NAV_SESSION_STARTING)lv_obj_add_state(b,LV_STATE_DISABLED);
  }else{center_region(s.content,34,36,"路线准备失败",24,false);center_region(s.content,78,28,ui_nav_result_text(s.result.code),20,true);
   button(s.content,16,142,210,72,"返回",A_BACK,0,false);button(s.content,238,142,210,72,"重试",A_RETRY,0,true);}
 }else if(v==UI_NAV_VIEW_LIVE){
  lv_obj_clear_flag(s.trip.root,LV_OBJ_FLAG_HIDDEN);ui_nav_data_strip_update(&s.trip,m);ui_text_set_label(s.trip.titles[0],"当前速度",20);lv_label_set_text(s.trip.units[0],"km/h");
  if(s.map&&s.map->state==NAV_OVERVIEW){ui_text_set_label(s.trip.titles[0],"总距离",20);lv_label_set_text(s.trip.units[0],"km");
   if(q->route.distance_valid)lv_label_set_text_fmt(s.trip.values[0],"%.1f",q->route.distance_m/1000.);else lv_label_set_text(s.trip.values[0],"--");
   button(s.content,16,142,432,72,q->state==UI_NAV_SESSION_ARRIVED?"完成导航":"返回跟随",q->state==UI_NAV_SESSION_ARRIVED?A_STOP_YES:A_FOLLOW,0,false);
  }else{lv_obj_clear_flag(s.maneuver.root,LV_OBJ_FLAG_HIDDEN);ui_nav_command_dock_update(&s.maneuver,m,false);
   lv_obj_add_flag(s.content,LV_OBJ_FLAG_CLICKABLE);

  }
 }else if(v==UI_NAV_VIEW_MENU){
  s.compact=label(s.content,20,12,424,"",20,false);lv_label_set_long_mode(s.compact,LV_LABEL_LONG_DOT);lv_obj_set_height(s.compact,28);compact_update();
  const char *names[]={"路线总览","回到当前位置",ui_navigation_prompts_enabled()?"提示音：开":"提示音：关","结束导航"};
  action_t acts[]={A_OVERVIEW,A_FOLLOW,A_SOUND,A_STOP};nav_icon_t icons[]={NAV_ICON_MAP,NAV_ICON_ARROW,NAV_ICON_CLOCK,NAV_ICON_STOP};
  for(unsigned i=0;i<4;i++){lv_obj_t *b=button(s.content,16+(i%2)*222,54+(i/2)*80,210,72,"",acts[i],0,false);
   lv_obj_t *icon;
   if(i==2){icon=lv_label_create(b);lv_label_set_text(icon,ui_navigation_prompts_enabled()?LV_SYMBOL_VOLUME_MAX:LV_SYMBOL_MUTE);lv_obj_set_style_text_font(icon,&lv_font_montserrat_24,0);lv_obj_set_style_text_color(icon,lv_color_white(),0);lv_obj_set_size(icon,28,lv_font_montserrat_24.line_height);lv_obj_set_style_text_align(icon,LV_TEXT_ALIGN_CENTER,0);}
   else icon=ui_nav_icon(b,0,0,28,icons[i],i==3?0xFF4D4F:0xFFFFFF);
   if(i==3){lv_obj_set_style_bg_color(b,lv_color_hex(0x291919),0);lv_obj_set_style_border_color(b,lv_color_hex(0xFF4D4F),0);}
   lv_obj_t *t=label(b,0,0,154,names[i],20,false);ui_nav_center_pair(icon,t,12,0,186,72,8);if(i==3)lv_obj_set_style_text_color(t,lv_color_hex(0xFF4D4F),0);}
 }else if(v==UI_NAV_VIEW_STOP||v==UI_NAV_VIEW_DELETE){
  bool status=s.snapshot.busy||s.result.code;
  center_region(s.content,v==UI_NAV_VIEW_STOP||status?34:16,v==UI_NAV_VIEW_STOP||status?36:110,v==UI_NAV_VIEW_STOP?"结束本次导航与骑行？":"删除这条路线？",24,false);
  if(v==UI_NAV_VIEW_STOP||status)center_region(s.content,78,28,status?(s.snapshot.busy?"正在处理…":ui_nav_result_text(s.result.code)):"停止记录并保存骑行数据",20,true);
  button(s.content,16,142,210,72,"取消",A_BACK,0,false);button(s.content,238,142,210,72,v==UI_NAV_VIEW_STOP?"结束导航":"确认删除",v==UI_NAV_VIEW_STOP?A_STOP_YES:A_DELETE_YES,0,false);
 }else if(v==UI_NAV_VIEW_ARRIVAL){
  ui_nav_icon_label(s.content,16,6,432,32,"已到达",24,false,NAV_ICON_CHECK,28,0xFFD900);
  center_region(s.content,40,24,q->destination.name,20,true);
  statistics(s.content,66,true);
  button(s.content,16,142,210,72,"查看地图",A_OVERVIEW,0,false);button(s.content,238,142,210,72,"完成导航",A_STOP_YES,0,true);
 }else if(v==UI_NAV_VIEW_ERROR){
  ui_nav_icon_label(s.content,16,24,432,48,m->reroute_failed?"无法重新规划":"已偏离路线",24,false,NAV_ICON_WARNING,32,0xFFD900);
  if(!(s.snapshot.capabilities&UI_NAV_CAP_REROUTE))center_region(s.content,78,32,"当前设备无法独立重新规划",20,true);
  if(s.snapshot.capabilities&UI_NAV_CAP_REROUTE)button(s.content,16,88,432,48,"重试",A_RETRY,1,true);
  button(s.content,16,142,210,72,"查看原路线",A_OVERVIEW,0,false);button(s.content,238,142,210,72,"结束导航",A_STOP,0,false);
 }else if(aux){
  const char *title=v==UI_NAV_VIEW_MAPS?"离线地图":v==UI_NAV_VIEW_MAP_DETAIL?"地图详情":v==UI_NAV_VIEW_MAP_VERSIONS?"地图版本记录":v==UI_NAV_VIEW_STORAGE?"存储空间管理":v==UI_NAV_VIEW_PHONE?"获取路线":s.list==UI_NAV_LIST_RECENT_ROUTES?"最近使用":"我的导航内容";
  label(s.aux,68,12,380,title,24,false);lv_obj_t *back=button(s.aux,12,6,44,44,"",A_BACK,0,false);
  lv_obj_t *arrow=lv_obj_get_child(back,0);lv_label_set_text(arrow,LV_SYMBOL_LEFT);lv_obj_set_style_text_font(arrow,&lv_font_montserrat_24,0);
  s.aux_footer=ui_nav_surface(s.aux,8,UI_NAV_DATA_TOP-UI_NAV_MAP_TOP,464,UI_NAV_FIXED_DOCK_HEIGHT,true);
  lv_obj_set_style_radius(s.aux_footer,24,0);
  center_region(s.aux_footer,v==UI_NAV_VIEW_MAPS?10:16,v==UI_NAV_VIEW_MAPS?36:110,
   v==UI_NAV_VIEW_PHONE?"路线传输":v==UI_NAV_VIEW_MAPS?"地图管理":v==UI_NAV_VIEW_MAP_DETAIL?"地图文件":"导航资源",24,false);
  if(v==UI_NAV_VIEW_PHONE){
   ui_nav_icon(s.aux,200,110,80,NAV_ICON_PHONE,0xFFD900);
   center(s.aux,220,q->phone_connected?"手机已连接":"手机尚未连接",24,false);
   center(s.aux,270,"路线传输由通信服务提供",20,true);
   center(s.aux,320,(s.snapshot.capabilities&UI_NAV_CAP_PHONE)?"等待手机发送路线":"手机同步服务尚未接入",20,true);
   lv_obj_t *b=button(s.aux_footer,16,142,432,72,"导入路线",A_IMPORT,0,true);if(!(s.snapshot.capabilities&UI_NAV_CAP_IMPORT_ROUTE))lv_obj_add_state(b,LV_STATE_DISABLED);
  }else if(v==UI_NAV_VIEW_MAP_DETAIL){
   ui_nav_icon(s.aux,30,96,64,NAV_ICON_MAP,0xFFD900);lv_obj_t *name=label(s.aux,118,96,334,s.selected_map.name,24,false);
   lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_height(name,34);
   label(s.aux,118,140,334,s.selected_map.map_status==UI_NAV_MAP_READY?"已安装":"地图尚不可用",20,true);
   char text[120];snprintf(text,sizeof(text),"版本：%s",s.selected_map.version_name[0]?s.selected_map.version_name:"待提供");label(s.aux,28,218,424,text,24,false);
   if(s.selected_map.size_valid)snprintf(text,sizeof(text),"占用空间：%.1f MB",s.selected_map.size_kib/1024.);else strcpy(text,"占用空间：待提供");label(s.aux,28,278,424,text,24,false);
   label(s.aux,28,338,424,s.selected_map.coverage[0]?s.selected_map.coverage:"覆盖范围：待提供",24,true);
   label(s.aux,28,398,424,s.selected_map.detail,20,true);
   button(s.aux_footer,16,142,210,72,"返回",A_BACK,0,false);lv_obj_t *b=button(s.aux_footer,238,142,210,72,"导入 / 更新",A_MAP_IMPORT,0,true);if(!(s.snapshot.capabilities&UI_NAV_CAP_IMPORT_MAP))lv_obj_add_state(b,LV_STATE_DISABLED);
  }else{
   s.rows=ui_nav_surface(s.aux,12,68,456,UI_NAV_DATA_TOP-UI_NAV_MAP_TOP-84,false);lv_obj_add_flag(s.rows,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(s.rows,LV_DIR_VER);
   lv_obj_set_scrollbar_mode(s.rows,LV_SCROLLBAR_MODE_AUTO);lv_obj_set_style_width(s.rows,4,LV_PART_SCROLLBAR);lv_obj_set_style_bg_color(s.rows,UI_TOKEN_COLOR_ACCENT,LV_PART_SCROLLBAR);
   if(v==UI_NAV_VIEW_STORAGE){char text[120];if(s.catalog.storage_valid)snprintf(text,sizeof(text),"可用 %.1f MB / 总计 %.1f MB",s.catalog.storage_free_kib/1024.,s.catalog.storage_total_kib/1024.);else strcpy(text,"存储容量暂不可用");center(s.rows,120,text,24,false);}
   else if(!s.catalog.count)center(s.rows,136,s.snapshot.busy?"正在读取…":s.result.code?ui_nav_result_text(s.result.code):"暂无内容",24,false);
   for(unsigned i=0;i<s.catalog.count;i++){const ui_nav_item_t *it=&s.catalog.items[i];lv_obj_t *b=button(s.rows,0,i*158,444,146,"",A_ITEM,i,false);
    ui_nav_icon(b,18,32,56,s.list==UI_NAV_LIST_MAPS?NAV_ICON_MAP:NAV_ICON_STAR,0xFFD900);lv_obj_t *name=label(b,90,20,326,it->name,24,false);
    lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_height(name,34);
    lv_obj_t *detail=label(b,90,66,326,it->detail,20,true);lv_label_set_long_mode(detail,LV_LABEL_LONG_DOT);lv_obj_set_height(detail,56);}
   if(v==UI_NAV_VIEW_MAPS){button(s.aux_footer,16,54,210,72,"地图版本记录",A_VERSIONS,0,false);button(s.aux_footer,238,54,210,72,"存储空间管理",A_STORAGE,0,false);}
   button(s.aux_footer,16,142,210,72,"返回",A_BACK,0,false);
   lv_obj_t *b=button(s.aux_footer,238,142,210,72,v==UI_NAV_VIEW_MAPS?"导入 / 更新":"刷新",v==UI_NAV_VIEW_MAPS?A_MAP_IMPORT:A_UPDATE,0,true);
   if(v==UI_NAV_VIEW_MAPS&&!(s.snapshot.capabilities&UI_NAV_CAP_IMPORT_MAP))lv_obj_add_state(b,LV_STATE_DISABLED);
  }
 }

 /* Geometry is shared with the riding normal dock. Every state reflows
  * inside it; no map or card resize is performed on a state transition. */
 lv_obj_update_layout(s.content);
 if(!aux){
  if(v==UI_NAV_VIEW_LIVE){
   bool overview=s.map&&s.map->state==NAV_OVERVIEW;
   lv_obj_set_y(s.trip.root,overview?33:4);lv_obj_set_y(s.maneuver.root,86);if(!overview)divider(s.content,82);
  }
  lv_obj_update_layout(s.content);
  unsigned bottom=0;for(unsigned i=0;i<s.count;i++)if(lv_obj_get_parent(s.focus[i])==s.content&&lv_obj_get_y(s.focus[i])==142)bottom++;
  if(bottom==2){
   s.choice_track=ui_nav_surface(s.content,16,142,432,72,false);
   lv_obj_set_style_bg_color(s.choice_track,lv_color_hex(0xB8B8B8),0);lv_obj_set_style_bg_opa(s.choice_track,LV_OPA_COVER,0);lv_obj_set_style_radius(s.choice_track,36,0);
   lv_obj_move_background(s.choice_track);lv_obj_move_to_index(s.focus_capsule,1);
  }
 }else{
  unsigned group=0;
  for(int row=0;row<2;row++){
   lv_obj_t *buttons[4];unsigned count=0;int y=row?142:54;
   for(unsigned i=0;i<s.count;i++)if(lv_obj_get_parent(s.focus[i])==s.aux_footer&&lv_obj_get_style_y(s.focus[i],0)==y)buttons[count++]=s.focus[i];
   if(count){
    for(unsigned i=0;i<count;i++)ui_text_set_label(lv_obj_get_child(buttons[i],0),lv_label_get_text(lv_obj_get_child(buttons[i],0)),count==1?24:20);
    ui_nav_capsule_row_create(&s.aux_choices[group++],s.aux_footer,16,y,432,72,buttons,count);
   }
  }
 }
 s.height=UI_NAV_FIXED_DOCK_HEIGHT;lv_obj_set_size(s.content,464,s.height);
 if(s.map&&lv_obj_get_style_height(s.map->root,0)==UI_NAV_MAP_HEIGHT)ui_nav_map_set_safe_height(s.map,UI_NAV_MAP_HEIGHT);
 if(s.active&&!s.suspended)ui_bottom_surface_set_page_content(aux?NULL:s.root,s.height,false);
 restore_focus();s.render_revision=s.snapshot.revision;
 if(changed&&s.active&&!aux)ui_motion_translate_fade(s.root,0,0,LV_OPA_60,LV_OPA_COVER,160,NULL,NULL);
}
static void perform(action_t a,unsigned i)
{
 if(a==A_BACK){if(!ui_nav_workspace_back()&&s.exit_page)s.exit_page();return;}
 if(s.snapshot.busy&&a!=A_MENU&&a!=A_FOLLOW&&a!=A_OVERVIEW&&a!=A_SOUND&&a!=A_STOP)return;
 switch(a){
 case A_SEARCH:ui_navigation_open_search();break;
 case A_PICK:if(!ui_nav_session_active()){build(UI_NAV_VIEW_PICK);if(s.map){ui_nav_map_set_north_up(s.map,true);s.map->state=NAV_EXPLORE;}}break;
 case A_PICK_CONFIRM:if(s.map){lv_area_t area;lv_obj_get_coords(s.map->root,&area);ui_nav_map_select_at(s.map,area.x1+232,area.y1+s.map->safe_height/2);}break;
 case A_ROUTES:ui_navigation_open_routes();break;
 case A_RECENT:ui_navigation_open_routes_kind(UI_NAV_LIST_RECENT_ROUTES);break;
 case A_MAPS:load(UI_NAV_LIST_MAPS,0);break;
 case A_MENU:build(UI_NAV_VIEW_MENU);break;
 case A_FOLLOW:ui_navigation_set_overview(false);build(UI_NAV_VIEW_LIVE);break;
 case A_OVERVIEW:if(s.snapshot.session.state==UI_NAV_SESSION_ARRIVED)s.arrival_map=true;ui_navigation_set_overview(true);build(UI_NAV_VIEW_LIVE);break;
 case A_SOUND:ui_navigation_toggle_prompts();build(s.view);break;
 case A_STOP:s.return_view=s.view;s.result.code=UI_NAV_OK;build(UI_NAV_VIEW_STOP);break;
 case A_STOP_YES:if(!s.finish_callback||!s.finish_callback()){request_kind(UI_NAV_REQ_STOP,0,0,NULL);build(s.view);}break;
 case A_PLAN:if(s.snapshot.capabilities&UI_NAV_CAP_PLAN){request_kind(UI_NAV_REQ_PLAN,0,0,&s.selected_place);build(UI_NAV_VIEW_PREVIEW);}break;
 case A_START:if(s.snapshot.session.state==UI_NAV_SESSION_PREVIEW&&s.snapshot.session.route.start_allowed){if(s.start_callback)s.start_callback();else ui_nav_workspace_start();}break;
 case A_ITEM:if(i<s.catalog.count){if(s.list==UI_NAV_LIST_MAPS){s.selected_map=s.catalog.items[i];build(UI_NAV_VIEW_MAP_DETAIL);}
  else ui_nav_workspace_preview_route(&s.catalog.items[i]);}break;
 case A_RETRY:if(i==1)request_kind(UI_NAV_REQ_REROUTE,0,0,NULL);else request(&s.retry);build(i==1?UI_NAV_VIEW_LIVE:s.view);break;
 case A_UPDATE:load(s.list,s.catalog.offset+s.catalog.count<s.catalog.total?s.catalog.offset+s.catalog.count:0);break;
 case A_IMPORT:if(!(s.snapshot.capabilities&UI_NAV_CAP_IMPORT_ROUTE))break;request_kind(UI_NAV_REQ_IMPORT_ROUTE,0,0,NULL);build(UI_NAV_VIEW_PREVIEW);break;
 case A_PHONE:build(UI_NAV_VIEW_PHONE);break;
 case A_VERSIONS:load(UI_NAV_LIST_MAP_VERSIONS,0);break;
 case A_STORAGE:load(UI_NAV_LIST_STORAGE,0);break;
 case A_MAP_IMPORT:if(!(s.snapshot.capabilities&UI_NAV_CAP_IMPORT_MAP))break;request_kind(UI_NAV_REQ_IMPORT_MAP,UI_NAV_LIST_MAPS,0,NULL);build(UI_NAV_VIEW_MAPS);break;
 default:break;
 }
}
void ui_nav_workspace_refresh(void)
{
 if(!s.created)return;static UI_LARGE_RAM ui_nav_snapshot_t next;ui_nav_store_get(&next);bool changed=next.revision!=s.snapshot.revision;
 ui_nav_session_state_t old=s.snapshot.session.state;bool identity=next.session.id!=s.snapshot.session.id||next.session.route.revision!=s.snapshot.session.route.revision;
 s.snapshot=next;if(identity)s.arrival_map=false;
 static UI_LARGE_RAM ui_nav_result_t r;uint32_t rv;ui_nav_session_get_result(&r,&rv);bool result=rv!=s.result_version&&r.request_id==s.request_id;s.result_version=rv;if(result)s.result=r;
 static UI_LARGE_RAM ui_nav_catalog_t c;uint32_t cv;ui_nav_session_get_catalog(&c,&cv);bool catalog=cv!=s.catalog_version&&c.kind==s.list;s.catalog_version=cv;if(catalog&&(!c.request_id||c.request_id==s.request_id))s.catalog=c;else catalog=false;
 /* Recording owns the STOP request in the product app, including while this page is hidden. */
 if(next.session.state==UI_NAV_SESSION_IDLE&&
    (s.view==UI_NAV_VIEW_STOP||s.view==UI_NAV_VIEW_LIVE||s.view==UI_NAV_VIEW_ARRIVAL||s.view==UI_NAV_VIEW_MENU||s.view==UI_NAV_VIEW_ERROR)){
  s.arrival_map=false;s.view=UI_NAV_VIEW_CENTER;
  if(s.active&&!s.suspended)build(s.view);
  return;
 }
 if(!s.active||s.suspended)return;
 if(result&&r.code==UI_NAV_OK&&s.retry.kind==UI_NAV_REQ_STOP){s.arrival_map=false;build(UI_NAV_VIEW_CENTER);return;}
 if(result&&r.code==UI_NAV_OK&&s.retry.kind==UI_NAV_REQ_IMPORT_ROUTE&&next.session.state==UI_NAV_SESSION_PREVIEW){build(UI_NAV_VIEW_PREVIEW);return;}
 if(next.session.state==UI_NAV_SESSION_ACTIVE&&(old==UI_NAV_SESSION_STARTING||s.view==UI_NAV_VIEW_CENTER||s.view==UI_NAV_VIEW_PREVIEW)){build(UI_NAV_VIEW_LIVE);return;}
 if(next.session.state==UI_NAV_SESSION_ARRIVED&&s.view==UI_NAV_VIEW_LIVE&&!s.arrival_map){build(UI_NAV_VIEW_ARRIVAL);return;}
 if(s.view==UI_NAV_VIEW_LIVE&&(next.guidance.reroute_failed || (next.guidance.off_route&&!(next.capabilities&UI_NAV_CAP_REROUTE)))){build(UI_NAV_VIEW_ERROR);return;}
 if(s.view==UI_NAV_VIEW_ERROR&&!next.guidance.off_route&&!next.guidance.reroute_failed&&next.guidance.guidance_valid){build(UI_NAV_VIEW_LIVE);return;}
 if((catalog&&auxiliary(s.view))||(result&&s.view!=UI_NAV_VIEW_LIVE)||(changed&&s.view==UI_NAV_VIEW_PREVIEW&&old!=next.session.state)){build(s.view);return;}
 if(s.view==UI_NAV_VIEW_LIVE){ui_nav_data_strip_update(&s.trip,&next.guidance);ui_text_set_label(s.trip.titles[0],"当前速度",20);lv_label_set_text(s.trip.units[0],"km/h");ui_nav_command_dock_update(&s.maneuver,&next.guidance,false);
  if(s.map&&s.map->state==NAV_OVERVIEW){ui_text_set_label(s.trip.titles[0],"总距离",20);lv_label_set_text(s.trip.units[0],"km");
   if(next.session.route.distance_valid)lv_label_set_text_fmt(s.trip.values[0],"%.1f",next.session.route.distance_m/1000.);else lv_label_set_text(s.trip.values[0],"--");}}
 compact_update();
}
void ui_nav_workspace_create(lv_obj_t *parent,void (*unused)(bool),void (*exit_page)(void))
{
 (void)unused;memset(&s,0,sizeof(s));s.exit_page=exit_page;
 s.root=ui_nav_surface(parent,8,UI_PAGE_CONTENT_BOTTOM-UI_NAV_FIXED_DOCK_HEIGHT,464,UI_NAV_FIXED_DOCK_HEIGHT,false);s.content=ui_nav_surface(s.root,0,0,464,UI_NAV_FIXED_DOCK_HEIGHT,false);
 s.controls_active=true;
 lv_obj_add_event_cb(s.content,live_click,LV_EVENT_CLICKED,NULL);
 s.aux=ui_nav_surface(parent,0,62,480,776,false);lv_obj_set_style_bg_color(s.aux,lv_color_black(),0);lv_obj_set_style_bg_opa(s.aux,LV_OPA_COVER,0);
 s.fallback_slot=ui_nav_surface(parent,8,62,464,776,false);lv_obj_add_flag(s.fallback_slot,LV_OBJ_FLAG_OVERFLOW_VISIBLE);
 ui_nav_data_strip_create(&s.trip,s.root);lv_obj_set_pos(s.trip.root,0,0);lv_obj_set_style_bg_opa(s.trip.root,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(s.trip.root,0,0);
 ui_nav_command_dock_create(&s.maneuver,s.root);lv_obj_set_pos(s.maneuver.root,0,96);lv_obj_set_style_bg_opa(s.maneuver.root,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(s.maneuver.root,0,0);lv_obj_add_flag(s.maneuver.accent,LV_OBJ_FLAG_HIDDEN);
 lv_obj_move_foreground(s.content);s.created=true;s.view=UI_NAV_VIEW_CENTER;ui_nav_store_get(&s.snapshot);build(s.view);
}
void ui_nav_workspace_bind_map(ui_nav_map_t *map){s.map=map;}
void ui_nav_workspace_set_controls_active(bool active){if(!s.created||s.controls_active==active)return;s.controls_active=active;focus_update(false);}
void ui_nav_workspace_set_active(bool active)
{
 s.active=active;if(!active){ui_bottom_surface_set_page_content(NULL,0,false);lv_obj_add_flag(s.aux,LV_OBJ_FLAG_HIDDEN);return;}
 ui_bottom_surface_state_t state;if(ui_bottom_surface_get_state(&state)&&!state.mounted)ui_bottom_surface_mount(s.fallback_slot,false);
 ui_nav_store_get(&s.snapshot);
 if(s.snapshot.session.state==UI_NAV_SESSION_ACTIVE)build(s.view==UI_NAV_VIEW_MENU||s.view==UI_NAV_VIEW_STOP||s.view==UI_NAV_VIEW_ERROR?s.view:UI_NAV_VIEW_LIVE);
 else if(s.snapshot.session.state==UI_NAV_SESSION_ARRIVED&&!s.arrival_map)build(UI_NAV_VIEW_ARRIVAL);
 else build(s.view);
}
bool ui_nav_workspace_back(void)
{
 if(s.view==UI_NAV_VIEW_MENU){build(UI_NAV_VIEW_LIVE);return true;}
 if(s.view==UI_NAV_VIEW_STOP||s.view==UI_NAV_VIEW_DELETE){if(s.snapshot.busy)return true;build(s.return_view);return true;}
 if(s.view==UI_NAV_VIEW_PICK&&s.search_origin){if(s.map)ui_nav_map_center_point(s.map,s.selected_place.point);build(UI_NAV_VIEW_DESTINATION);return true;}
 if(s.view==UI_NAV_VIEW_PICK||s.view==UI_NAV_VIEW_DESTINATION){if(s.map)s.map->pick_point_valid=false;build(UI_NAV_VIEW_CENTER);if(s.search_origin)ui_navigation_open_search();return true;}
 if(s.view==UI_NAV_VIEW_PREVIEW){if(s.snapshot.busy)ui_nav_session_cancel();if(s.search_origin){ui_nav_session_discard_preview();if(s.map){s.map->route_point_count=0;s.map->pick_point_valid=false;s.map->detail_frame.destination_valid=false;lv_obj_invalidate(s.map->route_layer);}build(UI_NAV_VIEW_CENTER);ui_navigation_open_search();return true;}if(s.retry.kind==UI_NAV_REQ_PLAN){build(UI_NAV_VIEW_DESTINATION);return true;}ui_navigation_open_routes();return true;}
 if(s.view==UI_NAV_VIEW_MAP_DETAIL||s.view==UI_NAV_VIEW_MAP_VERSIONS||s.view==UI_NAV_VIEW_STORAGE){load(UI_NAV_LIST_MAPS,0);return true;}
 if(auxiliary(s.view)){build(ui_nav_session_active()?UI_NAV_VIEW_LIVE:UI_NAV_VIEW_CENTER);return true;}
 if(s.view==UI_NAV_VIEW_ARRIVAL){perform(A_OVERVIEW,0);return true;}
 return false;
}
bool ui_nav_workspace_handle_input(ui_input_event_t input)
{
 if(input==UI_INPUT_BACK)return ui_nav_workspace_back();
 if(s.view==UI_NAV_VIEW_LIVE&&!s.count)return false;
 if(input==UI_INPUT_ENCODER_CW||input==UI_INPUT_ENCODER_CCW){if(s.count){s.selected=(s.selected+s.count+(input==UI_INPUT_ENCODER_CW?1:-1))%s.count;focus_update(true);}return true;}
 if(input==UI_INPUT_ENCODER_PRESS){if(s.count&&!lv_obj_has_state(s.focus[s.selected],LV_STATE_DISABLED))lv_obj_send_event(s.focus[s.selected],LV_EVENT_CLICKED,NULL);return true;}
 return false;
}
bool ui_nav_workspace_contains_point(int32_t x,int32_t y)
{
 ui_bottom_surface_state_t state;if(!s.active||auxiliary(s.view)||!ui_bottom_surface_get_state(&state)||state.mode!=UI_BOTTOM_SURFACE_DYNAMIC_CARD_MODE)return false;
 lv_area_t a;lv_obj_get_coords(ui_bottom_surface_root(),&a);return x>=a.x1&&x<=a.x2&&y>=a.y1&&y<=a.y2;
}
void ui_nav_workspace_cancel_pointer(void){if(s.pointer_dragged)ui_motion_spring(s.content,capsule_y,lv_obj_get_y(s.content),0,true,NULL,NULL);s.pointer_owned=s.pointer_dragged=false;}
void ui_nav_workspace_handle_pointer(const ui_input_pointer_event_t *event)
{
 if(event->type==UI_INPUT_TOUCH_BEGIN){s.pointer_owned=ui_nav_workspace_contains_point(event->x,event->y);s.pointer_dragged=false;s.pointer_x=event->x;s.pointer_y=event->y;s.pointer_view=s.view;return;}
 if(event->type==UI_INPUT_TOUCH_CANCEL){ui_nav_workspace_cancel_pointer();return;}
 if(!s.pointer_owned)return;
 if(s.view!=s.pointer_view){ui_nav_workspace_cancel_pointer();return;}
 int dx=event->x-s.pointer_x,dy=event->y-s.pointer_y;
 bool up=s.view==UI_NAV_VIEW_LIVE&&dy<0,down=s.view==UI_NAV_VIEW_MENU&&dy>0;
 if(event->type==UI_INPUT_TOUCH_MOVE&&(up||down)&&abs(dy)>24&&abs(dy)>abs(dx)){
  if(!s.pointer_dragged){s.pointer_dragged=true;for(lv_indev_t *i=lv_indev_get_next(NULL);i;i=lv_indev_get_next(i))if(lv_indev_get_type(i)==LV_INDEV_TYPE_POINTER){lv_indev_reset(i,NULL);lv_indev_wait_release(i);}}
  ui_motion_cancel(s.content,capsule_y);lv_obj_set_y(s.content,LV_CLAMP(-12,dy/6,12));
 }
 if(event->type==UI_INPUT_TOUCH_END){bool commit=s.pointer_dragged&&abs(dy)>=48;s.pointer_owned=false;
  if(commit)build(up?UI_NAV_VIEW_MENU:UI_NAV_VIEW_LIVE);else if(s.pointer_dragged)ui_motion_spring(s.content,capsule_y,lv_obj_get_y(s.content),0,true,NULL,NULL);s.pointer_dragged=false;}
}

void ui_nav_workspace_open_menu(void){if(ui_nav_session_active())build(s.view==UI_NAV_VIEW_MENU?UI_NAV_VIEW_LIVE:UI_NAV_VIEW_MENU);else ui_navigation_open_routes();}
void ui_nav_workspace_open_routes(void){load(UI_NAV_LIST_ROUTES,0);}
void ui_nav_workspace_preview_destination(const ui_nav_item_t *place){if(!place||ui_nav_session_active())return;
 if(s.search_origin&&s.view==UI_NAV_VIEW_PICK){s.selected_place.point=place->point;s.search_entrance=false;s.search_manual=true;}
 else {s.search_origin=false;s.selected_place=*place;}build(UI_NAV_VIEW_DESTINATION);}
void ui_nav_workspace_preview_search_destination(const ui_nav_item_t *place,bool mapped_entrance,bool online){if(!place||ui_nav_session_active())return;ui_nav_session_cancel();ui_nav_session_discard_preview();s.search_origin=true;s.search_entrance=mapped_entrance;s.search_online=online;s.search_manual=false;s.selected_place=*place;s.saved[UI_NAV_VIEW_DESTINATION].id=A_PLAN;build(UI_NAV_VIEW_DESTINATION);}
void ui_nav_workspace_suspend(bool value){s.suspended=value;if(ui_bottom_surface_root()){if(value)lv_obj_add_flag(ui_bottom_surface_root(),LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(ui_bottom_surface_root(),LV_OBJ_FLAG_HIDDEN);}}
void ui_nav_workspace_preview_route(const ui_nav_item_t *r)
{if(!r||ui_nav_session_active())return;s.search_origin=false;request_kind(UI_NAV_REQ_LOAD_ROUTE,0,0,r);build(UI_NAV_VIEW_PREVIEW);}
void ui_nav_workspace_open_phone(void){build(UI_NAV_VIEW_PHONE);}
void ui_nav_workspace_import_route(void){perform(A_IMPORT,0);}
void ui_nav_workspace_stop(void){perform(A_STOP,0);}
void ui_nav_workspace_set_start_callback(void (*callback)(void)){s.start_callback=callback;}
void ui_nav_workspace_set_finish_callback(bool (*callback)(void)){s.finish_callback=callback;}
bool ui_nav_workspace_start(void)
{
 static UI_LARGE_RAM ui_nav_session_t session;bool busy;ui_nav_session_get(&session);static UI_LARGE_RAM ui_nav_snapshot_t snapshot;ui_nav_store_get(&snapshot);busy=snapshot.busy;
 if(busy||session.state!=UI_NAV_SESSION_PREVIEW||!session.route.start_allowed)return false;
 request_kind(UI_NAV_REQ_START,0,0,NULL);build(UI_NAV_VIEW_PREVIEW);return s.request_id!=0;
}
void ui_nav_workspace_open_maps(void){load(UI_NAV_LIST_MAPS,0);}
void ui_nav_workspace_rebuild(void){build(s.view);}
bool ui_nav_workspace_map_visible(void){return !auxiliary(s.view);}
int32_t ui_nav_workspace_height(void){return s.height;}
lv_obj_t *ui_nav_workspace_root(void){return s.root;}
ui_nav_view_t ui_nav_workspace_view(void){return s.view;}
