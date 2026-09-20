#include "ui/pages/ui_achievements_page.h"
#include "ui/pages/ui_page_common.h"
#include "ui/pages/ui_profile_page.h"
#include "ui/core/ui_app_state.h"
#include "ui/core/ui_background.h"
#include "ui/core/ui_motion.h"
#include "ui/core/ui_sound.h"
#include "ui/models/ui_home_data_model.h"
#include "ui/models/ui_ride_history_model.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_W 480
#define TEST_H 854

static uint8_t s_draw_buffer[TEST_W * 20 * 2];
static uint8_t s_framebuffer[TEST_W * TEST_H * 2];
static bool s_render_failed;
static ui_page_action_t s_action;

static void flush(lv_display_t *display, const lv_area_t *area,
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

static void save_snapshot_if_requested(lv_obj_t *screen,
                                       const char *environment_name)
{
    const char *path = getenv(environment_name);
    if(path == NULL || path[0] == '\0') return;
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    lv_obj_invalidate(screen);
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

static void log_message(lv_log_level_t level, const char *message)
{
    (void)level;
    if(message != NULL &&
       (strstr(message, "No memory") != NULL ||
        strstr(message, "Allocating layer buffer failed") != NULL ||
        strstr(message, "glyph dsc. not found") != NULL))
        s_render_failed = true;
}

static void page_action(ui_page_action_t action, void *user_data)
{
    (void)user_data;
    s_action = action;
}

static void finish_animations(void)
{
    for(uint8_t step = 0U; step < 24U; step++) {
        lv_tick_inc(25U);
        (void)lv_timer_handler();
    }
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
    ui_home_data_model_init();
    ui_ride_history_model_init();
    ui_background_init();
    ui_page_status_registry_init();

    lv_obj_t *profile = ui_profile_page_create(page_action, NULL);
    assert(profile != NULL);
    lv_screen_load(profile);
    ui_profile_page_set_active(true);
    lv_obj_update_layout(profile);
    lv_refr_now(display);
    assert(!s_render_failed);
    assert(ui_profile_page_selected_index() == 0);
    assert(ui_profile_page_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(s_action == UI_PAGE_ACTION_ACHIEVEMENT);
    assert(ui_profile_page_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_profile_page_selected_index() == 1);
    assert(ui_profile_page_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(s_action == UI_PAGE_ACTION_HISTORY);
    assert(ui_profile_page_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_profile_page_selected_index() == 2);
    assert(ui_profile_page_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(s_action == UI_PAGE_ACTION_SETTINGS);
    save_snapshot_if_requested(profile, "UI_PROFILE_SNAPSHOT_PATH");
    ui_profile_page_set_active(false);

    lv_obj_t *achievements = ui_achievements_page_create();
    assert(achievements != NULL);
    lv_screen_load(achievements);
    ui_achievements_page_set_active(true);
    lv_obj_update_layout(achievements);
    lv_refr_now(display);
    assert(!s_render_failed);
    assert(ui_achievements_page_contains_scroll_point(16, 260));
    assert(ui_achievements_page_contains_scroll_point(463, 825));
    assert(!ui_achievements_page_contains_scroll_point(15, 260));
    assert(!ui_achievements_page_contains_scroll_point(16, 826));
    assert(ui_achievements_page_count() == 6U);
    assert(ui_achievements_page_selected_index() == 0);
    for(int32_t index = 1; index < 6; index++) {
        assert(ui_achievements_page_handle_input(UI_INPUT_ENCODER_CW));
        assert(ui_achievements_page_selected_index() == index);
    }
    assert(ui_achievements_page_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_achievements_page_selected_index() == 5);
    assert(ui_achievements_page_handle_input(UI_INPUT_ENCODER_PRESS));
    finish_animations();

    ui_ride_record_t records[10] = {0};
    for(uint32_t index = 0U; index < 10U; index++) {
        records[index].id = index + 1U;
        records[index].distance_km = 55.0f;
        records[index].duration_seconds = 5U * 3600U;
        records[index].ascent_m = 250U;
    }
    assert(ui_ride_history_model_replace(records, 10U));
    ui_achievements_page_refresh();
    ui_profile_page_refresh();
    lv_refr_now(display);
    assert(!s_render_failed);
    save_snapshot_if_requested(achievements,
                               "UI_ACHIEVEMENTS_SNAPSHOT_PATH");

    /* The application currently owns more than ten status-bearing pages.
     * Keep a regression check here so adding routes cannot silently leave
     * later pages detached from the shared status model. */
    ui_page_status_registry_init();
    lv_obj_t *status_screen = lv_obj_create(NULL);
    assert(status_screen != NULL);
    ui_status_bar_t status_bars[12] = {0};
    for(uint32_t index = 0U; index < 12U; index++) {
        ui_status_bar_create(&status_bars[index], status_screen);
        assert(ui_page_status_register(&status_bars[index]));
    }
    assert(UI_PAGE_STATUS_CAPACITY >= 12U);
    lv_obj_delete(status_screen);

    ui_page_status_registry_deinit();
    ui_background_deinit();
    lv_display_delete(display);
    puts("ui_profile_achievements_test: PASS");
    return 0;
}
