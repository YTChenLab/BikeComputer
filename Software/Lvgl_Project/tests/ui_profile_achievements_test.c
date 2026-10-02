#include "ui/pages/ui_achievements_page.h"
#include "ui/pages/ui_page_common.h"
#include "ui/pages/ui_profile_page.h"
#include "ui/core/ui_app_state.h"
#include "ui/core/ui_background.h"
#include "ui/core/ui_motion.h"
#include "ui/core/ui_sound.h"
#include "ui/models/ui_home_data_model.h"
#include "ui/models/ui_ride_history_model.h"
#include "ui/models/ui_achievement_model.h"
#include "ui/models/ui_dashboard_layout.h"
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
static lv_obj_t *find_label(lv_obj_t *root,const char *text)
{
    if(lv_obj_check_type(root,&lv_label_class) && strcmp(lv_label_get_text(root),text)==0) return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i) {
        lv_obj_t *label=find_label(lv_obj_get_child(root,i),text); if(label) return label;
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
    lv_sysmon_hide_performance(display);
    ui_motion_init();
    ui_sound_init();
    ui_app_state_init();
    ui_home_data_model_init();
    ui_ride_history_model_init();
    /* Catalog boundaries and lifetime progress must be independent of the
     * number of detailed records retained in RAM. */
    ui_ride_history_totals_t totals = {0};
    for(size_t i = 0; i < UI_ACHIEVEMENT_COUNT; ++i) {
        ui_achievement_progress_t progress;
        assert(ui_achievement_evaluate(i, &totals, &progress));
        assert(!progress.unlocked && progress.progress_permille == 0U);
    }
    ui_achievement_progress_t progress;
    totals.total_distance_km = 99.999;
    assert(ui_achievement_evaluate(4, &totals, &progress));
    assert(!progress.unlocked && progress.progress_permille < 1000U);
    totals.total_distance_km = 100;
    assert(ui_achievement_evaluate(4, &totals, &progress));
    assert(progress.unlocked && progress.remaining == 0 && progress.progress_permille == 1000U);
    totals = (ui_ride_history_totals_t){.ride_count = 100, .total_distance_km = 5000,
        .total_ascent_m = 50000, .total_duration_seconds = 500U * 3600U};
    assert(ui_ride_history_model_set_totals(&totals));
    assert(ui_ride_history_model_count() == 0);
    ui_ride_history_totals_t persisted;
    ui_ride_history_model_totals(&persisted);
    for(size_t i = 0; i < UI_ACHIEVEMENT_COUNT; ++i) {
        assert(ui_achievement_evaluate(i, &persisted, &progress));
        assert(progress.unlocked && progress.progress_permille == 1000U);
    }
    assert(!ui_achievement_evaluate(UI_ACHIEVEMENT_COUNT, &totals, &progress));
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
    assert(!find_label(profile,"LV 12") && !find_label(profile,"ACTIVE"));
    assert(find_label(profile,"0 / 16"));
    assert(find_label(profile,"首骑完成"));
    assert(find_label(profile,"暂无记录"));
    assert(find_label(profile,"日期未校准"));
    assert(ui_profile_page_selected_index() == 0);
    assert(ui_profile_page_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(s_action == UI_PAGE_ACTION_PROFILE_REFRESH);
    assert(ui_profile_page_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_profile_page_selected_index() == 1);
    assert(ui_profile_page_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(s_action == UI_PAGE_ACTION_ACHIEVEMENT);
    assert(ui_profile_page_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_profile_page_selected_index() == 2);
    assert(ui_profile_page_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(s_action == UI_PAGE_ACTION_HISTORY);
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
    assert(ui_achievements_page_contains_scroll_point(16, UI_PAGE_CONTENT_BOTTOM - 1));
    assert(!ui_achievements_page_contains_scroll_point(16, UI_PAGE_CONTENT_BOTTOM));
    assert(ui_achievements_page_count() == UI_ACHIEVEMENT_COUNT);
    assert(ui_achievements_page_selected_index() == 0);
    for(int32_t index = 1; index < (int32_t)UI_ACHIEVEMENT_COUNT; index++) {
        assert(ui_achievements_page_handle_input(UI_INPUT_ENCODER_CW));
        assert(ui_achievements_page_selected_index() == index);
    }
    assert(ui_achievements_page_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_achievements_page_selected_index() == UI_ACHIEVEMENT_COUNT - 1);
    assert(ui_achievements_page_handle_input(UI_INPUT_ENCODER_PRESS));
    finish_animations();
    assert(ui_achievements_page_detail_visible());
    assert(!ui_achievements_page_contains_scroll_point(16, 260));
    assert(ui_achievements_page_handle_input(UI_INPUT_BACK));
    assert(!ui_achievements_page_detail_visible());

    ui_ride_record_t records[10] = {0};
    for(uint32_t index = 0U; index < 10U; index++) {
        records[index].id = index + 1U;
        records[index].distance_km = 55.0f;
        records[index].duration_seconds = 5U * 3600U;
        records[index].ascent_m = 250U;
        records[index].average_speed_kmh=11.0f;
    }
    assert(ui_ride_history_model_replace(records, 10U));
    const int64_t activity_now = 1750000000;
    ui_profile_page_set_activity_time(activity_now);
    ui_achievements_page_refresh();
    ui_profile_page_refresh();
    assert(find_label(profile,"8 / 16"));
    assert(find_label(profile,"千里骑士"));
    assert(find_label(profile,"还需 450.0 km"));
    assert(find_label(profile,"记录日期缺失"));
    /* Calendar activity is independent from lifetime totals: exclude old,
     * future and undated records, and preserve missing distance quality. */
    records[0].started_at_utc = activity_now - 86400;
    records[1].started_at_utc = activity_now - 30LL * 86400;
    records[2].started_at_utc = activity_now + 86400;
    for(unsigned i=0U;i<3U;++i)
        records[i].finished_at_utc=records[i].started_at_utc+records[i].duration_seconds;
    assert(ui_ride_history_model_replace(records, 10U));
    ui_profile_page_refresh();
    assert(find_label(profile,"1 次 · 55 km"));
    records[0].metric_flags = UI_REPORT_METRICS_KNOWN;
    assert(ui_ride_history_model_replace(records, 10U));
    ui_profile_page_refresh();
    assert(find_label(profile,"1 次 · -- km"));
    records[0].metric_flags = 0;
    for(unsigned i=0U;i<10U;++i) {
        records[i].started_at_utc=activity_now-(int64_t)i*86400;
        records[i].finished_at_utc=records[i].started_at_utc+records[i].duration_seconds;
    }
    assert(ui_ride_history_model_replace(records, 10U));
    ui_profile_page_refresh();
    assert(find_label(profile,"10 次 · 550 km"));
    /* No invisible legacy captions are created merely to satisfy tests. */
    assert(!find_label(profile,"上次平均速度  11.0 km/h"));
    assert(!find_label(profile,"个人主页"));
    ui_profile_page_set_preset(UI_DASHBOARD_CLIMB);
    assert(find_label(profile,"爬坡"));
    for(size_t i = 1; i < UI_ACHIEVEMENT_COUNT; ++i)
        assert(ui_achievements_page_handle_input(UI_INPUT_ENCODER_CCW));
    finish_animations();
    lv_refr_now(display);
    assert(!s_render_failed);
    save_snapshot_if_requested(achievements,
                               "UI_ACHIEVEMENTS_SNAPSHOT_PATH");
    assert(ui_achievements_page_handle_input(UI_INPUT_ENCODER_PRESS));
    finish_animations();
    save_snapshot_if_requested(achievements, "UI_ACHIEVEMENT_DETAIL_SNAPSHOT_PATH");
    ui_achievements_page_set_active(false);
    assert(!ui_achievements_page_detail_visible());
    lv_screen_load(profile); ui_profile_page_set_active(true); finish_animations();
    save_snapshot_if_requested(profile,"UI_PROFILE_POPULATED_SNAPSHOT_PATH");
    assert(!s_render_failed);
    totals=(ui_ride_history_totals_t){.ride_count=100,.total_distance_km=5000,
        .total_ascent_m=50000,.total_duration_seconds=500U*3600U};
    assert(ui_ride_history_model_set_totals(&totals)); ui_profile_page_refresh();
    assert(find_label(profile,"16 / 16"));
    assert(find_label(profile,"全部成就已达成"));
    save_snapshot_if_requested(profile,"UI_PROFILE_COMPLETE_SNAPSHOT_PATH");
    ui_ride_history_model_init();
    totals=(ui_ride_history_totals_t){.ride_count=20000,.total_distance_km=400000,
        .total_duration_seconds=20000U*12U*60U};
    assert(ui_ride_history_model_set_totals(&totals)); ui_profile_page_refresh();
    assert(ui_ride_history_model_count()==0);
    assert(find_label(profile,"20000"));
    assert(find_label(profile,"暂无可读活动"));
    totals.ride_count=UINT32_MAX;
    assert(ui_ride_history_model_set_totals(&totals)); ui_profile_page_refresh();
    lv_obj_update_layout(profile);
    lv_obj_t *count_label=find_label(profile,"4294967295");
    assert(count_label);
    lv_point_t text_size;
    lv_text_get_size(&text_size,lv_label_get_text(count_label),
        lv_obj_get_style_text_font(count_label,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
    assert(text_size.x<=lv_obj_get_width(count_label));
    lv_refr_now(display); assert(!s_render_failed);
    ui_profile_page_set_active(false);

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
