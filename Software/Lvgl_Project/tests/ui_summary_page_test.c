#include "ui/pages/ui_summary_page.h"
#include "ui/pages/ui_ride_report_page.h"
#include "ui/pages/ui_page_common.h"
#include "ui/core/ui_app_state.h"
#include "ui/core/ui_motion.h"
#include "ui/core/ui_sound.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_W 480
#define TEST_H 854

static uint8_t s_draw_buffer[TEST_W * 20 * 2];
static ui_page_action_t s_action = UI_PAGE_ACTION_SAVE_RIDE;
static bool s_memory_failed;

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

static void action(ui_page_action_t value, void *user_data)
{
    (void)user_data;
    s_action = value;
}

static lv_obj_t *find_label(lv_obj_t *root, const char *text)
{
    if(lv_obj_check_type(root, &lv_label_class) &&
       strcmp(lv_label_get_text(root), text) == 0) return root;
    for(uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, i), text);
        if(found != NULL) return found;
    }
    return NULL;
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

    lv_obj_t *screen = ui_summary_page_create(action, NULL);
    assert(screen != NULL);
    lv_screen_load(screen);
    ui_ride_summary_t summary = {
        .distance_km = 50.25f,
        .duration_seconds = 5425U,
        .moving_time_seconds = 5300U,
        .average_speed_kmh = 33.4f,
        .maximum_speed_kmh = 48.7f,
        .ascent_m = 1268U,
        .descent_m = 1200U,
        .maximum_elevation_m = 1826,
        .calories_kcal = 1536U,
        .has_heart_rate = true,
        .average_heart_rate = 148U,
        .maximum_heart_rate = 182U,
        .minimum_heart_rate = 102U,
        .has_power = true,
        .average_power = 201U,
        .maximum_power = 782U,
        .has_cadence = true,
        .average_cadence = 85U,
        .maximum_cadence = 112U,
        .has_temperature = true,
        .average_temperature_tenths_c = 214,
        .saved = true,
        .version = 1U
    };
    summary.trace.point_count = UI_RIDE_REPORT_POINT_MAX;
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        summary.trace.speed_tenths[index] = 180U + index;
        summary.trace.heart_rate[index] = 120U + index % 45U;
        summary.trace.power[index] = 180U + index % 220U;
        summary.trace.cadence[index] = 75U + index % 25U;
        summary.trace.elevation_m[index] = 420 + index * 8;
    }
    summary.trace.heart_rate_zone_seconds[2] = 2200U;
    summary.trace.power_zone_seconds[2] = 1800U;
    ui_summary_page_set_data(&summary);
    ui_summary_page_set_active(true);
    lv_tick_inc(1000U);
    lv_anim_refr_now();
    lv_obj_update_layout(screen);
    lv_refr_now(display);
    assert(!s_memory_failed);
    assert(ui_summary_page_contains_scroll_point(16, 150));
    assert(ui_summary_page_contains_scroll_point(463, UI_PAGE_ACTION_Y - 13));
    assert(!ui_summary_page_contains_scroll_point(463, UI_PAGE_ACTION_Y));
    assert(!ui_summary_page_contains_scroll_point(15, 150));
    assert(ui_ride_report_page_content_height(
               UI_RIDE_REPORT_SUMMARY_MODE) > 2500U);
    assert(ui_summary_page_handle_input(UI_INPUT_ENCODER_CW));
    lv_tick_inc(400U);
    lv_anim_refr_now();
    assert(ui_ride_report_page_scroll_y(
               UI_RIDE_REPORT_SUMMARY_MODE) > 0);
    int32_t before_fast_turn=ui_ride_report_page_scroll_y(UI_RIDE_REPORT_SUMMARY_MODE);
    for(unsigned i=0U;i<3U;++i)
        assert(ui_summary_page_handle_input(UI_INPUT_ENCODER_CW));
    lv_tick_inc(400U); lv_anim_refr_now();
    assert(ui_ride_report_page_scroll_y(UI_RIDE_REPORT_SUMMARY_MODE)==before_fast_turn+780);
    for(unsigned i=0U;i<2U;++i)
        assert(ui_summary_page_handle_input(UI_INPUT_ENCODER_CCW));
    lv_tick_inc(400U); lv_anim_refr_now();
    assert(ui_ride_report_page_scroll_y(UI_RIDE_REPORT_SUMMARY_MODE)==before_fast_turn+260);
    for(uint8_t index = 0U; index < 4U; index++) {
        assert(ui_summary_page_handle_input(UI_INPUT_ENCODER_CW));
        lv_tick_inc(400U);
        lv_anim_refr_now();
    }
    for(uint8_t index = 0U; index < 9U; index++) {
        assert(ui_summary_page_handle_input(UI_INPUT_ENCODER_CW));
        lv_tick_inc(400U);
        lv_anim_refr_now();
    }
    assert(ui_ride_report_page_scroll_y(UI_RIDE_REPORT_SUMMARY_MODE) <=
           (int32_t)ui_ride_report_page_content_height(
               UI_RIDE_REPORT_SUMMARY_MODE) - (UI_PAGE_ACTION_Y - 12 - 150));
    /* Completion stays accessible while the report is scrolled/rebuilt. */
    lv_obj_t *done = find_label(screen, "完成");
    assert(done != NULL);
    lv_obj_t *done_button = lv_obj_get_parent(done);
    assert(lv_obj_get_parent(done_button) == screen);
    assert(lv_obj_get_y(done_button) == UI_PAGE_ACTION_Y);
    assert(lv_obj_get_height(done_button) == UI_PAGE_ACTION_HEIGHT);
    lv_area_t button_area, label_area;
    lv_obj_get_coords(done_button, &button_area);
    lv_obj_get_coords(done, &label_area);
    assert(abs((button_area.y1 + button_area.y2) -
               (label_area.y1 + label_area.y2)) <= 1);
    for(unsigned i = 0; i < 20; ++i)
        assert(ui_summary_page_handle_input(UI_INPUT_ENCODER_CW));
    lv_tick_inc(400U); lv_anim_refr_now();
    assert(ui_ride_report_page_scroll_y(UI_RIDE_REPORT_SUMMARY_MODE) ==
        (int32_t)ui_ride_report_page_content_height(UI_RIDE_REPORT_SUMMARY_MODE) -
        (UI_PAGE_ACTION_Y - 12 - 150));

    ui_ride_report_model_t complete;
    ui_ride_report_model_make_mock(&complete,
                                   UI_RIDE_REPORT_CASE_COMPLETE);
    assert(ui_ride_report_page_set_model(UI_RIDE_REPORT_SUMMARY_MODE,
                                         &complete));
    uint32_t complete_height = ui_ride_report_page_content_height(
        UI_RIDE_REPORT_SUMMARY_MODE);
    ui_ride_report_model_t no_power;
    ui_ride_report_model_make_mock(&no_power,
                                   UI_RIDE_REPORT_CASE_NO_POWER);
    assert(ui_ride_report_page_set_model(UI_RIDE_REPORT_SUMMARY_MODE,
                                         &no_power));
    assert(ui_ride_report_page_content_height(
               UI_RIDE_REPORT_SUMMARY_MODE) < complete_height);
    ui_ride_report_model_t no_hr_cadence;
    ui_ride_report_model_make_mock(&no_hr_cadence,
                                   UI_RIDE_REPORT_CASE_NO_HR_CADENCE);
    assert(ui_ride_report_page_set_model(UI_RIDE_REPORT_SUMMARY_MODE,
                                         &no_hr_cadence));
    assert(ui_ride_report_page_content_height(
               UI_RIDE_REPORT_SUMMARY_MODE) < complete_height);

    /* A stationary recording has zero moving time, not its full duration.
     * Missing metrics remain distinct from measured zeros after rendering. */
    ui_ride_summary_t stationary = {.duration_seconds = 60U,
        .metric_flags = UI_REPORT_METRICS_KNOWN | UI_REPORT_HAS_SPEED,
        .saved = true};
    ui_summary_page_set_data(&stationary);
    lv_obj_update_layout(screen);
    lv_obj_t *moving = find_label(screen, "MOVING TIME");
    assert(moving != NULL);
    assert(find_label(lv_obj_get_parent(moving), "00:00:00") != NULL);
    assert(find_label(screen,"骑行已结束，记录已保存"));
    lv_obj_t *calories = find_label(screen, "CALORIES");
    assert(calories != NULL);
    assert(find_label(lv_obj_get_parent(calories), "--") != NULL);
    stationary.saved=false;
    ui_summary_page_set_data(&stationary); lv_obj_update_layout(screen);
    assert(find_label(screen,"骑行已结束，记录尚未保存"));
    int32_t saved_scroll=ui_ride_report_page_scroll_y(UI_RIDE_REPORT_SUMMARY_MODE);
    ui_summary_page_set_saved(true); lv_obj_update_layout(screen);
    assert(find_label(screen,"骑行已结束，记录已保存"));
    assert(ui_ride_report_page_scroll_y(UI_RIDE_REPORT_SUMMARY_MODE)==saved_scroll);
    ui_summary_page_set_active(false);
    ui_summary_page_set_active(true);
    lv_tick_inc(1000U);
    lv_anim_refr_now();
    lv_refr_now(display);
    assert(!s_memory_failed);
    assert(find_label(screen, "--") != NULL);
    /* A valid large backend value must never overflow the count-up target. */
    ui_ride_report_model_t large;
    ui_ride_report_model_make_mock(&large,UI_RIDE_REPORT_CASE_COMPLETE);
    large.distance_km=500000000.0f;
    assert(ui_ride_report_page_set_model(UI_RIDE_REPORT_SUMMARY_MODE,&large));
    ui_summary_page_set_active(true);
    lv_obj_update_layout(screen);
    assert(find_label(screen,"500000000.0") != NULL);
    assert(find_label(screen,"骑行已结束，记录已保存"));
    lv_refr_now(display); assert(!s_memory_failed);

    assert(ui_summary_page_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(s_action == UI_PAGE_ACTION_HOME);
    ui_summary_page_set_active(false);
    assert(ui_ride_report_page_content_height(
               UI_RIDE_REPORT_SUMMARY_MODE) == 0U);

    ui_page_status_registry_deinit();
    lv_display_delete(display);
    puts("ui_summary_page_test: PASS");
    return 0;
}
