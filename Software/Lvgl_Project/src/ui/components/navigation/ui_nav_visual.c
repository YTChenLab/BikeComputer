#include "ui_nav_visual.h"
#include "../../core/ui_theme.h"
#include "../../core/ui_text.h"
#include "../../core/ui_motion.h"
#include "lvgl/src/draw/lv_draw_line.h"
#include "lvgl/src/draw/lv_draw_triangle.h"
#include "lvgl/src/draw/lv_draw_arc.h"
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

lv_obj_t *ui_nav_surface(lv_obj_t *parent,int x,int y,int w,int h,bool card)
{
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    if(card) {
        /* Neutral RGB565 gray avoids a green tint in intermediate gradients. */
        lv_obj_set_style_bg_color(o,lv_color_hex(0x181818),0);
        lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
        lv_obj_set_style_border_color(o,lv_color_hex(0x4B4B4B),0);
        lv_obj_set_style_border_width(o,1,0); lv_obj_set_style_radius(o,14,0);
    }
    return o;
}
lv_obj_t *ui_nav_label(lv_obj_t *parent,int x,int y,int w,const char *text,int size,bool muted)
{
    lv_obj_t *o=lv_label_create(parent); ui_text_set_label(o,text,size);
    lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w); lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(o,lv_color_hex(muted?0xAAAAAA:0xFFFFFF),0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE); return o;
}
static void large_button_offset(void *object,int32_t value)
{lv_obj_set_style_translate_y(object,value,0);}
static void button_press(lv_event_t *e)
{
    lv_obj_t *o=lv_event_get_target(e);
    lv_event_code_t code=lv_event_get_code(e);
    bool pressed=code==LV_EVENT_PRESSED;
    if(!pressed&&code!=LV_EVENT_RELEASED&&code!=LV_EVENT_PRESS_LOST)return;
    /* Large route cards cannot afford an extra full-card ARGB scale layer. */
    if((int64_t)lv_obj_get_width(o)*lv_obj_get_height(o)>=60000){
        ui_motion_value(o,large_button_offset,lv_obj_get_style_translate_y(o,0),pressed?2:0,
                        100,lv_anim_path_ease_out,NULL,NULL);
    }
    else if(pressed)ui_motion_button_press(o);
    else ui_motion_button_release(o);
}
lv_obj_t *ui_nav_button(lv_obj_t *parent,int x,int y,int w,int h,const char *text,bool primary,bool danger)
{
    lv_obj_t *o=ui_nav_surface(parent,x,y,w,h,true);
    lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);
    if((int64_t)w*h>=60000)
        lv_obj_set_style_bg_color(o,lv_color_hex(0x242424),LV_STATE_PRESSED);
    if(primary||danger) {
        lv_obj_add_flag(o,primary?LV_OBJ_FLAG_USER_1:LV_OBJ_FLAG_USER_2);
        lv_obj_set_style_bg_color(o,lv_color_hex(danger?0xFF3B30:0xFFD900),0);
        lv_obj_set_style_bg_grad_color(o,lv_color_hex(danger?0xE32924:0xFFD900),0);
        lv_obj_set_style_border_color(o,lv_color_hex(danger?0xFF3B30:0xFFD900),0);
    }
    lv_obj_t *t=ui_nav_label(o,0,0,w-24,text,24,false);
    lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);
    if(primary) lv_obj_set_style_text_color(t,lv_color_black(),0);
    lv_obj_align(t,LV_ALIGN_CENTER,0,0);
    lv_obj_add_event_cb(o,button_press,LV_EVENT_ALL,NULL); return o;
}
void ui_nav_button_focus(lv_obj_t *o,bool selected)
{
    bool primary=lv_obj_has_flag(o,LV_OBJ_FLAG_USER_1),danger=lv_obj_has_flag(o,LV_OBJ_FLAG_USER_2);
    lv_obj_set_style_border_color(o,lv_color_hex(selected?(primary||danger?0xFFFFFF:0xFFD900):
        primary?0xFFD900:danger?0xFF3B30:0x4B4B4B),0);
    lv_obj_set_style_border_width(o,selected?2:1,0);
}
static void stroke(lv_layer_t *layer,lv_point_precise_t a,lv_point_precise_t b,int w,lv_color_t color)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d); d.p1=a; d.p2=b; d.color=color;
    d.width=w; d.round_start=d.round_end=1; lv_draw_line(layer,&d);
}
static void triangle(lv_layer_t *layer,lv_point_precise_t a,lv_point_precise_t b,lv_point_precise_t c,lv_color_t color)
{
    lv_draw_triangle_dsc_t d; lv_draw_triangle_dsc_init(&d); d.p[0]=a;d.p[1]=b;d.p[2]=c;
    d.bg_color=color;d.bg_opa=LV_OPA_COVER;lv_draw_triangle(layer,&d);
}
static void icon_draw(lv_event_t *e)
{
    lv_area_t a;lv_obj_get_coords(lv_event_get_target(e),&a);lv_layer_t *l=lv_event_get_layer(e);
    int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2,r=lv_area_get_width(&a)/2-3;
    nav_icon_t kind=(nav_icon_t)(uintptr_t)lv_event_get_user_data(e);
    lv_color_t c=lv_obj_get_style_text_color(lv_event_get_target(e),0);int w=LV_MAX(2,r/6);
    if(kind==NAV_ICON_SEARCH){lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.center=(lv_point_t){x-r/4,y-r/4};d.radius=r*2/3;d.width=w;d.start_angle=0;d.end_angle=360;d.opa=LV_OPA_COVER;d.color=c;lv_draw_arc(l,&d);stroke(l,(lv_point_precise_t){x+r/4,y+r/4},(lv_point_precise_t){x+r,y+r},w,c);return;}
#define P(dx,dy) ((lv_point_precise_t){x+(dx)*r/20,y+(dy)*r/20})
#define L(ax,ay,bx,by) stroke(l,P(ax,ay),P(bx,by),w,c)
    if(kind==NAV_ICON_ARROW) {
        triangle(l,P(-17,-3),P(17,-18),P(4,18),c);
        triangle(l,P(-5,4),P(1,-1),P(4,18),c);
    } else if(kind==NAV_ICON_STAR) {
        for(int i=0;i<5;i++) {
            double p=(i*72-90)*0.01745329252,q=(i*72-54)*0.01745329252,t=(i*72-18)*0.01745329252;
            triangle(l,(lv_point_precise_t){x,y},(lv_point_precise_t){x+r*cos(p),y+r*sin(p)},
                (lv_point_precise_t){x+r*.46*cos(q),y+r*.46*sin(q)},c);
            triangle(l,(lv_point_precise_t){x,y},(lv_point_precise_t){x+r*.46*cos(q),y+r*.46*sin(q)},
                (lv_point_precise_t){x+r*cos(t),y+r*sin(t)},c);
        }
    } else if(kind==NAV_ICON_CLOCK||kind==NAV_ICON_PIN) {
        lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.center=(lv_point_t){x,y-(kind==NAV_ICON_PIN?r/3:0)};
        d.radius=kind==NAV_ICON_PIN?r*2/3:r;d.start_angle=0;d.end_angle=360;d.width=w;d.color=c;d.opa=LV_OPA_COVER;
        lv_draw_arc(l,&d);
        if(kind==NAV_ICON_CLOCK){L(0,0,0,-12);L(0,0,10,6);}
        else {L(-12,4,0,20);L(12,4,0,20);}
    } else if(kind==NAV_ICON_CHECK) { L(-16,0,-4,12);L(-4,12,17,-13); }
    else if(kind==NAV_ICON_WARNING) { triangle(l,P(0,-20),P(-20,18),P(20,18),c);c=lv_color_black();L(0,-7,0,5);L(0,11,0,12); }
    else if(kind==NAV_ICON_STOP){
        const int xx[]={-8,8,18,18,8,-8,-18,-18},yy[]={-18,-18,-8,8,18,18,8,-8};
        for(unsigned i=0;i<8;i++){unsigned j=(i+1)%8;stroke(l,P(xx[i],yy[i]),P(xx[j],yy[j]),w,c);}L(-7,-7,7,7);L(-7,7,7,-7);
    }
    else if(kind==NAV_ICON_MAP) {L(-18,-12,-18,15);L(-18,-12,-5,-18);L(-18,15,-5,9);L(-5,-18,-5,9);L(-5,-18,8,-10);L(-5,9,8,17);L(8,-10,8,17);L(8,-10,18,-17);L(8,17,18,10);L(18,-17,18,10);}
    else if(kind==NAV_ICON_PHONE) {L(-11,-18,11,-18);L(11,-18,11,18);L(11,18,-11,18);L(-11,18,-11,-18);L(-3,11,3,11);}
    else if(kind==NAV_ICON_UPLOAD) {L(-13,-17,6,-17);L(6,-17,15,-8);L(15,-8,15,18);L(15,18,-13,18);L(-13,18,-13,-17);L(0,12,0,-2);L(-7,4,0,-3);L(0,-3,7,4);}
    else {L(-16,-15,16,-15);L(-16,-3,16,-3);L(-16,9,16,9);L(-16,19,16,19);L(-16,-15,-16,19);L(16,-15,16,19);}
#undef L
#undef P
}
lv_obj_t *ui_nav_icon(lv_obj_t *parent,int x,int y,int size,nav_icon_t kind,uint32_t color)
{
    lv_obj_t *o=ui_nav_surface(parent,x,y,size,size,false);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_add_event_cb(o,icon_draw,LV_EVENT_DRAW_MAIN,(void *)(uintptr_t)kind);return o;
}
void ui_nav_icon_set_kind(lv_obj_t *icon,nav_icon_t kind)
{if(icon){lv_obj_remove_event_cb(icon,icon_draw);lv_obj_add_event_cb(icon,icon_draw,LV_EVENT_DRAW_MAIN,(void *)(uintptr_t)kind);lv_obj_invalidate(icon);}}
void ui_nav_center_pair(lv_obj_t *icon,lv_obj_t *text,int x,int y,int width,int height,int gap)
{
    const lv_font_t *font=lv_obj_get_style_text_font(text,0);lv_point_t extent;
    lv_text_get_size(&extent,lv_label_get_text(text),font,lv_obj_get_style_text_letter_space(text,0),
        0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
    int iw=lv_obj_get_style_width(icon,0),ih=lv_obj_get_style_height(icon,0);
    int tw=LV_CLAMP(1,extent.x,width-iw-gap),left=x+(width-iw-gap-tw)/2;
    lv_obj_set_align(icon,LV_ALIGN_TOP_LEFT);lv_obj_set_align(text,LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(icon,left,y+(height-ih)/2);
    lv_label_set_long_mode(text,LV_LABEL_LONG_DOT);lv_obj_set_size(text,tw,font->line_height);
    lv_obj_set_style_text_align(text,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_pos(text,left+iw+gap,y+(height-font->line_height)/2);
}
lv_obj_t *ui_nav_icon_label(lv_obj_t *parent,int x,int y,int width,int height,const char *text,int size,
    bool muted,nav_icon_t kind,int icon_size,uint32_t color)
{
    lv_obj_t *icon=ui_nav_icon(parent,0,0,icon_size,kind,color);
    lv_obj_t *name=ui_nav_label(parent,0,0,width,text,size,muted);
    ui_nav_center_pair(icon,name,x,y,width,height,12);return name;
}
void ui_nav_metric_triplet(lv_obj_t *p,int y,int width,const char *a,const char *b,const char *c,const char *ta,const char *tb,const char *tc)
{
    const char *values[]={a,b,c},*titles[]={ta,tb,tc};
    for(unsigned i=0;i<3;i++) {
        int x=i*width/3,col=(i+1)*width/3-x;
        char number[40];snprintf(number,sizeof(number),"%s",values[i]);char *unit=strchr(number,' ');if(unit)*unit++=0;
        lv_obj_t *row=ui_nav_surface(p,x,y,col,44,false);lv_obj_set_layout(row,LV_LAYOUT_FLEX);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_CENTER);lv_obj_set_style_pad_column(row,4,0);
        lv_obj_t *v=lv_label_create(row);lv_label_set_text(v,number);lv_obj_set_style_text_font(v,UI_FONT_NAV_STRIP_VALUE,0);lv_obj_set_style_text_color(v,lv_color_white(),0);
        if(unit&&*unit){lv_obj_t *u=ui_nav_label(row,0,0,LV_SIZE_CONTENT,unit,20,true);lv_obj_set_style_text_font(u,UI_FONT_PAGE_TEXT,0);}
        lv_obj_t *t=ui_nav_label(p,x,y+44,col,titles[i],20,true);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);
        if(i) {lv_obj_t *d=ui_nav_surface(p,x,y+3,1,61,false);lv_obj_set_style_bg_color(d,lv_color_hex(0x414141),0);lv_obj_set_style_bg_opa(d,LV_OPA_COVER,0);}
    }
}
