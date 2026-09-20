#include "ui/overlays/ui_control_center.h"
#include "ui/core/ui_theme.h"
#include "lvgl.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_W 480
#define TEST_H 854

static uint8_t s_draw_buffer[TEST_W * 20 * 2];
static uint8_t s_framebuffer[TEST_W * TEST_H * 2];
static ui_control_center_action_t s_last_action;
static int32_t s_last_value;
static uint32_t s_callback_count;
static bool s_layer_failure;

static void test_flush(lv_display_t *display, const lv_area_t *area,
                       uint8_t *pixels)
{
    int32_t width = lv_area_get_width(area);
    int32_t height = lv_area_get_height(area);
    for(int32_t row = 0; row < height; row++) {
        int32_t target_y = area->y1 + row;
        if(target_y < 0 || target_y >= TEST_H) continue;
        int32_t source_x = area->x1 < 0 ? -area->x1 : 0;
        int32_t target_x = area->x1 < 0 ? 0 : area->x1;
        int32_t copy_width = width - source_x;
        if(target_x + copy_width > TEST_W)
            copy_width = TEST_W - target_x;
        if(copy_width <= 0) continue;
        memcpy(&s_framebuffer[(target_y * TEST_W + target_x) * 2],
               &pixels[(row * width + source_x) * 2],
               (size_t)copy_width * 2U);
    }
    lv_display_flush_ready(display);
}

static void save_snapshot_if_requested(lv_obj_t *root)
{
    const char *path = getenv("UI_CONTROL_CENTER_SNAPSHOT_PATH");
    if(path == NULL || path[0] == '\0') return;
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    lv_obj_invalidate(root);
    lv_refr_now(lv_display_get_default());
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    fprintf(file, "P6\n%d %d\n255\n", TEST_W, TEST_H);
    for(size_t index = 0; index < TEST_W * TEST_H; index++) {
        uint16_t pixel = (uint16_t)s_framebuffer[index * 2U] |
                         ((uint16_t)s_framebuffer[index * 2U + 1U] << 8U);
        uint8_t rgb[3] = {
            (uint8_t)((((pixel >> 11U) & 0x1FU) * 255U) / 31U),
            (uint8_t)((((pixel >> 5U) & 0x3FU) * 255U) / 63U),
            (uint8_t)(((pixel & 0x1FU) * 255U) / 31U)
        };
        assert(fwrite(rgb, 1U, sizeof(rgb), file) == sizeof(rgb));
    }
    fclose(file);
}

static void save_snapshot_for_env(lv_obj_t *root, const char *env_name)
{
    const char *path = getenv(env_name);
    if(path == NULL || path[0] == '\0') return;
    _putenv_s("UI_CONTROL_CENTER_SNAPSHOT_PATH", path);
    save_snapshot_if_requested(root);
    _putenv_s("UI_CONTROL_CENTER_SNAPSHOT_PATH", "");
}

static void capture_log(lv_log_level_t level, const char *message)
{
    (void)level;
    if(message != NULL &&
       (strstr(message, "No memory") != NULL ||
        strstr(message, "Allocating layer buffer failed") != NULL ||
        strstr(message, "glyph dsc. not found") != NULL))
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

static void finish_animations(void)
{
    for(uint8_t step = 0U; step < 12U; step++) {
        lv_tick_inc(20U);
        (void)lv_timer_handler();
    }
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
    lv_obj_t *panel = ui_control_center_panel();
    assert(panel != NULL);
    lv_obj_t *focus_capsule = ui_control_center_focus_capsule();
    assert(focus_capsule != NULL);
    lv_obj_update_layout(panel);
    assert(lv_obj_get_width(panel) == TEST_W);
    assert(lv_obj_get_height(panel) == UI_CONTROL_CENTER_COMPACT_HEIGHT);
    for(uint8_t index = 0U; index < 4U; index++) {
        ui_control_center_action_t action =
            (ui_control_center_action_t)index;
        lv_obj_t *control = ui_control_center_control(action);
        assert(control != NULL);
        assert(lv_obj_get_x(control) >= 0);
        assert(lv_obj_get_x(control) + lv_obj_get_width(control) <= TEST_W);
        assert(lv_obj_get_y(control) >= 0);
        assert(lv_obj_get_y(control) + lv_obj_get_height(control) <=
               UI_CONTROL_CENTER_COMPACT_HEIGHT);
    }
    assert(lv_obj_get_x(focus_capsule) == 12);
    assert(lv_obj_get_y(focus_capsule) == 48);
    for(uint8_t index = 0U; index < 4U; index++) {
        assert(ui_control_center_selected_action() ==
               (ui_control_center_action_t)index);
        assert(ui_control_center_handle_input(UI_INPUT_ENCODER_CW));
    }
    assert(ui_control_center_selected_action() ==
           UI_CONTROL_CENTER_ACTION_WIFI);
    finish_animations();
    assert(lv_obj_get_x(focus_capsule) == 12);
    save_snapshot_for_env(root, "UI_CONTROL_CENTER_COMPACT_SNAPSHOT_PATH");
    uint32_t panel_children = lv_obj_get_child_count(panel);
    for(int32_t height = UI_CONTROL_CENTER_COMPACT_HEIGHT;
        height <= UI_CONTROL_CENTER_PANEL_HEIGHT; height += 7) {
        ui_control_center_set_panel_height(height);
        for(uint8_t index = 0U; index < 4U; index++) {
            lv_obj_t *control = ui_control_center_control(
                (ui_control_center_action_t)index);
            assert(lv_obj_get_x(control) >= 0);
            assert(lv_obj_get_x(control) + lv_obj_get_width(control) <= TEST_W);
            assert(lv_obj_get_y(control) >= 0);
            assert(lv_obj_get_y(control) + lv_obj_get_height(control) <=
                   height - 8);
        }
        assert(lv_obj_get_child_count(panel) == panel_children);
    }
    ui_control_center_set_panel_height(
        (UI_CONTROL_CENTER_COMPACT_HEIGHT + UI_CONTROL_CENTER_PANEL_HEIGHT) / 2);
    save_snapshot_for_env(root, "UI_CONTROL_CENTER_MORPH_SNAPSHOT_PATH");
    ui_control_center_set_mode(UI_CONTROL_CENTER_MODE_EXPANDED);
    lv_obj_update_layout(panel);
    assert(lv_obj_get_height(panel) == UI_CONTROL_CENTER_PANEL_HEIGHT);
    assert(lv_obj_get_child_count(panel) == panel_children);

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

    lv_obj_t *gps = ui_control_center_control(
        UI_CONTROL_CENTER_ACTION_GPS);
    lv_obj_t *power_save = ui_control_center_control(
        UI_CONTROL_CENTER_ACTION_POWER_SAVE);
    lv_obj_t *gps_state = lv_obj_get_child(gps, 2);
    lv_obj_t *power_save_state = lv_obj_get_child(power_save, 2);
    assert(strcmp(lv_label_get_text(gps_state), "良好") == 0);
    assert(strcmp(lv_label_get_text(power_save_state), "标准") == 0);
    assert(lv_obj_get_style_text_font(gps_state, 0) == UI_FONT_PAGE_TEXT);
    assert(lv_obj_get_style_text_font(power_save_state, 0) ==
           UI_FONT_PAGE_TEXT);

    for(uint8_t action = 0; action < UI_CONTROL_CENTER_ACTION_COUNT;
        action++) {
        lv_obj_t *control = ui_control_center_control(
            (ui_control_center_action_t)action);
        assert(control != NULL);
        assert(lv_obj_get_x(control) >= 0);
        assert(lv_obj_get_y(control) >= 0);
        assert(lv_obj_get_x(control) + lv_obj_get_width(control) <=
               lv_obj_get_width(panel));
        assert(lv_obj_get_y(control) + lv_obj_get_height(control) <=
               UI_CONTROL_CENTER_PANEL_HEIGHT);
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
    assert(s_callback_count == 1U);
    assert(ui_control_center_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(!ui_control_center_is_adjusting());
    assert(s_last_action == UI_CONTROL_CENTER_ACTION_SOUND);
    assert(s_last_value == 65);
    assert(ui_control_center_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_control_center_selected_action() ==
           UI_CONTROL_CENTER_ACTION_BRIGHTNESS);
    assert(ui_control_center_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(ui_control_center_is_adjusting());
    assert(ui_control_center_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(!ui_control_center_is_adjusting());
    assert(s_last_action == UI_CONTROL_CENTER_ACTION_BRIGHTNESS);
    assert(s_last_value == 75);

    assert(ui_control_center_slider_at(200, 180) ==
           UI_CONTROL_CENTER_SLIDER_SOUND);
    assert(ui_control_center_slider_at(200, 250) ==
           UI_CONTROL_CENTER_SLIDER_BRIGHTNESS);
    assert(ui_control_center_slider_at(10, 200) ==
           UI_CONTROL_CENTER_SLIDER_NONE);
    assert(ui_control_center_slider_at(50, 180) ==
           UI_CONTROL_CENTER_SLIDER_NONE);
    assert(ui_control_center_level_from_x(132) == 0U);
    assert(ui_control_center_level_from_x(414) == 100U);
    assert(ui_control_center_level_from_x(273) == 50U);
    ui_control_center_preview_level(UI_CONTROL_CENTER_SLIDER_BRIGHTNESS, 42);
    finish_animations();
    lv_obj_t *brightness = ui_control_center_control(
        UI_CONTROL_CENTER_ACTION_BRIGHTNESS);
    lv_obj_t *brightness_fill = lv_obj_get_child(brightness, 4);
    lv_obj_t *brightness_thumb = lv_obj_get_child(brightness, 5);
    assert(lv_obj_get_width(brightness_fill) == 118);
    assert(lv_obj_get_x(brightness_thumb) == 228);

    for(uint16_t cycle = 0U; cycle < 250U; cycle++) {
        state.sound_level = (uint8_t)(cycle % 101U);
        state.brightness_level = (uint8_t)((cycle * 3U) % 101U);
        ui_control_center_set_state(&state);
        ui_control_center_set_feedback(
            UI_CONTROL_CENTER_ACTION_WIFI,
            (cycle % 5U) == 0U ? UI_CONTROL_CENTER_FEEDBACK_PENDING :
                                 UI_CONTROL_CENTER_FEEDBACK_IDLE);
    }
    finish_animations();
    assert(lv_anim_count_running() == 0U);
    assert(lv_obj_get_child_count(panel) == panel_children);

    for(uint8_t cycle = 0U; cycle < 50U; cycle++) {
        ui_control_center_set_mode(UI_CONTROL_CENTER_MODE_COMPACT);
        ui_control_center_set_panel_height(UI_CONTROL_CENTER_COMPACT_HEIGHT);
        ui_control_center_set_mode(UI_CONTROL_CENTER_MODE_EXPANDED);
        ui_control_center_set_panel_height(UI_CONTROL_CENTER_PANEL_HEIGHT);
        assert(lv_obj_get_child_count(panel) == panel_children);
    }
    assert(lv_anim_count_running() == 0U);

    render();
    save_snapshot_if_requested(root);
    lv_display_delete(display);
    puts("ui_control_center_test: PASS");
    return 0;
}
