#ifndef UI_ROUTER_H
#define UI_ROUTER_H

#include "lvgl.h"
#include "ui_motion.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UI_ROUTE_BOOT = 0,
    UI_ROUTE_HOME,
    UI_ROUTE_RIDE_DASHBOARD,
    UI_ROUTE_NAVIGATION,
    UI_ROUTE_RIDE_SUMMARY,
    UI_ROUTE_HISTORY,
    UI_ROUTE_RIDE_DETAIL,
    UI_ROUTE_SETTINGS,
    UI_ROUTE_PROFILE,
    UI_ROUTE_ACHIEVEMENTS,
    UI_ROUTE_SYSTEM,
    UI_ROUTE_DEVICES,
    UI_ROUTE_SAVED_ROUTES,
    UI_ROUTE_POWER,
    UI_ROUTE_POWER_OFF,
    UI_ROUTE_COUNT,
    UI_ROUTE_NONE = 0xFF
} ui_route_id_t;

typedef void (*ui_router_changed_cb_t)(ui_route_id_t previous,
                                       ui_route_id_t current,
                                       void *user_data);

typedef lv_obj_t *(*ui_router_create_cb_t)(void *user_data);
typedef void (*ui_router_lifecycle_cb_t)(ui_route_id_t route,
                                         lv_obj_t *screen,
                                         void *user_data);

typedef struct
{
    lv_obj_t *screen;
    ui_router_create_cb_t create;
    ui_router_lifecycle_cb_t on_enter;
    ui_router_lifecycle_cb_t on_leave;
    ui_router_lifecycle_cb_t on_destroy;
    void *user_data;
} ui_router_page_t;

void ui_router_init(ui_route_id_t fallback);
bool ui_router_register(ui_route_id_t route, lv_obj_t *screen);
bool ui_router_register_page(ui_route_id_t route,
                             const ui_router_page_t *page);
bool ui_router_is_registered(ui_route_id_t route);
bool ui_router_navigate(ui_route_id_t route, ui_motion_page_direction_t direction);
bool ui_router_replace(ui_route_id_t route, ui_motion_page_direction_t direction);
bool ui_router_back(void);
void ui_router_clear_history(void);
ui_route_id_t ui_router_current(void);
uint8_t ui_router_history_depth(void);
lv_obj_t *ui_router_screen(ui_route_id_t route);
void ui_router_set_changed_callback(ui_router_changed_cb_t callback, void *user_data);
void ui_router_deinit(void);

#endif /* UI_ROUTER_H */
