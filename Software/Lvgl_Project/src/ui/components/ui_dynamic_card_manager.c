#include "ui_dynamic_card_manager.h"
#include "../core/ui_text.h"
#include "ui_metric_card.h"
#include "../models/ui_dashboard_layout.h"

#include "../core/ui_colors.h"
#include "../core/ui_app_state.h"
#include "../core/ui_bottom_surface.h"
#include "../core/ui_motion.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"
#include "../design/ui_alignment.h"
#include "../../services/ui_command_service.h"
#include "../../services/ui_navigation_service.h"
#include "../models/ui_weather_model.h"
#include "../models/ui_ride_data_model.h"
#include "../models/ui_ride_summary_model.h"

#include "lvgl/src/draw/lv_draw_line.h"
#include "lvgl/src/draw/lv_draw_triangle.h"

#include <stddef.h>

#define DOCK_WIDTH              UI_BOTTOM_SURFACE_WIDTH
#define DOCK_HEIGHT             UI_BOTTOM_SURFACE_NORMAL_HEIGHT
#define INDICATOR_X             9
#define INDICATOR_Y             73
#define INDICATOR_DOT_SIZE      9
#define INDICATOR_DOT_STEP      22
#define MUSIC_CONTROL_COUNT     4
#define MUSIC_PRESS_SCALE       236
#define MUSIC_NORMAL_SCALE      256

typedef enum
{
    MUSIC_CONTROL_PREVIOUS = 0,
    MUSIC_CONTROL_PLAY_PAUSE,
    MUSIC_CONTROL_NEXT,
    MUSIC_CONTROL_MODE
} music_control_id_t;

typedef struct
{
    lv_obj_t *viewport;
    lv_obj_t *cards[UI_DYNAMIC_CARD_COUNT];
    lv_obj_t *indicator_overlay;
    lv_obj_t *indicator_dots[UI_DYNAMIC_CARD_COUNT];
    lv_obj_t *music_controls[MUSIC_CONTROL_COUNT];
    ui_metric_card_t extended_metrics[4];
    lv_obj_t *extended_page_label;
    ui_ride_data_t latest_ride_data;
    unsigned extended_page;
    lv_obj_t *music_play_icon;
    lv_obj_t *music_track_label;
    lv_obj_t *music_artist_label;
    lv_obj_t *music_mode_label;
    lv_obj_t *weather_temperature;
    lv_obj_t *weather_condition;
    lv_obj_t *weather_advice;
    lv_obj_t *weather_wind;
    lv_obj_t *weather_rain;
    lv_obj_t *weather_air;
    lv_obj_t *navigation_arrow;
    lv_obj_t *navigation_distance;
    lv_obj_t *navigation_road;
    lv_obj_t *navigation_instruction;
    lv_obj_t *navigation_remaining;
    lv_obj_t *navigation_eta;
    lv_obj_t *navigation_ascent;
    ui_dynamic_card_id_t active_card;
    ui_dynamic_card_id_t target_card;
    ui_dynamic_card_mode_t mode;
    int8_t control_focus;
    int32_t music_state_subscription;
    int32_t weather_subscription;
    int32_t ride_data_subscription;
    int32_t navigation_subscription;
    bool created;
} dynamic_card_manager_t;

static dynamic_card_manager_t s_manager;

static void update_control_visuals(void);
static void execute_music_control(uint8_t control_index);

static const char * const s_weather_conditions[] = {
    "晴朗", "多云", "小雨"
};

static const char * const s_weather_advice[] = {
    "适合骑行", "适合骑行", "注意湿滑"
};

static const char * const s_music_tracks[] = {
    "End of a Long Day",
    "Speed of Light",
    "Road to Tomorrow"
};

static const char * const s_music_artists[] = {
    "Arknights",
    "PRTS Cycling Mix",
    "Rhodes Island"
};

static const char * const s_music_modes[] = {
    "顺序播放",
    "单曲循环",
    "随机播放"
};

static void apply_music_state(const ui_app_state_t *state)
{
    if(state == NULL || s_manager.music_play_icon == NULL) return;
    uint8_t track = (uint8_t)(state->media_track % 3U);
    uint8_t mode = state->media_mode < UI_MEDIA_MODE_COUNT ?
                   (uint8_t)state->media_mode : 0U;
    lv_label_set_text(s_manager.music_play_icon,
                      state->media_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_label_set_text(s_manager.music_track_label, s_music_tracks[track]);
    lv_label_set_text(s_manager.music_artist_label, s_music_artists[track]);
    lv_label_set_text(s_manager.music_mode_label, s_music_modes[mode]);
}

static void music_state_changed(const ui_app_state_t *state,
                                ui_app_state_change_mask_t changed,
                                void *user_data)
{
    (void)changed;
    (void)user_data;
    apply_music_state(state);
}

static void apply_weather(const ui_weather_data_t *weather)
{
    if(weather == NULL || s_manager.weather_temperature == NULL) return;
    if(weather->quality_known && !weather->valid) {
        lv_label_set_text(s_manager.weather_temperature, "--");
        lv_label_set_text(s_manager.weather_condition, "天气未更新");
        lv_label_set_text(s_manager.weather_advice, "等待有效天气数据");
        lv_label_set_text(s_manager.weather_wind, "--");
        lv_label_set_text(s_manager.weather_rain, "--");
        lv_label_set_text(s_manager.weather_air, "--");
        return;
    }
    ui_weather_condition_t condition = weather->condition;
    if(condition >= UI_WEATHER_CONDITION_COUNT)
        condition = UI_WEATHER_CLOUDY;
    char text[32];
    lv_snprintf(text, sizeof(text), "%d", weather->temperature_c);
    lv_label_set_text(s_manager.weather_temperature, text);
    lv_label_set_text(s_manager.weather_condition,
                      s_weather_conditions[condition]);
    lv_label_set_text(s_manager.weather_advice,
                      s_weather_advice[condition]);
    lv_snprintf(text, sizeof(text), "%u km/h", weather->wind_kmh);
    lv_label_set_text(s_manager.weather_wind, text);
    lv_snprintf(text, sizeof(text), "%u%%", weather->rain_percent);
    lv_label_set_text(s_manager.weather_rain, text);
    lv_snprintf(text, sizeof(text), "AQI  %u", weather->air_quality_index);
    lv_label_set_text(s_manager.weather_air, text);
}

static void weather_changed(const ui_weather_data_t *weather,
                            void *user_data)
{
    (void)user_data;
    apply_weather(weather);
}

static void apply_ride_data(const ui_ride_data_t *data)
{
    if(data == NULL || s_manager.extended_metrics[0].root == NULL) return;
    s_manager.latest_ride_data = *data;
    static const ui_ride_field_t fields[8] = {
        UI_RIDE_FIELD_AVERAGE_SPEED, UI_RIDE_FIELD_MAXIMUM_SPEED,
        UI_RIDE_FIELD_ALTITUDE, UI_RIDE_FIELD_ASCENT, UI_RIDE_FIELD_CALORIES,
        UI_RIDE_FIELD_TEMPERATURE, UI_RIDE_FIELD_HEART_RATE, UI_RIDE_FIELD_LAP
    };
    for(unsigned i = 0; i < 4; ++i) {
        unsigned index = s_manager.extended_page * 4 + i;
        ui_ride_field_t field = fields[index];
        ui_metric_card_t *card = &s_manager.extended_metrics[i];
        const ui_dashboard_field_definition_t *definition = ui_dashboard_field_definition(field);
        bool special = index >= 6;
        lv_label_set_text(card->title, special ? (index == 6 ? "心率区间" : "当前圈数") : definition->title);
        ui_metric_card_set_unit(card, special ? (index == 6 ? "Z1-Z5" : "圈") : definition->unit);
        ui_metric_card_set_value_font(card, index == 6 ? &ui_font_misans_zone_24 :
            special ? &ui_font_misans_ui_24 : UI_FONT_METRIC_VALUE_SMALL);
        char text[48];
        if(!special) ui_dashboard_format_field(field, data, lv_tick_get(), text, sizeof(text));
        else if(!ui_ride_data_field_valid(data, field, lv_tick_get())) lv_snprintf(text,sizeof(text),"--");
        else if(index == 6) {
            uint16_t heart[4], power[4];
            ui_ride_summary_model_get_zones(heart, power);
            unsigned zone = 1;
            while(zone < 5 && data->heart_rate >= heart[zone - 1]) ++zone;
            lv_snprintf(text, sizeof(text), "Z%u", zone);
        } else lv_snprintf(text, sizeof(text), "%u/%u", data->current_lap, data->total_laps);
        ui_metric_card_set_value(card, text);
    }
    lv_label_set_text(s_manager.extended_page_label, s_manager.extended_page ? "DATA 2/2" : "DATA 1/2");
}

static void change_extended_page(int direction)
{
    s_manager.extended_page = (s_manager.extended_page + (direction != 0)) % 2;
    apply_ride_data(&s_manager.latest_ride_data);
    for(unsigned i = 0; i < 4; ++i) {
        int32_t y = 44 + (i / 2) * 88;
        ui_motion_translate_fade(s_manager.extended_metrics[i].root, y + 4, y,
            LV_OPA_40, LV_OPA_COVER, 140, NULL, NULL);
    }
}

static void extended_clicked(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) change_extended_page(1);
}

void ui_dynamic_card_manager_refresh_data(const ui_ride_data_t *data) { apply_ride_data(data); }

static void ride_data_changed(const ui_ride_data_t *data, uint32_t version,
                              void *user_data)
{
    (void)version;
    (void)user_data;
    apply_ride_data(data);
}

static const char *navigation_instruction(nav_maneuver_t maneuver)
{
    switch(maneuver) {
        case NAV_MANEUVER_LEFT: return "左转进入前方道路";
        case NAV_MANEUVER_RIGHT: return "右转进入前方道路";
        case NAV_MANEUVER_SLIGHT_LEFT: return "靠左继续行驶";
        case NAV_MANEUVER_SLIGHT_RIGHT: return "靠右继续行驶";
        case NAV_MANEUVER_U_TURN: return "前方安全位置掉头";
        case NAV_MANEUVER_ROUNDABOUT: return "进入环岛";
        case NAV_MANEUVER_ARRIVE: return "即将到达目的地";
        default: return "沿当前道路直行";
    }
}

static void apply_navigation(const nav_model_t *model)
{
    if(model == NULL || s_manager.navigation_distance == NULL) return;
    char text[40];
    lv_snprintf(text, sizeof(text), "%lu",
                (unsigned long)model->direction_distance);
    lv_label_set_text(s_manager.navigation_distance, text);
    (void)ui_text_set_label(s_manager.navigation_road,
                      model->road_name[0] != '\0' ? model->road_name :
                                                   "未命名道路", 20);
    lv_label_set_text(s_manager.navigation_instruction,
                      navigation_instruction(model->direction));
    lv_snprintf(text, sizeof(text), "%.1f km", (double)model->distance);
    lv_label_set_text(s_manager.navigation_remaining, text);
    lv_snprintf(text, sizeof(text), "%02u:%02u",
                model->eta_hour, model->eta_min);
    lv_label_set_text(s_manager.navigation_eta, text);
    lv_snprintf(text, sizeof(text), "%lu m",
                (unsigned long)model->remaining_ascent_m);
    lv_label_set_text(s_manager.navigation_ascent, text);

    int32_t rotation = 2700;
    if(model->direction == NAV_MANEUVER_RIGHT ||
       model->direction == NAV_MANEUVER_SLIGHT_RIGHT ||
       model->direction == NAV_MANEUVER_SHARP_RIGHT)
        rotation = 0;
    else if(model->direction == NAV_MANEUVER_LEFT ||
            model->direction == NAV_MANEUVER_SLIGHT_LEFT ||
            model->direction == NAV_MANEUVER_SHARP_LEFT)
        rotation = 1800;
    else if(model->direction == NAV_MANEUVER_U_TURN)
        rotation = 900;
    lv_obj_set_style_transform_rotation(s_manager.navigation_arrow,
                                        rotation, 0);
}

static void navigation_changed(const nav_model_t *model, uint32_t version,
                               void *user_data)
{
    (void)version;
    (void)user_data;
    apply_navigation(model);
}

static lv_obj_t *create_rect(lv_obj_t *parent, int32_t x, int32_t y,
                             int32_t width, int32_t height, lv_color_t color,
                             lv_opa_t opa)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static lv_obj_t *create_label(lv_obj_t *parent, int32_t x, int32_t y,
                              int32_t width, const char *text,
                              const lv_font_t *font, lv_color_t color,
                              lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

static lv_obj_t *create_card_header(lv_obj_t *card, const char *symbol,
                               const char *title, const char *subtitle,
                               const char *page)
{
    const lv_color_t secondary = UI_TOKEN_COLOR_TEXT_TERTIARY;
    create_label(card, 46, (40 - lv_font_montserrat_22.line_height) / 2, 28, symbol, &lv_font_montserrat_22,
                 UI_COLOR_YELLOW, LV_TEXT_ALIGN_CENTER);
    create_label(card, 76, (40 - UI_FONT_PAGE_TEXT->line_height) / 2, 112, title, UI_FONT_PAGE_TEXT,
                 UI_COLOR_YELLOW, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *subtitle_label = create_label(card, 188, (40 - lv_font_montserrat_16.line_height) / 2, 170, subtitle, &lv_font_montserrat_16,
                 secondary, LV_TEXT_ALIGN_LEFT);
    create_label(card, 410, (40 - lv_font_montserrat_16.line_height) / 2, 34, page, &lv_font_montserrat_16,
                 secondary, LV_TEXT_ALIGN_RIGHT);
    return subtitle_label;
}

static lv_obj_t *create_card(lv_obj_t *viewport)
{
    lv_obj_t *card = lv_obj_create(viewport);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, DOCK_WIDTH, DOCK_HEIGHT);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 0, 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_SNAPPABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
                            LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
    /* Clickable controls may hand a vertical drag to the viewport.  The viewport
     * itself still disables chaining, so the gesture cannot escape the Dock. */
    lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    return card;
}

static void create_extended_card(lv_obj_t *card)
{
    s_manager.extended_page_label = create_card_header(card, LV_SYMBOL_BARS, "骑行扩展", "DATA 1/2", "1/4");
    lv_obj_set_width(s_manager.extended_page_label, 210);
    lv_obj_set_style_text_align(s_manager.extended_page_label, LV_TEXT_ALIGN_CENTER, 0);
    create_rect(card, 48, 130, 396, 1, UI_TOKEN_COLOR_SURFACE_OUTLINE, LV_OPA_COVER);
    create_rect(card, 247, 47, 1, 172, UI_TOKEN_COLOR_SURFACE_OUTLINE, LV_OPA_COVER);
    for(unsigned i = 0; i < 4; ++i) {
        ui_metric_card_t *metric = &s_manager.extended_metrics[i];
        ui_metric_card_create(metric, card, 48 + (i % 2) * 199, 44 + (i / 2) * 88,
            196, 86, "", "--", "", UI_FONT_METRIC_VALUE_SMALL);
        lv_obj_set_style_text_font(metric->title, UI_FONT_PAGE_TEXT, 0);
        lv_obj_set_style_text_color(metric->title, UI_TOKEN_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(metric->unit, UI_FONT_PAGE_TEXT, 0);
        lv_obj_set_style_text_color(metric->unit, UI_TOKEN_COLOR_TEXT_SECONDARY, 0);
        lv_obj_clear_flag(metric->root, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, extended_clicked, LV_EVENT_CLICKED, NULL);
}

static void navigation_arrow_draw_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *obj = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);

    const lv_point_precise_t start = {area.x1 + 27, area.y2 - 10};
    const lv_point_precise_t corner = {area.x1 + 27, area.y1 + 34};
    const lv_point_precise_t end = {area.x1 + 68, area.y1 + 34};
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.round_start = 1;
    line.round_end = 1;
    line.color = UI_COLOR_YELLOW;
    line.opa = LV_OPA_20;
    line.width = 28;
    line.p1 = start;
    line.p2 = corner;
    lv_draw_line(layer, &line);
    line.p1 = corner;
    line.p2 = end;
    lv_draw_line(layer, &line);

    lv_draw_triangle_dsc_t triangle;
    lv_draw_triangle_dsc_init(&triangle);
    triangle.bg_color = UI_COLOR_YELLOW;
    triangle.bg_opa = LV_OPA_20;
    triangle.p[0] = (lv_point_precise_t){area.x1 + 101, area.y1 + 34};
    triangle.p[1] = (lv_point_precise_t){area.x1 + 66, area.y1 + 5};
    triangle.p[2] = (lv_point_precise_t){area.x1 + 66, area.y1 + 63};
    lv_draw_triangle(layer, &triangle);

    line.color = UI_COLOR_WHITE;
    line.opa = LV_OPA_COVER;
    line.width = 17;
    line.p1 = start;
    line.p2 = corner;
    lv_draw_line(layer, &line);
    line.p1 = corner;
    line.p2 = end;
    lv_draw_line(layer, &line);

    triangle.bg_color = UI_COLOR_WHITE;
    triangle.bg_opa = LV_OPA_COVER;
    triangle.p[0] = (lv_point_precise_t){area.x1 + 96, area.y1 + 34};
    triangle.p[1] = (lv_point_precise_t){area.x1 + 69, area.y1 + 12};
    triangle.p[2] = (lv_point_precise_t){area.x1 + 69, area.y1 + 56};
    lv_draw_triangle(layer, &triangle);
}

static void create_navigation_card(lv_obj_t *card)
{
    const lv_color_t secondary = lv_color_hex(0xA9ADAE);
    const lv_color_t border = UI_TOKEN_COLOR_SURFACE_BORDER;
    create_card_header(card, LV_SYMBOL_GPS, "导航中", "NAVIGATION", "2/4");

    lv_obj_t *arrow = lv_obj_create(card);
    s_manager.navigation_arrow = arrow;
    lv_obj_remove_style_all(arrow);
    lv_obj_set_pos(arrow, 42, 59);
    lv_obj_set_size(arrow, 103, 112);
    lv_obj_set_style_transform_pivot_x(arrow, 51, 0);
    lv_obj_set_style_transform_pivot_y(arrow, 56, 0);
    lv_obj_clear_flag(arrow, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(arrow, navigation_arrow_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    s_manager.navigation_distance = create_label(
        card, 144, 55, 126, "300", UI_FONT_METRIC_VALUE_SMALL,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_RIGHT);
    create_label(card, 273, 75, 25, "m", UI_FONT_SPEED_UNIT,
                 UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
    s_manager.navigation_road = create_label(
        card, 146, 108, 146, "中山路", UI_FONT_PAGE_TEXT,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_CENTER);
    s_manager.navigation_instruction = create_label(
        card, 146, 137, 146, "右转进入中山路", UI_FONT_PAGE_TEXT,
        secondary, LV_TEXT_ALIGN_CENTER);

    create_rect(card, 302, 52, 1, 129, border, LV_OPA_80);
    create_label(card, 324, 50, 120, "剩余里程", UI_FONT_PAGE_TEXT,
                 secondary, LV_TEXT_ALIGN_LEFT);
    s_manager.navigation_remaining = create_label(
        card, 324, 73, 120, "12.8 km", UI_FONT_PAGE_TEXT,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
    create_label(card, 324, 100, 120, "预计到达", UI_FONT_PAGE_TEXT,
                 secondary, LV_TEXT_ALIGN_LEFT);
    s_manager.navigation_eta = create_label(
        card, 324, 123, 120, "10:52", UI_FONT_PAGE_TEXT,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
    create_label(card, 324, 150, 120, "剩余爬升", UI_FONT_PAGE_TEXT,
                 secondary, LV_TEXT_ALIGN_LEFT);
    s_manager.navigation_ascent = create_label(
        card, 324, 173, 120, "156 m", UI_FONT_PAGE_TEXT,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
}

static lv_obj_t *create_music_control(lv_obj_t *card, int32_t x, int32_t y,
                                      int32_t width, int32_t height,
                                      const char *symbol, const lv_font_t *font)
{
    lv_obj_t *control = create_rect(card, x, y, width, height,
                                    UI_COLOR_YELLOW, LV_OPA_TRANSP);
    lv_obj_set_style_radius(control, height / 2, 0);
    lv_obj_set_style_border_color(control, UI_COLOR_YELLOW, 0);
    lv_obj_set_style_border_opa(control, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(control, 1, 0);
    lv_obj_set_style_transform_pivot_x(control, width / 2, 0);
    lv_obj_set_style_transform_pivot_y(control, height / 2, 0);
    lv_obj_set_style_transform_scale(control, MUSIC_NORMAL_SCALE, 0);
    lv_obj_add_flag(control, LV_OBJ_FLAG_CLICKABLE);
    ui_align_label_center(create_label(control, 0, 0, width, symbol,
        font, UI_COLOR_WHITE, LV_TEXT_ALIGN_CENTER));
    return control;
}

static void music_control_scale_anim_cb(void *object, int32_t scale)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)object, scale, 0);
}

static void animate_music_control_scale(lv_obj_t *control, int32_t target,
                                        uint32_t duration)
{
    ui_motion_value(control, music_control_scale_anim_cb,
                    lv_obj_get_style_transform_scale_x(control, 0), target,
                    duration, target == MUSIC_NORMAL_SCALE ?
                    lv_anim_path_ease_out : lv_anim_path_ease_in,
                    NULL, NULL);
}

static void music_control_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *control = lv_event_get_current_target(event);
    uint8_t control_index = (uint8_t)(uintptr_t)lv_event_get_user_data(event);

    if(code == LV_EVENT_PRESSED)
    {
        animate_music_control_scale(control, MUSIC_PRESS_SCALE, 65);
        lv_obj_set_style_bg_opa(control, LV_OPA_30, 0);
        lv_obj_set_style_border_opa(control, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_color(control, UI_COLOR_YELLOW, 0);
        lv_obj_set_style_shadow_width(control, 12, 0);
        lv_obj_set_style_shadow_opa(control, LV_OPA_40, 0);
    }
    else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
    {
        animate_music_control_scale(control, MUSIC_NORMAL_SCALE, 145);
        update_control_visuals();
    }
    else if(code == LV_EVENT_CLICKED)
    {
        if(s_manager.mode == UI_DYNAMIC_CARD_CONTROL)
        {
            s_manager.control_focus = (int8_t)control_index;
        }
        execute_music_control(control_index);
        update_control_visuals();
    }
}

static void make_music_control_interactive(lv_obj_t *control,
                                           music_control_id_t control_id)
{
    lv_obj_add_flag(control, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(control, music_control_event_cb, LV_EVENT_ALL,
                        (void *)(uintptr_t)control_id);
}

static void create_music_card(lv_obj_t *card)
{
    const lv_color_t secondary = lv_color_hex(0x999E9F);
    create_card_header(card, LV_SYMBOL_AUDIO, "音乐播放", "MUSIC", "3/4");

    lv_obj_t *disc = create_rect(card, 105, 67, 87, 87,
                                 lv_color_hex(0x443D10), LV_OPA_COVER);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    create_rect(disc, 34, 34, 19, 19, lv_color_hex(0x0A0D0C), LV_OPA_COVER);

    lv_obj_t *cover = create_rect(card, 51, 57, 104, 104,
                                  lv_color_hex(0x111714), LV_OPA_COVER);
    lv_obj_set_style_border_color(cover, lv_color_hex(0x59605E), 0);
    lv_obj_set_style_border_width(cover, 1, 0);
    lv_obj_set_style_radius(cover, 8, 0);
    create_label(cover, 8, 25, 88, "RIDE", &lv_font_montserrat_16,
                 UI_COLOR_YELLOW, LV_TEXT_ALIGN_CENTER);
    create_label(cover, 8, 48, 88, "MIX", &lv_font_montserrat_24,
                 UI_COLOR_WHITE, LV_TEXT_ALIGN_CENTER);

    s_manager.music_track_label = create_label(
        card, 174, 57, 169, s_music_tracks[0], &lv_font_montserrat_16,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT
    );
    s_manager.music_artist_label = create_label(
        card, 174, 84, 169, s_music_artists[0], &lv_font_montserrat_14,
        secondary, LV_TEXT_ALIGN_LEFT
    );
    create_label(card, 174, 111, 169, "02:17 / 04:26", &lv_font_montserrat_14,
                 UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
    create_rect(card, 174, 137, 160, 5, secondary, LV_OPA_60);
    create_rect(card, 174, 137, 89, 5, UI_COLOR_YELLOW, LV_OPA_COVER);

    create_rect(card, 351, 50, 1, 133, UI_TOKEN_COLOR_SURFACE_BORDER,
                UI_TOKEN_OPA_SECONDARY);
    s_manager.music_controls[0] = create_music_control(
        card, 367, 48, 72, 42, LV_SYMBOL_UP, &lv_font_montserrat_22
    );
    s_manager.music_controls[1] = create_music_control(
        card, 370, 91, 66, 54, LV_SYMBOL_PLAY, &lv_font_montserrat_24
    );
    lv_obj_set_style_border_opa(s_manager.music_controls[1], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_manager.music_controls[1], LV_RADIUS_CIRCLE, 0);
    s_manager.music_play_icon = lv_obj_get_child(s_manager.music_controls[1], 0);
    s_manager.music_controls[2] = create_music_control(
        card, 367, 146, 72, 42, LV_SYMBOL_DOWN, &lv_font_montserrat_22
    );

    s_manager.music_controls[3] = create_rect(
        card, 174, 159, 160, 38, UI_COLOR_YELLOW, LV_OPA_TRANSP
    );
    lv_obj_set_style_radius(s_manager.music_controls[3], 19, 0);
    lv_obj_set_style_border_color(s_manager.music_controls[3], UI_COLOR_YELLOW, 0);
    lv_obj_set_style_border_width(s_manager.music_controls[3], 1, 0);
    lv_obj_set_style_border_opa(s_manager.music_controls[3], LV_OPA_TRANSP, 0);
    lv_obj_set_style_transform_pivot_x(s_manager.music_controls[3], 80, 0);
    lv_obj_set_style_transform_pivot_y(s_manager.music_controls[3], 19, 0);
    lv_obj_set_style_transform_scale(s_manager.music_controls[3], MUSIC_NORMAL_SCALE, 0);
    lv_obj_t *mode_icon = create_label(s_manager.music_controls[3], 8, 0,
        25, LV_SYMBOL_LOOP, &lv_font_montserrat_16, UI_COLOR_WHITE,
        LV_TEXT_ALIGN_CENTER);
    ui_align_center_y(mode_icon, 8);
    s_manager.music_mode_label = create_label(
        s_manager.music_controls[3], 34, 7, 116, s_music_modes[0],
        UI_FONT_PAGE_TEXT, secondary, LV_TEXT_ALIGN_LEFT
    );
    ui_align_center_y(s_manager.music_mode_label, 34);

    make_music_control_interactive(s_manager.music_controls[0], MUSIC_CONTROL_PREVIOUS);
    make_music_control_interactive(s_manager.music_controls[1], MUSIC_CONTROL_PLAY_PAUSE);
    make_music_control_interactive(s_manager.music_controls[2], MUSIC_CONTROL_NEXT);
    make_music_control_interactive(s_manager.music_controls[3], MUSIC_CONTROL_MODE);
}

static void create_weather_graphic(lv_obj_t *card)
{
    lv_obj_t *sun = create_rect(card, 92, 69, 62, 62,
                                UI_COLOR_YELLOW, LV_OPA_COVER);
    lv_obj_set_style_radius(sun, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_color(sun, UI_COLOR_YELLOW, 0);
    lv_obj_set_style_shadow_width(sun, 15, 0);
    lv_obj_set_style_shadow_opa(sun, LV_OPA_30, 0);

    lv_obj_t *cloud_left = create_rect(card, 58, 111, 57, 47,
                                       UI_COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_style_radius(cloud_left, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *cloud_mid = create_rect(card, 83, 95, 65, 65,
                                      UI_COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_style_radius(cloud_mid, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *cloud_base = create_rect(card, 61, 128, 111, 34,
                                       UI_COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_style_radius(cloud_base, 17, 0);
}

static void create_weather_card(lv_obj_t *card)
{
    const lv_color_t secondary = UI_TOKEN_COLOR_TEXT_SECONDARY;
    create_card_header(card, LV_SYMBOL_CHARGE, "天气信息", "WEATHER", "4/4");
    create_weather_graphic(card);

    s_manager.weather_temperature = create_label(
        card, 178, 66, 92, "22", UI_FONT_METRIC_VALUE_SMALL,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_RIGHT);
    create_label(card, 272, 83, 48, "℃", UI_FONT_PAGE_TEXT,
                 UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
    s_manager.weather_condition = create_label(
        card, 194, 119, 112, "多云", UI_FONT_PAGE_TEXT,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_CENTER);
    s_manager.weather_advice = create_label(
        card, 194, 147, 112, "适合骑行", UI_FONT_PAGE_TEXT,
        secondary, LV_TEXT_ALIGN_CENTER);

    create_rect(card, 317, 52, 1, 129, UI_TOKEN_COLOR_SURFACE_BORDER,
                UI_TOKEN_OPA_SECONDARY);
    create_label(card, 340, 53, 105, "风速", UI_FONT_PAGE_TEXT,
                 secondary, LV_TEXT_ALIGN_LEFT);
    s_manager.weather_wind = create_label(
        card, 340, 76, 105, "12 km/h", UI_FONT_PAGE_TEXT,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
    create_label(card, 340, 108, 105, "降雨概率", UI_FONT_PAGE_TEXT,
                 secondary, LV_TEXT_ALIGN_LEFT);
    s_manager.weather_rain = create_label(
        card, 340, 131, 105, "10%", UI_FONT_PAGE_TEXT,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
    s_manager.weather_air = create_label(
        card, 340, 163, 105, "AQI  42", UI_FONT_PAGE_TEXT,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
}

static void update_indicator(void)
{
    for(uint8_t index = 0; index < UI_DYNAMIC_CARD_COUNT; index++)
    {
        bool active = index == (uint8_t)s_manager.active_card;
        lv_obj_set_style_bg_color(s_manager.indicator_dots[index],
                                  active ? UI_COLOR_YELLOW : UI_TOKEN_COLOR_INDICATOR, 0);
        lv_obj_set_style_bg_opa(s_manager.indicator_dots[index],
                                active ? LV_OPA_COVER : LV_OPA_70, 0);
        lv_obj_set_style_shadow_color(s_manager.indicator_dots[index], UI_COLOR_YELLOW, 0);
        lv_obj_set_style_shadow_width(s_manager.indicator_dots[index], active ? 10 : 0, 0);
        lv_obj_set_style_shadow_opa(s_manager.indicator_dots[index],
                                    active ? LV_OPA_50 : LV_OPA_TRANSP, 0);
    }
}

static void update_control_visuals(void)
{
    ui_bottom_surface_set_dynamic_focus(
        s_manager.mode == UI_DYNAMIC_CARD_CONTROL);

    for(uint8_t index = 0; index < MUSIC_CONTROL_COUNT; index++)
    {
        bool focused = s_manager.mode == UI_DYNAMIC_CARD_CONTROL &&
                       s_manager.active_card == UI_DYNAMIC_CARD_MUSIC &&
                       s_manager.control_focus == (int8_t)index;
        lv_obj_set_style_bg_opa(s_manager.music_controls[index],
                                focused ? LV_OPA_20 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(s_manager.music_controls[index],
                                    focused || index == 1 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_color(s_manager.music_controls[index], UI_COLOR_YELLOW, 0);
        lv_obj_set_style_shadow_width(s_manager.music_controls[index], focused ? 8 : 0, 0);
        lv_obj_set_style_shadow_opa(s_manager.music_controls[index],
                                    focused ? LV_OPA_30 : LV_OPA_TRANSP, 0);
    }
}

static ui_dynamic_card_id_t centered_card(void)
{
    lv_area_t viewport_area;
    lv_obj_get_coords(s_manager.viewport, &viewport_area);
    int32_t viewport_center = (viewport_area.y1 + viewport_area.y2) / 2;
    int32_t smallest_distance = INT32_MAX;
    ui_dynamic_card_id_t nearest = UI_DYNAMIC_CARD_EXTENDED_DATA;

    for(uint8_t index = 0; index < UI_DYNAMIC_CARD_COUNT; index++)
    {
        lv_area_t card_area;
        lv_obj_get_coords(s_manager.cards[index], &card_area);
        int32_t card_center = (card_area.y1 + card_area.y2) / 2;
        int32_t distance = card_center - viewport_center;
        if(distance < 0) distance = -distance;
        if(distance < smallest_distance)
        {
            smallest_distance = distance;
            nearest = (ui_dynamic_card_id_t)index;
        }
    }
    return nearest;
}

static void leave_control_mode(void)
{
    s_manager.mode = UI_DYNAMIC_CARD_BROWSER;
    s_manager.control_focus = -1;
    update_control_visuals();
}

static void viewport_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if(code == LV_EVENT_SCROLL_BEGIN)
    {
        if(s_manager.mode == UI_DYNAMIC_CARD_CONTROL) leave_control_mode();
    }
    else if(code == LV_EVENT_SCROLL_END)
    {
        s_manager.active_card = centered_card();
        s_manager.target_card = s_manager.active_card;
        update_indicator();
        update_control_visuals();
    }
}

static void scroll_to_card(ui_dynamic_card_id_t card)
{
    if(card >= UI_DYNAMIC_CARD_COUNT) return;
    s_manager.target_card = card;
    lv_obj_update_layout(s_manager.viewport);
    lv_obj_scroll_to_view(s_manager.cards[card], ui_motion_scroll_mode());
}

static void execute_music_control(uint8_t control_index)
{
    switch(control_index)
    {
        case 0:
            (void)ui_command_service_submit(UI_COMMAND_MEDIA_PREVIOUS,
                                            0, 1500U);
            return;
        case 1:
            (void)ui_command_service_submit(
                UI_COMMAND_MEDIA_TOGGLE_PLAYBACK, 0, 1500U);
            return;
        case 2:
            (void)ui_command_service_submit(UI_COMMAND_MEDIA_NEXT,
                                            0, 1500U);
            return;
        case 3:
            (void)ui_command_service_submit(UI_COMMAND_MEDIA_CYCLE_MODE,
                                            0, 1500U);
            return;
        default:
            return;
    }
}

void ui_dynamic_card_manager_create(lv_obj_t *parent)
{
    if(parent == NULL || s_manager.created) return;

    s_manager.viewport = lv_obj_create(parent);
    lv_obj_remove_style_all(s_manager.viewport);
    lv_obj_set_pos(s_manager.viewport, 0, 0);
    lv_obj_set_size(s_manager.viewport, DOCK_WIDTH, DOCK_HEIGHT);
    lv_obj_set_style_pad_all(s_manager.viewport, 0, 0);
    lv_obj_set_style_pad_row(s_manager.viewport, 0, 0);
    lv_obj_set_scroll_dir(s_manager.viewport, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_manager.viewport, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(s_manager.viewport, LV_SCROLL_SNAP_NONE);
    lv_obj_set_scroll_snap_y(s_manager.viewport, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_flex_flow(s_manager.viewport, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_manager.viewport, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(s_manager.viewport, LV_OBJ_FLAG_SCROLLABLE |
                                         LV_OBJ_FLAG_SCROLL_ONE |
                                         LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(s_manager.viewport, LV_OBJ_FLAG_SCROLL_ELASTIC |
                                           LV_OBJ_FLAG_SCROLL_CHAIN |
                                           LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_manager.viewport, viewport_event_cb, LV_EVENT_ALL, NULL);

    for(uint8_t index = 0; index < UI_DYNAMIC_CARD_COUNT; index++)
    {
        s_manager.cards[index] = create_card(s_manager.viewport);
    }
    create_extended_card(s_manager.cards[UI_DYNAMIC_CARD_EXTENDED_DATA]);
    create_navigation_card(s_manager.cards[UI_DYNAMIC_CARD_NAVIGATION]);
    create_music_card(s_manager.cards[UI_DYNAMIC_CARD_MUSIC]);
    create_weather_card(s_manager.cards[UI_DYNAMIC_CARD_WEATHER]);

    s_manager.indicator_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_manager.indicator_overlay);
    lv_obj_set_pos(s_manager.indicator_overlay, INDICATOR_X, INDICATOR_Y);
    lv_obj_set_size(s_manager.indicator_overlay, 20,
                    INDICATOR_DOT_STEP * (UI_DYNAMIC_CARD_COUNT - 1) + INDICATOR_DOT_SIZE);
    lv_obj_clear_flag(s_manager.indicator_overlay,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    for(uint8_t index = 0; index < UI_DYNAMIC_CARD_COUNT; index++)
    {
        s_manager.indicator_dots[index] = create_rect(
            s_manager.indicator_overlay, 5, index * INDICATOR_DOT_STEP,
            INDICATOR_DOT_SIZE, INDICATOR_DOT_SIZE,
            index == 0 ? UI_COLOR_YELLOW : UI_TOKEN_COLOR_INDICATOR,
            index == 0 ? LV_OPA_COVER : LV_OPA_70
        );
        lv_obj_set_style_radius(s_manager.indicator_dots[index], LV_RADIUS_CIRCLE, 0);
    }
    lv_obj_move_foreground(s_manager.indicator_overlay);

    s_manager.active_card = UI_DYNAMIC_CARD_EXTENDED_DATA;
    s_manager.target_card = UI_DYNAMIC_CARD_EXTENDED_DATA;
    s_manager.mode = UI_DYNAMIC_CARD_BROWSER;
    s_manager.control_focus = -1;
    s_manager.music_state_subscription = ui_app_state_subscribe(
        UI_APP_STATE_CHANGE_MEDIA, music_state_changed, NULL);
    s_manager.weather_subscription = ui_weather_model_subscribe(
        weather_changed, NULL);
    s_manager.ride_data_subscription = ui_ride_data_model_subscribe(
        ride_data_changed, NULL);
    s_manager.navigation_subscription = ui_navigation_service_subscribe(
        navigation_changed, NULL);
    s_manager.created = true;
    ui_app_state_t app_state;
    if(ui_app_state_get(&app_state)) apply_music_state(&app_state);
    ui_weather_data_t weather;
    if(ui_weather_model_get(&weather)) apply_weather(&weather);
    ui_ride_data_t ride_data;
    if(ui_ride_data_model_get(&ride_data, NULL)) apply_ride_data(&ride_data);
    nav_model_t navigation;
    uint32_t navigation_version = 0U;
    if(ui_navigation_service_get(&navigation, &navigation_version) &&
       navigation_version > 0U)
        apply_navigation(&navigation);
    update_indicator();
    update_control_visuals();
}

bool ui_dynamic_card_manager_contains_point(int32_t screen_x, int32_t screen_y)
{
    if(!s_manager.created || s_manager.viewport == NULL) return false;
    lv_area_t area;
    lv_obj_get_coords(s_manager.viewport, &area);
    return screen_x >= area.x1 && screen_x <= area.x2 &&
           screen_y >= area.y1 && screen_y <= area.y2;
}

bool ui_dynamic_card_manager_handle_input(ui_input_event_t input)
{
    if(!s_manager.created) return false;

    if(input == UI_INPUT_BACK) return ui_dynamic_card_manager_back();

    if(input == UI_INPUT_ENCODER_PRESS)
    {
        if(s_manager.mode == UI_DYNAMIC_CARD_BROWSER)
        {
            s_manager.mode = UI_DYNAMIC_CARD_CONTROL;
            s_manager.control_focus = -1;
            update_control_visuals();
        }
        else if(s_manager.control_focus < 0)
        {
            leave_control_mode();
        }
        else if(s_manager.active_card == UI_DYNAMIC_CARD_MUSIC)
        {
            execute_music_control((uint8_t)s_manager.control_focus);
        }
        return true;
    }

    int32_t direction = input == UI_INPUT_ENCODER_CW ? 1 :
                        input == UI_INPUT_ENCODER_CCW ? -1 : 0;
    if(direction == 0) return false;

    if(s_manager.mode == UI_DYNAMIC_CARD_BROWSER)
    {
        int32_t next = (int32_t)s_manager.target_card + direction;
        if(next < 0) next = 0;
        if(next >= UI_DYNAMIC_CARD_COUNT) next = UI_DYNAMIC_CARD_COUNT - 1;
        if(next != (int32_t)s_manager.target_card)
        {
            scroll_to_card((ui_dynamic_card_id_t)next);
        }
        return true;
    }

    if(s_manager.active_card == UI_DYNAMIC_CARD_EXTENDED_DATA) {
        change_extended_page(direction);
        return true;
    }

    if(s_manager.active_card == UI_DYNAMIC_CARD_MUSIC)
    {
        if(s_manager.control_focus < 0)
        {
            s_manager.control_focus = direction > 0 ? 0 : MUSIC_CONTROL_COUNT - 1;
        }
        else
        {
            int32_t next = s_manager.control_focus + direction;
            if(next < 0) next = MUSIC_CONTROL_COUNT - 1;
            if(next >= MUSIC_CONTROL_COUNT) next = 0;
            s_manager.control_focus = (int8_t)next;
        }
        update_control_visuals();
    }
    return true;
}

bool ui_dynamic_card_manager_back(void)
{
    if(!s_manager.created || s_manager.mode != UI_DYNAMIC_CARD_CONTROL) return false;
    leave_control_mode();
    return true;
}

ui_dynamic_card_id_t ui_dynamic_card_manager_get_active_card(void)
{
    return s_manager.active_card;
}

ui_dynamic_card_mode_t ui_dynamic_card_manager_get_mode(void)
{
    return s_manager.mode;
}

int8_t ui_dynamic_card_manager_get_control_focus(void)
{
    return s_manager.control_focus;
}

unsigned ui_dynamic_card_manager_get_extended_page(void) { return s_manager.extended_page; }
