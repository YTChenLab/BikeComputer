#include "ui_home_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"
#include "../models/ui_home_data_model.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HOME_ACTION_COUNT 6

typedef struct
{
    lv_obj_t *screen;
    ui_status_bar_t status;
    lv_obj_t *actions[HOME_ACTION_COUNT];
    ui_page_action_t action_ids[HOME_ACTION_COUNT];
    lv_obj_t *user_name;
    lv_obj_t *tagline;
    lv_obj_t *total_distance;
    lv_obj_t *total_time;
    lv_obj_t *total_ascent;
    lv_obj_t *last_date;
    lv_obj_t *last_distance;
    lv_obj_t *last_duration;
    lv_obj_t *last_ascent;
    lv_obj_t *last_caption;
    ui_page_action_cb_t callback;
    void *user_data;
    int32_t model_subscription;
    int8_t focus_index;
    bool active;
} ui_home_context_t;

static ui_home_context_t s_home;

static const lv_point_precise_t s_mountain_points[] = {
    {244, 132}, {276, 78}, {304, 108}, {334, 52},
    {366, 98}, {398, 70}, {448, 128}
};
static const lv_point_precise_t s_road_points[] = {
    {262, 178}, {292, 150}, {326, 132}, {360, 122},
    {397, 114}, {444, 110}
};
static const lv_point_precise_t s_route_points[] = {
    {10, 54}, {28, 46}, {42, 52}, {59, 35}, {76, 40},
    {92, 23}, {112, 28}, {136, 12}
};
static const lv_point_precise_t s_bike_frame_points[] = {
    {9, 31}, {22, 12}, {35, 31}, {16, 31}, {27, 9}, {34, 8}
};

static lv_obj_t *home_label(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t width, const char *text,
                            const lv_font_t *font, lv_color_t color,
                            lv_text_align_t align)
{
    lv_obj_t *label = ui_page_label(parent, x, y, width, text, font, align);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

static lv_obj_t *home_card(lv_obj_t *parent, int32_t x, int32_t y,
                           int32_t width, int32_t height)
{
    lv_obj_t *card = ui_page_box(parent, x, y, width, height,
                                 lv_color_hex(0x30373E), 1);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x090D10), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_set_style_clip_corner(card, true, 0);
    return card;
}

static lv_obj_t *home_line(lv_obj_t *parent,
                           const lv_point_precise_t *points,
                           uint32_t count, lv_color_t color,
                           int32_t width, lv_opa_t opacity)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, points, count);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_opa(line, opacity, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    return line;
}

static void home_action_event(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED ||
       s_home.callback == NULL) return;
    ui_page_action_t action = (ui_page_action_t)(uintptr_t)
                              lv_event_get_user_data(event);
    s_home.callback(action, s_home.user_data);
}

static void make_action(lv_obj_t *object, uint8_t index,
                        ui_page_action_t action)
{
    if(object == NULL || index >= HOME_ACTION_COUNT) return;
    s_home.actions[index] = object;
    s_home.action_ids[index] = action;
    lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(object, home_action_event, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)action);
    lv_obj_set_style_transform_scale(object, 248, LV_STATE_PRESSED);
    lv_obj_set_style_opa(object, LV_OPA_80, LV_STATE_PRESSED);
}

static void update_focus(void)
{
    for(uint8_t index = 0; index < HOME_ACTION_COUNT; index++) {
        bool focused = s_home.active && s_home.focus_index == (int8_t)index;
        lv_obj_set_style_border_color(s_home.actions[index],
                                      focused ? UI_TOKEN_COLOR_ACCENT :
                                                lv_color_hex(0x30373E), 0);
        lv_obj_set_style_border_width(s_home.actions[index],
                                      focused ? UI_TOKEN_BORDER_FOCUS :
                                                UI_TOKEN_BORDER_THIN, 0);
        lv_obj_set_style_shadow_color(s_home.actions[index],
                                      UI_TOKEN_COLOR_ACCENT, 0);
        lv_obj_set_style_shadow_width(s_home.actions[index],
                                      focused ? 8 : 0, 0);
        lv_obj_set_style_shadow_opa(s_home.actions[index],
                                    focused ? LV_OPA_30 : LV_OPA_TRANSP, 0);
    }
}

static void create_avatar(lv_obj_t *card)
{
    lv_obj_t *avatar = ui_page_box(card, 12, 8, 62, 62,
                                   lv_color_hex(0xC7CDD3), 1);
    lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(avatar, lv_color_hex(0x253947), 0);
    lv_obj_set_style_bg_grad_color(avatar, lv_color_hex(0xD58C3E), 0);
    lv_obj_set_style_bg_grad_dir(avatar, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);
    home_label(avatar, 0, 19, 62, "AC", &lv_font_montserrat_20,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER);
}

static void create_bike_icon(lv_obj_t *parent)
{
    lv_obj_t *left = ui_page_box(parent, 15, 31, 19, 19,
                                 UI_TOKEN_COLOR_CANVAS, 2);
    lv_obj_t *right = ui_page_box(parent, 46, 31, 19, 19,
                                  UI_TOKEN_COLOR_CANVAS, 2);
    lv_obj_set_style_radius(left, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(right, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_t *frame = home_line(parent, s_bike_frame_points,
                                sizeof(s_bike_frame_points) /
                                sizeof(s_bike_frame_points[0]),
                                UI_TOKEN_COLOR_CANVAS, 3, LV_OPA_COVER);
    lv_obj_set_pos(frame, 19, 18);
}

static void create_lifetime_decoration(lv_obj_t *card)
{
    lv_obj_t *mountains = home_line(
        card, s_mountain_points,
        sizeof(s_mountain_points) / sizeof(s_mountain_points[0]),
        lv_color_hex(0x374047), 2, LV_OPA_50);
    lv_obj_set_pos(mountains, 0, 0);
    lv_obj_t *road = home_line(
        card, s_road_points,
        sizeof(s_road_points) / sizeof(s_road_points[0]),
        lv_color_hex(0x697179), 5, LV_OPA_40);
    lv_obj_set_pos(road, 0, 0);
}

static void create_route_preview(lv_obj_t *card)
{
    lv_obj_t *preview = ui_page_box(card, 16, 48, 156, 78,
                                    lv_color_hex(0x2B333A), 1);
    lv_obj_set_style_radius(preview, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_set_style_bg_color(preview, lv_color_hex(0x0A1115), 0);
    lv_obj_set_style_bg_opa(preview, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(preview, true, 0);
    lv_obj_t *route = home_line(
        preview, s_route_points,
        sizeof(s_route_points) / sizeof(s_route_points[0]),
        UI_TOKEN_COLOR_ACCENT, 3, LV_OPA_COVER);
    lv_obj_set_pos(route, 3, 7);
    lv_obj_t *start = ui_page_box(preview, 8, 57, 10, 10,
                                  UI_TOKEN_COLOR_ACCENT, 2);
    lv_obj_t *end = ui_page_box(preview, 137, 12, 10, 10,
                                UI_TOKEN_COLOR_ACCENT, 2);
    lv_obj_set_style_radius(start, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(end, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(start, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_color(end, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(start, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(end, LV_OPA_COVER, 0);
}

static lv_obj_t *create_menu_row(lv_obj_t *screen, int32_t y,
                                  const char *icon, const char *title,
                                  const char *subtitle,
                                  uint8_t action_index,
                                  ui_page_action_t action)
{
    lv_obj_t *row = home_card(screen, 8, y, 464, 58);
    lv_obj_t *icon_box = ui_page_box(row, 14, 8, 42, 42,
                                     lv_color_hex(0x394149), 0);
    lv_obj_set_style_radius(icon_box, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_set_style_bg_color(icon_box, lv_color_hex(0x2A3036), 0);
    lv_obj_set_style_bg_opa(icon_box, LV_OPA_COVER, 0);
    home_label(icon_box, 0, 9, 42, icon, &lv_font_montserrat_22,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER);
    home_label(row, 70, 8, 260, title, UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    home_label(row, 70, 33, 260, subtitle, &lv_font_montserrat_12,
               UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    home_label(row, 416, 15, 28, LV_SYMBOL_RIGHT,
               &lv_font_montserrat_22, UI_TOKEN_COLOR_TEXT_PRIMARY,
               LV_TEXT_ALIGN_CENTER);
    make_action(row, action_index, action);
    return row;
}

static void update_data(const ui_home_data_t *data)
{
    if(data == NULL || s_home.screen == NULL) return;
    char text[64];
    lv_label_set_text(s_home.user_name, data->user_name);
    lv_label_set_text(s_home.tagline, data->tagline);
    snprintf(text, sizeof(text), "%lu",
             (unsigned long)data->total_distance_km);
    lv_label_set_text(s_home.total_distance, text);
    snprintf(text, sizeof(text), "%lu h  %u m",
             (unsigned long)data->total_time_hours,
             (unsigned int)data->total_time_minutes);
    lv_label_set_text(s_home.total_time, text);
    snprintf(text, sizeof(text), "%lu m",
             (unsigned long)data->total_ascent_m);
    lv_label_set_text(s_home.total_ascent, text);
    lv_label_set_text(s_home.last_date, data->last_ride_date);
    snprintf(text, sizeof(text), "%lu.%lu km",
             (unsigned long)(data->last_distance_tenths_km / 10U),
             (unsigned long)(data->last_distance_tenths_km % 10U));
    lv_label_set_text(s_home.last_distance, text);
    snprintf(text, sizeof(text), "%lu h %02lu m",
             (unsigned long)(data->last_duration_minutes / 60U),
             (unsigned long)(data->last_duration_minutes % 60U));
    lv_label_set_text(s_home.last_duration, text);
    snprintf(text, sizeof(text), "%lu m",
             (unsigned long)data->last_ascent_m);
    lv_label_set_text(s_home.last_ascent, text);
    lv_label_set_text(s_home.last_caption, data->last_ride_caption);
}

static void home_data_changed(const ui_home_data_t *data, void *user_data)
{
    (void)user_data;
    update_data(data);
}

lv_obj_t *ui_home_page_create(ui_page_action_cb_t callback,
                              void *user_data)
{
    memset(&s_home, 0, sizeof(s_home));
    s_home.callback = callback;
    s_home.user_data = user_data;
    s_home.focus_index = -1;

    s_home.screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_home.screen);
    lv_obj_set_size(s_home.screen, UI_TOKEN_SCREEN_WIDTH,
                    UI_TOKEN_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_home.screen, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(s_home.screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_home.screen, LV_OBJ_FLAG_SCROLLABLE);
    ui_page_status_create(&s_home.status, s_home.screen);

    lv_obj_t *identity = home_card(s_home.screen, 8, 72, 464, 78);
    create_avatar(identity);
    s_home.user_name = home_label(identity, 90, 12, 230, "",
                                  &lv_font_montserrat_22,
                                  UI_TOKEN_COLOR_TEXT_PRIMARY,
                                  LV_TEXT_ALIGN_LEFT);
    ui_page_line(identity, 90, 50, 20, 3, UI_TOKEN_COLOR_ACCENT);
    s_home.tagline = home_label(identity, 118, 43, 240, "",
                               &lv_font_montserrat_12,
                               UI_TOKEN_COLOR_TEXT_SECONDARY,
                               LV_TEXT_ALIGN_LEFT);
    home_label(identity, 374, 18, 80, "RIDE\nEXPLORE\nKEEP GOING",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);

    lv_obj_t *lifetime = home_card(s_home.screen, 8, 160, 464, 180);
    create_lifetime_decoration(lifetime);
    home_label(lifetime, 18, 17, 240, "TOTAL DISTANCE",
               &lv_font_montserrat_16, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);
    home_label(lifetime, 354, 17, 92, "RIDE DATA  >",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_RIGHT);
    s_home.total_distance = home_label(lifetime, 18, 43, 210, "",
                                       &ui_font_misans_metric_60,
                                       UI_TOKEN_COLOR_TEXT_PRIMARY,
                                       LV_TEXT_ALIGN_LEFT);
    home_label(lifetime, 192, 78, 48, "km", &lv_font_montserrat_20,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    home_label(lifetime, 52, 129, 100, "TOTAL TIME",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);
    home_label(lifetime, 18, 126, 28, LV_SYMBOL_REFRESH,
               &lv_font_montserrat_22, UI_TOKEN_COLOR_TEXT_PRIMARY,
               LV_TEXT_ALIGN_CENTER);
    s_home.total_time = home_label(lifetime, 52, 147, 128, "",
                                   &lv_font_montserrat_16,
                                   UI_TOKEN_COLOR_TEXT_PRIMARY,
                                   LV_TEXT_ALIGN_LEFT);
    ui_page_line(lifetime, 190, 130, 1, 36,
                 UI_TOKEN_COLOR_TEXT_TERTIARY);
    home_label(lifetime, 236, 129, 118, "TOTAL ASCENT",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);
    home_label(lifetime, 200, 126, 28, LV_SYMBOL_UP,
               &lv_font_montserrat_22, UI_TOKEN_COLOR_TEXT_PRIMARY,
               LV_TEXT_ALIGN_CENTER);
    s_home.total_ascent = home_label(lifetime, 236, 147, 130, "",
                                     &lv_font_montserrat_16,
                                     UI_TOKEN_COLOR_TEXT_PRIMARY,
                                     LV_TEXT_ALIGN_LEFT);
    home_label(lifetime, 374, 136, 72, "HIGHER\nFURTHER",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_RIGHT);

    lv_obj_t *start = ui_page_box(s_home.screen, 8, 350, 250, 88,
                                  UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_set_style_radius(start, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_set_style_bg_color(start, UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(start, LV_OPA_COVER, 0);
    create_bike_icon(start);
    home_label(start, 82, 22, 130, "开始骑行", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_CANVAS, LV_TEXT_ALIGN_LEFT);
    home_label(start, 82, 52, 132, "START RIDE",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_CANVAS,
               LV_TEXT_ALIGN_LEFT);
    home_label(start, 218, 29, 24, LV_SYMBOL_RIGHT,
               &lv_font_montserrat_22, UI_TOKEN_COLOR_CANVAS,
               LV_TEXT_ALIGN_CENTER);
    make_action(start, 0, UI_PAGE_ACTION_START_RIDE);

    lv_obj_t *navigation = home_card(s_home.screen, 266, 350, 206, 88);
    home_label(navigation, 16, 27, 42, LV_SYMBOL_GPS,
               &lv_font_montserrat_28, UI_TOKEN_COLOR_TEXT_PRIMARY,
               LV_TEXT_ALIGN_CENTER);
    home_label(navigation, 64, 22, 112, "开始导航", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    home_label(navigation, 64, 52, 120, "START NAVIGATION",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);
    home_label(navigation, 174, 29, 24, LV_SYMBOL_RIGHT,
               &lv_font_montserrat_22, UI_TOKEN_COLOR_TEXT_PRIMARY,
               LV_TEXT_ALIGN_CENTER);
    make_action(navigation, 1, UI_PAGE_ACTION_START_NAVIGATION);

    lv_obj_t *recent = home_card(s_home.screen, 8, 448, 464, 146);
    home_label(recent, 16, 12, 150, "最近一次骑行", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_home.last_date = home_label(recent, 248, 15, 184, "",
                                  &lv_font_montserrat_12,
                                  UI_TOKEN_COLOR_TEXT_SECONDARY,
                                  LV_TEXT_ALIGN_RIGHT);
    home_label(recent, 432, 13, 22, LV_SYMBOL_RIGHT,
               &lv_font_montserrat_20, UI_TOKEN_COLOR_TEXT_PRIMARY,
               LV_TEXT_ALIGN_CENTER);
    create_route_preview(recent);
    ui_page_line(recent, 184, 50, 1, 78,
                 UI_TOKEN_COLOR_TEXT_TERTIARY);
    home_label(recent, 200, 50, 72, "距离", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_home.last_distance = home_label(recent, 200, 70, 88, "",
                                      &lv_font_montserrat_16,
                                      UI_TOKEN_COLOR_TEXT_PRIMARY,
                                      LV_TEXT_ALIGN_LEFT);
    home_label(recent, 294, 50, 54, "用时", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_home.last_duration = home_label(recent, 294, 70, 86, "",
                                      &lv_font_montserrat_16,
                                      UI_TOKEN_COLOR_TEXT_PRIMARY,
                                      LV_TEXT_ALIGN_LEFT);
    home_label(recent, 386, 50, 54, "爬升", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_home.last_ascent = home_label(recent, 386, 70, 68, "",
                                    &lv_font_montserrat_16,
                                    UI_TOKEN_COLOR_TEXT_PRIMARY,
                                    LV_TEXT_ALIGN_LEFT);
    s_home.last_caption = home_label(recent, 200, 106, 240, "",
                                     UI_FONT_PAGE_TEXT,
                                     UI_TOKEN_COLOR_TEXT_PRIMARY,
                                     LV_TEXT_ALIGN_LEFT);
    make_action(recent, 2, UI_PAGE_ACTION_HISTORY);

    create_menu_row(s_home.screen, 604, LV_SYMBOL_LIST,
                    "历史记录", "RIDE HISTORY", 3,
                    UI_PAGE_ACTION_HISTORY);
    create_menu_row(s_home.screen, 670, LV_SYMBOL_OK,
                    "我的成就", "MY ACHIEVEMENTS", 4,
                    UI_PAGE_ACTION_ACHIEVEMENT);
    create_menu_row(s_home.screen, 736, LV_SYMBOL_SETTINGS,
                    "系统设置", "SYSTEM SETTINGS", 5,
                    UI_PAGE_ACTION_SETTINGS);

    ui_home_data_t data;
    if(ui_home_data_model_get(&data)) update_data(&data);
    s_home.model_subscription = ui_home_data_model_subscribe(
        home_data_changed, NULL);
    update_focus();
    return s_home.screen;
}

void ui_home_page_set_active(bool active)
{
    s_home.active = active;
    s_home.focus_index = active ? 0 : -1;
    update_focus();
}

bool ui_home_page_handle_input(ui_input_event_t input)
{
    if(!s_home.active) return false;
    if(input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW) {
        int32_t direction = input == UI_INPUT_ENCODER_CW ? 1 : -1;
        int32_t next = s_home.focus_index;
        if(next < 0) next = direction > 0 ? 0 : HOME_ACTION_COUNT - 1;
        else {
            next += direction;
            if(next < 0) next = HOME_ACTION_COUNT - 1;
            if(next >= HOME_ACTION_COUNT) next = 0;
        }
        s_home.focus_index = (int8_t)next;
        update_focus();
        return true;
    }
    if(input == UI_INPUT_ENCODER_PRESS && s_home.focus_index >= 0 &&
       s_home.focus_index < HOME_ACTION_COUNT) {
        if(s_home.callback != NULL)
            s_home.callback(s_home.action_ids[s_home.focus_index],
                            s_home.user_data);
        return true;
    }
    return false;
}
