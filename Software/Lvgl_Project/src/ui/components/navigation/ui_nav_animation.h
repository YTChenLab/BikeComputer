#ifndef UI_NAV_ANIMATION_H
#define UI_NAV_ANIMATION_H

#include "lvgl.h"
#include "../../core/ui_motion.h"

#include <stdint.h>

#define UI_ANIM_FAST       UI_MOTION_DURATION_FAST
#define UI_ANIM_NORMAL     UI_MOTION_DURATION_NORMAL
#define UI_ANIM_PAGE       UI_MOTION_DURATION_PAGE
#define UI_ANIM_RECENTER   UI_MOTION_DURATION_RECENTER

typedef void (*ui_nav_anim_value_cb_t)(void *target, int32_t value);

void ui_nav_animation_value(void *target, ui_nav_anim_value_cb_t callback,
                            int32_t from, int32_t to, uint32_t duration,
                            lv_anim_path_cb_t path);
void ui_nav_animation_button_press(lv_obj_t *button);
void ui_nav_animation_button_release(lv_obj_t *button);
void ui_nav_animation_move_y(lv_obj_t *object, int32_t from_y, int32_t to_y,
                             uint32_t duration,
                             lv_anim_completed_cb_t completed,
                             void *user_data);
void ui_nav_animation_fade_move(lv_obj_t *object, int32_t from_y, int32_t to_y,
                                lv_opa_t from_opa, lv_opa_t to_opa,
                                uint32_t duration,
                                lv_anim_completed_cb_t completed,
                                void *user_data);

#endif /* UI_NAV_ANIMATION_H */
