#include "ui_router.h"

#include <string.h>

#define UI_ROUTER_HISTORY_MAX 12

typedef struct
{
    ui_router_page_t pages[UI_ROUTE_COUNT];
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

static lv_obj_t *ensure_screen(ui_route_id_t route)
{
    if(!valid_route(route)) return NULL;
    ui_router_page_t *page = &s_router.pages[route];
    if(page->screen == NULL && page->create != NULL)
        page->screen = page->create(page->user_data);
    return page->screen;
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
    lv_obj_t *screen = ensure_screen(route);
    if(screen == NULL) return false;
    if(route == s_router.current) return true;
    ui_route_id_t previous = s_router.current;
    if(remember_current) history_push(previous);

    if(valid_route(previous)) {
        ui_router_page_t *previous_page = &s_router.pages[previous];
        if(previous_page->on_leave != NULL) {
            previous_page->on_leave(previous, previous_page->screen,
                                    previous_page->user_data);
        }
    }

    s_router.current = route;
    ui_motion_page_load(screen, direction);

    ui_router_page_t *page = &s_router.pages[route];
    if(page->on_enter != NULL)
        page->on_enter(route, screen, page->user_data);

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
    s_router.pages[route].screen = screen;
    return true;
}

bool ui_router_register_page(ui_route_id_t route,
                             const ui_router_page_t *page)
{
    if(!valid_route(route) || page == NULL ||
       (page->screen == NULL && page->create == NULL)) return false;
    s_router.pages[route] = *page;
    return true;
}

bool ui_router_is_registered(ui_route_id_t route)
{
    return valid_route(route) &&
           (s_router.pages[route].screen != NULL ||
            s_router.pages[route].create != NULL);
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
    return valid_route(route) ? s_router.pages[route].screen : NULL;
}

void ui_router_deinit(void)
{
    if(valid_route(s_router.current)) {
        ui_router_page_t *current = &s_router.pages[s_router.current];
        if(current->on_leave != NULL) {
            current->on_leave(s_router.current, current->screen,
                              current->user_data);
        }
    }

    for(uint8_t route = 0; route < UI_ROUTE_COUNT; route++) {
        ui_router_page_t *page = &s_router.pages[route];
        if(page->screen != NULL && page->on_destroy != NULL) {
            page->on_destroy((ui_route_id_t)route, page->screen,
                             page->user_data);
        }
    }
    memset(&s_router, 0, sizeof(s_router));
    s_router.current = UI_ROUTE_NONE;
}

void ui_router_set_changed_callback(ui_router_changed_cb_t callback, void *user_data)
{
    s_router.changed = callback;
    s_router.changed_user_data = user_data;
}
