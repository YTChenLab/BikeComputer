#include "ui_nav_map.h"

#include "ui_nav_animation.h"
#include "ui_nav_geometry.h"
#include "ui_nav_layout.h"
#include "../../core/ui_colors.h"
#include "../../core/ui_motion.h"
#include "../../core/ui_theme.h"

#include <stdio.h>

#define NAV_MAP_W             UI_NAV_MAP_WIDTH
#define NAV_MAP_H             UI_NAV_MAP_HEIGHT
#define NAV_CONTENT_SIZE      620
#define NAV_CONTENT_CENTER_X  310
#define NAV_CONTENT_CENTER_Y  310
#define NAV_CONTENT_HOME_X    ((NAV_MAP_W - NAV_CONTENT_SIZE) / 2)
#define NAV_CONTENT_HOME_Y    ((NAV_MAP_H - NAV_CONTENT_SIZE) / 2)
#define NAV_CONTROL_SIZE      48
#define NAV_HUD_MARGIN        12
#define NAV_COMPASS_TOP       12
#define NAV_ZOOM_IN_TOP       76
#define NAV_ZOOM_OUT_TOP      132
#define NAV_CONTROL_RIGHT     (NAV_MAP_W - NAV_HUD_MARGIN - NAV_CONTROL_SIZE)
#define NAV_LOCATE_TOP        (NAV_MAP_H - NAV_HUD_MARGIN - NAV_CONTROL_SIZE)
#define NAV_SCALE_W           100
#define NAV_SCALE_H           38
#define NAV_SCALE_X           ((NAV_MAP_W - NAV_SCALE_W) / 2)
#define NAV_SCALE_Y           12
#define NAV_MARKER_SIZE       22
#define NAV_MARKER_RADIUS     (NAV_MARKER_SIZE / 2)
#define NAV_ZOOM_MIN          10
#define NAV_ZOOM_MAX          18
#define NAV_ZOOM_BASE         14
#define NAV_ZOOM_SCALE_STEP   18
#define NAV_DRAG_THROW_FACTOR 7
#define NAV_YELLOW            lv_color_hex(0xFFE600)
#define NAV_MOCK_ORIGIN_LAT   34.3416f
#define NAV_MOCK_ORIGIN_LON   108.9398f

static int32_t clamp_content_x(int32_t value)
{
    int32_t min = NAV_MAP_W - NAV_CONTENT_SIZE;
    if(value < min) return min;
    if(value > 0) return 0;
    return value;
}

static int32_t clamp_content_y(int32_t value)
{
    int32_t min = NAV_MAP_H - NAV_CONTENT_SIZE;
    if(value < min) return min;
    if(value > 0) return 0;
    return value;
}

static void follow_render_position(ui_nav_map_t *map)
{
    if(map == NULL || map->state != NAV_FOLLOW) return;
    lv_point_precise_t marker = ui_nav_geometry_transform_point(
        map, (lv_point_precise_t){map->render_x, map->render_y});
    lv_obj_set_pos(map->map_content,
                   clamp_content_x(NAV_MAP_W / 2 - marker.x),
                   clamp_content_y(NAV_MAP_H / 2 - marker.y));
}

static void update_marker_visual(ui_nav_map_t *map)
{
    if(map == NULL || map->position_marker == NULL) return;
    lv_point_precise_t marker = ui_nav_geometry_transform_point(
        map, (lv_point_precise_t){map->render_x, map->render_y});
    lv_obj_set_pos(map->position_marker, marker.x - NAV_MARKER_RADIUS,
                   marker.y - NAV_MARKER_RADIUS);
    follow_render_position(map);
}

static void set_content_x(void *target, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)target, clamp_content_x(value));
}

static void set_content_y(void *target, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)target, clamp_content_y(value));
}

static void set_marker_x(void *target, int32_t value)
{
    ui_nav_map_t *map = target;
    map->render_x = value;
    update_marker_visual(map);
}

static void set_marker_y(void *target, int32_t value)
{
    ui_nav_map_t *map = target;
    map->render_y = value;
    update_marker_visual(map);
}

static void invalidate_map_geometry(ui_nav_map_t *map)
{
    if(map == NULL) return;
    ui_nav_geometry_invalidate(map);
    update_marker_visual(map);
}

static void set_map_scale(void *target, int32_t value)
{
    ui_nav_map_t *map = target;
    map->render_scale = value;
    invalidate_map_geometry(map);
}

static void set_map_rotation(void *target, int32_t value)
{
    ui_nav_map_t *map = target;
    map->render_rotation = value;
    invalidate_map_geometry(map);
}

static void set_scale_opa(void *target, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)target, (lv_opa_t)value, 0);
}

static void scale_draw_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;
    lv_obj_t *obj = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = UI_COLOR_WHITE;
    line.opa = LV_OPA_COVER;
    line.width = 2;
    line.p1 = (lv_point_precise_t){area.x1 + 8, area.y1 + 6};
    line.p2 = (lv_point_precise_t){area.x2 - 8, area.y1 + 6};
    lv_draw_line(layer, &line);
    line.p1 = (lv_point_precise_t){area.x1 + 8, area.y1 + 1};
    line.p2 = (lv_point_precise_t){area.x1 + 8, area.y1 + 11};
    lv_draw_line(layer, &line);
    line.p1 = (lv_point_precise_t){area.x2 - 8, area.y1 + 1};
    line.p2 = (lv_point_precise_t){area.x2 - 8, area.y1 + 11};
    lv_draw_line(layer, &line);
}

static void map_set_state(ui_nav_map_t *map, nav_state_t state)
{
    if(map == NULL) return;
    map->state = state;
    ui_nav_control_set_active(&map->locate, state == NAV_EXPLORE);
}

void ui_nav_map_set_focus(ui_nav_map_t *map, ui_nav_focus_t focus,
                          bool visible)
{
    if(map == NULL || focus >= UI_NAV_FOCUS_COUNT) return;
    map->focused_control = focus;
    map->focus_visible = visible;
    ui_nav_control_set_focused(&map->compass,
                               visible && focus == UI_NAV_FOCUS_COMPASS);
    ui_nav_control_set_focused(&map->zoom_in,
                               visible && focus == UI_NAV_FOCUS_ZOOM_IN);
    ui_nav_control_set_focused(&map->zoom_out,
                               visible && focus == UI_NAV_FOCUS_ZOOM_OUT);
    ui_nav_control_set_focused(&map->locate,
                               visible && focus == UI_NAV_FOCUS_LOCATE);
}

static void recenter_done(lv_anim_t *animation)
{
    ui_nav_map_t *map = lv_anim_get_user_data(animation);
    if(map != NULL) map_set_state(map, NAV_FOLLOW);
}

static void recenter(ui_nav_map_t *map)
{
    if(map == NULL) return;
    map_set_state(map, NAV_RECENTER);
    lv_point_precise_t marker = ui_nav_geometry_transform_point(
        map, (lv_point_precise_t){map->render_x, map->render_y});
    int32_t target_x = clamp_content_x(NAV_MAP_W / 2 - marker.x);
    int32_t target_y = clamp_content_y(NAV_MAP_H / 2 - marker.y);
    int32_t from_x = lv_obj_get_x(map->map_content);
    int32_t from_y = lv_obj_get_y(map->map_content);
    ui_nav_animation_value(map->map_content, set_content_x, from_x,
                           target_x, UI_ANIM_RECENTER, lv_anim_path_ease_in_out);

    ui_motion_value(map->map_content, set_content_y, from_y, target_y,
                    UI_MOTION_DURATION_RECENTER, lv_anim_path_ease_in_out,
                    recenter_done, map);
}

static void compass_clicked(void *user_data)
{
    ui_nav_map_t *map = user_data;
    if(map == NULL) return;
    ui_nav_map_set_focus(map, UI_NAV_FOCUS_COMPASS, true);
    map->north_up = !map->north_up;
    int32_t rotation = map->north_up ? 0 : -(int32_t)(map->heading * 10.0f);
    ui_nav_animation_value(map, set_map_rotation, map->render_rotation,
                           rotation, UI_ANIM_NORMAL, lv_anim_path_ease_in_out);
    ui_nav_control_set_active(&map->compass, !map->north_up);
}

static void zoom_in_clicked(void *user_data)
{
    ui_nav_map_t *map = user_data;
    if(map == NULL) return;
    ui_nav_map_set_focus(map, UI_NAV_FOCUS_ZOOM_IN, true);
    if(map->zoom < NAV_ZOOM_MAX) nav_map_zoom(map, map->zoom + 1, true);
}

static void zoom_out_clicked(void *user_data)
{
    ui_nav_map_t *map = user_data;
    if(map == NULL) return;
    ui_nav_map_set_focus(map, UI_NAV_FOCUS_ZOOM_OUT, true);
    if(map->zoom > NAV_ZOOM_MIN) nav_map_zoom(map, map->zoom - 1, true);
}

static void locate_clicked(void *user_data)
{
    ui_nav_map_t *map = user_data;
    if(map == NULL) return;
    ui_nav_map_set_focus(map, UI_NAV_FOCUS_LOCATE, true);
    recenter(map);
}

static void map_pointer_cb(lv_event_t *event)
{
    ui_nav_map_t *map = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if(map == NULL) return;
    if(code == LV_EVENT_PRESSED) {
        map->last_drag_vector = (lv_point_t){0, 0};
    } else if(code == LV_EVENT_PRESSING) {
        lv_indev_t *indev = lv_indev_active();
        if(indev == NULL) return;
        lv_indev_get_vect(indev, &map->last_drag_vector);
        map_set_state(map, NAV_EXPLORE);
        nav_map_move(map, map->last_drag_vector.x, map->last_drag_vector.y, false);
    } else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if(map->state == NAV_EXPLORE) {
            nav_map_move(map, map->last_drag_vector.x * NAV_DRAG_THROW_FACTOR,
                         map->last_drag_vector.y * NAV_DRAG_THROW_FACTOR, true);
        }
    }
}

static lv_obj_t *plain_layer(lv_obj_t *parent)
{
    lv_obj_t *layer = lv_obj_create(parent);
    lv_obj_remove_style_all(layer);
    lv_obj_set_pos(layer, 0, 0);
    lv_obj_set_size(layer, NAV_CONTENT_SIZE, NAV_CONTENT_SIZE);
    lv_obj_clear_flag(layer, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return layer;
}

static void create_scale(ui_nav_map_t *map)
{
    map->scale_root = lv_obj_create(map->map_hud);
    lv_obj_remove_style_all(map->scale_root);
    lv_obj_set_pos(map->scale_root, NAV_SCALE_X, NAV_SCALE_Y);
    lv_obj_set_size(map->scale_root, NAV_SCALE_W, NAV_SCALE_H);
    lv_obj_clear_flag(map->scale_root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(map->scale_root, scale_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    map->scale_label = lv_label_create(map->scale_root);
    lv_label_set_text(map->scale_label, "500 m");
    lv_obj_set_pos(map->scale_label, 0, 13);
    lv_obj_set_width(map->scale_label, NAV_SCALE_W);
    lv_obj_set_style_text_align(map->scale_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(map->scale_label, UI_COLOR_WHITE, 0);
    lv_obj_set_style_text_font(map->scale_label, UI_FONT_PAGE_TEXT, 0);
}

static void create_hud(ui_nav_map_t *map)
{
    map->map_hud = lv_obj_create(map->root);
    lv_obj_remove_style_all(map->map_hud);
    lv_obj_set_pos(map->map_hud, 0, 0);
    lv_obj_set_size(map->map_hud, NAV_MAP_W, NAV_MAP_H);
    lv_obj_clear_flag(map->map_hud, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    create_scale(map);
    ui_nav_control_create(&map->compass, map->map_hud, NAV_CONTROL_RIGHT, NAV_COMPASS_TOP,
                          LV_SYMBOL_UP, compass_clicked, map);
    lv_obj_set_style_text_font(map->compass.icon, &lv_font_montserrat_20, 0);
    lv_obj_align(map->compass.icon, LV_ALIGN_TOP_MID, 0, 3);
    map->compass_n = lv_label_create(map->compass.root);
    lv_label_set_text(map->compass_n, "N");
    lv_obj_set_style_text_font(map->compass_n, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(map->compass_n, UI_COLOR_WHITE, 0);
    lv_obj_align(map->compass_n, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_clear_flag(map->compass_n, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    ui_nav_control_create(&map->zoom_in, map->map_hud, NAV_CONTROL_RIGHT, NAV_ZOOM_IN_TOP,
                          LV_SYMBOL_PLUS, zoom_in_clicked, map);
    ui_nav_control_create(&map->zoom_out, map->map_hud, NAV_CONTROL_RIGHT, NAV_ZOOM_OUT_TOP,
                          LV_SYMBOL_MINUS, zoom_out_clicked, map);
    ui_nav_control_create(&map->locate, map->map_hud, NAV_CONTROL_RIGHT,
                          NAV_LOCATE_TOP, LV_SYMBOL_GPS, locate_clicked, map);
}

void ui_nav_map_create(ui_nav_map_t *map, lv_obj_t *parent, int32_t x, int32_t y)
{
    if(map == NULL || parent == NULL) return;
    map->root = lv_obj_create(parent);
    lv_obj_remove_style_all(map->root);
    lv_obj_set_pos(map->root, x, y);
    lv_obj_set_size(map->root, NAV_MAP_W, NAV_MAP_H);
    lv_obj_set_style_bg_color(map->root, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(map->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(map->root, UI_TOKEN_COLOR_SURFACE_BORDER, 0);
    lv_obj_set_style_border_opa(map->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(map->root, 1, 0);
    lv_obj_set_style_radius(map->root, UI_TOKEN_RADIUS_MEDIUM, 0);
    lv_obj_add_flag(map->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(map->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(map->root, map_pointer_cb, LV_EVENT_ALL, map);

    map->map_content = lv_obj_create(map->root);
    lv_obj_remove_style_all(map->map_content);
    lv_obj_set_pos(map->map_content, NAV_CONTENT_HOME_X, NAV_CONTENT_HOME_Y);
    lv_obj_set_size(map->map_content, NAV_CONTENT_SIZE, NAV_CONTENT_SIZE);
    lv_obj_clear_flag(map->map_content, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    map->map_tiles = plain_layer(map->map_content);
    lv_obj_set_style_bg_color(map->map_tiles, lv_color_hex(0x07100C), 0);
    lv_obj_set_style_bg_opa(map->map_tiles, LV_OPA_COVER, 0);
    map->road_layer = plain_layer(map->map_content);
    map->route_layer = plain_layer(map->map_content);
    ui_nav_geometry_attach(map);

    map->render_x = NAV_CONTENT_CENTER_X;
    map->render_y = NAV_CONTENT_CENTER_Y;
    map->target_x = map->render_x;
    map->target_y = map->render_y;
    map->render_scale = 256;
    map->render_rotation = 0;
    map->position_marker = lv_obj_create(map->map_content);
    lv_obj_remove_style_all(map->position_marker);
    lv_obj_set_pos(map->position_marker, map->render_x - NAV_MARKER_RADIUS,
                   map->render_y - NAV_MARKER_RADIUS);
    lv_obj_set_size(map->position_marker, NAV_MARKER_SIZE, NAV_MARKER_SIZE);
    lv_obj_set_style_bg_color(map->position_marker, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(map->position_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(map->position_marker, NAV_YELLOW, 0);
    lv_obj_set_style_border_width(map->position_marker, 4, 0);
    lv_obj_set_style_radius(map->position_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(map->position_marker, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    map->marker_center = lv_obj_create(map->position_marker);
    lv_obj_remove_style_all(map->marker_center);
    lv_obj_set_size(map->marker_center, 6, 6);
    lv_obj_set_style_bg_color(map->marker_center, UI_COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(map->marker_center, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(map->marker_center, LV_RADIUS_CIRCLE, 0);
    lv_obj_center(map->marker_center);

    map->state = NAV_FOLLOW;
    map->zoom = NAV_ZOOM_BASE;
    map->heading = 0.0f;
    map->north_up = true;
    create_hud(map);
    map->focused_control = UI_NAV_FOCUS_LOCATE;
    map->focus_visible = false;

    const lv_point_precise_t default_route[] = {
        {72, 515}, {205, 485}, {226, 392}, {348, 390}, {350, 300}, {430, 298}, {432, 240}
    };
    nav_map_update_route(map, default_route,
                         (uint8_t)(sizeof(default_route) / sizeof(default_route[0])));
}

bool ui_nav_map_contains_point(const ui_nav_map_t *map, int32_t x, int32_t y)
{
    if(map == NULL || map->root == NULL) return false;
    lv_area_t area;
    lv_obj_get_coords(map->root, &area);
    return x >= area.x1 && x <= area.x2 && y >= area.y1 && y <= area.y2;
}

void nav_map_set_tile(ui_nav_map_t *map, uint16_t tile_id)
{
    if(map == NULL || map->map_tiles == NULL) return;
    uint8_t tone = (uint8_t)(10 + (tile_id % 3) * 3);
    lv_obj_set_style_bg_color(map->map_tiles, lv_color_make(4, tone, 8), 0);
}

void nav_map_move(ui_nav_map_t *map, int32_t delta_x, int32_t delta_y, bool animated)
{
    if(map == NULL || map->map_content == NULL) return;
    int32_t to_x = clamp_content_x(lv_obj_get_x(map->map_content) + delta_x);
    int32_t to_y = clamp_content_y(lv_obj_get_y(map->map_content) + delta_y);
    if(animated) {
        ui_nav_animation_value(map->map_content, set_content_x, lv_obj_get_x(map->map_content),
                               to_x, UI_ANIM_NORMAL, lv_anim_path_ease_out);
        ui_nav_animation_value(map->map_content, set_content_y, lv_obj_get_y(map->map_content),
                               to_y, UI_ANIM_NORMAL, lv_anim_path_ease_out);
    } else {
        lv_obj_set_pos(map->map_content, to_x, to_y);
    }
}

void nav_map_zoom(ui_nav_map_t *map, uint8_t zoom, bool animated)
{
    if(map == NULL || map->map_content == NULL) return;
    if(zoom < NAV_ZOOM_MIN) zoom = NAV_ZOOM_MIN;
    if(zoom > NAV_ZOOM_MAX) zoom = NAV_ZOOM_MAX;
    map->zoom = zoom;
    int32_t scale = 256 + ((int32_t)zoom - NAV_ZOOM_BASE) * NAV_ZOOM_SCALE_STEP;
    uint32_t scale_distance = 500U;
    int32_t level_delta = (int32_t)zoom - NAV_ZOOM_BASE;
    if(level_delta > 0) scale_distance >>= (uint32_t)((level_delta + 1) / 2);
    else if(level_delta < 0) scale_distance <<= (uint32_t)((-level_delta + 1) / 2);
    lv_label_set_text_fmt(map->scale_label, "%lu m", (unsigned long)scale_distance);
    if(animated) {
        ui_nav_animation_value(map, set_map_scale, map->render_scale,
                               scale, UI_ANIM_NORMAL, lv_anim_path_ease_in_out);
        ui_nav_animation_value(map->scale_root, set_scale_opa, LV_OPA_TRANSP,
                               LV_OPA_COVER, UI_ANIM_NORMAL, lv_anim_path_ease_in);
    } else {
        set_map_scale(map, scale);
    }
}

void nav_map_update_route(ui_nav_map_t *map, const lv_point_precise_t *points,
                          uint8_t point_count)
{
    if(map == NULL || points == NULL) return;
    if(point_count > UI_NAV_ROUTE_POINT_MAX) point_count = UI_NAV_ROUTE_POINT_MAX;
    for(uint8_t i = 0; i < point_count; i++) map->route_points[i] = points[i];
    map->route_point_count = point_count;
    lv_obj_invalidate(map->route_layer);
}

void nav_map_update_position(ui_nav_map_t *map, float latitude, float longitude,
                             float heading, bool animated)
{
    if(map == NULL) return;
    map->target_x = NAV_CONTENT_CENTER_X + (int32_t)((longitude - NAV_MOCK_ORIGIN_LON) * 15000.0f);
    map->target_y = NAV_CONTENT_CENTER_Y - (int32_t)((latitude - NAV_MOCK_ORIGIN_LAT) * 15000.0f);
    if(map->target_x < 30) map->target_x = 30;
    if(map->target_x > NAV_CONTENT_SIZE - 30) map->target_x = NAV_CONTENT_SIZE - 30;
    if(map->target_y < 30) map->target_y = 30;
    if(map->target_y > NAV_CONTENT_SIZE - 30) map->target_y = NAV_CONTENT_SIZE - 30;
    map->heading = heading;
    if(!map->north_up) {
        ui_nav_animation_value(map, set_map_rotation, map->render_rotation,
                               -(int32_t)(heading * 10.0f), UI_ANIM_NORMAL,
                               lv_anim_path_ease_in_out);
    }
    if(animated) {
        ui_nav_animation_value(map, set_marker_x, map->render_x, map->target_x,
                               UI_ANIM_NORMAL, lv_anim_path_ease_in_out);
        ui_nav_animation_value(map, set_marker_y, map->render_y, map->target_y,
                               UI_ANIM_NORMAL, lv_anim_path_ease_in_out);
    } else {
        set_marker_x(map, map->target_x);
        set_marker_y(map, map->target_y);
    }
    follow_render_position(map);
}

void ui_nav_map_apply_model(ui_nav_map_t *map, const nav_model_t *model)
{
    if(map == NULL || model == NULL) return;
    if(model->zoom != map->zoom) nav_map_zoom(map, model->zoom, true);
    nav_map_update_position(map, model->latitude, model->longitude,
                            model->heading, true);
}

void ui_nav_map_handle_input(ui_nav_map_t *map, int32_t zoom_delta, bool locate)
{
    if(map == NULL) return;
    ui_nav_control_t *control = NULL;
    if(locate) {
        ui_nav_map_set_focus(map, UI_NAV_FOCUS_LOCATE, true);
        control = &map->locate;
    }
    else if(zoom_delta > 0) {
        ui_nav_map_set_focus(map, UI_NAV_FOCUS_ZOOM_IN, true);
        control = &map->zoom_in;
    }
    else if(zoom_delta < 0) {
        ui_nav_map_set_focus(map, UI_NAV_FOCUS_ZOOM_OUT, true);
        control = &map->zoom_out;
    }
    if(control == NULL) return;
    ui_nav_control_activate(control);
}
