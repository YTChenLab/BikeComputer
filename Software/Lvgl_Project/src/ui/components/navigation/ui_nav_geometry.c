#include "ui_nav_geometry.h"

#include "lvgl/src/draw/lv_draw_line.h"

#define NAV_CONTENT_CENTER_X 310
#define NAV_CONTENT_CENTER_Y 310
#define NAV_YELLOW            lv_color_hex(0xFFE600)

typedef struct
{
    lv_point_precise_t a;
    lv_point_precise_t b;
    int32_t width;
    uint32_t color;
} road_t;

static const road_t s_roads[] = {
    {{-20, 112}, {640, 248}, 6, 0x414647}, {{44, 500}, {260, -20}, 8, 0x5B6061},
    {{-20, 402}, {640, 412}, 8, 0x555A5B}, {{92, -20}, {382, 640}, 5, 0x34393A},
    {{-20, 555}, {315, 332}, 5, 0x454A4B}, {{244, 640}, {308, -20}, 5, 0x3A3F40},
    {{392, 640}, {640, 80}, 6, 0x474C4D}, {{-20, 268}, {216, 246}, 5, 0x3A3F40},
    {{315, 332}, {640, 520}, 5, 0x383D3E}, {{132, 160}, {108, 640}, 4, 0x303536}
};

lv_point_precise_t ui_nav_geometry_transform_point(const ui_nav_map_t *map,
                                                    lv_point_precise_t point)
{
    int32_t dx = ((int32_t)point.x - NAV_CONTENT_CENTER_X) * map->render_scale / 256;
    int32_t dy = ((int32_t)point.y - NAV_CONTENT_CENTER_Y) * map->render_scale / 256;
    int32_t angle = (map->render_rotation / 10) % 360;
    if(angle < 0) angle += 360;
    int32_t sine = lv_trigo_sin(angle);
    int32_t cosine = lv_trigo_sin(angle + 90);
    int32_t rotated_x = (int32_t)(((int64_t)dx * cosine - (int64_t)dy * sine) >> LV_TRIGO_SHIFT);
    int32_t rotated_y = (int32_t)(((int64_t)dx * sine + (int64_t)dy * cosine) >> LV_TRIGO_SHIFT);
    return (lv_point_precise_t){NAV_CONTENT_CENTER_X + rotated_x,
                                NAV_CONTENT_CENTER_Y + rotated_y};
}

static void draw_line(lv_layer_t *layer, const lv_area_t *area,
                      lv_point_precise_t p1, lv_point_precise_t p2,
                      int32_t width, lv_color_t color)
{
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.p1 = (lv_point_precise_t){area->x1 + p1.x, area->y1 + p1.y};
    line.p2 = (lv_point_precise_t){area->x1 + p2.x, area->y1 + p2.y};
    line.width = width;
    line.color = color;
    line.opa = LV_OPA_COVER;
    line.round_start = 1;
    line.round_end = 1;
    lv_draw_line(layer, &line);
}

static void roads_draw_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;
    ui_nav_map_t *map = lv_event_get_user_data(event);
    if(map == NULL) return;
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(event), &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    for(uint32_t i = 0; i < sizeof(s_roads) / sizeof(s_roads[0]); i++) {
        draw_line(layer, &area,
                  ui_nav_geometry_transform_point(map, s_roads[i].a),
                  ui_nav_geometry_transform_point(map, s_roads[i].b),
                  s_roads[i].width, lv_color_hex(s_roads[i].color));
    }
}

static void route_draw_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;
    ui_nav_map_t *map = lv_event_get_user_data(event);
    if(map == NULL || map->route_point_count < 2) return;
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(event), &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    for(uint8_t i = 1; i < map->route_point_count; i++) {
        draw_line(layer, &area,
                  ui_nav_geometry_transform_point(map, map->route_points[i - 1]),
                  ui_nav_geometry_transform_point(map, map->route_points[i]),
                  6, NAV_YELLOW);
    }
}

void ui_nav_geometry_attach(ui_nav_map_t *map)
{
    if(map == NULL) return;
    lv_obj_add_event_cb(map->road_layer, roads_draw_cb, LV_EVENT_DRAW_MAIN, map);
    lv_obj_add_event_cb(map->route_layer, route_draw_cb, LV_EVENT_DRAW_MAIN, map);
}

void ui_nav_geometry_invalidate(ui_nav_map_t *map)
{
    if(map == NULL) return;
    lv_obj_invalidate(map->road_layer);
    lv_obj_invalidate(map->route_layer);
}
