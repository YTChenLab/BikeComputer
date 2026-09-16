#include "ui/overlays/ui_control_center.h"
#include "ui/core/ui_theme.h"
#include "lvgl.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TEST_W 480
#define TEST_H 854

static uint8_t s_draw_buffer[TEST_W * 20 * 2];
static ui_control_center_action_t s_last_action;
static int32_t s_last_value;
static uint32_t s_callback_count;
static bool s_layer_failure;

static void test_flush(lv_display_t *display, const lv_area_t *area,
                       uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}

static void capture_log(lv_log_level_t level, const char *message)
{
    (void)level;
    if(message != NULL &&
       (strstr(message, "No memory") != NULL ||
        strstr(message, "Allocating layer buffer failed") != NULL))
        s_layer_failure = true;
}

static void action_received(ui_control_center_action_t action,
                            int32_t value, void *user_data)
{
    (void)user_data;
    s_last_action = action;
    s_last_value = value;
    s_callback_count++;
}

static void render(void)
{
    lv_obj_update_layout(ui_control_center_root());
    lv_refr_now(lv_display_get_default());
    assert(!s_layer_failure);
}

int main(void)
{
    lv_init();
    lv_log_register_print_cb(capture_log);
    lv_display_t *display = lv_display_create(TEST_W, TEST_H);
    assert(display != NULL);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, s_draw_buffer, NULL,
                           sizeof(s_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, test_flush);

    ui_control_center_create(action_received, NULL);
    lv_obj_t *root = ui_control_center_root();
    assert(root != NULL);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(root, 0);

    ui_control_center_state_t state = {
        .wifi_enabled = true,
        .wifi_connected = true,
        .bluetooth_enabled = true,
        .bluetooth_connected = true,
        .gps = true,
        .power_save = false,
        .touch_locked = false,
        .battery_charging = false,
        .satellites = 10,
        .sound_level = 60,
        .brightness_level = 75,
        .battery_percent = 82
    };
    snprintf(state.wifi_name, sizeof(state.wifi_name), "%s",
             "Bike-SpeedView");
    snprintf(state.bluetooth_name, sizeof(state.bluetooth_name), "%s",
             "Riding Sensors");
    ui_control_center_set_state(&state);
    ui_control_center_prepare_show();
    assert(ui_control_center_selected_action() ==
           UI_CONTROL_CENTER_ACTION_WIFI);

    for(uint8_t action = 0; action < UI_CONTROL_CENTER_ACTION_COUNT;
        action++) {
        lv_obj_t *control = ui_control_center_control(
            (ui_control_center_action_t)action);
        assert(control != NULL);
        assert(lv_obj_get_x(control) >= 0);
        assert(lv_obj_get_y(control) >= 0);
        assert(lv_obj_get_x(control) + lv_obj_get_width(control) <= TEST_W);
        assert(lv_obj_get_y(control) + lv_obj_get_height(control) <= TEST_H);
    }

    lv_obj_t *wifi = ui_control_center_control(
        UI_CONTROL_CENTER_ACTION_WIFI);
    assert(lv_obj_send_event(wifi, LV_EVENT_CLICKED, NULL) == LV_RESULT_OK);
    assert(s_callback_count == 1U);
    assert(s_last_action == UI_CONTROL_CENTER_ACTION_WIFI);
    assert(s_last_value == -1);

    ui_control_center_set_feedback(UI_CONTROL_CENTER_ACTION_WIFI,
                                   UI_CONTROL_CENTER_FEEDBACK_PENDING);
    assert(lv_obj_send_event(wifi, LV_EVENT_CLICKED, NULL) == LV_RESULT_OK);
    assert(s_callback_count == 1U);
    ui_control_center_set_feedback(UI_CONTROL_CENTER_ACTION_WIFI,
                                   UI_CONTROL_CENTER_FEEDBACK_FAILED);

    assert(ui_control_center_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_control_center_selected_action() ==
           UI_CONTROL_CENTER_ACTION_BLUETOOTH);
    assert(ui_control_center_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_control_center_selected_action() ==
           UI_CONTROL_CENTER_ACTION_SOUND);
    assert(ui_control_center_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(ui_control_center_is_adjusting());
    assert(ui_control_center_handle_input(UI_INPUT_ENCODER_CW));
    assert(s_last_action == UI_CONTROL_CENTER_ACTION_SOUND);
    assert(s_last_value == 65);
    assert(ui_control_center_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(!ui_control_center_is_adjusting());

    assert(ui_control_center_slider_at(50, 200) ==
           UI_CONTROL_CENTER_SLIDER_SOUND);
    assert(ui_control_center_slider_at(160, 300) ==
           UI_CONTROL_CENTER_SLIDER_BRIGHTNESS);
    assert(ui_control_center_slider_at(300, 300) ==
           UI_CONTROL_CENTER_SLIDER_NONE);
    assert(ui_control_center_level_from_y(194) == 100U);
    assert(ui_control_center_level_from_y(424) == 0U);
    ui_control_center_preview_level(UI_CONTROL_CENTER_SLIDER_BRIGHTNESS, 42);

    render();
    lv_display_delete(display);
    puts("ui_control_center_test: PASS");
    return 0;
}
