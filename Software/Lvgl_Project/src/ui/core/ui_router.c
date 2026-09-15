#include "ui_router.h"

#include <string.h>

#define UI_ROUTER_HISTORY_MAX 12

typedef struct
{
    lv_obj_t *screens[UI_ROUTE_COUNT];
    ui_route_id_t history[UI_ROUTER_HISTORY_MAX];
    ui_route_id_t current;
    ui_route_id_t fallback;
    uint8_t history_count;
    ui_router_changed_cb_t changed;
    void *changed_user_data;
} ui_router_context_t;

static ui_router_context_t s_router;

static bool valid_route(ui_route_id_t route)
{
    return route < UI_ROUTE_COUNT;
}

static bool should_remember(ui_route_id_t route)
{
    return route != UI_ROUTE_NONE && route != UI_ROUTE_BOOT &&
           route != UI_ROUTE_POWER_OFF;
}

static void history_push(ui_route_id_t route)
{
    if(!should_remember(route)) return;
    if(s_router.history_count > 0 &&
       s_router.history[s_router.history_count - 1] == route) return;
    if(s_router.history_count == UI_ROUTER_HISTORY_MAX) {
        memmove(&s_router.history[0], &s_router.history[1],
                sizeof(s_router.history[0]) * (UI_ROUTER_HISTORY_MAX - 1));
        s_router.history_count--;
    }
    s_router.history[s_router.history_count++] = route;
}

static bool load_route(ui_route_id_t route, ui_motion_page_direction_t direction,
                       bool remember_current)
{
    if(!valid_route(route) || s_router.screens[route] == NULL) return false;
    if(route == s_router.current) return true;
    ui_route_id_t previous = s_router.current;
    if(remember_current) history_push(previous);
    s_router.current = route;
    ui_motion_page_load(s_router.screens[route], direction);
    if(s_router.changed != NULL) {
        s_router.changed(previous, route, s_router.changed_user_data);
    }
    return true;
}

void ui_router_init(ui_route_id_t fallback)
{
    memset(&s_router, 0, sizeof(s_router));
    s_router.current = UI_ROUTE_NONE;
    s_router.fallback = valid_route(fallback) ? fallback : UI_ROUTE_RIDE_DASHBOARD;
}

bool ui_router_register(ui_route_id_t route, lv_obj_t *screen)
{
    if(!valid_route(route) || screen == NULL) return false;
    s_router.screens[route] = screen;
    return true;
}

bool ui_router_is_registered(ui_route_id_t route)
{
    return valid_route(route) && s_router.screens[route] != NULL;
}

bool ui_router_navigate(ui_route_id_t route, ui_motion_page_direction_t direction)
{
    return load_route(route, direction, true);
}

bool ui_router_replace(ui_route_id_t route, ui_motion_page_direction_t direction)
{
    return load_route(route, direction, false);
}

bool ui_router_back(void)
{
    while(s_router.history_count > 0) {
        ui_route_id_t route = s_router.history[--s_router.history_count];
        if(route != s_router.current && ui_router_is_registered(route)) {
            return load_route(route, UI_MOTION_PAGE_NONE, false);
        }
    }
    if(s_router.current != s_router.fallback &&
       ui_router_is_registered(s_router.fallback)) {
        return load_route(s_router.fallback, UI_MOTION_PAGE_NONE, false);
    }
    return false;
}

void ui_router_clear_history(void)
{
    s_router.history_count = 0;
}

ui_route_id_t ui_router_current(void)
{
    return s_router.current;
}

uint8_t ui_router_history_depth(void)
{
    return s_router.history_count;
}

lv_obj_t *ui_router_screen(ui_route_id_t route)
{
    return valid_route(route) ? s_router.screens[route] : NULL;
}

void ui_router_set_changed_callback(ui_router_changed_cb_t callback, void *user_data)
{
    s_router.changed = callback;
    s_router.changed_user_data = user_data;
}
