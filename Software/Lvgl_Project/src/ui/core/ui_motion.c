#include "ui_motion.h"

static void set_scale(void *target, int32_t value)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)target, value, 0);
}

static void set_y(void *target, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)target, value);
}

static void set_opacity(void *target, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)target, (lv_opa_t)value, 0);
}

void ui_motion_init(void)
{
    /* Reserved for reduced-motion and platform timing policy. */
}

void ui_motion_value(void *target, ui_motion_value_cb_t callback,
                     int32_t from, int32_t to, uint32_t duration,
                     lv_anim_path_cb_t path,
                     lv_anim_completed_cb_t completed, void *user_data)
{
    if(target == NULL || callback == NULL) return;
    lv_anim_delete(target, callback);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, target);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_time(&animation, duration);
    lv_anim_set_path_cb(&animation, path != NULL ? path : lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, callback);
    lv_anim_set_user_data(&animation, user_data);
    if(completed != NULL) lv_anim_set_completed_cb(&animation, completed);
    lv_anim_start(&animation);
}

void ui_motion_cancel(void *target, ui_motion_value_cb_t callback)
{
    if(target != NULL && callback != NULL) lv_anim_delete(target, callback);
}

void ui_motion_button_press(lv_obj_t *button)
{
    if(button == NULL) return;
    ui_motion_value(button, set_scale,
                    lv_obj_get_style_transform_scale_x(button, 0),
                    UI_MOTION_PRESS_SCALE_ACTIVE, UI_MOTION_DURATION_PRESS,
                    lv_anim_path_ease_out, NULL, NULL);
}

void ui_motion_button_release(lv_obj_t *button)
{
    if(button == NULL) return;
    ui_motion_value(button, set_scale,
                    lv_obj_get_style_transform_scale_x(button, 0),
                    UI_MOTION_PRESS_SCALE_NORMAL, UI_MOTION_DURATION_PRESS,
                    lv_anim_path_ease_out, NULL, NULL);
}

void ui_motion_move_y(lv_obj_t *object, int32_t target_y, uint32_t duration,
                      lv_anim_path_cb_t path,
                      lv_anim_completed_cb_t completed, void *user_data)
{
    if(object == NULL) return;
    ui_motion_value(object, set_y, lv_obj_get_y(object), target_y, duration,
                    path, completed, user_data);
}

void ui_motion_cancel_y(lv_obj_t *object)
{
    ui_motion_cancel(object, set_y);
}

void ui_motion_translate_fade(lv_obj_t *object,
                              int32_t from_y, int32_t to_y,
                              lv_opa_t from_opa, lv_opa_t to_opa,
                              uint32_t duration,
                              lv_anim_completed_cb_t completed,
                              void *user_data)
{
    if(object == NULL) return;
    ui_motion_cancel(object, set_y);
    ui_motion_cancel(object, set_opacity);
    lv_obj_set_y(object, from_y);
    lv_obj_set_style_opa(object, from_opa, 0);
    ui_motion_value(object, set_y, from_y, to_y, duration,
                    lv_anim_path_ease_in_out, NULL, NULL);
    ui_motion_value(object, set_opacity, from_opa, to_opa, duration,
                    lv_anim_path_ease_in_out, completed, user_data);
}

void ui_motion_dock_enter(lv_obj_t *object, int32_t resting_y)
{
    if(object == NULL) return;
    ui_motion_cancel(object, set_y);
    ui_motion_cancel(object, set_opacity);
    ui_motion_cancel(object, set_scale);
    lv_obj_set_y(object, resting_y + 50);
    lv_obj_set_style_opa(object, LV_OPA_TRANSP, 0);
    lv_obj_set_style_transform_scale(object, 248, 0);
    ui_motion_value(object, set_y, resting_y + 50, resting_y,
                    UI_MOTION_DURATION_DOCK, lv_anim_path_ease_out, NULL, NULL);
    ui_motion_value(object, set_opacity, LV_OPA_TRANSP, LV_OPA_COVER,
                    UI_MOTION_DURATION_DOCK, lv_anim_path_ease_out, NULL, NULL);
    ui_motion_value(object, set_scale, 248, UI_MOTION_PRESS_SCALE_NORMAL,
                    UI_MOTION_DURATION_DOCK, lv_anim_path_ease_out, NULL, NULL);
}

void ui_motion_dock_exit(lv_obj_t *object, int32_t resting_y,
                         lv_anim_completed_cb_t completed, void *user_data)
{
    if(object == NULL) return;
    ui_motion_cancel(object, set_y);
    ui_motion_cancel(object, set_opacity);
    ui_motion_cancel(object, set_scale);
    ui_motion_value(object, set_y, lv_obj_get_y(object), resting_y + 50,
                    UI_MOTION_DURATION_DOCK, lv_anim_path_ease_in, NULL, NULL);
    ui_motion_value(object, set_scale,
                    lv_obj_get_style_transform_scale_x(object, 0), 248,
                    UI_MOTION_DURATION_DOCK, lv_anim_path_ease_in, NULL, NULL);
    ui_motion_value(object, set_opacity, lv_obj_get_style_opa(object, 0),
                    LV_OPA_TRANSP, UI_MOTION_DURATION_DOCK,
                    lv_anim_path_ease_in, completed, user_data);
}

void ui_motion_page_load(lv_obj_t *screen, ui_motion_page_direction_t direction)
{
    if(screen == NULL) return;
    if(direction == UI_MOTION_PAGE_NONE) {
        lv_screen_load(screen);
    }
    else {
        lv_screen_load_anim(screen,
                            direction == UI_MOTION_PAGE_FORWARD ?
                            LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                            UI_MOTION_DURATION_PAGE, 0, false);
    }
    lv_obj_invalidate(screen);
}
