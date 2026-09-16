#ifndef UI_NAV_CONTROL_H
#define UI_NAV_CONTROL_H

#include "lvgl.h"

#include <stdbool.h>

typedef void (*ui_nav_control_cb_t)(void *user_data);

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *icon;
    ui_nav_control_cb_t callback;
    void *user_data;
    bool active;
    bool focused;
} ui_nav_control_t;

void ui_nav_control_create(ui_nav_control_t *control, lv_obj_t *parent,
                           int32_t x, int32_t y, const char *symbol,
                           ui_nav_control_cb_t callback, void *user_data);
void ui_nav_control_set_symbol(ui_nav_control_t *control, const char *symbol);
void ui_nav_control_set_active(ui_nav_control_t *control, bool active);
void ui_nav_control_set_focused(ui_nav_control_t *control, bool focused);
void ui_nav_control_activate(ui_nav_control_t *control);

#endif /* UI_NAV_CONTROL_H */
