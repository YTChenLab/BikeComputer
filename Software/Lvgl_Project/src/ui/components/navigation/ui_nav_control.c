#include "ui_nav_control.h"

#include "ui_nav_animation.h"
#include "../../core/ui_colors.h"
#include "../../design/ui_design_tokens.h"

#define NAV_CONTROL_SIZE 48
#define NAV_CONTROL_RADIUS 12

static void apply_visual(ui_nav_control_t *control)
{
    if(control == NULL || control->root == NULL || control->icon == NULL)
        return;
    bool emphasized = control->active || control->focused;
    lv_color_t color = emphasized ? UI_TOKEN_COLOR_ACCENT :
                                    UI_TOKEN_COLOR_TEXT_PRIMARY;
    lv_obj_set_style_border_color(control->root, color, 0);
    lv_obj_set_style_border_width(
        control->root,
        control->focused ? UI_TOKEN_BORDER_FOCUS : UI_TOKEN_BORDER_THIN, 0);
    lv_obj_set_style_bg_color(
        control->root,
        control->focused ? lv_color_hex(0x15170A) : UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_text_color(control->icon, color, 0);
}

static void control_event_cb(lv_event_t *event)
{
    ui_nav_control_t *control = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if(control == NULL || control->root == NULL) return;

    if(code == LV_EVENT_PRESSED) {
        ui_nav_control_set_focused(control, true);
        ui_nav_animation_button_press(control->root);
    } else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ui_nav_animation_button_release(control->root);
    } else if(code == LV_EVENT_CLICKED) {
        ui_nav_control_activate(control);
    }
}

void ui_nav_control_create(ui_nav_control_t *control, lv_obj_t *parent,
                           int32_t x, int32_t y, const char *symbol,
                           ui_nav_control_cb_t callback, void *user_data)
{
    if(control == NULL || parent == NULL) return;

    control->callback = callback;
    control->user_data = user_data;
    control->active = false;
    control->focused = false;
    control->root = lv_obj_create(parent);
    lv_obj_remove_style_all(control->root);
    lv_obj_set_pos(control->root, x, y);
    lv_obj_set_size(control->root, NAV_CONTROL_SIZE, NAV_CONTROL_SIZE);
    lv_obj_set_style_bg_color(control->root, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(control->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(control->root, UI_COLOR_WHITE, 0);
    lv_obj_set_style_border_opa(control->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(control->root, 1, 0);
    lv_obj_set_style_radius(control->root, NAV_CONTROL_RADIUS, 0);
    lv_obj_set_style_transform_pivot_x(control->root, NAV_CONTROL_SIZE / 2, 0);
    lv_obj_set_style_transform_pivot_y(control->root, NAV_CONTROL_SIZE / 2, 0);
    lv_obj_add_flag(control->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(control->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(control->root, control_event_cb, LV_EVENT_ALL, control);

    control->icon = lv_label_create(control->root);
    lv_label_set_text(control->icon, symbol != NULL ? symbol : "");
    lv_obj_set_style_text_color(control->icon, UI_COLOR_WHITE, 0);
    lv_obj_set_style_text_font(control->icon, &lv_font_montserrat_28, 0);
    lv_obj_center(control->icon);
    lv_obj_clear_flag(control->icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    apply_visual(control);
}

void ui_nav_control_set_symbol(ui_nav_control_t *control, const char *symbol)
{
    if(control == NULL || control->icon == NULL) return;
    lv_label_set_text(control->icon, symbol != NULL ? symbol : "");
    lv_obj_center(control->icon);
}

void ui_nav_control_set_active(ui_nav_control_t *control, bool active)
{
    if(control == NULL) return;
    control->active = active;
    apply_visual(control);
}

void ui_nav_control_set_focused(ui_nav_control_t *control, bool focused)
{
    if(control == NULL) return;
    control->focused = focused;
    apply_visual(control);
}

void ui_nav_control_activate(ui_nav_control_t *control)
{
    if(control == NULL || control->callback == NULL) return;
    control->callback(control->user_data);
}
