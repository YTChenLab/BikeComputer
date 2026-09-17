#ifndef UI_SETTINGS_PAGE_H
#define UI_SETTINGS_PAGE_H

#include "ui_page_action.h"
#include "../core/ui_app_state.h"
#include "../core/ui_input.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UI_SETTINGS_ITEM_WIFI = 0,
    UI_SETTINGS_ITEM_BLUETOOTH,
    UI_SETTINGS_ITEM_GPS,
    UI_SETTINGS_ITEM_SOUND,
    UI_SETTINGS_ITEM_BRIGHTNESS,
    UI_SETTINGS_ITEM_POWER_SAVE,
    UI_SETTINGS_ITEM_TOUCH_LOCK,
    UI_SETTINGS_ITEM_SYSTEM,
    UI_SETTINGS_ITEM_POWER,
    UI_SETTINGS_ITEM_COUNT
} ui_settings_item_t;

typedef enum
{
    UI_SETTINGS_FEEDBACK_IDLE = 0,
    UI_SETTINGS_FEEDBACK_PENDING,
    UI_SETTINGS_FEEDBACK_FAILED
} ui_settings_feedback_t;

lv_obj_t *ui_settings_page_create(ui_page_action_cb_t callback,
                                  void *user_data);
void ui_settings_page_set_state(const ui_app_state_t *state);
void ui_settings_page_set_feedback(ui_settings_item_t item,
                                   ui_settings_feedback_t feedback);
bool ui_settings_page_handle_input(ui_input_event_t input);
bool ui_settings_page_handle_key(uint32_t key);

#endif /* UI_SETTINGS_PAGE_H */
