#ifndef UI_CONTROL_CENTER_H
#define UI_CONTROL_CENTER_H

#include "../core/ui_input.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UI_CONTROL_CENTER_ACTION_WIFI = 0,
    UI_CONTROL_CENTER_ACTION_BLUETOOTH,
    UI_CONTROL_CENTER_ACTION_SOUND,
    UI_CONTROL_CENTER_ACTION_BRIGHTNESS,
    UI_CONTROL_CENTER_ACTION_GPS,
    UI_CONTROL_CENTER_ACTION_POWER_SAVE,
    UI_CONTROL_CENTER_ACTION_TOUCH_LOCK,
    UI_CONTROL_CENTER_ACTION_COUNT
} ui_control_center_action_t;

typedef enum
{
    UI_CONTROL_CENTER_SLIDER_NONE = 0,
    UI_CONTROL_CENTER_SLIDER_SOUND,
    UI_CONTROL_CENTER_SLIDER_BRIGHTNESS
} ui_control_center_slider_t;

typedef enum
{
    UI_CONTROL_CENTER_FEEDBACK_IDLE = 0,
    UI_CONTROL_CENTER_FEEDBACK_PENDING,
    UI_CONTROL_CENTER_FEEDBACK_FAILED
} ui_control_center_feedback_t;

typedef struct
{
    bool wifi_enabled;
    bool wifi_connected;
    bool bluetooth_enabled;
    bool bluetooth_connected;
    bool gps;
    bool power_save;
    bool touch_locked;
    bool battery_charging;
    char wifi_name[32];
    char bluetooth_name[32];
    uint8_t satellites;
    uint8_t sound_level;
    uint8_t brightness_level;
    uint8_t battery_percent;
} ui_control_center_state_t;

typedef void (*ui_control_center_action_cb_t)(
    ui_control_center_action_t action, int32_t value, void *user_data);

void ui_control_center_create(ui_control_center_action_cb_t callback,
                              void *user_data);
lv_obj_t *ui_control_center_root(void);
lv_obj_t *ui_control_center_control(ui_control_center_action_t action);
void ui_control_center_set_state(const ui_control_center_state_t *state);
void ui_control_center_set_feedback(ui_control_center_action_t action,
                                    ui_control_center_feedback_t feedback);
void ui_control_center_prepare_show(void);
bool ui_control_center_handle_input(ui_input_event_t input);
void ui_control_center_preview_level(ui_control_center_slider_t slider,
                                     uint8_t level);
ui_control_center_action_t ui_control_center_selected_action(void);
bool ui_control_center_is_adjusting(void);
ui_control_center_slider_t ui_control_center_slider_at(int32_t screen_x,
                                                       int32_t screen_y);
uint8_t ui_control_center_level_from_y(int32_t screen_y);

#endif /* UI_CONTROL_CENTER_H */
