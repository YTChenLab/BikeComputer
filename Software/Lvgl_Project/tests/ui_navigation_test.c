#include "services/ui_navigation_service.h"
#include "ui/components/navigation/ui_nav_command_dock.h"
#include "ui/components/navigation/ui_nav_data_strip.h"
#include "ui/components/navigation/ui_nav_layout.h"
#include "ui/components/navigation/ui_nav_map.h"
#include "ui/core/ui_event_bus.h"
#include "ui/core/ui_theme.h"
#include "ui/core/ui_motion.h"
#include "ui/pages/navigation/ui_navigation.h"
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
static bool s_layer_failure;
static ui_event_t s_last_event;
static uint32_t s_event_count;

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

static void save_snapshot_if_requested(lv_obj_t *screen)
{
    const char *path = getenv("UI_NAV_SNAPSHOT_PATH");
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

static void capture_log(lv_log_level_t level, const char *message)
{
    (void)level;
    if(message != NULL &&
       (strstr(message, "No memory") != NULL ||
        strstr(message, "Allocating layer buffer failed") != NULL ||
        strstr(message, "glyph dsc. not found") != NULL))
        s_layer_failure = true;
}

static void capture_event(const ui_event_t *event, void *user_data)
{
    (void)user_data;
    s_last_event = *event;
    s_event_count++;
}

static void finish_animations(void)
{
    for(uint8_t step = 0; step < 24U; step++) {
        lv_tick_inc(25U);
        (void)lv_timer_handler();
    }
}

static lv_obj_t *make_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, TEST_W, TEST_H);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_screen_load(screen);
    return screen;
}

static nav_model_t test_model(void)
{
    nav_model_t model = {
        .speed = 27.0f,
        .distance = 12.6f,
        .remaining_ascent_m = 156U,
        .eta_hour = 11,
        .eta_min = 5,
        .latitude = 34.3417f,
        .longitude = 108.9399f,
        .heading = 48.0f,
        .zoom = 14,
        .direction = NAV_MANEUVER_LEFT,
        .direction_distance = 420U,
        .ui_state = NAV_UI_CRUISE,
        .road_name = "环城南路"
    };
    return model;
}

static void test_map_controls(lv_obj_t *screen)
{
    ui_nav_map_t map = {0};
    ui_nav_map_create(&map, screen, UI_NAV_PAGE_MARGIN, UI_NAV_MAP_TOP);
    lv_obj_update_layout(map.root);
    assert(map.root != NULL);
    assert(lv_obj_get_height(map.root) == UI_NAV_MAP_HEIGHT);
    assert(map.zoom == 14U);
    assert(!map.focus_visible);
    ui_nav_map_handle_input(&map, 1, false);
    assert(map.zoom == 15U);
    assert(map.focused_control == UI_NAV_FOCUS_ZOOM_IN);
    ui_nav_map_handle_input(&map, -1, false);
    assert(map.zoom == 14U);
    assert(map.focused_control == UI_NAV_FOCUS_ZOOM_OUT);
    ui_nav_map_handle_input(&map, 0, true);
    assert(map.focused_control == UI_NAV_FOCUS_LOCATE);
    finish_animations();
    nav_model_t live = {.position_valid = true, .latitude = 34.3416, .longitude = 108.9398,
        .heading = 359, .zoom = 14, .map_geometry_valid = true,
        .map_origin_latitude = 34.3416, .map_origin_longitude = 108.9398, .meters_per_pixel = 5};
    ui_nav_map_apply_model(&map, &live);
    nav_map_zoom(&map, 16, false);
    live.heading = 1;
    ui_nav_map_apply_model(&map, &live);
    assert(map.zoom == 16); /* Live updates preserve the user's zoom. */
    assert(map.origin_valid && map.meters_per_pixel == 5);
    nav_map_update_position(&map, 34.3416f, 108.9398f, -721, false);
    assert(map.heading >= 0 && map.heading < 360);
    finish_animations();
    lv_obj_delete(map.root);
}

static void test_data_strip(lv_obj_t *screen)
{
    ui_nav_data_strip_t strip = {0};
    nav_model_t model = test_model();
    ui_nav_data_strip_create(&strip, screen);
    ui_nav_data_strip_update(&strip, &model);
    lv_obj_update_layout(strip.root);
    assert(lv_obj_get_x(strip.root) == UI_NAV_PAGE_MARGIN);
    assert(lv_obj_get_y(strip.root) == UI_NAV_DATA_TOP);
    assert(lv_obj_get_width(strip.root) == UI_NAV_DATA_WIDTH);
    assert(lv_obj_get_height(strip.root) == UI_NAV_DATA_HEIGHT);
    assert(strcmp(lv_label_get_text(strip.values[0]), "27.0") == 0);
    assert(strcmp(lv_label_get_text(strip.values[1]), "12.6") == 0);
    assert(strcmp(lv_label_get_text(strip.values[2]), "11:05") == 0);
    assert(strcmp(lv_label_get_text(strip.titles[1]), "剩余里程") == 0);
    for(uint8_t index = 0; index < 3U; index++) {
        lv_obj_update_layout(strip.value_rows[index]);
        assert(lv_obj_get_x(strip.values[index]) >= 0);
        assert(lv_obj_get_x(strip.values[index]) +
               lv_obj_get_width(strip.values[index]) <=
               lv_obj_get_width(strip.value_rows[index]));
        if(!lv_obj_has_flag(strip.units[index], LV_OBJ_FLAG_HIDDEN)) {
            assert(lv_obj_get_x(strip.units[index]) +
                   lv_obj_get_width(strip.units[index]) <=
                   lv_obj_get_width(strip.value_rows[index]));
        }
    }
    assert(lv_obj_get_x(strip.groups[2]) + lv_obj_get_width(strip.groups[2]) <=
           UI_NAV_DATA_WIDTH);
    lv_obj_delete(strip.root);
}

static void test_command_dock_states(lv_obj_t *screen)
{
    ui_nav_command_dock_t dock = {0};
    nav_model_t model = test_model();
    ui_nav_command_dock_create(&dock, screen);
    ui_nav_command_dock_update(&dock, &model, false);
    lv_obj_update_layout(dock.root);
    assert(lv_obj_get_x(dock.root) == UI_NAV_DOCK_X);
    assert(lv_obj_get_y(dock.root) == UI_NAV_DOCK_Y);
    assert(lv_obj_get_width(dock.root) == UI_NAV_DOCK_WIDTH);
    assert(lv_obj_get_height(dock.root) == UI_NAV_DOCK_HEIGHT);
    assert(UI_NAV_DOCK_BOTTOM <= TEST_H - 10);
    assert(strcmp(lv_label_get_text(dock.distance_value), "420") == 0);
    assert(strcmp(lv_label_get_text(dock.distance_unit), "m") == 0);
    assert(strcmp(lv_label_get_text(dock.action), "左转") == 0);
    assert(strcmp(lv_label_get_text(dock.road), "环城南路") == 0);
    assert(lv_obj_get_parent(dock.action) == dock.distance_row);
    assert(lv_obj_get_style_text_font(dock.action, 0) == UI_FONT_NAV_ACTION);

    model.direction = NAV_MANEUVER_STRAIGHT;
    model.direction_distance = 1200U;
    ui_nav_command_dock_update(&dock, &model, false);
    assert(strcmp(lv_label_get_text(dock.distance_value), "1.2") == 0);
    assert(strcmp(lv_label_get_text(dock.distance_unit), "km") == 0);
    assert(strcmp(lv_label_get_text(dock.action), "直行") == 0);

    model.ui_state = NAV_UI_IMMINENT;
    model.direction = NAV_MANEUVER_RIGHT;
    model.direction_distance = 35U;
    ui_nav_command_dock_update(&dock, &model, true);
    assert(lv_obj_has_flag(dock.prefix, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(dock.road, LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(dock.distance_row, LV_OBJ_FLAG_HIDDEN));
    ui_motion_set_reduced(true);
    ui_nav_command_dock_update(&dock, &model, true);
    assert(dock.icon_scale == 100);
    assert(lv_anim_count_running() == 0U);
    ui_motion_set_reduced(false);

    model.ui_state = NAV_UI_NOW;
    ui_nav_command_dock_update(&dock, &model, false);
    assert(strcmp(lv_label_get_text(dock.headline), "现在") == 0);
    assert(!lv_obj_has_flag(dock.headline, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(dock.distance_row, LV_OBJ_FLAG_HIDDEN));

    model.ui_state = NAV_UI_REROUTING;
    ui_nav_command_dock_update(&dock, &model, false);
    assert(strcmp(lv_label_get_text(dock.headline), "已偏离路线") == 0);
    assert(strcmp(lv_label_get_text(dock.special_detail),
                  "正在重新规划…") == 0);

    model.ui_state = NAV_UI_GPS_WEAK;
    ui_nav_command_dock_update(&dock, &model, false);
    assert(dock.previous_state == NAV_UI_REROUTING);
    assert(strcmp(lv_label_get_text(dock.headline), "GPS 信号较弱") == 0);

    model.ui_state = NAV_UI_ARRIVED;
    model.direction = NAV_MANEUVER_ARRIVE;
    ui_nav_command_dock_update(&dock, &model, false);
    assert(strcmp(lv_label_get_text(dock.headline), "已到达目的地") == 0);
    assert(strcmp(lv_label_get_text(dock.special_detail),
                  "本次导航完成") == 0);
    finish_animations();
    lv_refr_now(lv_display_get_default());
    assert(!s_layer_failure);
    lv_obj_delete(dock.root);
}

static void test_navigation_page(lv_obj_t *screen)
{
    ui_event_bus_init();
    ui_navigation_service_init();
    assert(ui_event_bus_subscribe(UI_EVENT_MASK(UI_EVENT_NAVIGATION_TURN),
                                  capture_event, NULL) > 0);
    ui_navigation_create(screen);
    ui_navigation_set_active(true);
    lv_obj_update_layout(screen);
    assert(lv_obj_get_child_count(screen) == 3U);

    lv_obj_t *map_root = lv_obj_get_child(screen, 0);
    lv_obj_t *strip_root = lv_obj_get_child(screen, 1);
    lv_obj_t *dock_root = lv_obj_get_child(screen, 2);
    assert(lv_obj_get_y(map_root) == UI_NAV_MAP_TOP);
    assert(lv_obj_get_height(map_root) == UI_NAV_MAP_HEIGHT);
    assert(lv_obj_get_y(strip_root) == UI_NAV_DATA_TOP);
    assert(lv_obj_get_height(strip_root) == UI_NAV_DATA_HEIGHT);
    assert(lv_obj_get_y(dock_root) == UI_NAV_DOCK_Y);
    assert(lv_obj_get_height(dock_root) == UI_NAV_DOCK_HEIGHT);

    nav_model_t turn = test_model();
    ui_navigation_service_submit(&turn);
    assert(ui_navigation_service_process());
    assert(ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX) == 0U);
    assert(s_event_count == 0U);

    ui_navigation_set_active(false);
    turn.direction = NAV_MANEUVER_RIGHT;
    ui_navigation_service_submit(&turn);
    assert(ui_navigation_service_process());
    assert(ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX) == 1U);
    assert(s_event_count == 1U);
    assert(s_last_event.type == UI_EVENT_NAVIGATION_TURN);
    assert(s_last_event.value == UI_NAVIGATION_TURN_RIGHT);
    assert(strstr(s_last_event.message, "420 m") != NULL);
    finish_animations();
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

    lv_obj_t *component_screen = make_screen();
    test_map_controls(component_screen);
    test_data_strip(component_screen);
    test_command_dock_states(component_screen);

    lv_obj_t *page_screen = make_screen();
    lv_obj_delete(component_screen);
    test_navigation_page(page_screen);
    save_snapshot_if_requested(page_screen);

    lv_display_delete(display);
    puts("ui_navigation_test: PASS");
    return 0;
}
