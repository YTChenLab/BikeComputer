#ifndef UI_DOCK_TEMPLATES_H
#define UI_DOCK_TEMPLATES_H

#include "../../core/ui_dock.h"
#include "lvgl.h"

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *accent;
    lv_obj_t *icon_bg;
    lv_obj_t *icon;
    lv_obj_t *title;
    lv_obj_t *message;
    lv_obj_t *badge;
    lv_obj_t *close_button;
    lv_obj_t *primary_button;
    lv_obj_t *primary_label;
    lv_obj_t *secondary_button;
    lv_obj_t *secondary_label;
    lv_obj_t *decrement_button;
    lv_obj_t *toggle_button;
    lv_obj_t *increment_button;
    lv_obj_t *slider;
    lv_obj_t *value_label;
    ui_dock_type_t type;
} ui_dock_template_t;

void ui_dock_template_create(ui_dock_template_t *view, lv_obj_t *parent,
                             ui_dock_type_t type, lv_event_cb_t event_cb);
void ui_dock_template_update(ui_dock_template_t *view,
                             const ui_dock_card_t *card);

#endif /* UI_DOCK_TEMPLATES_H */
