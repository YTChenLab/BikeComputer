#include "ui_nav_command_dock.h"
#include "../../core/ui_text.h"
#include "../../core/ui_motion.h"

#include "ui_nav_layout.h"
#include "../../core/ui_colors.h"
#include "../../core/ui_theme.h"
#include "../../design/ui_design_tokens.h"
#include "lvgl/src/draw/lv_draw_arc.h"
#include "lvgl/src/draw/lv_draw_line.h"
#include "lvgl/src/draw/lv_draw_triangle.h"

#include <stdio.h>
#include <string.h>

#define NAV_DOCK_BG              UI_TOKEN_COLOR_SURFACE
#define NAV_DOCK_BORDER          UI_TOKEN_COLOR_SURFACE_OUTLINE
#define NAV_DOCK_DIVIDER         UI_TOKEN_COLOR_DIVIDER
#define NAV_DOCK_YELLOW          UI_TOKEN_COLOR_ACCENT
#define NAV_DOCK_ICON_X          10
#define NAV_DOCK_ICON_Y          14
#define NAV_DOCK_ICON_W         170
#define NAV_DOCK_ICON_H         160
#define NAV_DOCK_DIVIDER_X      184
#define NAV_DOCK_INFO_X         194
#define NAV_DOCK_INFO_W         252

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, lv_color_t color,
                            lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

static void draw_line(lv_layer_t *layer, lv_point_precise_t p1,
                      lv_point_precise_t p2, int32_t width)
{
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = NAV_DOCK_YELLOW;
    line.opa = LV_OPA_COVER;
    line.width = width;
    line.round_start = 1;
    line.round_end = 1;
    line.p1 = p1;
    line.p2 = p2;
    lv_draw_line(layer, &line);
}

static void draw_triangle(lv_layer_t *layer, lv_point_precise_t p0,
                          lv_point_precise_t p1, lv_point_precise_t p2)
{
    lv_draw_triangle_dsc_t triangle;
    lv_draw_triangle_dsc_init(&triangle);
    triangle.bg_color = NAV_DOCK_YELLOW;
    triangle.bg_opa = LV_OPA_COVER;
    triangle.p[0] = p0;
    triangle.p[1] = p1;
    triangle.p[2] = p2;
    lv_draw_triangle(layer, &triangle);
}

static lv_point_precise_t icon_point(int32_t cx, int32_t cy,
                                     int32_t x, int32_t y,
                                     int32_t scale)
{
    return (lv_point_precise_t){cx + (x * scale) / 100,
                                cy + (y * scale) / 100};
}

static void draw_arrow_icon(lv_layer_t *layer, int32_t cx, int32_t cy,
                            nav_maneuver_t maneuver, int32_t scale)
{
    const int32_t width = LV_MAX(12, (18 * scale) / 100);
    int32_t sign = (maneuver == NAV_MANEUVER_RIGHT ||
                    maneuver == NAV_MANEUVER_SLIGHT_RIGHT ||
                    maneuver == NAV_MANEUVER_SHARP_RIGHT) ? 1 : -1;

    if(maneuver == NAV_MANEUVER_LEFT || maneuver == NAV_MANEUVER_RIGHT) {
        int32_t stem_x = -sign * 34;
        draw_line(layer, icon_point(cx, cy, stem_x, 60, scale),
                  icon_point(cx, cy, stem_x, -9, scale), width);
        draw_line(layer, icon_point(cx, cy, stem_x, -9, scale),
                  icon_point(cx, cy, sign * 42, -9, scale), width);
        draw_triangle(layer, icon_point(cx, cy, sign * 69, -9, scale),
                      icon_point(cx, cy, sign * 39, -35, scale),
                      icon_point(cx, cy, sign * 39, 17, scale));
        return;
    }

    if(maneuver == NAV_MANEUVER_SLIGHT_LEFT ||
       maneuver == NAV_MANEUVER_SLIGHT_RIGHT) {
        draw_line(layer, icon_point(cx, cy, -sign * 27, 60, scale),
                  icon_point(cx, cy, -sign * 27, 12, scale), width);
        draw_line(layer, icon_point(cx, cy, -sign * 27, 12, scale),
                  icon_point(cx, cy, sign * 38, -36, scale), width);
        draw_triangle(layer, icon_point(cx, cy, sign * 59, -51, scale),
                      icon_point(cx, cy, sign * 26, -46, scale),
                      icon_point(cx, cy, sign * 49, -20, scale));
        return;
    }

    if(maneuver == NAV_MANEUVER_SHARP_LEFT ||
       maneuver == NAV_MANEUVER_SHARP_RIGHT) {
        draw_line(layer, icon_point(cx, cy, -sign * 30, 60, scale),
                  icon_point(cx, cy, -sign * 30, 17, scale), width);
        draw_line(layer, icon_point(cx, cy, -sign * 30, 17, scale),
                  icon_point(cx, cy, sign * 42, -39, scale), width);
        draw_triangle(layer, icon_point(cx, cy, sign * 63, -55, scale),
                      icon_point(cx, cy, sign * 30, -50, scale),
                      icon_point(cx, cy, sign * 54, -24, scale));
        return;
    }

    if(maneuver == NAV_MANEUVER_U_TURN) {
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.center = (lv_point_t){cx, cy - 14};
        arc.radius = (uint16_t)((44 * scale) / 100);
        arc.start_angle = 0;
        arc.end_angle = 180;
        arc.width = width;
        arc.color = NAV_DOCK_YELLOW;
        arc.opa = LV_OPA_COVER;
        arc.rounded = 1;
        lv_draw_arc(layer, &arc);
        draw_line(layer, icon_point(cx, cy, 44, -14, scale),
                  icon_point(cx, cy, 44, 62, scale), width);
        draw_line(layer, icon_point(cx, cy, -44, -14, scale),
                  icon_point(cx, cy, -44, 17, scale), width);
        draw_triangle(layer, icon_point(cx, cy, -44, 45, scale),
                      icon_point(cx, cy, -69, 13, scale),
                      icon_point(cx, cy, -19, 13, scale));
        return;
    }

    if(maneuver == NAV_MANEUVER_ROUNDABOUT) {
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.center = (lv_point_t){cx, cy};
        arc.radius = (uint16_t)((48 * scale) / 100);
        arc.start_angle = 35;
        arc.end_angle = 325;
        arc.width = width;
        arc.color = NAV_DOCK_YELLOW;
        arc.opa = LV_OPA_COVER;
        arc.rounded = 1;
        lv_draw_arc(layer, &arc);
        draw_triangle(layer, icon_point(cx, cy, 43, -17, scale),
                      icon_point(cx, cy, 25, -31, scale),
                      icon_point(cx, cy, 48, -39, scale));
        return;
    }

    draw_line(layer, icon_point(cx, cy, 0, 62, scale),
              icon_point(cx, cy, 0, -34, scale), width);
    draw_triangle(layer, icon_point(cx, cy, 0, -67, scale),
                  icon_point(cx, cy, -31, -27, scale),
                  icon_point(cx, cy, 31, -27, scale));
}

static void draw_status_icon(lv_layer_t *layer, int32_t cx, int32_t cy,
                             nav_ui_state_t state, int32_t scale)
{
    const int32_t width = LV_MAX(8, (13 * scale) / 100);
    if(state == NAV_UI_REROUTING) {
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.center = (lv_point_t){cx, cy};
        arc.radius = (uint16_t)((40 * scale) / 100);
        arc.start_angle = 30;
        arc.end_angle = 155;
        arc.width = width;
        arc.color = NAV_DOCK_YELLOW;
        arc.opa = LV_OPA_COVER;
        arc.rounded = 1;
        lv_draw_arc(layer, &arc);
        arc.start_angle = 210;
        arc.end_angle = 335;
        lv_draw_arc(layer, &arc);
        draw_triangle(layer, icon_point(cx, cy, 43, -22, scale),
                      icon_point(cx, cy, 20, -26, scale),
                      icon_point(cx, cy, 36, -44, scale));
        draw_triangle(layer, icon_point(cx, cy, -43, 22, scale),
                      icon_point(cx, cy, -20, 26, scale),
                      icon_point(cx, cy, -36, 44, scale));
        return;
    }
    if(state == NAV_UI_GPS_WEAK) {
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.center = (lv_point_t){cx, cy};
        arc.radius = (uint16_t)((42 * scale) / 100);
        arc.start_angle = 210;
        arc.end_angle = 330;
        arc.width = width;
        arc.color = NAV_DOCK_YELLOW;
        arc.opa = LV_OPA_COVER;
        arc.rounded = 1;
        lv_draw_arc(layer, &arc);
        arc.radius = (uint16_t)((24 * scale) / 100);
        lv_draw_arc(layer, &arc);
        draw_line(layer, icon_point(cx, cy, 0, 15, scale),
                  icon_point(cx, cy, 0, 48, scale), width);
        return;
    }

    draw_line(layer, icon_point(cx, cy, -43, 2, scale),
              icon_point(cx, cy, -12, 36, scale), width);
    draw_line(layer, icon_point(cx, cy, -12, 36, scale),
              icon_point(cx, cy, 47, -38, scale), width);
}

static void icon_draw_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;
    ui_nav_command_dock_t *dock = lv_event_get_user_data(event);
    if(dock == NULL) return;
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(event), &area);
    int32_t cx = (area.x1 + area.x2) / 2;
    int32_t cy = (area.y1 + area.y2) / 2;
    if(dock->state == NAV_UI_REROUTING ||
       dock->state == NAV_UI_GPS_WEAK ||
       dock->state == NAV_UI_ARRIVED)
        draw_status_icon(lv_event_get_layer(event), cx, cy, dock->state,
                         dock->icon_scale);
    else
        draw_arrow_icon(lv_event_get_layer(event), cx, cy, dock->maneuver,
                        dock->icon_scale);
}

static const char *maneuver_name(nav_maneuver_t maneuver)
{
    switch(maneuver) {
        case NAV_MANEUVER_LEFT: return "左转";
        case NAV_MANEUVER_RIGHT: return "右转";
        case NAV_MANEUVER_SLIGHT_LEFT: return "轻微左转";
        case NAV_MANEUVER_SLIGHT_RIGHT: return "轻微右转";
        case NAV_MANEUVER_SHARP_LEFT: return "急左转";
        case NAV_MANEUVER_SHARP_RIGHT: return "急右转";
        case NAV_MANEUVER_U_TURN: return "掉头";
        case NAV_MANEUVER_ROUNDABOUT: return "进入环岛";
        case NAV_MANEUVER_ARRIVE: return "到达目的地";
        default: return "直行";
    }
}

static void set_icon_scale(void *target, int32_t value)
{
    ui_nav_command_dock_t *dock = target;
    if(dock == NULL || dock->icon == NULL) return;
    dock->icon_scale = value;
    lv_obj_invalidate(dock->icon);
}

static void set_accent_opa(void *target, int32_t value)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)target, (lv_opa_t)value, 0);
}

static void animate_state(ui_nav_command_dock_t *dock, nav_ui_state_t state,
                          bool maneuver_changed)
{
    lv_anim_delete(dock, set_icon_scale);
    lv_anim_delete(dock->accent, set_accent_opa);
    dock->icon_scale = maneuver_changed ? 85 :
                       (state == NAV_UI_APPROACH ? 92 : 100);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, dock);
    lv_anim_set_exec_cb(&animation, set_icon_scale);
    lv_anim_set_values(&animation, dock->icon_scale,
                       state == NAV_UI_IMMINENT ? 106 : 100);
    lv_anim_set_duration(&animation, 180U);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    if(state == NAV_UI_IMMINENT && !maneuver_changed)
        lv_anim_set_playback_duration(&animation, 160U);
    lv_anim_start(&animation);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, dock->accent);
    lv_anim_set_exec_cb(&animation, set_accent_opa);
    lv_anim_set_values(&animation, LV_OPA_COVER, LV_OPA_60);
    lv_anim_set_duration(&animation, 220U);
    lv_anim_set_playback_duration(&animation, 180U);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

static void set_normal_visibility(ui_nav_command_dock_t *dock, bool visible)
{
    lv_obj_t *normal[] = {dock->prefix, dock->distance_row,
                          dock->action, dock->road};
    for(uint8_t index = 0; index < 4U; index++) {
        if(visible) lv_obj_clear_flag(normal[index], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(normal[index], LV_OBJ_FLAG_HIDDEN);
    }
    if(visible) lv_obj_add_flag(dock->headline, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(dock->headline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(dock->special_detail, LV_OBJ_FLAG_HIDDEN);
}

void ui_nav_command_dock_create(ui_nav_command_dock_t *dock,
                                lv_obj_t *parent)
{
    if(dock == NULL || parent == NULL) return;
    memset(dock, 0, sizeof(*dock));
    dock->state = NAV_UI_CRUISE;
    dock->previous_state = NAV_UI_CRUISE;
    dock->maneuver = NAV_MANEUVER_STRAIGHT;
    dock->icon_scale = 100;

    dock->root = plain_obj(parent);
    lv_obj_set_pos(dock->root, UI_NAV_DOCK_X, UI_NAV_DOCK_Y);
    lv_obj_set_size(dock->root, UI_NAV_DOCK_WIDTH, UI_NAV_DOCK_HEIGHT);
    lv_obj_set_style_bg_color(dock->root, NAV_DOCK_BG, 0);
    lv_obj_set_style_bg_opa(dock->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dock->root, NAV_DOCK_BORDER, 0);
    lv_obj_set_style_border_opa(dock->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dock->root, 1, 0);
    lv_obj_set_style_radius(dock->root, 20, 0);
    lv_obj_set_style_clip_corner(dock->root, true, 0);

    dock->accent = plain_obj(dock->root);
    lv_obj_set_pos(dock->accent, 4, 15);
    lv_obj_set_size(dock->accent, 4, UI_NAV_DOCK_HEIGHT - 30);
    lv_obj_set_style_bg_color(dock->accent, NAV_DOCK_YELLOW, 0);
    lv_obj_set_style_bg_opa(dock->accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dock->accent, LV_RADIUS_CIRCLE, 0);

    dock->icon = plain_obj(dock->root);
    lv_obj_set_pos(dock->icon, NAV_DOCK_ICON_X, NAV_DOCK_ICON_Y);
    lv_obj_set_size(dock->icon, NAV_DOCK_ICON_W, NAV_DOCK_ICON_H);
    lv_obj_add_event_cb(dock->icon, icon_draw_cb, LV_EVENT_DRAW_MAIN, dock);

    dock->divider = plain_obj(dock->root);
    lv_obj_set_pos(dock->divider, NAV_DOCK_DIVIDER_X, 23);
    lv_obj_set_size(dock->divider, 1, UI_NAV_DOCK_HEIGHT - 46);
    lv_obj_set_style_bg_color(dock->divider, NAV_DOCK_DIVIDER, 0);
    lv_obj_set_style_bg_opa(dock->divider, LV_OPA_COVER, 0);

    dock->info = plain_obj(dock->root);
    lv_obj_set_pos(dock->info, NAV_DOCK_INFO_X, 10);
    lv_obj_set_size(dock->info, NAV_DOCK_INFO_W,
                    UI_NAV_DOCK_HEIGHT - 20);

    dock->prefix = make_label(dock->info, "前方", UI_FONT_PAGE_TEXT,
                              UI_TOKEN_COLOR_TEXT_SECONDARY,
                              LV_TEXT_ALIGN_CENTER);
    lv_obj_set_pos(dock->prefix, 0, 8);
    lv_obj_set_size(dock->prefix, NAV_DOCK_INFO_W, 28);

    dock->distance_row = plain_obj(dock->info);
    lv_obj_set_pos(dock->distance_row, 0, 38);
    lv_obj_set_size(dock->distance_row, NAV_DOCK_INFO_W, 65);
    lv_obj_set_layout(dock->distance_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(dock->distance_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dock->distance_row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dock->distance_row, 5, 0);
    dock->distance_value = make_label(dock->distance_row, "420",
                                      UI_FONT_METRIC_VALUE_SMALL,
                                      UI_COLOR_WHITE, LV_TEXT_ALIGN_CENTER);
    dock->distance_unit = make_label(dock->distance_row, "m",
                                     UI_FONT_SURFACE_TITLE,
                                     UI_COLOR_WHITE, LV_TEXT_ALIGN_CENTER);
    dock->action = make_label(dock->distance_row, "直行",
                              UI_FONT_NAV_ACTION,
                              NAV_DOCK_YELLOW, LV_TEXT_ALIGN_CENTER);

    dock->headline = make_label(dock->info, "", UI_FONT_NAV_ACTION,
                                UI_COLOR_WHITE, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_pos(dock->headline, 0, 43);
    lv_obj_set_size(dock->headline, NAV_DOCK_INFO_W, 42);
    lv_obj_add_flag(dock->headline, LV_OBJ_FLAG_HIDDEN);

    dock->special_detail = make_label(dock->info, "", UI_FONT_PAGE_TEXT,
                                      UI_TOKEN_COLOR_TEXT_SECONDARY,
                                      LV_TEXT_ALIGN_CENTER);
    lv_obj_set_pos(dock->special_detail, 0, 96);
    lv_obj_set_size(dock->special_detail, NAV_DOCK_INFO_W, 38);
    lv_obj_add_flag(dock->special_detail, LV_OBJ_FLAG_HIDDEN);

    dock->road = make_label(dock->info, "环城南路", UI_FONT_PAGE_TEXT,
                            UI_TOKEN_COLOR_TEXT_PRIMARY,
                            LV_TEXT_ALIGN_CENTER);
    lv_obj_set_pos(dock->road, 8, 123);
    lv_obj_set_size(dock->road, NAV_DOCK_INFO_W - 16, 30);
    lv_label_set_long_mode(dock->road, LV_LABEL_LONG_DOT);
}

void ui_nav_command_dock_update(ui_nav_command_dock_t *dock,
                                const nav_model_t *model,
                                bool animate_state_change)
{
    if(dock == NULL || dock->root == NULL || model == NULL) return;
    nav_ui_state_t old_state = dock->state;
    nav_maneuver_t old_maneuver = dock->maneuver;
    if(model->ui_state == NAV_UI_GPS_WEAK && old_state != NAV_UI_GPS_WEAK)
        dock->previous_state = old_state;
    dock->state = model->ui_state < NAV_UI_STATE_COUNT ?
                  model->ui_state : NAV_UI_CRUISE;
    dock->maneuver = model->direction < NAV_MANEUVER_COUNT ?
                     model->direction : NAV_MANEUVER_STRAIGHT;

    lv_obj_set_style_text_color(dock->distance_value,
        dock->state == NAV_UI_IMMINENT ? NAV_DOCK_YELLOW : UI_COLOR_WHITE, 0);

    if(dock->state == NAV_UI_REROUTING) {
        set_normal_visibility(dock, false);
        lv_label_set_text(dock->headline, "已偏离路线");
        lv_label_set_text(dock->special_detail, "正在重新规划…");
        lv_obj_clear_flag(dock->special_detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(dock->special_detail, UI_FONT_PAGE_TEXT, 0);
        lv_obj_set_style_text_color(dock->special_detail,
                                    UI_TOKEN_COLOR_TEXT_SECONDARY, 0);
    } else if(dock->state == NAV_UI_GPS_WEAK) {
        set_normal_visibility(dock, false);
        lv_label_set_text(dock->headline, "GPS 信号较弱");
        lv_label_set_text(dock->special_detail, "正在重新定位…");
        lv_obj_clear_flag(dock->special_detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(dock->special_detail, UI_FONT_PAGE_TEXT, 0);
        lv_obj_set_style_text_color(dock->special_detail,
                                    UI_TOKEN_COLOR_TEXT_SECONDARY, 0);
    } else if(dock->state == NAV_UI_ARRIVED ||
              dock->maneuver == NAV_MANEUVER_ARRIVE) {
        dock->state = NAV_UI_ARRIVED;
        set_normal_visibility(dock, false);
        lv_label_set_text(dock->headline, "已到达目的地");
        lv_label_set_text(dock->special_detail, "本次导航完成");
        lv_obj_clear_flag(dock->special_detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(dock->special_detail, UI_FONT_PAGE_TEXT, 0);
        lv_obj_set_style_text_color(dock->special_detail,
                                    UI_TOKEN_COLOR_TEXT_SECONDARY, 0);
    } else {
        set_normal_visibility(dock, true);
        lv_obj_set_style_text_color(dock->action, NAV_DOCK_YELLOW, 0);
        lv_label_set_text(dock->action, maneuver_name(dock->maneuver));
        (void)ui_text_set_label(dock->road,
                          model->road_name[0] != '\0' ? model->road_name : "当前道路", 20);
        if(model->direction_distance >= 1000U) {
            lv_label_set_text_fmt(dock->distance_value, "%.1f",
                                  (double)model->direction_distance / 1000.0);
            lv_label_set_text(dock->distance_unit, "km");
        } else {
            lv_label_set_text_fmt(dock->distance_value, "%lu",
                                  (unsigned long)model->direction_distance);
            lv_label_set_text(dock->distance_unit, "m");
        }
        if(dock->state == NAV_UI_NOW) {
            lv_obj_add_flag(dock->prefix, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(dock->distance_row, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(dock->headline, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(dock->headline, "现在");
            lv_label_set_text(dock->special_detail,
                              maneuver_name(dock->maneuver));
            lv_obj_clear_flag(dock->special_detail, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_font(dock->special_detail,
                                       UI_FONT_NAV_ACTION, 0);
            lv_obj_set_style_text_color(dock->special_detail,
                                        NAV_DOCK_YELLOW, 0);
            lv_obj_add_flag(dock->road, LV_OBJ_FLAG_HIDDEN);
        } else if(dock->state == NAV_UI_IMMINENT) {
            lv_obj_add_flag(dock->prefix, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(dock->road, LV_OBJ_FLAG_HIDDEN);
        }
    }

    bool maneuver_changed = dock->maneuver != old_maneuver;
    if(animate_state_change && !ui_motion_is_reduced() &&
       (maneuver_changed || dock->state == NAV_UI_APPROACH ||
        dock->state == NAV_UI_IMMINENT))
        animate_state(dock, dock->state, maneuver_changed);
    else {
        lv_anim_delete(dock, set_icon_scale);
        lv_anim_delete(dock->accent, set_accent_opa);
        lv_obj_set_style_bg_opa(dock->accent, LV_OPA_COVER, 0);
        dock->icon_scale = 100;
        lv_obj_invalidate(dock->icon);
    }
}
