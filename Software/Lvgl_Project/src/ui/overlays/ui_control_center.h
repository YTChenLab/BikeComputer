#ifndef UI_CONTROL_CENTER_H
#define UI_CONTROL_CENTER_H

#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UI_CONTROL_CENTER_ACTION_WIFI = 0,
    UI_CONTROL_CENTER_ACTION_BLUETOOTH,
    UI_CONTROL_CENTER_ACTION_GPS,
    UI_CONTROL_CENTER_ACTION_POWER_SAVE
} ui_control_center_action_t;

typedef enum
{
    UI_CONTROL_CENTER_SLIDER_NONE = 0,
    UI_CONTROL_CENTER_SLIDER_SOUND,
    UI_CONTROL_CENTER_SLIDER_BRIGHTNESS
} ui_control_center_slider_t;

typedef struct
{
    bool wifi;
    bool bluetooth;
    bool gps;
    bool power_save;
    uint8_t satellites;
    uint8_t sound_level;
    uint8_t brightness_level;
} ui_control_center_state_t;

typedef void (*ui_control_center_action_cb_t)(ui_control_center_action_t action,
                                              void *user_data);

void ui_control_center_create(ui_control_center_action_cb_t callback,
                              void *user_data);
lv_obj_t *ui_control_center_root(void);
void ui_control_center_set_state(const ui_control_center_state_t *state);
ui_control_center_slider_t ui_control_center_slider_at(int32_t screen_x,
                                                       int32_t screen_y);
uint8_t ui_control_center_level_from_y(int32_t screen_y);

#endif /* UI_CONTROL_CENTER_H */
