#include "ui_nav_animation.h"
#include "../../core/ui_motion.h"

void ui_nav_animation_value(void *target, ui_nav_anim_value_cb_t callback,
                            int32_t from, int32_t to, uint32_t duration,
                            lv_anim_path_cb_t path)
{
    ui_motion_value(target, callback, from, to, duration, path, NULL, NULL);
}

void ui_nav_animation_button_press(lv_obj_t *button)
{
    ui_motion_button_press(button);
}

void ui_nav_animation_button_release(lv_obj_t *button)
{
    ui_motion_button_release(button);
}

void ui_nav_animation_fade_move(lv_obj_t *object, int32_t from_y, int32_t to_y,
                                lv_opa_t from_opa, lv_opa_t to_opa,
                                uint32_t duration,
                                lv_anim_completed_cb_t completed,
                                void *user_data)
{
    ui_motion_translate_fade(object, from_y, to_y, from_opa, to_opa,
                             duration, completed, user_data);
}
