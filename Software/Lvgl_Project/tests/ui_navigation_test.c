#include "services/ui_navigation_service.h"
#include "ui/components/navigation/ui_nav_layout.h"
#include "ui/components/navigation/ui_nav_map.h"
#include "ui/core/ui_event_bus.h"
#include "ui/pages/navigation/ui_navigation.h"
#include "lvgl.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TEST_W 480
#define TEST_H 854

static uint8_t s_draw_buffer[TEST_W * 20 * 2];
static bool s_layer_failure;
static ui_event_t s_last_event;
static uint32_t s_event_count;

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

static void capture_event(const ui_event_t *event, void *user_data)
{
    (void)user_data;
    s_last_event = *event;
    s_event_count++;
}

static void finish_animations(void)
{
    for(uint8_t step = 0; step < 20U; step++) {
        lv_tick_inc(25U);
        (void)lv_timer_handler();
    }
}

static void test_map_controls(lv_obj_t *screen)
{
    ui_nav_map_t map = {0};
    ui_nav_map_create(&map, screen, UI_NAV_PAGE_MARGIN, UI_NAV_MAP_TOP);
    assert(map.root != NULL);
    assert(map.zoom == 14U);
    assert(!map.focus_visible);

    ui_nav_map_handle_input(&map, 1, false);
    assert(map.zoom == 15U);
    assert(map.focus_visible);
    assert(map.focused_control == UI_NAV_FOCUS_ZOOM_IN);
    assert(map.zoom_in.focused);
    assert(!map.zoom_out.focused);

    ui_nav_map_handle_input(&map, -1, false);
    assert(map.zoom == 14U);
    assert(map.focused_control == UI_NAV_FOCUS_ZOOM_OUT);
    assert(map.zoom_out.focused);

    ui_nav_map_handle_input(&map, 0, true);
    assert(map.focused_control == UI_NAV_FOCUS_LOCATE);
    assert(map.locate.focused);
    finish_animations();
    lv_obj_delete(map.root);
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
    assert(lv_obj_get_child_count(screen) == 2U);

    lv_obj_t *map_root = lv_obj_get_child(screen, 0);
    lv_obj_t *grid = lv_obj_get_child(screen, 1);
    assert(map_root != NULL && grid != NULL);
    assert(lv_obj_get_x(map_root) == UI_NAV_PAGE_MARGIN);
    assert(lv_obj_get_y(map_root) == UI_NAV_MAP_TOP);
    assert(lv_obj_get_width(map_root) == UI_NAV_MAP_WIDTH);
    assert(lv_obj_get_height(map_root) == UI_NAV_MAP_HEIGHT);
    assert(lv_obj_get_x(grid) == UI_NAV_PAGE_MARGIN);
    assert(lv_obj_get_y(grid) == UI_NAV_GRID_TOP);
    assert(lv_obj_get_width(grid) == UI_NAV_GRID_WIDTH);
    assert(lv_obj_get_height(grid) == UI_NAV_GRID_HEIGHT);
    assert(lv_obj_get_child_count(grid) == 4U);

    for(uint8_t index = 0; index < 4U; index++) {
        lv_obj_t *card = lv_obj_get_child(grid, index);
        assert(card != NULL);
        assert(lv_obj_get_width(card) == UI_NAV_CARD_WIDTH);
        assert(lv_obj_get_height(card) == UI_NAV_CARD_HEIGHT);
        assert(lv_obj_get_x(card) >= 0 && lv_obj_get_y(card) >= 0);
        assert(lv_obj_get_x(card) + lv_obj_get_width(card) <=
               UI_NAV_GRID_WIDTH);
        assert(lv_obj_get_y(card) + lv_obj_get_height(card) <=
               UI_NAV_GRID_HEIGHT);
    }

    nav_model_t turn = {
        .speed = 27.4f,
        .distance = 52.3f,
        .eta_hour = 11,
        .eta_min = 8,
        .latitude = 34.3417f,
        .longitude = 108.9399f,
        .heading = 48.0f,
        .zoom = 14,
        .direction = NAV_DIRECTION_LEFT,
        .direction_distance = 180U
    };
    ui_navigation_service_submit(&turn);
    assert(ui_navigation_service_process());
    assert(ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX) == 1U);
    assert(s_event_count == 1U);
    assert(s_last_event.type == UI_EVENT_NAVIGATION_TURN);
    assert(s_last_event.priority == UI_EVENT_PRIORITY_P1_RIDING_SAFETY);
    assert(strstr(s_last_event.message, "180 m") != NULL);

    finish_animations();
    lv_refr_now(lv_display_get_default());
    assert(!s_layer_failure);
    ui_navigation_set_active(false);
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

    lv_obj_t *map_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(map_screen);
    lv_obj_set_size(map_screen, TEST_W, TEST_H);
    lv_screen_load(map_screen);
    test_map_controls(map_screen);

    lv_obj_t *page_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(page_screen);
    lv_obj_set_size(page_screen, TEST_W, TEST_H);
    lv_screen_load(page_screen);
    lv_obj_delete(map_screen);
    test_navigation_page(page_screen);

    lv_display_delete(display);
    puts("ui_navigation_test: PASS");
    return 0;
}
