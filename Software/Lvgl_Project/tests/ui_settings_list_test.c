#include "ui/components/settings/ui_settings_list.h"
#include "ui/core/ui_motion.h"
#include "ui/core/ui_sound.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_W 480
#define TEST_H 854

static uint8_t s_draw_buffer[TEST_W * 20 * 2];
static uint16_t s_last_action = UINT16_MAX;

static void test_flush(lv_display_t *display, const lv_area_t *area,
                       uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}

static void activated(uint16_t id, void *user_data)
{
    (void)user_data;
    s_last_action = id;
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(TEST_W, TEST_H);
    assert(display != NULL);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, s_draw_buffer, NULL,
                           sizeof(s_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, test_flush);
    ui_motion_init();
    ui_sound_init();

    ui_settings_list_t list;
    ui_settings_list_create(&list, lv_screen_active(), 20, 100, 440, 300,
                            activated, NULL);
    assert(ui_settings_list_add(&list, 10U, UI_SETTINGS_ROW_TOGGLE,
                                "WiFi", "NETWORK", ""));
    assert(ui_settings_list_add(&list, 11U, UI_SETTINGS_ROW_SLIDER,
                                "声音", "VOLUME", "50%"));
    assert(ui_settings_list_add(&list, 12U, UI_SETTINGS_ROW_INFO,
                                "版本", "VERSION", "v1.0.0"));
    assert(list.row_count == 3U && list.selection == 0U);
    assert(ui_settings_list_set_toggle(&list, 10U, true));
    assert(list.rows[0].enabled);
    assert(ui_settings_list_set_value(&list, 11U, "65%"));
    assert(strcmp(lv_label_get_text(list.rows[1].value), "65%") == 0);
    assert(ui_settings_list_set_feedback(&list, 10U,
                                         UI_SETTINGS_ROW_PENDING));
    assert(strcmp(lv_label_get_text(list.rows[0].value), "WAIT") == 0);

    assert(ui_settings_list_handle_input(&list, UI_INPUT_ENCODER_CW));
    assert(list.selection == 1U);
    assert(ui_settings_list_handle_input(&list, UI_INPUT_ENCODER_PRESS));
    assert(s_last_action == 11U);
    assert(ui_settings_list_handle_input(&list, UI_INPUT_ENCODER_CCW));
    assert(list.selection == 0U);

    lv_display_delete(display);
    puts("ui_settings_list_test: PASS");
    return 0;
}
