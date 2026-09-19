#ifndef UI_NAV_COMMAND_DOCK_H
#define UI_NAV_COMMAND_DOCK_H

#include "lvgl.h"
#include "../../../services/ui_navigation_service.h"

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *accent;
    lv_obj_t *icon;
    lv_obj_t *divider;
    lv_obj_t *info;
    lv_obj_t *prefix;
    lv_obj_t *distance_row;
    lv_obj_t *distance_value;
    lv_obj_t *distance_unit;
    lv_obj_t *headline;
    lv_obj_t *action;
    lv_obj_t *special_detail;
    lv_obj_t *road;
    nav_maneuver_t maneuver;
    nav_ui_state_t state;
    nav_ui_state_t previous_state;
    int32_t icon_scale;
} ui_nav_command_dock_t;

void ui_nav_command_dock_create(ui_nav_command_dock_t *dock,
                                lv_obj_t *parent);
void ui_nav_command_dock_update(ui_nav_command_dock_t *dock,
                                const nav_model_t *model,
                                bool animate_state);

#endif /* UI_NAV_COMMAND_DOCK_H */
