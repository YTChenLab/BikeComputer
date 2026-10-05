#ifndef UI_NAV_VISUAL_H
#define UI_NAV_VISUAL_H
#include "lvgl.h"
typedef enum { NAV_ICON_ARROW,NAV_ICON_STAR,NAV_ICON_CLOCK,NAV_ICON_PHONE,NAV_ICON_MAP,
    NAV_ICON_PIN,NAV_ICON_UPLOAD,NAV_ICON_STORAGE,NAV_ICON_CHECK,NAV_ICON_WARNING,NAV_ICON_STOP,NAV_ICON_SEARCH } nav_icon_t;
lv_obj_t *ui_nav_surface(lv_obj_t *parent,int x,int y,int width,int height,bool card);
lv_obj_t *ui_nav_label(lv_obj_t *parent,int x,int y,int width,const char *text,int size,bool muted);
lv_obj_t *ui_nav_button(lv_obj_t *parent,int x,int y,int width,int height,const char *text,bool primary,bool danger);
lv_obj_t *ui_nav_icon(lv_obj_t *parent,int x,int y,int size,nav_icon_t kind,uint32_t color);
void ui_nav_icon_set_kind(lv_obj_t *icon,nav_icon_t kind);
void ui_nav_center_pair(lv_obj_t *icon,lv_obj_t *text,int x,int y,int width,int height,int gap);
lv_obj_t *ui_nav_icon_label(lv_obj_t *parent,int x,int y,int width,int height,const char *text,int size,
    bool muted,nav_icon_t kind,int icon_size,uint32_t color);
void ui_nav_button_focus(lv_obj_t *button,bool selected);
void ui_nav_metric_triplet(lv_obj_t *parent,int y,int width,const char *a,const char *b,const char *c,
    const char *ta,const char *tb,const char *tc);
#endif
