#ifndef UI_BOOT_MOTION_H
#define UI_BOOT_MOTION_H

#include "lvgl.h"
#include <stdbool.h>

#define UI_BOOT_MOTION_DURATION_MS 5000U

/* LVGL 9.3; 480 x 854; all calls must run on the LVGL thread.
 * The rail shows presentation time, not hardware initialization progress.
 * Returned object owns its timer and context; lv_obj_delete releases both.
 */
lv_obj_t *ui_boot_motion_create(lv_obj_t *parent);
void ui_boot_motion_set_active(lv_obj_t *view, bool active, bool reduced_motion);
bool ui_boot_motion_is_complete(lv_obj_t *view);

#endif
