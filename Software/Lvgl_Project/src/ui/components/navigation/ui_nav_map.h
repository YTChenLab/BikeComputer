#ifndef UI_NAV_MAP_H
#define UI_NAV_MAP_H

#include "lvgl.h"
#include "ui_nav_control.h"
#include "ui_nav_model.h"

#include <stdbool.h>

#define UI_NAV_ROUTE_POINT_MAX 16

typedef enum
{
    UI_NAV_FOCUS_COMPASS = 0,
    UI_NAV_FOCUS_ZOOM_IN,
    UI_NAV_FOCUS_ZOOM_OUT,
    UI_NAV_FOCUS_LOCATE,
    UI_NAV_FOCUS_COUNT
} ui_nav_focus_t;

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *map_content;
    lv_obj_t *map_tiles;
    lv_obj_t *road_layer;
    lv_obj_t *route_layer;
    lv_obj_t *position_marker;
    lv_obj_t *marker_center;
    lv_obj_t *map_hud;
    lv_obj_t *scale_root;
    lv_obj_t *scale_label;
    lv_obj_t *compass_n;
    ui_nav_control_t compass;
    ui_nav_control_t zoom_in;
    ui_nav_control_t zoom_out;
    ui_nav_control_t locate;
    lv_point_precise_t route_points[UI_NAV_ROUTE_POINT_MAX];
    uint8_t route_point_count;
    int32_t target_x;
    int32_t target_y;
    int32_t render_x;
    int32_t render_y;
    int32_t render_scale;
    int32_t render_rotation;
    lv_point_t last_drag_vector;
    nav_state_t state;
    uint8_t zoom;
    float heading;
    bool north_up;
    ui_nav_focus_t focused_control;
    bool focus_visible;
    bool model_initialized, demo, origin_valid;
    double origin_latitude, origin_longitude;
    float meters_per_pixel;
} ui_nav_map_t;

void ui_nav_map_create(ui_nav_map_t *map, lv_obj_t *parent, int32_t x, int32_t y);
bool ui_nav_map_contains_point(const ui_nav_map_t *map, int32_t x, int32_t y);
void ui_nav_map_apply_model(ui_nav_map_t *map, const nav_model_t *model);
void ui_nav_map_handle_input(ui_nav_map_t *map, int32_t zoom_delta, bool locate);
void ui_nav_map_set_focus(ui_nav_map_t *map, ui_nav_focus_t focus,
                          bool visible);

void nav_map_set_tile(ui_nav_map_t *map, uint16_t tile_id);
void nav_map_move(ui_nav_map_t *map, int32_t delta_x, int32_t delta_y, bool animated);
void nav_map_zoom(ui_nav_map_t *map, uint8_t zoom, bool animated);
void nav_map_update_route(ui_nav_map_t *map, const lv_point_precise_t *points,
                          uint8_t point_count);
void nav_map_update_position(ui_nav_map_t *map, float latitude, float longitude,
                             float heading, bool animated);

#endif /* UI_NAV_MAP_H */
