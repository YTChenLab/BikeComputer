#include "ui/core/ui_motion.h"
#include "ui/core/ui_router.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>

#define TEST_DISPLAY_W 480
#define TEST_DISPLAY_H 854

static uint8_t s_draw_buffer[TEST_DISPLAY_W * 20 * 2];
static uint8_t s_enter[UI_ROUTE_COUNT];
static uint8_t s_leave[UI_ROUTE_COUNT];
static uint8_t s_destroy[UI_ROUTE_COUNT];
static uint8_t s_create_count;

static lv_obj_t *lazy_create(void *user_data)
{
    (void)user_data;
    s_create_count++;
    return lv_obj_create(NULL);
}

static void on_enter(ui_route_id_t route, lv_obj_t *screen, void *user_data)
{
    (void)user_data;
    assert(screen != NULL);
    s_enter[route]++;
}

static void on_leave(ui_route_id_t route, lv_obj_t *screen, void *user_data)
{
    (void)user_data;
    assert(screen != NULL);
    s_leave[route]++;
}

static void on_destroy(ui_route_id_t route, lv_obj_t *screen, void *user_data)
{
    (void)user_data;
    assert(screen != NULL);
    s_destroy[route]++;
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(TEST_DISPLAY_W, TEST_DISPLAY_H);
    assert(display != NULL);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, s_draw_buffer, NULL, sizeof(s_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_obj_t *dashboard_screen = lv_obj_create(NULL);
    const ui_router_page_t dashboard = {
        .screen = dashboard_screen,
        .on_enter = on_enter,
        .on_leave = on_leave,
        .on_destroy = on_destroy
    };
    const ui_router_page_t navigation = {
        .create = lazy_create,
        .on_enter = on_enter,
        .on_leave = on_leave,
        .on_destroy = on_destroy
    };

    ui_motion_init();
    ui_router_init(UI_ROUTE_RIDE_DASHBOARD);
    assert(ui_router_register_page(UI_ROUTE_RIDE_DASHBOARD, &dashboard));
    assert(ui_router_register_page(UI_ROUTE_NAVIGATION, &navigation));
    assert(ui_router_is_registered(UI_ROUTE_NAVIGATION));
    assert(ui_router_screen(UI_ROUTE_NAVIGATION) == NULL);

    assert(ui_router_replace(UI_ROUTE_RIDE_DASHBOARD, UI_MOTION_PAGE_NONE));
    assert(s_enter[UI_ROUTE_RIDE_DASHBOARD] == 1);
    assert(ui_router_navigate(UI_ROUTE_NAVIGATION, UI_MOTION_PAGE_NONE));
    assert(s_create_count == 1);
    assert(s_leave[UI_ROUTE_RIDE_DASHBOARD] == 1);
    assert(s_enter[UI_ROUTE_NAVIGATION] == 1);
    assert(ui_router_screen(UI_ROUTE_NAVIGATION) != NULL);

    assert(ui_router_back());
    assert(s_leave[UI_ROUTE_NAVIGATION] == 1);
    assert(s_enter[UI_ROUTE_RIDE_DASHBOARD] == 2);

    ui_router_deinit();
    assert(s_leave[UI_ROUTE_RIDE_DASHBOARD] == 2);
    assert(s_destroy[UI_ROUTE_RIDE_DASHBOARD] == 1);
    assert(s_destroy[UI_ROUTE_NAVIGATION] == 1);

    lv_display_delete(display);
    puts("ui_router_lifecycle_test: PASS");
    return 0;
}

