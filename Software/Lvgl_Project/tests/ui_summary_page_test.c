#include "ui/pages/ui_summary_page.h"
#include "ui/pages/ui_page_common.h"
#include "ui/core/ui_app_state.h"
#include "ui/core/ui_motion.h"
#include "ui/core/ui_sound.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>
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
        strstr(message, "Allocating layer buffer failed") != NULL))
        s_memory_failed = true;
}

static void action(ui_page_action_t value, void *user_data)
{
    (void)user_data;
    s_action = value;
}

static void assert_tree_inside(lv_obj_t *object)
{
    if(object == NULL || lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) return;
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    assert(area.x1 >= 0 && area.y1 >= 0);
    assert(area.x2 < TEST_W && area.y2 < TEST_H);
    uint32_t count = lv_obj_get_child_count(object);
    for(uint32_t index = 0; index < count; index++)
        assert_tree_inside(lv_obj_get_child(object, index));
}

static uint32_t count_top_level_geometry(lv_obj_t *screen, int32_t y,
                                         int32_t width, int32_t height)
{
    uint32_t matches = 0U;
    uint32_t count = lv_obj_get_child_count(screen);
    for(uint32_t index = 0; index < count; index++) {
        lv_obj_t *child = lv_obj_get_child(screen, index);
        if(lv_obj_get_y(child) == y &&
           lv_obj_get_width(child) == width &&
           lv_obj_get_height(child) == height)
            matches++;
    }
    return matches;
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
        .average_speed_kmh = 33.4f,
        .maximum_speed_kmh = 48.7f,
        .ascent_m = 1268U,
        .average_heart_rate = 148U,
        .average_power = 201U,
        .average_cadence = 85U,
        .saved = true,
        .version = 1U
    };
    ui_summary_page_set_data(&summary);
    ui_summary_page_set_active(true);
    lv_tick_inc(1000U);
    lv_anim_refr_now();
    lv_obj_update_layout(screen);
    lv_refr_now(display);
    assert(!s_memory_failed);
    assert_tree_inside(screen);
    assert(count_top_level_geometry(screen, 158, 448, 142) == 1U);
    assert(count_top_level_geometry(screen, 312, 216, 74) == 2U);
    assert(count_top_level_geometry(screen, 392, 216, 74) == 2U);
    assert(count_top_level_geometry(screen, 472, 216, 74) == 2U);
    assert(count_top_level_geometry(screen, 552, 216, 74) == 2U);
    assert(count_top_level_geometry(screen, 712, 448, 72) == 1U);
    assert(ui_summary_page_handle_input(UI_INPUT_ENCODER_PRESS));
    assert(s_action == UI_PAGE_ACTION_HOME);

    ui_page_status_registry_deinit();
    lv_display_delete(display);
    puts("ui_summary_page_test: PASS");
    return 0;
}
