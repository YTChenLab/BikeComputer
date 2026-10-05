#ifndef UI_NAV_MAP_PROVIDER_H
#define UI_NAV_MAP_PROVIDER_H
#include "ui_nav_session.h"
#define UI_NAV_MAP_ROAD_MAX 192
#define UI_NAV_WINDOW_POINT_MAX 128
#define UI_NAV_MAP_AREA_MAX 192
#define UI_NAV_MAP_AREA_POINT_MAX 6
#define UI_NAV_MAP_LABEL_MAX 64
/* Optional map detail in meters east/north of frame.origin. Convex polygons only. */
typedef struct { float east_m,north_m; } ui_nav_local_point_t;
typedef enum { UI_NAV_AREA_BUILDING,UI_NAV_AREA_PARK,UI_NAV_AREA_WATER } ui_nav_area_kind_t;
typedef struct { uint8_t count; ui_nav_area_kind_t kind; ui_nav_local_point_t points[UI_NAV_MAP_AREA_POINT_MAX]; } ui_nav_area_t;
/* kind: 0 road, 1 park, 2 water, 3 school, 4 landmark, 5 healthcare, 6 station, 7 other POI. */
typedef struct { ui_nav_local_point_t point; uint8_t kind; char text[48]; } ui_nav_map_label_t;
typedef struct { ui_nav_point_t a, b; uint8_t level; } ui_nav_road_t;
typedef enum { UI_NAV_MAP_FOLLOW,UI_NAV_MAP_OVERVIEW,UI_NAV_MAP_BROWSE,UI_NAV_MAP_PICK } ui_nav_map_purpose_t;
typedef struct {
    uint32_t id, route_id, route_revision;
    ui_nav_point_t center;
    float meters_per_pixel, heading;
    /* North-aligned coverage in projection pixels at meters_per_pixel, not framebuffer dimensions. */
    uint16_t width, height;
    ui_nav_map_purpose_t purpose;
} ui_nav_map_request_t;
typedef struct {
    uint32_t request_id, route_id, route_revision, map_revision;
    ui_nav_point_t origin;
    float meters_per_pixel;
    ui_nav_result_code_t status;
    uint32_t raster_id;
    float raster_latitude_ref;
    bool demo;
    bool destination_valid;
    ui_nav_point_t destination;
    uint8_t road_count, point_count;
    ui_nav_road_t roads[UI_NAV_MAP_ROAD_MAX];
    ui_nav_point_t points[UI_NAV_WINDOW_POINT_MAX];
    uint8_t area_count,label_count;
    ui_nav_area_t areas[UI_NAV_MAP_AREA_MAX];
    ui_nav_map_label_t labels[UI_NAV_MAP_LABEL_MAX];
} ui_nav_map_frame_t;
typedef struct {
    bool (*request)(const ui_nav_map_request_t *request, void *context);
    void (*cancel)(uint32_t request_id, void *context);
    void *context;
} ui_nav_map_backend_t;
void ui_nav_map_provider_init(void);
bool ui_nav_map_provider_register(const ui_nav_map_backend_t *backend);
uint32_t ui_nav_map_provider_request(const ui_nav_map_request_t *request, uint32_t now_ms);
void ui_nav_map_provider_cancel(void);
/* All coordinates in a frame must share origin.system; buffers are copied. */
bool ui_nav_map_provider_submit(const ui_nav_map_frame_t *frame);
bool ui_nav_map_provider_process(uint32_t now_ms);
bool ui_nav_map_provider_get(ui_nav_map_frame_t *frame, uint32_t *version);
bool ui_nav_map_provider_busy(void);
#endif
