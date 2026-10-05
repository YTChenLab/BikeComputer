#include "ui_nav_search_panel.h"
#include "ui_nav_visual.h"
#include "ui_nav_layout.h"
#include "ui/pages/navigation/ui_navigation.h"
#include "ui/core/ui_motion.h"
#include "ui/core/ui_text.h"
#include "ui/core/ui_theme.h"
#include "lvgl/src/draw/lv_draw_line.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { K_DELETE=256,K_RESULTS,K_EXIT,K_QUERY,K_RESULT=512 };
enum { LETTER_PITCH=84,LETTER_UNIT=256,LETTER_CELLS=7,RESULT_HEIGHT=86,RESULT_PITCH=94 };
typedef enum { FOCUS_LETTERS,FOCUS_RESULTS } focus_zone_t;
static struct {
 lv_obj_t *root,*header,*header_bg,*query_label,*status_label,*caret,*list,*input,*strip,*capsule,*index_mark;
 lv_obj_t *cells[LETTER_CELLS],*letters[LETTER_CELLS],*rows[UI_PLACE_RESULT_MAX],*delete_button,*results_button;
 ui_place_results_t results;ui_nav_point_t center;
 focus_zone_t zone;unsigned selected_result;uint32_t selected_place_id;
 int32_t selected_letter,position,pointer_position,shape,candidates_progress;int pointer_x,pointer_y,pressed_letter;
 bool center_valid,active,pending,pointer_down,pointer_dragged,pointer_cancelled,candidates_open;
 char query[UI_PLACE_QUERY_MAX];uint32_t edited,request,version;
} s;
static void render_results(void);
static void action(unsigned key);
static void move_y(void *o,int32_t v){lv_obj_set_y(o,v);}
static void width(void *o,int32_t v){lv_obj_set_width(o,v);}
static void height(void *o,int32_t v){lv_obj_set_height(o,v);}
static int letter_index(int value){return (value%26+26)%26;}
static int nearest_letter(int32_t position){return (int)lround(position/(double)LETTER_UNIT);}
static bool results_ready(void)
{return !s.pending&&s.request&&s.results.request_id==s.request&&!s.results.code&&s.results.count;}
static void candidates_geometry(void *unused,int32_t value)
{
 (void)unused;s.candidates_progress=LV_CLAMP(0,value,1000);
 int h=UI_NAV_SEARCH_LIST_HEIGHT*s.candidates_progress/1000;
 lv_obj_set_pos(s.list,8,UI_NAV_SEARCH_INPUT_TOP-12-h-62);
 lv_obj_set_height(s.list,LV_MAX(1,h));
 lv_obj_set_style_opa(s.list,(lv_opa_t)(LV_OPA_COVER*s.candidates_progress/1000),0);
 if(h)lv_obj_clear_flag(s.list,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(s.list,LV_OBJ_FLAG_HIDDEN);
 ui_navigation_search_candidates(s.candidates_progress);
}
static void candidates_show(bool show)
{
 if(show==s.candidates_open)return;s.candidates_open=show;
 if(s.active)ui_motion_value(s.list,candidates_geometry,s.candidates_progress,show?1000:0,
  show?300:220,lv_anim_path_ease_in_out,NULL,NULL);
 else{ui_motion_cancel(s.list,candidates_geometry);candidates_geometry(NULL,show?1000:0);}
}
static void dial_draw(lv_event_t *event)
{
 lv_area_t a;lv_obj_get_coords(s.strip,&a);lv_layer_t *layer=lv_event_get_layer(event);
 int base=nearest_letter(s.position);
 for(int quarter=-12;quarter<=12;quarter++){
  int dx=((base*LETTER_UNIT+quarter*LETTER_UNIT/4-s.position)*LETTER_PITCH)/LETTER_UNIT;
  if(abs(dx)>212)continue;bool major=quarter%4==0;
  unsigned gray=(unsigned)(160-LV_MIN(212,abs(dx))*100/212);
  lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
  d.p1=(lv_point_precise_t){a.x1+216+dx,a.y1+92-(major?7:3)};
  d.p2=(lv_point_precise_t){a.x1+216+dx,a.y1+92};d.width=major?2:1;
  d.color=lv_color_make(gray,gray,gray);d.opa=LV_OPA_COVER;
  lv_draw_line(layer,&d);
 }
}
static void capsule_draw(lv_event_t *event)
{
 lv_area_t a;lv_obj_get_coords(s.capsule,&a);
 lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.width=1;d.round_start=d.round_end=true;
 d.p1=(lv_point_precise_t){a.x1+14,a.y1+4};d.p2=(lv_point_precise_t){a.x2-14,a.y1+4};
 d.color=lv_color_hex(0xFFFFFF);d.opa=s.zone==FOCUS_LETTERS?LV_OPA_40:LV_OPA_10;
 lv_draw_line(lv_event_get_layer(event),&d);
}
static void capsule_shape(void *object,int32_t value)
{
 value=LV_CLAMP(-12,value,16);s.shape=value;int w=72+value,h=84-value/2;
 lv_obj_set_size(object,w,h);lv_obj_set_pos(object,(432-w)/2,(94-h)/2);
}
static void capsule_release(lv_anim_t *animation)
{(void)animation;if(s.active)ui_motion_spring(s.capsule,capsule_shape,s.shape,0,false,NULL,NULL);}
static void capsule_feedback(int amount)
{
 ui_motion_cancel(s.capsule,capsule_shape);
 ui_motion_value(s.capsule,capsule_shape,s.shape,amount,65,lv_anim_path_ease_out,capsule_release,NULL);
}

/* Seven recycled cells supply edge animation frames. Five complete touch
 * targets remain visible at rest; the capsule never deforms their text. */
static void strip_position(void *unused,int32_t value)
{
 (void)unused;s.position=value;int base=nearest_letter(value);
 for(unsigned i=0;i<LETTER_CELLS;i++){
  int logical=base+(int)i-3,dx=(logical*LETTER_UNIT-value)*LETTER_PITCH/LETTER_UNIT;
  lv_obj_set_pos(s.cells[i],216+dx-LETTER_PITCH/2,0);
  if(abs(dx)>216)lv_obj_add_flag(s.cells[i],LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(s.cells[i],LV_OBJ_FLAG_HIDDEN);
  char text[2]={(char)('A'+letter_index(logical)),0};lv_label_set_text(s.letters[i],text);
  uint32_t color=abs(dx)<36&&s.zone==FOCUS_LETTERS?0x101010:abs(dx)<LETTER_PITCH*3/2?0xFFFFFF:0x999999;
  lv_obj_set_style_text_color(s.letters[i],lv_color_hex(color),0);
 }
 lv_obj_invalidate(s.strip); /* Tick positions follow every sub-cell frame. */
}
static void strip_stop(void)
{ui_motion_cancel(s.strip,strip_position);s.pointer_down=false;s.pointer_cancelled=true;}
static void strip_select(int index,bool animate)
{
 /* Rebase before a long session can overflow fixed-point animation values. */
 if(abs(index)>1024){int shift=(index/26)*26;index-=shift;s.position-=shift*LETTER_UNIT;}
 s.selected_letter=index;
 if(animate)ui_motion_value(s.strip,strip_position,s.position,index*LETTER_UNIT,120,lv_anim_path_ease_out,NULL,NULL);
 else{ui_motion_cancel(s.strip,strip_position);strip_position(s.strip,index*LETTER_UNIT);}
 if(animate&&s.zone==FOCUS_LETTERS)capsule_feedback(10);
}
static void focus_update(bool scroll)
{
 bool ready=results_ready();
 lv_obj_set_style_bg_color(s.capsule,lv_color_hex(s.zone==FOCUS_LETTERS?0xFFD900:0x343434),0);
 lv_obj_set_style_bg_grad_color(s.capsule,lv_color_hex(s.zone==FOCUS_LETTERS?0xF6C900:0x2A2A2A),0);
 lv_obj_set_style_border_color(s.capsule,lv_color_hex(s.zone==FOCUS_LETTERS?0xFFE85C:0x484848),0);
 lv_obj_set_style_bg_color(s.index_mark,lv_color_hex(s.zone==FOCUS_LETTERS?0xFFD900:0x777777),0);
 strip_position(s.strip,s.position);
 for(unsigned i=0;i<s.results.count;i++)if(s.rows[i]){
  bool chosen=ready&&s.zone==FOCUS_RESULTS&&i==s.selected_result;
  lv_obj_set_style_bg_color(s.rows[i],lv_color_hex(chosen?0xFFD900:0x282828),0);
  for(unsigned j=0;j<lv_obj_get_child_count(s.rows[i]);j++){
   lv_obj_t *child=lv_obj_get_child(s.rows[i],j);
   lv_obj_set_style_text_color(child,lv_color_hex(chosen?0x101010:j==2?0xAAAAAA:0xFFFFFF),0);
  }
 }
 if(ready)lv_obj_clear_state(s.results_button,LV_STATE_DISABLED);
 else lv_obj_add_state(s.results_button,LV_STATE_DISABLED);
 ui_text_set_label(lv_obj_get_child(s.results_button,0),s.zone==FOCUS_RESULTS?"继续输入":"查看结果",20);
 lv_obj_set_style_text_color(lv_obj_get_child(s.results_button,0),lv_color_hex(ready?0x101010:0x888888),0);
 if(s.query[0])lv_obj_clear_state(s.delete_button,LV_STATE_DISABLED);
 else lv_obj_add_state(s.delete_button,LV_STATE_DISABLED);
 if(scroll&&ready&&s.zone==FOCUS_RESULTS)lv_obj_scroll_to_view(s.rows[s.selected_result],ui_motion_scroll_mode());
}
static void set_zone(focus_zone_t zone)
{
 if(zone==FOCUS_RESULTS&&!results_ready())return;
 strip_stop();strip_select(s.selected_letter,false);s.zone=zone;focus_update(true);
}
static void query_header(void)
{
 char status[24]="";
 if(strlen(s.query)>=2){
  if(s.pending||s.results.request_id!=s.request)snprintf(status,sizeof(status),"查找中");
  else if(results_ready())snprintf(status,sizeof(status),"%u 个地点",s.results.count);
  else snprintf(status,sizeof(status),"%s",s.results.code==UI_NAV_NO_MAP?"无索引":
   s.results.code==UI_NAV_UNAVAILABLE?"未接入":s.results.code==UI_NAV_TIMEOUT?"超时":s.results.code?"失败":"无结果");
 }
 ui_text_set_label(s.status_label,status,20);
 int available=status[0]?176:268;lv_obj_set_width(s.query_label,available);
 if(!s.query[0]){ui_text_set_label(s.query_label,"搜索地点",24);lv_obj_add_flag(s.caret,LV_OBJ_FLAG_HIDDEN);return;}
 char display[UI_PLACE_QUERY_MAX];size_t n=strlen(s.query);
 for(size_t i=0;i<=n;i++)display[i]=s.query[i]>='a'&&s.query[i]<='z'?s.query[i]-'a'+'A':s.query[i];
 const char *tail=display;lv_point_t size;
 do{lv_text_get_size(&size,tail,&lv_font_montserrat_24,0,0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
  if(size.x<=available)break;++tail;}while(*tail);
 lv_obj_set_style_text_font(s.query_label,&lv_font_montserrat_24,0);lv_label_set_text(s.query_label,tail);
 lv_obj_set_x(s.caret,44+size.x+3);lv_obj_clear_flag(s.caret,LV_OBJ_FLAG_HIDDEN);
}
static void query_changed(void)
{
 ui_nav_search_cancel();s.request=0;s.results=(ui_place_results_t){0};
 s.pending=strlen(s.query)>=2;s.edited=lv_tick_get();s.selected_result=0;s.selected_place_id=0;
 render_results();
}
static void commit_letter(int index)
{
 size_t n=strlen(s.query);if(n>=sizeof(s.query)-1)return;
 s.zone=FOCUS_LETTERS;strip_select(index,false);
 s.query[n]=(char)('a'+letter_index(index));s.query[n+1]=0;
 capsule_feedback(-8);query_changed();
}
static void action(unsigned key)
{
 if(!s.active)return;
 if(key>=K_RESULT){unsigned i=key-K_RESULT;
  if(results_ready()&&i<s.results.count){ui_place_t place=s.results.items[i];ui_navigation_search_selected(&place);}return;}
 if(key==K_EXIT){ui_navigation_close_search();return;}
 if(key==K_QUERY){set_zone(FOCUS_LETTERS);return;}
 if(key==K_RESULTS){set_zone(s.zone==FOCUS_LETTERS?FOCUS_RESULTS:FOCUS_LETTERS);return;}
 if(key==K_DELETE){size_t n=strlen(s.query);if(n){s.zone=FOCUS_LETTERS;s.query[n-1]=0;query_changed();}}
}
static void click(lv_event_t *event)
{if(lv_event_get_code(event)==LV_EVENT_CLICKED)action((unsigned)(uintptr_t)lv_event_get_user_data(event));}
static lv_obj_t *button(lv_obj_t *parent,int x,int y,int w,int h,const char *text,unsigned key)
{
 lv_obj_t *o=ui_nav_button(parent,x,y,w,h,text,false,false);
 lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_radius(o,key>=K_RESULT?18:h/2,0);
 lv_obj_set_style_bg_color(o,lv_color_hex(0x282828),0);
 lv_obj_set_style_opa(o,LV_OPA_50,LV_STATE_DISABLED);
 lv_obj_add_event_cb(o,click,LV_EVENT_CLICKED,(void *)(uintptr_t)key);return o;
}
static void strip_event(lv_event_t *event)
{
 if(!s.active)return;lv_event_code_t code=lv_event_get_code(event);
 lv_indev_t *indev=lv_indev_active();if(!indev)return;lv_point_t point;lv_indev_get_point(indev,&point);
 if(code==LV_EVENT_PRESSED){
  ui_motion_cancel(s.strip,strip_position);ui_motion_cancel(s.capsule,capsule_shape);capsule_shape(s.capsule,0);
  s.zone=FOCUS_LETTERS;s.pointer_down=true;s.pointer_dragged=s.pointer_cancelled=false;
  s.pointer_x=point.x;s.pointer_y=point.y;s.pointer_position=s.position;
  lv_area_t a;lv_obj_get_coords(s.strip,&a);
  s.pressed_letter=nearest_letter(s.position+(point.x-(a.x1+a.x2)/2)*LETTER_UNIT/LETTER_PITCH);
  focus_update(false);
 }else if(code==LV_EVENT_PRESSING&&s.pointer_down){
  int dx=point.x-s.pointer_x,dy=point.y-s.pointer_y;
  if(!s.pointer_dragged&&abs(dy)>16&&abs(dy)>abs(dx))s.pointer_cancelled=true;
  if(s.pointer_cancelled)return;
  if(abs(dx)>8)s.pointer_dragged=true;
  if(s.pointer_dragged){
   strip_position(s.strip,s.pointer_position-dx*LETTER_UNIT/LETTER_PITCH);
   int next=nearest_letter(s.position);if(next!=s.selected_letter)capsule_feedback(4);s.selected_letter=next;
  }
 }else if(code==LV_EVENT_RELEASED){
  s.pointer_down=false;if(s.pointer_dragged||s.pointer_cancelled)strip_select(s.selected_letter,true);
 }else if(code==LV_EVENT_CLICKED){
  if(!s.pointer_dragged&&!s.pointer_cancelled)commit_letter(s.pressed_letter);
 }else if(code==LV_EVENT_PRESS_LOST){strip_stop();strip_select(s.selected_letter,true);}
}
static void render_results(void)
{
 bool ready=results_ready();
 if(!ready){
  /* Retain only the outgoing visual while retracting; stale rows cannot act. */
  for(unsigned i=0;i<UI_PLACE_RESULT_MAX;i++)if(s.rows[i])lv_obj_clear_flag(s.rows[i],LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(s.list,LV_OBJ_FLAG_SCROLLABLE);
 }else{
  lv_obj_clean(s.list);memset(s.rows,0,sizeof(s.rows));lv_obj_add_flag(s.list,LV_OBJ_FLAG_SCROLLABLE);
  if(s.selected_result>=s.results.count)s.selected_result=s.results.count-1;
  for(unsigned i=0;i<s.results.count;i++)if(s.results.items[i].id==s.selected_place_id)s.selected_result=i;
  s.selected_place_id=s.results.items[s.selected_result].id;
  for(unsigned i=0;i<s.results.count;i++){
   const ui_place_t *place=&s.results.items[i];lv_obj_t *o=button(s.list,8,8+i*RESULT_PITCH,448,RESULT_HEIGHT,"",K_RESULT+i);s.rows[i]=o;
   lv_obj_t *name=ui_nav_label(o,16,10,412,place->name,24,false);
   lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_height(name,28);
   lv_obj_t *detail=ui_nav_label(o,16,49,284,place->detail,20,true);
   lv_label_set_long_mode(detail,LV_LABEL_LONG_DOT);lv_obj_set_height(detail,24);
   char distance[32]="";
   if(s.center_valid){double meters=hypot((place->point.latitude-s.center.latitude)*111319.49,(place->point.longitude-s.center.longitude)*92000);
    if(meters>=1000)snprintf(distance,sizeof(distance),"%.1f km",meters/1000);else snprintf(distance,sizeof(distance),"%.0f m",meters);}
   lv_obj_t *d=ui_nav_label(o,304,49,124,distance,20,false);lv_obj_set_style_text_align(d,LV_TEXT_ALIGN_RIGHT,0);lv_obj_set_height(d,24);
  }
  lv_obj_scroll_to_y(s.list,0,LV_ANIM_OFF);
 }
 query_header();focus_update(false);candidates_show(ready);
}
static void poll(lv_timer_t *timer)
{
 (void)timer;if(!s.active)return;uint32_t now=lv_tick_get();ui_nav_search_tick(now);
 if(s.pending&&now-s.edited>=160){
  s.pending=false;s.request=ui_nav_search_request(s.query,s.center,s.center_valid,now);s.results=(ui_place_results_t){0};render_results();
 }
 ui_place_results_t results;uint32_t version;bool busy;ui_nav_search_get(&results,&version,&busy);
 if(version!=s.version){s.version=version;if(!busy&&results.request_id==s.request&&s.request){s.results=results;render_results();}}
}
void ui_nav_search_panel_create(lv_obj_t *parent)
{
 if(s.root)return;memset(&s,0,sizeof(s));s.root=ui_nav_surface(parent,0,62,480,776,false);
 s.header=ui_nav_surface(s.root,8,0,464,56,false);s.header_bg=ui_nav_surface(s.header,0,0,464,56,true);
 lv_obj_set_style_radius(s.header_bg,28,0);lv_obj_set_style_border_color(s.header_bg,lv_color_hex(0xFFD900),0);
 lv_obj_t *query=button(s.header,12,4,376,48,"",K_QUERY);lv_obj_set_style_bg_opa(query,LV_OPA_TRANSP,0);
 ui_nav_icon(query,8,10,28,NAV_ICON_SEARCH,0xFFFFFF);
 s.query_label=ui_nav_label(query,44,10,280,"搜索地点",24,false);lv_label_set_long_mode(s.query_label,LV_LABEL_LONG_CLIP);lv_obj_set_height(s.query_label,28);
 s.status_label=ui_nav_label(s.header,300,18,84,"",20,true);lv_obj_set_style_text_align(s.status_label,LV_TEXT_ALIGN_RIGHT,0);
 lv_label_set_long_mode(s.status_label,LV_LABEL_LONG_CLIP);lv_obj_set_height(s.status_label,24);
 s.caret=ui_nav_label(query,44,8,10,"|",24,false);lv_obj_set_style_text_font(s.caret,&lv_font_montserrat_24,0);
 lv_obj_set_style_text_color(s.caret,lv_color_hex(0xFFD900),0);lv_obj_add_flag(s.caret,LV_OBJ_FLAG_HIDDEN);
 lv_obj_t *exit=button(s.header,394,4,60,48,"退出",K_EXIT);ui_text_set_label(lv_obj_get_child(exit,0),"退出",20);lv_obj_set_width(lv_obj_get_child(exit,0),56);lv_obj_center(lv_obj_get_child(exit,0));
 s.list=ui_nav_surface(s.root,8,UI_NAV_SEARCH_LIST_TOP-62,464,UI_NAV_SEARCH_LIST_HEIGHT,true);
 lv_obj_set_style_radius(s.list,24,0);lv_obj_add_flag(s.list,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(s.list,LV_DIR_VER);
 lv_obj_set_scrollbar_mode(s.list,LV_SCROLLBAR_MODE_AUTO);lv_obj_set_style_width(s.list,4,LV_PART_SCROLLBAR);lv_obj_set_style_bg_color(s.list,lv_color_hex(0xFFD900),LV_PART_SCROLLBAR);
 candidates_geometry(NULL,0);
 s.input=ui_nav_surface(s.root,8,UI_NAV_SEARCH_INPUT_TOP-62,464,UI_NAV_SEARCH_INPUT_HEIGHT,true);lv_obj_set_style_radius(s.input,24,0);
 s.strip=ui_nav_surface(s.input,16,14,432,94,false);lv_obj_set_style_clip_corner(s.strip,true,0);lv_obj_add_flag(s.strip,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
 lv_obj_clear_flag(s.strip,LV_OBJ_FLAG_GESTURE_BUBBLE);lv_obj_add_event_cb(s.strip,strip_event,LV_EVENT_ALL,NULL);
 lv_obj_add_event_cb(s.strip,dial_draw,LV_EVENT_DRAW_MAIN,NULL);
 s.capsule=ui_nav_surface(s.strip,180,5,72,84,false);lv_obj_set_style_radius(s.capsule,20,0);
 lv_obj_set_style_bg_color(s.capsule,lv_color_hex(0xFFD900),0);lv_obj_set_style_bg_opa(s.capsule,LV_OPA_COVER,0);
 lv_obj_set_style_bg_grad_dir(s.capsule,LV_GRAD_DIR_VER,0);lv_obj_set_style_border_width(s.capsule,1,0);
 lv_obj_add_event_cb(s.capsule,capsule_draw,LV_EVENT_DRAW_MAIN,NULL);
 for(unsigned i=0;i<LETTER_CELLS;i++){
  s.cells[i]=ui_nav_surface(s.strip,0,0,LETTER_PITCH,94,false);
  s.letters[i]=lv_label_create(s.cells[i]);lv_label_set_text(s.letters[i],"A");lv_obj_set_style_text_font(s.letters[i],&ui_font_misans_nav_letter_40,0);
  lv_obj_set_style_text_color(s.letters[i],lv_color_white(),0);lv_obj_center(s.letters[i]);lv_obj_clear_flag(s.letters[i],LV_OBJ_FLAG_CLICKABLE);
 }
 s.index_mark=ui_nav_surface(s.strip,210,91,12,3,false);lv_obj_set_style_radius(s.index_mark,2,0);
 lv_obj_set_style_bg_color(s.index_mark,lv_color_hex(0xFFD900),0);lv_obj_set_style_bg_opa(s.index_mark,LV_OPA_COVER,0);
 s.delete_button=button(s.input,16,116,210,56,"退格",K_DELETE);
 lv_obj_t *delete_icon=lv_label_create(s.delete_button);lv_label_set_text(delete_icon,LV_SYMBOL_BACKSPACE);lv_obj_set_style_text_font(delete_icon,&lv_font_montserrat_24,0);
 lv_obj_set_size(delete_icon,28,lv_font_montserrat_24.line_height);lv_obj_set_style_text_color(delete_icon,lv_color_white(),0);
 ui_text_set_label(lv_obj_get_child(s.delete_button,0),"退格",20);
 ui_nav_center_pair(delete_icon,lv_obj_get_child(s.delete_button,0),12,0,186,56,12);
 s.results_button=button(s.input,238,116,210,56,"查看结果",K_RESULTS);ui_text_set_label(lv_obj_get_child(s.results_button,0),"查看结果",20);
 lv_obj_set_style_bg_color(s.results_button,lv_color_hex(0xFFD900),0);lv_obj_set_style_text_color(lv_obj_get_child(s.results_button,0),lv_color_black(),0);
 lv_obj_set_style_bg_color(s.results_button,lv_color_hex(0x242424),LV_STATE_DISABLED);
 lv_obj_set_style_opa(s.results_button,LV_OPA_COVER,LV_STATE_DISABLED);
 strip_position(s.strip,0);render_results();lv_obj_add_flag(s.root,LV_OBJ_FLAG_HIDDEN);lv_timer_create(poll,40,NULL);
}
void ui_nav_search_panel_open(ui_nav_point_t center,bool valid)
{
 bool visible=!lv_obj_has_flag(s.root,LV_OBJ_FLAG_HIDDEN);
 int header_y=visible?lv_obj_get_style_y(s.header,0):564,input_y=visible?lv_obj_get_style_y(s.input,0):776;
 s.center=center;s.center_valid=valid;s.active=true;s.zone=FOCUS_LETTERS;
 lv_obj_clear_flag(s.root,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(s.root);
 ui_motion_cancel(s.list,move_y);ui_motion_cancel(s.list,candidates_geometry);
 if(!visible){s.candidates_open=false;candidates_geometry(NULL,0);}else candidates_geometry(NULL,s.candidates_progress);
 ui_motion_cancel(s.header,move_y);lv_obj_set_y(s.header,header_y);
 ui_motion_value(s.header,move_y,header_y,0,300,lv_anim_path_ease_in_out,NULL,NULL);
 if(!visible){lv_obj_set_width(s.header_bg,432);lv_obj_set_height(s.header_bg,48);}
 ui_motion_spring(s.header_bg,width,lv_obj_get_style_width(s.header_bg,0),464,true,NULL,NULL);
 ui_motion_spring(s.header_bg,height,lv_obj_get_style_height(s.header_bg,0),56,true,NULL,NULL);
 strip_stop();strip_select(s.selected_letter,false);
 ui_motion_cancel(s.input,move_y);lv_obj_set_y(s.input,input_y);
 ui_motion_value(s.input,move_y,input_y,UI_NAV_SEARCH_INPUT_TOP-62,260,lv_anim_path_ease_in_out,NULL,NULL);
 ui_navigation_search_layout(true);query_changed();
}
static void close_done(lv_anim_t *animation){(void)animation;if(!s.active)lv_obj_add_flag(s.root,LV_OBJ_FLAG_HIDDEN);}
void ui_nav_search_panel_close(bool animated)
{
 if(!s.root)return;s.active=false;ui_nav_search_cancel();strip_stop();ui_motion_cancel(s.capsule,capsule_shape);capsule_shape(s.capsule,0);
 ui_motion_cancel(s.header,move_y);ui_motion_cancel(s.input,move_y);ui_motion_cancel(s.list,move_y);ui_motion_cancel(s.list,candidates_geometry);
 if(animated&&!ui_motion_is_reduced()){
  ui_motion_move_y(s.header,564,320,lv_anim_path_ease_in_out,close_done,NULL);
  ui_motion_move_y(s.input,776,260,lv_anim_path_ease_in_out,NULL,NULL);ui_motion_move_y(s.list,776,260,lv_anim_path_ease_in_out,NULL,NULL);
 }else lv_obj_add_flag(s.root,LV_OBJ_FLAG_HIDDEN);
}
bool ui_nav_search_panel_active(void){return s.active;}
bool ui_nav_search_panel_back(void)
{if(!s.active)return false;if(s.zone==FOCUS_RESULTS)set_zone(FOCUS_LETTERS);else ui_navigation_close_search();return true;}
bool ui_nav_search_panel_input(ui_input_event_t input)
{
 if(!s.active)return false;
 if(input==UI_INPUT_BACK)return ui_nav_search_panel_back();
 if(input==UI_INPUT_PAGE_PREV){action(K_DELETE);return true;}
 if(input==UI_INPUT_PAGE_NEXT||input==UI_INPUT_RIDE_TOGGLE||input==UI_INPUT_TOGGLE_CONTROL_CENTER){action(K_RESULTS);return true;}
 if(input==UI_INPUT_ENCODER_CW||input==UI_INPUT_ENCODER_CCW){
  int delta=input==UI_INPUT_ENCODER_CW?1:-1;
  if(s.zone==FOCUS_LETTERS){strip_stop();strip_select(s.selected_letter+delta,true);}
  else if(results_ready()){
   int selected=LV_CLAMP(0,(int)s.selected_result+delta,(int)s.results.count-1);
   s.selected_result=selected;s.selected_place_id=s.results.items[selected].id;focus_update(true);
  }return true;
 }
 if(input==UI_INPUT_ENCODER_PRESS){
  if(s.zone==FOCUS_LETTERS){strip_stop();commit_letter(s.selected_letter);}
  else action(K_RESULT+s.selected_result);return true;
 }return false;
}
const char *ui_nav_search_panel_query(void){return s.query;}
