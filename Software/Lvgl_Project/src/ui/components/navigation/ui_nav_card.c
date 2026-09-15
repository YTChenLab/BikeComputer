#include "ui_nav_card.h"

#include "ui_nav_animation.h"
#include "../../core/ui_colors.h"
#include "../../core/ui_theme.h"
#include "lvgl/src/draw/lv_draw_line.h"
#include "lvgl/src/draw/lv_draw_triangle.h"

#include <stdio.h>

#define NAV_CARD_W       227
#define NAV_CARD_H       154
#define NAV_CARD_RADIUS  10
#define NAV_CARD_PADDING       12
#define NAV_CARD_TITLE_Y       9
#define NAV_VALUE_BOTTOM       10
#define NAV_VALUE_UNIT_GAP     6
#define NAV_DIRECTION_TOP      40
#define NAV_DIRECTION_ARROW_X  44
#define NAV_DIRECTION_ARROW_W  139
#define NAV_DIRECTION_ARROW_H  74
#define NAV_DIRECTION_TEXT_Y   80
#define NAV_YELLOW             lv_color_hex(0xFFE600)

static lv_obj_t *card_label(lv_obj_t *parent, int32_t x, int32_t y, int32_t width,
                            const char *text, const lv_font_t *font,
                            lv_text_align_t align, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

static void layout_title(ui_nav_card_t *card)
{
    lv_obj_set_pos(card->title, 0, NAV_CARD_TITLE_Y);
    lv_obj_set_width(card->title, NAV_CARD_W);
    lv_obj_set_style_text_align(card->title, LV_TEXT_ALIGN_CENTER, 0);
}

static void layout_metric(ui_nav_card_t *card)
{
    if(card == NULL || card->value == NULL || card->unit == NULL) return;
    lv_obj_update_layout(card->root);
    int32_t value_w = lv_obj_get_width(card->value);
    int32_t value_h = lv_obj_get_height(card->value);
    int32_t unit_w = lv_obj_get_width(card->unit);
    int32_t unit_h = lv_obj_get_height(card->unit);
    int32_t group_w = value_w + NAV_VALUE_UNIT_GAP + unit_w;
    int32_t value_x = (NAV_CARD_W - group_w) / 2;
    int32_t value_y = NAV_CARD_H - NAV_VALUE_BOTTOM - value_h;
    lv_obj_set_pos(card->value, value_x, value_y);
    lv_obj_set_pos(card->unit, value_x + value_w + NAV_VALUE_UNIT_GAP,
                   value_y + value_h - unit_h);
}

static void layout_eta(ui_nav_card_t *card)
{
    if(card == NULL || card->value == NULL) return;
    lv_obj_update_layout(card->root);
    lv_obj_set_pos(card->value,
                   (NAV_CARD_W - lv_obj_get_width(card->value)) / 2,
                   NAV_CARD_H - NAV_VALUE_BOTTOM - lv_obj_get_height(card->value));
}

void nav_card_create(ui_nav_card_t *card, lv_obj_t *grid,
                     uint8_t column, uint8_t row,
                     const char *title, ui_nav_card_kind_t kind)
{
    if(card == NULL || grid == NULL) return;
    card->kind = kind;
    card->root = lv_obj_create(grid);
    lv_obj_remove_style_all(card->root);
    lv_obj_set_size(card->root, NAV_CARD_W, NAV_CARD_H);
    lv_obj_set_grid_cell(card->root, LV_GRID_ALIGN_STRETCH, column, 1,
                         LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_bg_color(card->root, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(card->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card->root, UI_COLOR_WHITE, 0);
    lv_obj_set_style_border_opa(card->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card->root, 1, 0);
    lv_obj_set_style_radius(card->root, NAV_CARD_RADIUS, 0);
    lv_obj_clear_flag(card->root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    card->title = card_label(card->root, NAV_CARD_PADDING, NAV_CARD_TITLE_Y,
                             NAV_CARD_W - (NAV_CARD_PADDING * 2), title,
                             UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_CENTER, UI_COLOR_WHITE);
    layout_title(card);
}

void ui_nav_card_create_metric(ui_nav_card_t *card, lv_obj_t *grid,
                               uint8_t column, uint8_t row,
                               const char *title, const char *unit)
{
    if(card == NULL || grid == NULL) return;
    nav_card_create(card, grid, column, row, title, UI_NAV_CARD_METRIC);
    card->content = lv_obj_create(card->root);
    lv_obj_remove_style_all(card->content);
    lv_obj_set_pos(card->content, 0, 0);
    lv_obj_set_size(card->content, NAV_CARD_W, NAV_CARD_H);
    lv_obj_clear_flag(card->content, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    card->value = card_label(card->content, 0, 0, LV_SIZE_CONTENT, "--",
                             &ui_font_misans_metric_60,
                             LV_TEXT_ALIGN_LEFT, UI_COLOR_WHITE);
    card->unit = card_label(card->content, 0, 0, LV_SIZE_CONTENT, unit,
                            UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_LEFT, UI_COLOR_WHITE);
    layout_metric(card);
}

void ui_nav_card_create_eta(ui_nav_card_t *card, lv_obj_t *grid,
                            uint8_t column, uint8_t row)
{
    if(card == NULL || grid == NULL) return;
    nav_card_create(card, grid, column, row, "预计到达时间", UI_NAV_CARD_ETA);
    card->value = card_label(card->root, 0, 0, LV_SIZE_CONTENT, "--:--",
                             UI_FONT_METRIC_VALUE_SMALL,
                             LV_TEXT_ALIGN_LEFT, UI_COLOR_WHITE);
    layout_eta(card);
}

static void draw_segment(lv_layer_t *layer, lv_point_precise_t p1,
                         lv_point_precise_t p2, int32_t width)
{
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = NAV_YELLOW;
    line.opa = LV_OPA_COVER;
    line.width = width;
    line.round_start = 1;
    line.round_end = 1;
    line.p1 = p1;
    line.p2 = p2;
    lv_draw_line(layer, &line);
}

static void direction_draw_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;
    ui_nav_card_t *card = lv_event_get_user_data(event);
    if(card == NULL) return;

    lv_obj_t *obj = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    int32_t cx = (area.x1 + area.x2) / 2;
    int32_t top = area.y1 + 4;
    int32_t bottom = area.y2 - 3;
    const int32_t width = 12;
    lv_draw_triangle_dsc_t arrow;
    lv_draw_triangle_dsc_init(&arrow);
    arrow.bg_color = NAV_YELLOW;
    arrow.bg_opa = LV_OPA_COVER;

    if(card->direction == NAV_DIRECTION_LEFT || card->direction == NAV_DIRECTION_RIGHT) {
        int32_t sign = card->direction == NAV_DIRECTION_RIGHT ? 1 : -1;
        int32_t corner_x = cx + sign * 30;
        draw_segment(layer, (lv_point_precise_t){cx, bottom},
                     (lv_point_precise_t){cx, top + 24}, width);
        draw_segment(layer, (lv_point_precise_t){cx, top + 24},
                     (lv_point_precise_t){corner_x, top + 24}, width);
        arrow.p[0] = (lv_point_precise_t){corner_x + sign * 18, top + 24};
        arrow.p[1] = (lv_point_precise_t){corner_x, top + 7};
        arrow.p[2] = (lv_point_precise_t){corner_x, top + 41};
    } else if(card->direction == NAV_DIRECTION_U_TURN) {
        draw_segment(layer, (lv_point_precise_t){cx + 16, bottom},
                     (lv_point_precise_t){cx + 16, top + 27}, width);
        draw_segment(layer, (lv_point_precise_t){cx + 16, top + 27},
                     (lv_point_precise_t){cx - 13, top + 27}, width);
        draw_segment(layer, (lv_point_precise_t){cx - 13, top + 27},
                     (lv_point_precise_t){cx - 13, top + 42}, width);
        arrow.p[0] = (lv_point_precise_t){cx - 13, top + 52};
        arrow.p[1] = (lv_point_precise_t){cx - 25, top + 39};
        arrow.p[2] = (lv_point_precise_t){cx - 1, top + 39};
    } else {
        draw_segment(layer, (lv_point_precise_t){cx, bottom},
                     (lv_point_precise_t){cx, top + 18}, width);
        arrow.p[0] = (lv_point_precise_t){cx, top};
        arrow.p[1] = (lv_point_precise_t){cx - 17, top + 22};
        arrow.p[2] = (lv_point_precise_t){cx + 17, top + 22};
    }
    lv_draw_triangle(layer, &arrow);
}

void ui_nav_card_create_direction(ui_nav_card_t *card, lv_obj_t *grid,
                                  uint8_t column, uint8_t row)
{
    if(card == NULL || grid == NULL) return;
    nav_card_create(card, grid, column, row, "导航航向", UI_NAV_CARD_DIRECTION);
    card->direction = NAV_DIRECTION_RIGHT;
    card->content = lv_obj_create(card->root);
    lv_obj_remove_style_all(card->content);
    lv_obj_set_pos(card->content, 0, NAV_DIRECTION_TOP);
    lv_obj_set_size(card->content, NAV_CARD_W, NAV_CARD_H - NAV_DIRECTION_TOP);
    lv_obj_clear_flag(card->content, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    card->arrow = lv_obj_create(card->content);
    lv_obj_remove_style_all(card->arrow);
    lv_obj_set_pos(card->arrow, NAV_DIRECTION_ARROW_X, 0);
    lv_obj_set_size(card->arrow, NAV_DIRECTION_ARROW_W, NAV_DIRECTION_ARROW_H);
    lv_obj_clear_flag(card->arrow, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(card->arrow, direction_draw_cb, LV_EVENT_DRAW_MAIN, card);
    card->instruction = card_label(card->content, 4, NAV_DIRECTION_TEXT_Y, NAV_CARD_W - 8,
                                   "前方 250 m 右转", UI_FONT_PAGE_TEXT,
                                   LV_TEXT_ALIGN_CENTER, UI_COLOR_WHITE);
}

void ui_nav_card_set_metric(ui_nav_card_t *card, const char *value)
{
    if(card == NULL || card->kind != UI_NAV_CARD_METRIC || card->value == NULL) return;
    lv_label_set_text(card->value, value != NULL ? value : "--");
    layout_metric(card);
}

void ui_nav_card_set_eta(ui_nav_card_t *card, uint8_t hour, uint8_t minute)
{
    if(card == NULL || card->kind != UI_NAV_CARD_ETA || card->value == NULL) return;
    lv_label_set_text_fmt(card->value, "%02u:%02u", (unsigned)hour, (unsigned)minute);
    layout_eta(card);
}

static const char *direction_name(nav_direction_t direction)
{
    if(direction == NAV_DIRECTION_LEFT) return "左转";
    if(direction == NAV_DIRECTION_RIGHT) return "右转";
    if(direction == NAV_DIRECTION_U_TURN) return "掉头";
    return "直行";
}

static void direction_fade_in_done(lv_anim_t *animation)
{
    (void)animation;
}

static void direction_fade_out_done(lv_anim_t *animation)
{
    ui_nav_card_t *card = lv_anim_get_user_data(animation);
    if(card == NULL || card->content == NULL) return;
    card->direction = card->pending_direction;
    lv_label_set_text_fmt(card->instruction, "前方 %lu m %s",
                          (unsigned long)card->pending_distance,
                          direction_name(card->direction));
    lv_obj_invalidate(card->arrow);
    ui_nav_animation_fade_move(card->content, NAV_DIRECTION_TOP + 8, NAV_DIRECTION_TOP,
                               LV_OPA_TRANSP,
                               LV_OPA_COVER, UI_ANIM_NORMAL / 2,
                               direction_fade_in_done, card);
}

void ui_nav_card_set_direction(ui_nav_card_t *card, nav_direction_t direction,
                               uint32_t distance_m, bool animate)
{
    if(card == NULL || card->kind != UI_NAV_CARD_DIRECTION || card->content == NULL) return;
    card->pending_direction = direction;
    card->pending_distance = distance_m;
    if(!animate) {
        card->direction = direction;
        lv_label_set_text_fmt(card->instruction, "前方 %lu m %s",
                              (unsigned long)distance_m, direction_name(direction));
        lv_obj_invalidate(card->arrow);
        return;
    }
    ui_nav_animation_fade_move(card->content, NAV_DIRECTION_TOP,
                               NAV_DIRECTION_TOP - 8, LV_OPA_COVER,
                               LV_OPA_TRANSP, UI_ANIM_NORMAL / 2,
                               direction_fade_out_done, card);
}
