#ifndef UI_LIQUID_SELECTION_H
#define UI_LIQUID_SELECTION_H
#include "lvgl.h"
#include <stdbool.h>
#define UI_LIQUID_ITEMS 16
#define UI_LIQUID_LABELS 12
typedef void (*ui_liquid_layout_cb_t)(lv_obj_t *item,int width,int height);
struct ui_liquid_selection;
typedef struct {
    struct ui_liquid_selection *owner;lv_obj_t *object;
    lv_obj_t *labels[UI_LIQUID_LABELS];lv_color_t colors[UI_LIQUID_LABELS];
    unsigned label_count;int x,y,width,height,scale,from_scale,draw_width,from_width;bool ink_dark;
} ui_liquid_item_t;
typedef struct ui_liquid_selection {
    lv_obj_t *viewport,*extent;ui_liquid_item_t items[UI_LIQUID_ITEMS];unsigned count,selected;
    ui_liquid_layout_cb_t layout;bool arc,active,initialized,wide_focus;int progress;
    lv_area_t rect,from;bool rect_valid;
} ui_liquid_selection_t;
void ui_liquid_selection_init(ui_liquid_selection_t *s,lv_obj_t *viewport,bool arc,ui_liquid_layout_cb_t layout);
void ui_liquid_selection_add(ui_liquid_selection_t *s,lv_obj_t *item);
/* Opt-in horizontal expansion; the existing vertical rhythm is independent. */
void ui_liquid_selection_set_wide_focus(ui_liquid_selection_t *s,bool wide);
void ui_liquid_selection_select(ui_liquid_selection_t *s,unsigned index,bool animate);
void ui_liquid_selection_set_active(ui_liquid_selection_t *s,bool active);
void ui_liquid_selection_refresh(ui_liquid_selection_t *s);
void ui_liquid_selection_color(ui_liquid_selection_t *s,lv_obj_t *label,lv_color_t color);
#endif
