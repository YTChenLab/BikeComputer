#include "ui_liquid_selection.h"
#include "../core/ui_motion.h"
#include "lvgl/src/draw/lv_draw_rect.h"
#include <string.h>
#include <stdlib.h>
static bool intersect(lv_area_t *out,const lv_area_t *a,const lv_area_t *b)
{*out=(lv_area_t){LV_MAX(a->x1,b->x1),LV_MAX(a->y1,b->y1),LV_MIN(a->x2,b->x2),LV_MIN(a->y2,b->y2)};return out->x1<=out->x2&&out->y1<=out->y2;}

/* Draw into each existing card, clipped to that card. No scaled framebuffers,
 * floating overlay hit targets or yellow trails across read-only content. */
static void fill_draw(lv_event_t *e)
{
    ui_liquid_item_t *item=lv_event_get_user_data(e);ui_liquid_selection_t *s=item->owner;
    if(!s->active || !s->rect_valid)return;
    lv_area_t viewport;lv_obj_get_coords(s->viewport,&viewport);lv_area_t rect=s->rect;
    lv_area_move(&rect,viewport.x1-lv_obj_get_scroll_x(s->viewport),viewport.y1-lv_obj_get_scroll_y(s->viewport));
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=lv_color_hex(0xFFD900);
    d.bg_opa=LV_OPA_COVER;d.radius=LV_MIN(24,lv_area_get_height(&rect)/2);
    lv_layer_t *layer=lv_event_get_layer(e);lv_area_t saved=layer->_clip_area,bounds,clip;
    lv_obj_get_coords(item->object,&bounds);
    if(intersect(&clip,&saved,&bounds)){layer->_clip_area=clip;lv_draw_rect(layer,&d,&rect);layer->_clip_area=saved;}
}
static void collect_labels(ui_liquid_item_t *item,lv_obj_t *object)
{
    if(lv_obj_check_type(object,&lv_label_class) && item->label_count<UI_LIQUID_LABELS) {
        unsigned i=item->label_count++;item->labels[i]=object;item->colors[i]=lv_obj_get_style_text_color(object,0);
    }
    for(unsigned i=0;i<lv_obj_get_child_count(object);i++)collect_labels(item,lv_obj_get_child(object,i));
}
static lv_area_t item_rect(ui_liquid_selection_t *s,ui_liquid_item_t *item)
{
    lv_area_t a,v;lv_obj_update_layout(item->object);lv_obj_get_coords(item->object,&a);lv_obj_get_coords(s->viewport,&v);
    lv_area_move(&a,-v.x1+lv_obj_get_scroll_x(s->viewport),-v.y1+lv_obj_get_scroll_y(s->viewport));return a;
}
static bool label_inset(const ui_liquid_item_t *item,lv_obj_t *label)
{
    for(lv_obj_t *p=lv_obj_get_parent(label);p&&p!=item->object;p=lv_obj_get_parent(p))
        if(lv_obj_get_style_bg_opa(p,0)==LV_OPA_COVER)return true;
    return false;
}
static void ink(ui_liquid_selection_t *s)
{
    for(unsigned i=0;i<s->count;i++) {
        ui_liquid_item_t *item=&s->items[i];lv_area_t a=item_rect(s,item),overlap;
        bool dark=s->active && s->rect_valid && intersect(&overlap,&a,&s->rect) &&
            (int64_t)lv_area_get_width(&overlap)*lv_area_get_height(&overlap)>
            (int64_t)lv_area_get_width(&a)*lv_area_get_height(&a)*3/5;
        if(dark!=item->ink_dark) {
            item->ink_dark=dark;
            for(unsigned j=0;j<item->label_count;j++) {
                lv_obj_set_style_text_color(item->labels[j],dark&&!label_inset(item,item->labels[j])?lv_color_hex(0x101010):item->colors[j],0);
            }
        }
        lv_obj_invalidate(item->object);
    }
}
static int interpolate(int a,int b,int progress){return a+(b-a)*progress/1000;}
static void layout(ui_liquid_selection_t *s)
{
    if(!s->arc)return;
    int scroll=lv_obj_get_scroll_y(s->viewport),half=LV_MAX(1,lv_obj_get_height(s->viewport)/2);
    int extent=0;
    for(unsigned i=0;i<s->count;i++) {
        ui_liquid_item_t *item=&s->items[i];
        /* Some shared lists move Cancel to a fixed page footer. It retains
         * focus and liquid feedback but must never enter the list's geometry. */
        if(lv_obj_get_parent(item->object)!=s->viewport)continue;
        extent=LV_MAX(extent,item->y+item->height+6);
        int distance=abs(item->y+item->height/2-scroll-half);
        int bend=LV_MIN(1000,distance*1000/half);
        int target=s->active&&i==s->selected?1020:965;
        item->scale=interpolate(item->from_scale,target,LV_CLAMP(0,s->progress,1000));
        int w=item->width*item->scale/1000,h=item->height*item->scale/1000;
        if(s->wide_focus){
            int target_width=s->active&&i==s->selected?lv_obj_get_width(s->viewport)-12:item->width*965/1000;
            w=interpolate(item->from_width,target_width,LV_CLAMP(0,s->progress,1000));
        }
        item->draw_width=w;
        int x=item->x+(item->width-w)/2+18*bend*bend/1000000;
        x=s->wide_focus?LV_CLAMP(6,x,lv_obj_get_width(s->viewport)-w-6):LV_MIN(x,lv_obj_get_width(s->viewport)-w-6);
        lv_obj_set_size(item->object,w,h);lv_obj_set_pos(item->object,x,item->y+(item->height-h)/2);
        if(s->layout)s->layout(item->object,w,h);
    }
    if(s->extent)lv_obj_set_pos(s->extent,0,extent);
}
static void step(void *object,int32_t value)
{
    ui_liquid_selection_t *s=lv_obj_get_user_data(object); /* Dedicated viewport. */
    if(!s || !s->count)return;s->progress=value;layout(s);
    lv_area_t to=item_rect(s,&s->items[s->selected]);
    int p=LV_CLAMP(0,value,1040),head=LV_CLAMP(0,p+(1000-LV_MIN(p,1000))*p/4500,1040);
    int tail=LV_CLAMP(0,p-(1000-LV_MIN(p,1000))*p/4500,1040);
    bool right=to.x1+to.x2>s->from.x1+s->from.x2,down=to.y1+to.y2>s->from.y1+s->from.y2;
    s->rect=(lv_area_t){interpolate(s->from.x1,to.x1,right?tail:head),interpolate(s->from.y1,to.y1,down?tail:head),
        interpolate(s->from.x2,to.x2,right?head:tail),interpolate(s->from.y2,to.y2,down?head:tail)};
    ink(s);
}
static void scroll_event(lv_event_t *e)
{
    ui_liquid_selection_t *s=lv_event_get_user_data(e);layout(s);
    if(s->progress>=1000 && s->count)s->rect=item_rect(s,&s->items[s->selected]);
    ink(s);
}
void ui_liquid_selection_init(ui_liquid_selection_t *s,lv_obj_t *viewport,bool arc,ui_liquid_layout_cb_t cb)
{
    memset(s,0,sizeof(*s));s->viewport=viewport;s->arc=arc;s->layout=cb;s->initialized=true;
    s->progress=1000;lv_obj_set_user_data(viewport,s);
    if(arc){
        /* Stable content extent: row enlargement cannot move the scroll limit. */
        s->extent=lv_obj_create(viewport);lv_obj_remove_style_all(s->extent);
        lv_obj_set_size(s->extent,1,1);lv_obj_clear_flag(s->extent,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(viewport,scroll_event,LV_EVENT_SCROLL,s);
    }
}
void ui_liquid_selection_add(ui_liquid_selection_t *s,lv_obj_t *object)
{
    if(s->count>=UI_LIQUID_ITEMS)return;ui_liquid_item_t *item=&s->items[s->count++];
    item->owner=s;item->object=object;lv_obj_update_layout(object);
    item->x=lv_obj_get_style_x(object,0);item->y=lv_obj_get_style_y(object,0);
    item->width=lv_obj_get_width(object);item->height=lv_obj_get_height(object);item->scale=item->from_scale=1000;
    item->draw_width=item->from_width=item->width;
    if(s->extent)lv_obj_set_pos(s->extent,0,item->y+item->height+6);
    collect_labels(item,object);lv_obj_add_event_cb(object,fill_draw,LV_EVENT_DRAW_MAIN,item);
    lv_obj_set_style_shadow_width(object,0,0);
}
void ui_liquid_selection_select(ui_liquid_selection_t *s,unsigned index,bool animate)
{
    if(!s->initialized || index>=s->count)return;
    for(unsigned i=0;i<s->count;i++){
        s->items[i].from_scale=s->items[i].scale;
        s->items[i].from_width=s->items[i].draw_width;
    }
    s->selected=index;s->from=s->rect_valid?s->rect:item_rect(s,&s->items[index]);s->rect_valid=true;
    if(s->arc && lv_obj_get_parent(s->items[index].object)==s->viewport) {
        int target=s->items[index].y+s->items[index].height/2-lv_obj_get_height(s->viewport)/2;
        lv_obj_update_layout(s->viewport);
        target=LV_CLAMP(0,target,LV_MAX(0,lv_obj_get_scroll_y(s->viewport)+lv_obj_get_scroll_bottom(s->viewport)));
        lv_obj_scroll_to_y(s->viewport,target,animate?ui_motion_scroll_mode():LV_ANIM_OFF);
    }
    if(animate && !ui_motion_is_reduced()){ui_motion_cancel(s->viewport,step);ui_motion_spring(s->viewport,step,0,1000,true,NULL,NULL);}
    else {ui_motion_cancel(s->viewport,step);step(s->viewport,1000);}
}
void ui_liquid_selection_set_wide_focus(ui_liquid_selection_t *s,bool wide)
{
    if(!s||!s->initialized||!s->arc)return;
    s->wide_focus=wide;
    if(s->count)ui_liquid_selection_select(s,s->selected,false);
}
void ui_liquid_selection_set_active(ui_liquid_selection_t *s,bool active)
{
    if(!s->initialized)return;s->active=active;
    if(s->count)ui_liquid_selection_select(s,s->selected,false);
}
void ui_liquid_selection_refresh(ui_liquid_selection_t *s)
{if(s->initialized&&s->count){layout(s);if(s->progress>=1000)s->rect=item_rect(s,&s->items[s->selected]);ink(s);}}
void ui_liquid_selection_color(ui_liquid_selection_t *s,lv_obj_t *label,lv_color_t color)
{
    for(unsigned i=0;i<s->count;i++)for(unsigned j=0;j<s->items[i].label_count;j++)if(s->items[i].labels[j]==label){
        s->items[i].colors[j]=color;lv_obj_set_style_text_color(label,s->items[i].ink_dark&&!label_inset(&s->items[i],label)?lv_color_hex(0x101010):color,0);return;
    }
    lv_obj_set_style_text_color(label,color,0);
}
