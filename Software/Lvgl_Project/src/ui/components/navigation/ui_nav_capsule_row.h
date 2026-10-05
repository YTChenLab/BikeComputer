#ifndef UI_NAV_CAPSULE_ROW_H
#define UI_NAV_CAPSULE_ROW_H

#include "lvgl.h"

/* Caller owns this state until the row is deleted. Labels remain stationary
 * while independently damped edges stretch the filled selection. Existing
 * input handlers call focus only after validating the action/filter. */
typedef struct {
    lv_obj_t *root, *capsule, *buttons[4];
    unsigned count, selected;
    bool active;
} ui_nav_capsule_row_t;

void ui_nav_capsule_row_create(ui_nav_capsule_row_t *row, lv_obj_t *parent,
                              int x, int y, int width, int height,
                              lv_obj_t **buttons, unsigned count);
bool ui_nav_capsule_row_contains(const ui_nav_capsule_row_t *row, lv_obj_t *button);
void ui_nav_capsule_row_focus(ui_nav_capsule_row_t *row, lv_obj_t *button,
                             bool active, bool animate);

#endif
