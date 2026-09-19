#ifndef UI_NAV_DATA_STRIP_H
#define UI_NAV_DATA_STRIP_H

#include "lvgl.h"
#include "../../../services/ui_navigation_service.h"

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *groups[3];
    lv_obj_t *value_rows[3];
    lv_obj_t *values[3];
    lv_obj_t *units[3];
    lv_obj_t *titles[3];
    lv_obj_t *dividers[2];
} ui_nav_data_strip_t;

void ui_nav_data_strip_create(ui_nav_data_strip_t *strip, lv_obj_t *parent);
void ui_nav_data_strip_update(ui_nav_data_strip_t *strip,
                              const nav_model_t *model);

#endif /* UI_NAV_DATA_STRIP_H */
