#include "ui_saved_routes_page.h"
#include "ui_page_common.h"
#include "../core/ui_text.h"
#include "../core/ui_motion.h"
#include "../components/navigation/ui_nav_visual.h"
#include "../components/navigation/ui_nav_capsule_row.h"
#include "../components/navigation/ui_nav_layout.h"
#include "services/ui_nav_store.h"
#include "lvgl/src/draw/lv_draw_line.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
typedef enum { F_ROUTE,F_MAPS,F_IMPORT,F_REFRESH,F_ROUTES_TAB,F_PLACES_TAB } focus_section_t;
static struct {
 lv_obj_t *screen,*list,*tabs[2],*rows[8],*refresh,*maps,*import,*footer;
 ui_nav_capsule_row_t footer_choices,tab_choices;focus_section_t focus_section;
 ui_status_bar_t status;ui_page_action_cb_t callback;void *user_data;
 ui_nav_catalog_t catalog;ui_nav_list_t kind;uint32_t request_id,catalog_version,result_version,focus_id;
 int32_t scroll;uint8_t selected;bool active,reload;
} s;
static void load_offset(uint32_t offset);
static void focus_update(bool animate)
{
 for(unsigned i=0;i<s.catalog.count;i++)if(s.rows[i])ui_nav_button_focus(s.rows[i],s.focus_section==F_ROUTE&&i==s.selected);
 lv_obj_t *target=s.focus_section==F_MAPS?s.maps:s.focus_section==F_IMPORT?s.import:s.focus_section==F_REFRESH?s.refresh:NULL;
 /* Keep the last toolbar action visible while browsing. The route focus is
  * remembered separately, so an asynchronous refresh cannot jump to row 0. */
 ui_nav_capsule_row_focus(&s.footer_choices,target,true,animate);
}
static void action(lv_event_t *e)
{
 if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
 if(lv_event_get_target(e)==s.maps)s.focus_section=F_MAPS;
 else if(lv_event_get_target(e)==s.import)s.focus_section=F_IMPORT;
 focus_update(true);
 if(s.callback)s.callback((ui_page_action_t)(uintptr_t)lv_event_get_user_data(e),s.user_data);
}
static void thumb(lv_event_t *e)
{
 if(lv_event_get_code(e)!=LV_EVENT_DRAW_MAIN)return;unsigned i=(unsigned)(uintptr_t)lv_event_get_user_data(e);if(i>=s.catalog.count)return;
 const ui_nav_item_t *r=&s.catalog.items[i];if(r->thumbnail_count<2)return;
 lv_area_t a;lv_obj_get_coords(lv_event_get_target(e),&a);double minx=r->thumbnail[0].longitude,maxx=minx,miny=r->thumbnail[0].latitude,maxy=miny;
 for(unsigned j=1;j<r->thumbnail_count;j++){minx=fmin(minx,r->thumbnail[j].longitude);maxx=fmax(maxx,r->thumbnail[j].longitude);miny=fmin(miny,r->thumbnail[j].latitude);maxy=fmax(maxy,r->thumbnail[j].latitude);}
 double c=cos(r->thumbnail[0].latitude*.01745329252),scale=fmin(120/fmax(1e-7,(maxx-minx)*c),120/fmax(1e-7,maxy-miny));
 for(unsigned j=1;j<r->thumbnail_count;j++){lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);line.color=lv_color_hex(0xFFD900);line.width=4;line.round_start=line.round_end=true;
  line.p1=(lv_point_precise_t){lround(a.x1+72+(r->thumbnail[j-1].longitude-(minx+maxx)/2)*c*scale),lround(a.y1+72- (r->thumbnail[j-1].latitude-(miny+maxy)/2)*scale)};
  line.p2=(lv_point_precise_t){lround(a.x1+72+(r->thumbnail[j].longitude-(minx+maxx)/2)*c*scale),lround(a.y1+72- (r->thumbnail[j].latitude-(miny+maxy)/2)*scale)};lv_draw_line(lv_event_get_layer(e),&line);}
}
static void select_item(lv_event_t *e)
{if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;unsigned i=(unsigned)(uintptr_t)lv_event_get_user_data(e);if(i>=s.catalog.count)return;
 s.focus_section=F_ROUTE;s.selected=i;s.focus_id=s.catalog.items[i].id;s.scroll=lv_obj_get_scroll_y(s.list);if(s.callback)s.callback(UI_PAGE_ACTION_START_NAVIGATION,s.user_data);}
static void refresh(lv_event_t *e){if(lv_event_get_code(e)==LV_EVENT_CLICKED){s.focus_section=F_REFRESH;focus_update(true);load_offset(s.catalog.offset+s.catalog.count<s.catalog.total?s.catalog.offset+s.catalog.count:0);}}
static void tab(lv_event_t *e)
{if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;unsigned i=(unsigned)(uintptr_t)lv_event_get_user_data(e);
 if(i&&!(ui_nav_store_capabilities()&UI_NAV_CAP_PLACES))return;
 if(ui_nav_session_busy())return;
 s.focus_section=i?F_PLACES_TAB:F_ROUTES_TAB;s.kind=i?UI_NAV_LIST_FAVORITES:UI_NAV_LIST_ROUTES;s.scroll=0;s.focus_id=0;load_offset(0);}
static void render(const char *empty)
{
 lv_obj_clean(s.list);memset(s.rows,0,sizeof(s.rows));
 ui_text_set_label(lv_obj_get_child(s.refresh,0),s.catalog.offset+s.catalog.count<s.catalog.total?"下一页":"刷新",20);
 ui_nav_capsule_row_focus(&s.tab_choices,s.tabs[s.kind==UI_NAV_LIST_FAVORITES],true,true);
 if(!s.catalog.count){lv_obj_t *c=ui_nav_surface(s.list,0,0,448,400,true);ui_nav_icon(c,190,46,68,NAV_ICON_MAP,0xFFD900);
  lv_obj_t *t=ui_nav_label(c,24,154,400,empty,24,false);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);
  t=ui_nav_label(c,24,204,400,s.kind==UI_NAV_LIST_RECENT_ROUTES?"开始使用路线后，将记录在这里":"同步或导入路线后，可先预览再开始",20,true);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);
 }else{
  if(s.selected>=s.catalog.count)s.selected=0;
  for(unsigned i=0;i<s.catalog.count;i++)if(s.catalog.items[i].id==s.focus_id)s.selected=i;
  for(unsigned i=0;i<s.catalog.count;i++){const ui_nav_item_t *r=&s.catalog.items[i];lv_obj_t *b=ui_nav_button(s.list,0,i*206,448,194,"",false,false);s.rows[i]=b;
   lv_obj_add_event_cb(b,select_item,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
   lv_obj_t *tile=ui_nav_surface(b,14,24,144,144,true);lv_obj_set_style_bg_color(tile,lv_color_hex(0x151C20),0);
   if(r->thumbnail_count>=2)lv_obj_add_event_cb(tile,thumb,LV_EVENT_DRAW_MAIN,(void *)(uintptr_t)i);else ui_nav_icon(tile,46,46,52,NAV_ICON_MAP,0xFFD900);
   lv_obj_t *name=ui_nav_label(b,178,16,240,r->name,24,false);lv_obj_set_height(name,34);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
   char a[40],btext[40],c[40];if(r->distance_valid)snprintf(a,40,"%.1f km",r->distance_m/1000.);else strcpy(a,"-- km");
   if(r->ascent_valid)snprintf(btext,40,"+%lu m",(unsigned long)r->ascent_m);else strcpy(btext,"-- m");
   if(r->duration_valid)snprintf(c,40,"%lu min",(unsigned long)(r->duration_s/60));else strcpy(c,"-- min");
   ui_nav_label(b,178,58,230,a,24,false);ui_nav_label(b,178,94,230,btext,24,false);ui_nav_label(b,178,130,230,c,24,false);
   ui_nav_label(b,178,163,248,"路线预览",16,true);
  }
 }
 lv_obj_update_layout(s.list);lv_obj_scroll_to_y(s.list,s.scroll,LV_ANIM_OFF);focus_update(true);
}
static void load_offset(uint32_t offset)
{
 if(ui_nav_session_busy())return;
 s.catalog=(ui_nav_catalog_t){.kind=s.kind,.offset=offset};s.reload=false;
 s.request_id=ui_nav_store_command(&(ui_nav_request_t){.kind=UI_NAV_REQ_LIST,.list=s.kind,.offset=offset},lv_tick_get());render("正在读取路线…");
}
static void tick(lv_timer_t *t)
{
 (void)t;if(!s.active)return;if(s.reload&&!ui_nav_session_busy())load_offset(s.catalog.offset);
 uint32_t cv;ui_nav_catalog_t c;ui_nav_session_get_catalog(&c,&cv);
 if(cv!=s.catalog_version&&c.kind==s.kind&&c.request_id==s.request_id){s.catalog=c;s.catalog_version=cv;render(s.kind==UI_NAV_LIST_RECENT_ROUTES?"暂无最近使用的路线":"暂无导航内容");}
 ui_nav_result_t r;uint32_t rv;ui_nav_session_get_result(&r,&rv);
 if(rv!=s.result_version){s.result_version=rv;if(r.request_id==s.request_id&&r.code!=UI_NAV_OK)render(ui_nav_result_text(r.code));}
 bool places=ui_nav_store_capabilities()&UI_NAV_CAP_PLACES;if(places)lv_obj_clear_state(s.tabs[1],LV_STATE_DISABLED);else lv_obj_add_state(s.tabs[1],LV_STATE_DISABLED);
}
lv_obj_t *ui_saved_routes_page_create(ui_page_action_cb_t cb,void *u)
{
 memset(&s,0,sizeof(s));s.kind=UI_NAV_LIST_ROUTES;s.callback=cb;s.user_data=u;s.screen=ui_page_screen_create();ui_page_status_create(&s.status,s.screen);
 lv_obj_t *back=ui_nav_button(s.screen,16,70,48,44,"",false,false);lv_obj_set_style_bg_opa(back,LV_OPA_TRANSP,0);lv_obj_set_style_border_opa(back,LV_OPA_TRANSP,0);
 lv_obj_t *arrow=lv_obj_get_child(back,0);lv_label_set_text(arrow,LV_SYMBOL_LEFT);lv_obj_set_style_text_font(arrow,&lv_font_montserrat_28,0);lv_obj_add_event_cb(back,action,LV_EVENT_CLICKED,(void *)UI_PAGE_ACTION_BACK);
 ui_nav_label(s.screen,78,74,366,"我的导航内容",24,false);
 s.tabs[0]=ui_nav_button(s.screen,16,126,224,54,"路线",true,false);s.tabs[1]=ui_nav_button(s.screen,240,126,224,54,"地点",false,false);
 for(unsigned i=0;i<2;i++)lv_obj_add_event_cb(s.tabs[i],tab,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
 ui_nav_capsule_row_create(&s.tab_choices,s.screen,16,126,448,54,s.tabs,2);
 s.list=ui_nav_surface(s.screen,16,194,448,UI_NAV_DATA_TOP-194-12,false);lv_obj_add_flag(s.list,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(s.list,LV_DIR_VER);ui_page_scrollbar_style(s.list);lv_obj_set_scrollbar_mode(s.list,LV_SCROLLBAR_MODE_AUTO);
 s.footer=ui_nav_surface(s.screen,8,UI_NAV_DATA_TOP,464,UI_NAV_FIXED_DOCK_HEIGHT,true);lv_obj_set_style_radius(s.footer,24,0);
 lv_obj_t *footer_title=ui_nav_label(s.footer,16,30,432,"导航资源",24,false);
 lv_obj_set_style_text_align(footer_title,LV_TEXT_ALIGN_CENTER,0);
 const nav_icon_t icons[]={NAV_ICON_MAP,NAV_ICON_PHONE,NAV_ICON_CLOCK};
 for(unsigned i=0;i<3;i++)ui_nav_icon(s.footer,16+i*144+55,78,34,icons[i],0xFFD900);
 s.maps=ui_nav_button(s.footer,16,142,144,72,"离线地图",false,false);s.import=ui_nav_button(s.footer,160,142,144,72,"获取路线",false,false);
 lv_obj_add_event_cb(s.maps,action,LV_EVENT_CLICKED,(void *)UI_PAGE_ACTION_NAV_MAPS);lv_obj_add_event_cb(s.import,action,LV_EVENT_CLICKED,(void *)UI_PAGE_ACTION_ROUTE_PHONE);
 s.refresh=ui_nav_button(s.footer,304,142,144,72,"刷新",true,false);lv_obj_add_event_cb(s.refresh,refresh,LV_EVENT_CLICKED,NULL);
 lv_obj_t *toolbar[]={s.maps,s.import,s.refresh};
 for(unsigned i=0;i<3;i++)ui_text_set_label(lv_obj_get_child(toolbar[i],0),lv_label_get_text(lv_obj_get_child(toolbar[i],0)),20);
 ui_nav_capsule_row_create(&s.footer_choices,s.footer,16,142,432,72,toolbar,3);ui_nav_capsule_row_focus(&s.footer_choices,s.refresh,true,false);
 render("暂无导航内容");lv_timer_create(tick,60,NULL);return s.screen;
}
void ui_saved_routes_page_set_kind(ui_nav_list_t kind){if(s.kind!=kind){s.kind=kind;s.catalog.offset=0;s.focus_id=0;s.scroll=0;s.selected=0;s.focus_section=F_ROUTE;}s.reload=true;}
void ui_saved_routes_page_set_active(bool active)
{if(!active){s.scroll=lv_obj_get_scroll_y(s.list);if(s.selected<s.catalog.count)s.focus_id=s.catalog.items[s.selected].id;
 ui_nav_session_cancel_request(s.request_id);}
 s.active=active;if(active){s.reload=true;tick(NULL);}}
ui_nav_list_t ui_saved_routes_page_kind(void){return s.kind;}
bool ui_saved_routes_page_selected(ui_nav_item_t *item){if(!item||s.selected>=s.catalog.count)return false;*item=s.catalog.items[s.selected];return true;}
bool ui_saved_routes_page_handle_input(ui_input_event_t input)
{
 lv_obj_t *focus[13];focus_section_t sections[13];unsigned count=0,current=0;
 for(unsigned i=0;i<s.catalog.count;i++)if(s.rows[i]){focus[count]=s.rows[i];sections[count]=F_ROUTE;if(s.focus_section==F_ROUTE&&s.selected==i)current=count;count++;}
 const focus_section_t actions[]={F_MAPS,F_IMPORT,F_REFRESH,F_ROUTES_TAB,F_PLACES_TAB};
 lv_obj_t *buttons[]={s.maps,s.import,s.refresh,s.tabs[0],s.tabs[1]};
 for(unsigned i=0;i<5;i++){if(lv_obj_has_state(buttons[i],LV_STATE_DISABLED))continue;focus[count]=buttons[i];sections[count]=actions[i];if(s.focus_section==actions[i])current=count;count++;}
 if(input==UI_INPUT_ENCODER_PRESS){if(!lv_obj_has_state(focus[current],LV_STATE_DISABLED))lv_obj_send_event(focus[current],LV_EVENT_CLICKED,NULL);return true;}
 if(input==UI_INPUT_ENCODER_CW||input==UI_INPUT_ENCODER_CCW){current=(current+count+(input==UI_INPUT_ENCODER_CW?1:-1))%count;s.focus_section=sections[current];
  if(s.focus_section==F_ROUTE){s.selected=current;s.focus_id=s.catalog.items[s.selected].id;lv_obj_scroll_to_view(focus[current],ui_motion_scroll_mode());}
  /* Tabs represent the applied content filter; focus must not change it until pressed. */
  for(unsigned i=0;i<2;i++)lv_obj_set_style_outline_width(s.tabs[i],s.focus_section==(i?F_PLACES_TAB:F_ROUTES_TAB)?2:0,0);
  focus_update(true);return true;}
 return false;
}
