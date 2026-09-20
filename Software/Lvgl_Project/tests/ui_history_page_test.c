#include "ui/pages/ui_history_page.h"
#include "ui/pages/ui_ride_detail_page.h"
#include "ui/pages/ui_page_common.h"
#include "ui/core/ui_app_state.h"
#include "ui/core/ui_motion.h"
#include "ui/core/ui_sound.h"
#include "ui/models/ui_ride_history_model.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_W 480
#define TEST_H 854

static uint8_t s_draw_buffer[TEST_W * 20 * 2];
static bool s_memory_failed;
static ui_page_action_t s_action;

static void page_action(ui_page_action_t action, void *user_data)
{
    (void)user_data;
    s_action = action;
}

static void flush(lv_display_t *display, const lv_area_t *area,
                  uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}

static void log_message(lv_log_level_t level, const char *message)
{
    (void)level;
    if(message != NULL &&
       (strstr(message, "No memory") != NULL ||
        strstr(message, "Allocating layer buffer failed") != NULL ||
        strstr(message, "glyph dsc. not found") != NULL))
        s_memory_failed = true;
}

static ui_ride_record_t record(uint64_t id)
{
    return (ui_ride_record_t) {
        .id = id,
        .started_at_utc = 1750000000 + (int64_t)id * 7200,
        .finished_at_utc = 1750003600 + (int64_t)id * 7200,
        .distance_km = 20.0f + (float)id,
        .duration_seconds = 3600U,
        .average_speed_kmh = 29.5f,
        .maximum_speed_kmh = 48.0f,
        .ascent_m = 300U + (uint32_t)id,
        .descent_m = 280U + (uint32_t)id,
        .maximum_elevation_m = 760,
        .calories_kcal = 540U,
        .has_heart_rate = true,
        .average_heart_rate = 145U,
        .maximum_heart_rate = 178U,
        .minimum_heart_rate = 96U,
        .has_power = true,
        .average_power = 210U,
        .maximum_power = 610U,
        .has_cadence = true,
        .average_cadence = 88U,
        .maximum_cadence = 112U,
        .has_temperature = true,
        .average_temperature_tenths_c = 215
    };
}

int main(void)
{
    lv_init();
    lv_log_register_print_cb(log_message);
    lv_display_t *display = lv_display_create(TEST_W, TEST_H);
    assert(display != NULL);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, s_draw_buffer, NULL,
                           sizeof(s_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    ui_motion_init();
    ui_sound_init();
    ui_app_state_init();
    ui_page_status_registry_init();
    ui_ride_history_model_init();

    lv_obj_t *screen = ui_history_page_create(page_action, NULL);
    assert(screen != NULL);
    lv_screen_load(screen);
    ui_history_page_set_active(true);
    lv_obj_update_layout(screen);
    lv_refr_now(display);
    assert(!s_memory_failed);
    assert(ui_history_page_contains_scroll_point(16, 278));
    assert(ui_history_page_contains_scroll_point(463, 833));
    assert(!ui_history_page_contains_scroll_point(15, 278));
    assert(!ui_history_page_contains_scroll_point(16, 834));
    assert(ui_history_page_visible_count() == 0U);
    assert(!ui_history_page_handle_input(UI_INPUT_ENCODER_CW));
    lv_obj_t *empty_state = lv_obj_get_child(
        screen, (int32_t)lv_obj_get_child_count(screen) - 1);
    assert(empty_state != NULL);
    assert(lv_label_get_long_mode(empty_state) == LV_LABEL_LONG_WRAP);
    assert(lv_obj_get_height(empty_state) == 72);

    ui_ride_record_t records[UI_RIDE_HISTORY_MAX_RECORDS];
    for(size_t index = 0U; index < UI_RIDE_HISTORY_MAX_RECORDS; index++)
        records[index] = record(index + 1U);
    assert(ui_ride_history_model_replace(
        records, UI_RIDE_HISTORY_MAX_RECORDS));
    ui_history_page_refresh();
    lv_obj_update_layout(screen);
    lv_refr_now(display);
    assert(!s_memory_failed);
    assert(ui_history_page_visible_count() == 50U);
    assert(ui_history_page_selected_index() == 0);
    assert(ui_history_page_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_history_page_selected_index() == 1);
    assert(ui_history_page_handle_input(UI_INPUT_ENCODER_CCW));
    assert(ui_history_page_selected_index() == 0);
    assert(ui_history_page_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(s_action == UI_PAGE_ACTION_RIDE_DETAIL);
    ui_ride_record_t selected;
    assert(ui_history_page_selected_record(&selected));
    assert(selected.id == UI_RIDE_HISTORY_MAX_RECORDS);

    lv_obj_t *detail = ui_ride_detail_page_create(page_action, NULL);
    assert(detail != NULL);
    ui_ride_detail_page_set_record(&selected);
    ui_ride_detail_page_set_active(true);
    lv_screen_load(detail);
    lv_obj_update_layout(detail);
    lv_refr_now(display);
    assert(!s_memory_failed);
    assert(ui_ride_detail_page_contains_scroll_point(16, 150));
    assert(ui_ride_detail_page_handle_input(UI_INPUT_ENCODER_CW));

    ui_page_status_registry_deinit();
    lv_display_delete(display);
    puts("ui_history_page_test: PASS");
    return 0;
}
