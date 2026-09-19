#ifndef UI_MOTION_H
#define UI_MOTION_H

#include "lvgl.h"

#include <stdint.h>

#define UI_MOTION_PRESS_SCALE_NORMAL  256
#define UI_MOTION_PRESS_SCALE_ACTIVE  241

typedef enum
{
    UI_MOTION_DURATION_PRESS = 80,
    UI_MOTION_DURATION_FAST = 120,
    UI_MOTION_DURATION_NORMAL = 180,
    UI_MOTION_DURATION_PAGE = 220,
    UI_MOTION_DURATION_DOCK = 220,
    UI_MOTION_DURATION_RECENTER = 260
} ui_motion_duration_t;

typedef enum
{
    UI_MOTION_PAGE_NONE = 0,
    UI_MOTION_PAGE_FORWARD,
    UI_MOTION_PAGE_BACKWARD
} ui_motion_page_direction_t;

typedef void (*ui_motion_value_cb_t)(void *target, int32_t value);

void ui_motion_init(void);
void ui_motion_value(void *target, ui_motion_value_cb_t callback,
                     int32_t from, int32_t to, uint32_t duration,
                     lv_anim_path_cb_t path,
                     lv_anim_completed_cb_t completed, void *user_data);
void ui_motion_cancel(void *target, ui_motion_value_cb_t callback);

void ui_motion_button_press(lv_obj_t *button);
void ui_motion_button_release(lv_obj_t *button);
void ui_motion_focus_capsule(lv_obj_t *capsule, int32_t target_x,
                             int32_t resting_width);
void ui_motion_toggle_press(lv_obj_t *object);
void ui_motion_toggle_release(lv_obj_t *object);
void ui_motion_pulse(lv_obj_t *object, int32_t active_scale,
                     uint32_t duration);
void ui_motion_move_y(lv_obj_t *object, int32_t target_y, uint32_t duration,
                      lv_anim_path_cb_t path,
                      lv_anim_completed_cb_t completed, void *user_data);
void ui_motion_cancel_y(lv_obj_t *object);
void ui_motion_translate_fade(lv_obj_t *object,
                              int32_t from_y, int32_t to_y,
                              lv_opa_t from_opa, lv_opa_t to_opa,
                              uint32_t duration,
                              lv_anim_completed_cb_t completed,
                              void *user_data);
void ui_motion_translate_fade_delayed(lv_obj_t *object,
                                      int32_t from_y, int32_t to_y,
                                      lv_opa_t from_opa, lv_opa_t to_opa,
                                      uint32_t duration, uint32_t delay,
                                      lv_anim_completed_cb_t completed,
                                      void *user_data);
void ui_motion_dock_enter(lv_obj_t *object, int32_t resting_y);
void ui_motion_dock_exit(lv_obj_t *object, int32_t resting_y,
                         lv_anim_completed_cb_t completed, void *user_data);
void ui_motion_page_load(lv_obj_t *screen, ui_motion_page_direction_t direction);

#endif /* UI_MOTION_H */
