#ifndef UI_NAV_MAP_H
#define UI_NAV_MAP_H

#include "lvgl.h"
#include "ui_nav_control.h"
#include "ui_nav_model.h"
#include "services/ui_nav_map_provider.h"

#include <stdbool.h>

#define UI_NAV_ROUTE_POINT_MAX UI_NAV_WINDOW_POINT_MAX

typedef enum
{
    UI_NAV_FOCUS_COMPASS = 0,
    UI_NAV_FOCUS_ZOOM_IN,
    UI_NAV_FOCUS_ZOOM_OUT,
    UI_NAV_FOCUS_LOCATE,
    UI_NAV_FOCUS_PICK,
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
    lv_obj_t *pick_cursor;
    lv_image_dsc_t raster_image;
    bool raster_valid,pick_mode,pick_claimed,pick_point_valid,pointer_down;
    ui_nav_point_t pick_point;
    void (*pick_callback)(ui_nav_point_t point,void *context);
    void *pick_context;
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
    lv_point_t drag_point;
    struct {lv_point_t point;uint32_t at;} drag_samples[12];
    uint8_t drag_sample_count;
    bool drag_started;
    nav_state_t state;
    uint8_t zoom;
    float heading;
    bool north_up;
    ui_nav_focus_t focused_control;
    bool focus_visible;
    bool model_initialized, demo, origin_valid;
    double origin_latitude, origin_longitude;
    float meters_per_pixel;
    lv_point_precise_t road_a[UI_NAV_MAP_ROAD_MAX],road_b[UI_NAV_MAP_ROAD_MAX];
    uint8_t road_level[UI_NAV_MAP_ROAD_MAX],road_count,coordinate_system;
    bool provider_ready,strict_geometry,heading_valid,read_only;
    uint32_t camera_step,camera_route,cruise_hold_until;
    int32_t auto_scale;
    ui_nav_map_frame_t detail_frame;
    bool position_seen,off_route;
    int32_t safe_height,drag_x,drag_y;
    float zoom_bias;
} ui_nav_map_t;

void ui_nav_map_create(ui_nav_map_t *map, lv_obj_t *parent, int32_t x, int32_t y);
bool ui_nav_map_contains_point(const ui_nav_map_t *map, int32_t x, int32_t y);
void ui_nav_map_apply_model(ui_nav_map_t *map, const nav_model_t *model);
void ui_nav_map_apply_frame(ui_nav_map_t *map,const ui_nav_map_frame_t *frame);
void ui_nav_map_camera(ui_nav_map_t *map,const nav_model_t *model,uint32_t now_ms);
ui_nav_point_t ui_nav_map_view_center(const ui_nav_map_t *map);
/* Expand provider coverage for camera scale/rotation at request projection resolution. */
void ui_nav_map_request_coverage(const ui_nav_map_t *map,ui_nav_map_request_t *request);
void ui_nav_map_set_safe_height(ui_nav_map_t *map,int32_t height);
void ui_nav_map_resize_view(ui_nav_map_t *map,int32_t y,int32_t height);
void ui_nav_map_center_point(ui_nav_map_t *map,ui_nav_point_t point);
void ui_nav_map_set_overview(ui_nav_map_t *map,bool overview);
void ui_nav_map_set_north_up(ui_nav_map_t *map,bool north_up);
bool ui_nav_map_is_browsing(const ui_nav_map_t *map);
void ui_nav_map_handle_input(ui_nav_map_t *map, int32_t zoom_delta, bool locate);
void ui_nav_map_activate_focus(ui_nav_map_t *map);
void ui_nav_map_set_focus(ui_nav_map_t *map, ui_nav_focus_t focus,
                          bool visible);
bool ui_nav_map_point_at(const ui_nav_map_t *map,int32_t x,int32_t y,ui_nav_point_t *point);
bool ui_nav_map_select_at(ui_nav_map_t *map,int32_t x,int32_t y);
void ui_nav_map_set_pick_mode(ui_nav_map_t *map,bool enabled);

void nav_map_set_tile(ui_nav_map_t *map, uint16_t tile_id);
void nav_map_move(ui_nav_map_t *map, int32_t delta_x, int32_t delta_y, bool animated);
void nav_map_zoom(ui_nav_map_t *map, uint8_t zoom, bool animated);
void nav_map_update_route(ui_nav_map_t *map, const lv_point_precise_t *points,
                          uint8_t point_count);
void nav_map_update_position(ui_nav_map_t *map, double latitude, double longitude,
                             float heading, bool animated);

#endif /* UI_NAV_MAP_H */
