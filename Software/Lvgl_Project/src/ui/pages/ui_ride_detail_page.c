#include "ui_ride_detail_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct
{
    lv_obj_t *screen;
    lv_obj_t *date;
    lv_obj_t *distance;
    lv_obj_t *duration;
    lv_obj_t *average_speed;
    lv_obj_t *maximum_speed;
    lv_obj_t *ascent;
    lv_obj_t *heart_rate;
    lv_obj_t *power;
    lv_obj_t *cadence;
    lv_obj_t *record_id;
} ride_detail_context_t;

static ride_detail_context_t s_detail;
static ui_status_bar_t s_status;

static const lv_point_precise_t s_route_points[] = {
    {18, 104}, {52, 82}, {79, 91}, {112, 54}, {151, 65},
    {194, 29}, {236, 44}, {278, 21}, {320, 46}, {374, 24}
};

static lv_obj_t *detail_label(lv_obj_t *parent, int32_t x, int32_t y,
                              int32_t width, const char *text,
                              const lv_font_t *font, lv_color_t color,
                              lv_text_align_t align)
{
    lv_obj_t *label = ui_page_label(parent, x, y, width, text, font, align);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

static lv_obj_t *metric(lv_obj_t *parent, int32_t x, int32_t y,
                        const char *title, lv_obj_t **value)
{
    lv_obj_t *card = ui_page_card(parent, x, y, 138, 88);
    detail_label(card, 12, 10, 114, title, &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    *value = detail_label(card, 12, 40, 114, "--", &lv_font_montserrat_20,
                          UI_TOKEN_COLOR_TEXT_PRIMARY,
                          LV_TEXT_ALIGN_LEFT);
    return card;
}

static void format_date(int64_t seconds, char *text, size_t capacity)
{
    time_t value = (time_t)seconds;
    struct tm local_time;
#if defined(_WIN32)
    if(localtime_s(&local_time, &value) != 0) {
#else
    if(localtime_r(&value, &local_time) == NULL) {
#endif
        snprintf(text, capacity, "DATE UNAVAILABLE");
        return;
    }
    snprintf(text, capacity, "%04d.%02d.%02d  %02d:%02d",
             local_time.tm_year + 1900, local_time.tm_mon + 1,
             local_time.tm_mday, local_time.tm_hour, local_time.tm_min);
}

lv_obj_t *ui_ride_detail_page_create(void)
{
    memset(&s_detail, 0, sizeof(s_detail));
    s_detail.screen = ui_page_screen_create();
    ui_page_status_create(&s_status, s_detail.screen);
    ui_page_title(s_detail.screen, "骑行详情", "RIDE DETAIL");

    lv_obj_t *route = ui_page_card(s_detail.screen, 16, 158, 448, 164);
    detail_label(route, 16, 12, 176, "ROUTE PREVIEW",
                 &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
                 LV_TEXT_ALIGN_LEFT);
    s_detail.date = detail_label(route, 200, 12, 228, "--",
                                 &lv_font_montserrat_14,
                                 UI_TOKEN_COLOR_TEXT_SECONDARY,
                                 LV_TEXT_ALIGN_RIGHT);
    lv_obj_t *line = lv_line_create(route);
    lv_line_set_points(line, s_route_points,
                       sizeof(s_route_points) / sizeof(s_route_points[0]));
    lv_obj_set_pos(line, 16, 28);
    lv_obj_set_style_line_color(line, UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_set_style_line_width(line, 4, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_t *start = ui_page_box(route, 27, 125, 12, 12,
                                  UI_TOKEN_COLOR_ACCENT, 2);
    lv_obj_t *end = ui_page_box(route, 395, 45, 12, 12,
                                UI_TOKEN_COLOR_ACCENT, 2);
    lv_obj_set_style_radius(start, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(end, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(start, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_color(end, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(start, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(end, LV_OPA_COVER, 0);

    metric(s_detail.screen, 16, 334, "DISTANCE", &s_detail.distance);
    metric(s_detail.screen, 171, 334, "DURATION", &s_detail.duration);
    metric(s_detail.screen, 326, 334, "AVG SPEED", &s_detail.average_speed);
    metric(s_detail.screen, 16, 434, "MAX SPEED", &s_detail.maximum_speed);
    metric(s_detail.screen, 171, 434, "ASCENT", &s_detail.ascent);
    metric(s_detail.screen, 326, 434, "AVG HEART", &s_detail.heart_rate);
    metric(s_detail.screen, 16, 534, "AVG POWER", &s_detail.power);
    metric(s_detail.screen, 171, 534, "AVG CADENCE", &s_detail.cadence);

    lv_obj_t *record = ui_page_card(s_detail.screen, 326, 534, 138, 88);
    detail_label(record, 12, 10, 114, "RECORD ID", &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_detail.record_id = detail_label(record, 12, 40, 114, "--",
                                      &lv_font_montserrat_16,
                                      UI_TOKEN_COLOR_ACCENT,
                                      LV_TEXT_ALIGN_LEFT);

    lv_obj_t *note = ui_page_card(s_detail.screen, 16, 636, 448, 72);
    detail_label(note, 16, 12, 416, "本地骑行记录", UI_FONT_PAGE_TEXT,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    detail_label(note, 16, 41, 416,
                 "Route geometry is a Simulator preview; metrics are persisted.",
                 &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
                 LV_TEXT_ALIGN_LEFT);
    return s_detail.screen;
}

void ui_ride_detail_page_set_record(const ui_ride_record_t *record)
{
    if(record == NULL || s_detail.screen == NULL) return;
    char text[48];
    format_date(record->finished_at_utc, text, sizeof(text));
    lv_label_set_text(s_detail.date, text);
    snprintf(text, sizeof(text), "%.1f km", record->distance_km);
    lv_label_set_text(s_detail.distance, text);
    snprintf(text, sizeof(text), "%02lu:%02lu:%02lu",
             (unsigned long)(record->duration_seconds / 3600U),
             (unsigned long)((record->duration_seconds / 60U) % 60U),
             (unsigned long)(record->duration_seconds % 60U));
    lv_label_set_text(s_detail.duration, text);
    snprintf(text, sizeof(text), "%.1f km/h", record->average_speed_kmh);
    lv_label_set_text(s_detail.average_speed, text);
    snprintf(text, sizeof(text), "%.1f km/h", record->maximum_speed_kmh);
    lv_label_set_text(s_detail.maximum_speed, text);
    snprintf(text, sizeof(text), "%lu m", (unsigned long)record->ascent_m);
    lv_label_set_text(s_detail.ascent, text);
    snprintf(text, sizeof(text), "%u bpm", record->average_heart_rate);
    lv_label_set_text(s_detail.heart_rate, text);
    snprintf(text, sizeof(text), "%u W", record->average_power);
    lv_label_set_text(s_detail.power, text);
    snprintf(text, sizeof(text), "%u rpm", record->average_cadence);
    lv_label_set_text(s_detail.cadence, text);
    snprintf(text, sizeof(text), "#%llu",
             (unsigned long long)record->id);
    lv_label_set_text(s_detail.record_id, text);
}
