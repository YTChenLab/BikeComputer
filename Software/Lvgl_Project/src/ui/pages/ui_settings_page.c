#include "ui_settings_page.h"

#include "ui_page_common.h"
#include "../components/settings/ui_settings_list.h"

#include <stdio.h>

static ui_status_bar_t s_status;
static ui_settings_list_t s_list;
static ui_page_action_cb_t s_callback;
static void *s_user_data;

static void list_action(uint16_t id, void *user_data)
{
    (void)user_data;
    if(s_callback == NULL) return;
    switch((ui_settings_item_t)id) {
        case UI_SETTINGS_ITEM_WIFI:
            s_callback(UI_PAGE_ACTION_WIFI_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_BLUETOOTH:
            s_callback(UI_PAGE_ACTION_BLUETOOTH_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_GPS:
            s_callback(UI_PAGE_ACTION_GPS_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_SOUND:
            s_callback(UI_PAGE_ACTION_SOUND_CONTROL, s_user_data); break;
        case UI_SETTINGS_ITEM_BRIGHTNESS:
            s_callback(UI_PAGE_ACTION_BRIGHTNESS_CONTROL, s_user_data); break;
        case UI_SETTINGS_ITEM_POWER_SAVE:
            s_callback(UI_PAGE_ACTION_POWER_SAVE_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_TOUCH_LOCK:
            s_callback(UI_PAGE_ACTION_TOUCH_LOCK_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_SYSTEM:
            s_callback(UI_PAGE_ACTION_SYSTEM, s_user_data); break;
        case UI_SETTINGS_ITEM_POWER:
            s_callback(UI_PAGE_ACTION_POWER_MENU, s_user_data); break;
        default: break;
    }
}

lv_obj_t *ui_settings_page_create(ui_page_action_cb_t callback,
                                  void *user_data)
{
    s_callback = callback;
    s_user_data = user_data;
    lv_obj_t *screen = ui_page_screen_create();
    ui_page_status_create(&s_status, screen);
    ui_page_title(screen, "系统设置", "SETTINGS");

    ui_settings_list_create(&s_list, screen, 20, 156, 440, 442,
                            list_action, NULL);
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_WIFI,
        UI_SETTINGS_ROW_TOGGLE, "WiFi", "WIRELESS NETWORK", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_BLUETOOTH,
        UI_SETTINGS_ROW_TOGGLE, "蓝牙", "RIDING SENSORS", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_GPS,
        UI_SETTINGS_ROW_TOGGLE, "卫星定位", "GNSS POSITIONING", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_SOUND,
        UI_SETTINGS_ROW_SLIDER, "声音", "SYSTEM VOLUME", "62%");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_BRIGHTNESS,
        UI_SETTINGS_ROW_SLIDER, "亮度", "DISPLAY BRIGHTNESS", "75%");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_POWER_SAVE,
        UI_SETTINGS_ROW_TOGGLE, "省电模式", "POWER MODE", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_TOUCH_LOCK,
        UI_SETTINGS_ROW_TOGGLE, "触摸锁定", "TOUCH LOCK", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_SYSTEM,
        UI_SETTINGS_ROW_ACTION, "系统信息", "DEVICE AND STORAGE", "DETAIL");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_POWER,
        UI_SETTINGS_ROW_ACTION, "电源设置", "POWER OFF OR RESTART", "OPEN");

    ui_app_state_t state;
    if(ui_app_state_get(&state)) ui_settings_page_set_state(&state);
    return screen;
}

void ui_settings_page_set_state(const ui_app_state_t *state)
{
    if(state == NULL || s_list.root == NULL) return;
    char text[16];
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_WIFI,
                                      state->wifi_enabled);
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_BLUETOOTH,
                                      state->bluetooth_enabled);
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_GPS,
                                      state->gps_enabled);
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_POWER_SAVE,
                                      state->power_save_enabled);
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_TOUCH_LOCK,
                                      state->touch_locked);
    snprintf(text, sizeof(text), "%u%%", state->sound_level);
    (void)ui_settings_list_set_value(&s_list, UI_SETTINGS_ITEM_SOUND, text);
    snprintf(text, sizeof(text), "%u%%", state->brightness_level);
    (void)ui_settings_list_set_value(&s_list, UI_SETTINGS_ITEM_BRIGHTNESS,
                                     text);
}

void ui_settings_page_set_feedback(ui_settings_item_t item,
                                   ui_settings_feedback_t feedback)
{
    ui_settings_row_feedback_t row_feedback = UI_SETTINGS_ROW_IDLE;
    if(feedback == UI_SETTINGS_FEEDBACK_PENDING)
        row_feedback = UI_SETTINGS_ROW_PENDING;
    else if(feedback == UI_SETTINGS_FEEDBACK_FAILED)
        row_feedback = UI_SETTINGS_ROW_FAILED;
    (void)ui_settings_list_set_feedback(&s_list, (uint16_t)item,
                                        row_feedback);
}

bool ui_settings_page_handle_input(ui_input_event_t input)
{
    return ui_settings_list_handle_input(&s_list, input);
}

bool ui_settings_page_handle_key(uint32_t key)
{
    if(key == LV_KEY_DOWN)
        return ui_settings_page_handle_input(UI_INPUT_ENCODER_CW);
    if(key == LV_KEY_UP)
        return ui_settings_page_handle_input(UI_INPUT_ENCODER_CCW);
    if(key == LV_KEY_ENTER)
        return ui_settings_page_handle_input(UI_INPUT_ENCODER_PRESS);
    return false;
}
