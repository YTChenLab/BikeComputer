#include "ui_zone_editor.h"
#include "services/ui_preferences.h"
#include "../core/ui_theme.h"
#include "../core/ui_motion.h"
#include "../core/ui_text.h"
#include "../design/ui_design_tokens.h"
#include "../design/ui_alignment.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdint.h>
static lv_obj_t *s_root, *s_values[8], *s_rows[10], *s_hint;
static ui_preferences_t s_draft;
static unsigned s_selected;
static bool s_editing;
static uint16_t *value(unsigned index) { return index<4 ? &s_draft.heart_zone_bounds[index] : &s_draft.power_zone_bounds[index-4]; }
bool ui_zone_editor_visible(void) { return s_root != NULL; }
void ui_zone_editor_close(void) { if(s_root) lv_obj_delete(s_root); s_root=NULL; s_editing=false; }
static void refresh(void)
{
    for(unsigned i=0;i<8;++i) lv_label_set_text_fmt(s_values[i],"%u %s",*value(i),i<4 ? "bpm" : "W");
    for(unsigned i=0;i<10;++i) {
        lv_obj_set_style_border_color(s_rows[i],i==s_selected ? UI_TOKEN_COLOR_ACCENT : UI_TOKEN_COLOR_SURFACE_OUTLINE,0);
        lv_obj_set_style_border_width(s_rows[i],i==s_selected ? 2 : 1,0);
    }
    ui_text_set_label(s_hint,s_editing ? "旋转调节 · 按压退出编辑" : "旋转选择 · 按压编辑 · 返回取消",20);
}
static void adjust(unsigned index, int direction)
{
    int next=(int)*value(index)+direction*(index<4 ? 1 : 5);
    unsigned maximum=index<4 ? 250 : 3000;
    int lower=index%4U == 0 ? 1 : (int)*value(index-1U)+1;
    int upper=index%4U == 3 ? (int)maximum : (int)*value(index+1U)-1;
    *value(index)=(uint16_t)LV_CLAMP(lower,next,upper); refresh();
}
static void activate(unsigned index)
{
    unsigned previous=s_selected;
    s_selected=index;
    if(index==8) {
        ui_preferences_t latest;
        if(!ui_preferences_get(&latest)) return;
        for(unsigned i=0;i<4;++i) { latest.heart_zone_bounds[i]=s_draft.heart_zone_bounds[i]; latest.power_zone_bounds[i]=s_draft.power_zone_bounds[i]; }
        if(ui_preferences_submit(&latest)) ui_zone_editor_close();
        else ui_text_set_label(s_hint,"区间边界必须由小到大排列",20);
    }
    else if(index==9) ui_zone_editor_close();
    else { s_editing=previous==index ? !s_editing : true; refresh(); }
}
static void pressed(lv_event_t *event)
{
    uintptr_t action=(uintptr_t)lv_event_get_user_data(event);
    if(action<10) activate((unsigned)action);
    else { unsigned index=(unsigned)(action-10)/2; s_selected=index;
        adjust(index,((action-10)&1) ? 1 : -1); }
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,const char *text)
{
    lv_obj_t *obj=lv_label_create(parent); lv_obj_set_pos(obj,x,y);
    ui_text_set_label(obj,text,20); lv_obj_set_style_text_color(obj,lv_color_white(),0); return obj;
}
static lv_obj_t *button(lv_obj_t *parent,int x,int y,int width,const char *text,uintptr_t action)
{
    lv_obj_t *obj=lv_button_create(parent); lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,width,action < 10 ? UI_PAGE_ACTION_HEIGHT : 48);
    lv_obj_set_style_radius(obj,12,0); lv_obj_set_style_bg_color(obj,UI_TOKEN_COLOR_SURFACE,0);
    lv_obj_set_style_border_color(obj,UI_TOKEN_COLOR_SURFACE_OUTLINE,0);
    lv_obj_set_style_border_width(obj,1,0);
    lv_obj_set_style_transform_scale(obj,248,LV_STATE_PRESSED);
    lv_obj_add_event_cb(obj,pressed,LV_EVENT_CLICKED,(void *)action);
    lv_obj_t *caption=label(obj,0,0,text); ui_align_label_center(caption); return obj;
}
void ui_zone_editor_open(void)
{
    if(s_root || !ui_preferences_get(&s_draft)) return;
    s_selected=0; s_editing=false;
    s_root=lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root,480,854); lv_obj_set_style_bg_color(s_root,lv_color_black(),0);
    lv_obj_set_style_bg_opa(s_root,LV_OPA_COVER,0); lv_obj_remove_flag(s_root,LV_OBJ_FLAG_SCROLLABLE);
    label(s_root,20,28,"个人骑行区间");
    label(s_root,20,68,"四个上限划分五个区间");
    label(s_root,20,99,"骑行中的修改在下一次骑行生效");
    for(unsigned i=0;i<8;++i) {
        int y=147+(int)i*56;
        s_rows[i]=lv_obj_create(s_root); lv_obj_remove_style_all(s_rows[i]);
        lv_obj_set_pos(s_rows[i],16,y); lv_obj_set_size(s_rows[i],220,48);
        lv_obj_set_style_radius(s_rows[i],12,0); lv_obj_add_flag(s_rows[i],LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(s_rows[i],UI_TOKEN_COLOR_SURFACE,0);
        lv_obj_set_style_bg_opa(s_rows[i],LV_OPA_COVER,0);
        lv_obj_add_event_cb(s_rows[i],pressed,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        char text[28]; snprintf(text,sizeof(text),"%s %u",i<4 ? "心率" : "功率",i%4+1);
        lv_obj_t *title=label(s_rows[i],12,0,text);
        ui_align_center_y(title,12);
        s_values[i]=label(s_rows[i],92,0,"");
        ui_align_center_y(s_values[i],92);
        lv_obj_set_width(s_values[i],112);
        lv_obj_set_style_text_align(s_values[i],LV_TEXT_ALIGN_RIGHT,0);
        button(s_root,248,y,96,"-",10+i*2); button(s_root,356,y,108,"+",11+i*2);
    }
    s_hint=label(s_root,20,610,"");
    s_rows[8]=button(s_root,16,UI_PAGE_ACTION_Y,216,"保存",8);
    lv_obj_set_style_bg_color(s_rows[8],UI_TOKEN_COLOR_ACCENT,0);
    lv_obj_set_style_text_color(lv_obj_get_child(s_rows[8],0),UI_TOKEN_COLOR_CANVAS,0);
    s_rows[9]=button(s_root,248,UI_PAGE_ACTION_Y,216,"取消",9);
    label(s_root,20,720,"返回会放弃本次修改");
    refresh(); ui_motion_translate_fade(s_root,20,0,LV_OPA_COVER,LV_OPA_COVER,180,NULL,NULL);
}
bool ui_zone_editor_handle_input(ui_input_event_t input)
{
    if(!s_root) return false;
    if(input==UI_INPUT_POWER_TOGGLE || input==UI_INPUT_OPEN_POWER ||
       input==UI_INPUT_RIDE_TOGGLE || input==UI_INPUT_TOGGLE_CONTROL_CENTER) {
        ui_zone_editor_close(); return false;
    }
    if(input==UI_INPUT_BACK || input==UI_INPUT_BACK_LONG) { ui_zone_editor_close(); return true; }
    if(input==UI_INPUT_ENCODER_PRESS) { activate(s_selected); return true; }
    if(input==UI_INPUT_ENCODER_CW || input==UI_INPUT_ENCODER_CCW) {
        int direction=input==UI_INPUT_ENCODER_CW ? 1 : -1;
        if(s_editing && s_selected<8) adjust(s_selected,direction);
        else { s_selected=(s_selected+10+direction)%10; refresh(); }
    }
    return true;
}
