#include "ui_ride_report_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_motion.h"
#include "../core/ui_sound.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include "../models/ui_ride_data_model.h"

#define REPORT_VIEWPORT_Y       150
#define REPORT_VIEWPORT_HEIGHT  (UI_PAGE_CONTENT_BOTTOM - REPORT_VIEWPORT_Y)
#define REPORT_SECTION_GAP       12
#define REPORT_SCROLL_STEP      260

typedef struct
{
    lv_obj_t *screen;
    lv_obj_t *viewport;
    lv_obj_t *content;
    lv_obj_t *distance_label;
    lv_obj_t *completion_status;
    lv_obj_t *done_button;
    ui_status_bar_t status;
    ui_page_action_cb_t callback;
    void *user_data;
    ui_ride_report_model_t model;
    ui_ride_report_mode_t mode;
    uint32_t content_height;
    bool active;
    lv_point_precise_t map_points[UI_RIDE_REPORT_POINT_MAX];
} report_page_t;

typedef struct
{
    const char *title;
    const char *subtitle;
    char value[24];
    const char *unit;
} performance_item_t;

static report_page_t s_pages[UI_RIDE_REPORT_MODE_COUNT];


static lv_obj_t *report_label(lv_obj_t *parent, int32_t x, int32_t y,
                              int32_t width, const char *text,
                              const lv_font_t *font, lv_color_t color,
                              lv_text_align_t align)
{
    lv_obj_t *label = ui_page_label(parent, x, y, width, text, font, align);
    lv_obj_set_style_text_color(label, color, 0);
    /* DOT needs a stable single-line height while long report sections are
     * rebuilt offscreen; content-size height can transiently collapse. */
    lv_obj_set_height(label,font->line_height);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    return label;
}

static lv_obj_t *report_card(lv_obj_t *parent, int32_t y, int32_t height)
{
    lv_obj_t *card = ui_page_card(parent, 0, y, 448, height);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_bg_color(card, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
    return card;
}

static void section_title(lv_obj_t *card, const char *title,
                          const char *subtitle)
{
    report_label(card, 14, 10, 176, title, UI_FONT_PAGE_TEXT,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    report_label(card, 116, 15, 230, subtitle, &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    ui_page_line(card, 14, 43, 420, 1, UI_TOKEN_COLOR_DIVIDER);
}

static void format_duration(uint32_t seconds, char *text, size_t capacity)
{
    snprintf(text, capacity, "%02lu:%02lu:%02lu",
             (unsigned long)(seconds / 3600U),
             (unsigned long)((seconds / 60U) % 60U),
             (unsigned long)(seconds % 60U));
}

static void format_short_time(uint32_t seconds, char *text, size_t capacity)
{
    snprintf(text, capacity, "%02lu:%02lu",
             (unsigned long)(seconds / 60U),
             (unsigned long)(seconds % 60U));
}

static void format_date(int64_t seconds, char *text, size_t capacity)
{
    if(seconds <= 0) {
        snprintf(text, capacity, "DATE UNAVAILABLE");
        return;
    }
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

static lv_obj_t *map_line(lv_obj_t *parent,
                          const lv_point_precise_t *points,
                          uint32_t count, lv_color_t color, int32_t width)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, points, count);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    return line;
}

static bool trace_position_valid(const ui_ride_report_trace_t *trace, unsigned i)
{
    return (trace->valid_fields[i] & UI_RIDE_FIELD_BIT(UI_RIDE_FIELD_POSITION)) &&
        trace->latitude_microdegrees[i] >= -90000000 && trace->latitude_microdegrees[i] <= 90000000 &&
        trace->longitude_microdegrees[i] >= -180000000 && trace->longitude_microdegrees[i] <= 180000000;
}

static double longitude_delta(double longitude, double origin)
{
    double delta = longitude - origin;
    if(delta > 180) delta -= 360;
    if(delta < -180) delta += 360;
    return delta;
}

static int32_t create_map(report_page_t *page, int32_t y)
{
    lv_obj_t *card = report_card(page->content, y, 226);
    section_title(card, "轨迹预览", "RECORDED TRACK");
    const ui_ride_report_trace_t *trace = &page->model.trace;
    unsigned first = UI_RIDE_REPORT_POINT_MAX, last = 0, count = 0;
    for(unsigned i = 0; i < trace->point_count; ++i) {
        if(!trace_position_valid(trace, i)) continue;
        if(first == UI_RIDE_REPORT_POINT_MAX) first = i;
        last = i;
        ++count;
    }
    if(count < 2) {
        report_label(card, 22, 102, 400, "没有有效的 GPS 轨迹", UI_FONT_PAGE_TEXT,
                     UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_CENTER);
        return y + 226 + REPORT_SECTION_GAP;
    }
    double lat0 = trace->latitude_microdegrees[first] * 0.000001;
    double lon0 = trace->longitude_microdegrees[first] * 0.000001;
    double longitude_scale = 111319.490 * cos(lat0 * 0.0174532925199433);
    double min_x = 0, max_x = 0, min_y = 0, max_y = 0;
    for(unsigned i = 0; i < trace->point_count; ++i) {
        if(!trace_position_valid(trace, i)) continue;
        double x = longitude_delta(trace->longitude_microdegrees[i] * 0.000001, lon0) * longitude_scale;
        double latitude = trace->latitude_microdegrees[i] * 0.000001;
        double yy = (latitude - lat0) * 111319.490;
        min_x = fmin(min_x, x); max_x = fmax(max_x, x);
        min_y = fmin(min_y, yy); max_y = fmax(max_y, yy);
    }
    double scale = fmin(392 / fmax(max_x - min_x, 20), 136 / fmax(max_y - min_y, 20));
    double center_x = (min_x + max_x) / 2, center_y = (min_y + max_y) / 2;
    for(int32_t offset = 65; offset < 196; offset += 32)
        ui_page_line(card, 16, offset, 416, 1, UI_TOKEN_COLOR_DIVIDER);
    unsigned segment_start = 0, segment_count = 0;
    for(unsigned i = 0; i <= trace->point_count; ++i) {
        if(i < trace->point_count && trace_position_valid(trace, i)) {
            double x = longitude_delta(trace->longitude_microdegrees[i] * 0.000001, lon0) * longitude_scale;
            double yy = (trace->latitude_microdegrees[i] * 0.000001 - lat0) * 111319.490;
            page->map_points[i] = (lv_point_precise_t){(int32_t)lround(224 + (x - center_x) * scale),
                                                     (int32_t)lround(124 - (yy - center_y) * scale)};
            if(segment_count == 0) segment_start = i;
            ++segment_count;
        }
        else {
            if(segment_count >= 2)
                map_line(card, &page->map_points[segment_start], segment_count, UI_COLOR_YELLOW, 4);
            segment_count = 0;
        }
    }
    unsigned endpoints[2] = {first, last};
    for(unsigned i = 0; i < 2; ++i) {
        lv_point_precise_t point = page->map_points[endpoints[i]];
        lv_obj_t *dot = ui_page_box(card, point.x - 5, point.y - 5, 10, 10, UI_COLOR_YELLOW, 2);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, i == 0 ? UI_COLOR_YELLOW : lv_color_black(), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }
    report_label(card, 16, 196, 278, "仅显示采样轨迹", UI_FONT_PAGE_TEXT,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    char scale_text[32];
    snprintf(scale_text, sizeof(scale_text), "%.0f m", 56 / scale);
    report_label(card, 330, 194, 100, scale_text, &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_RIGHT);
    ui_page_line(card, 374, 212, 56, 2, UI_TOKEN_COLOR_TEXT_PRIMARY);
    return y + 226 + REPORT_SECTION_GAP;
}

static int32_t create_identity(report_page_t *page, int32_t y)
{
    lv_obj_t *card = report_card(page->content, y, 54);
    report_label(card, 14, 13, 92, "户外骑行",
                 UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_PRIMARY,
                 LV_TEXT_ALIGN_LEFT);
    report_label(card, 104, 17, 132, "/ OUTDOOR RIDE",
                 &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
                 LV_TEXT_ALIGN_LEFT);
    char date[40];
    format_date(page->model.finished_at_utc, date, sizeof(date));
    report_label(card, 238, 15, 196, date, &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_RIGHT);
    return y + 54 + REPORT_SECTION_GAP;
}

static bool has_metric(const report_page_t *page, uint8_t flag)
{
    return !(page->model.metric_flags & UI_REPORT_METRICS_KNOWN) ||
           (page->model.metric_flags & flag) != 0U;
}

static void hero_metric(lv_obj_t *card, int32_t x, int32_t y,
                        const char *title, const char *subtitle,
                        const char *value)
{
    report_label(card, x, y, 182, title, UI_FONT_PAGE_TEXT,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    report_label(card, x, y + 23, 182, subtitle, &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    report_label(card, x, y + 43, 182, value, &lv_font_montserrat_20,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
}

static int32_t create_hero(report_page_t *page, int32_t y)
{
    lv_obj_t *card = report_card(page->content, y, 286);
    report_label(card, 16, 12, 230, "骑行距离", UI_FONT_PAGE_TEXT,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    report_label(card, 118, 17, 180, "DISTANCE", &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    char text[40];
    snprintf(text, sizeof(text), has_metric(page, UI_REPORT_HAS_DISTANCE) ? "%.1f" : "--", page->model.distance_km);
    page->distance_label = report_label(
        card, 16, 48, 295, text, has_metric(page, UI_REPORT_HAS_DISTANCE) ? &ui_font_misans_metric_60 : &lv_font_montserrat_48,
        UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    const lv_font_t *distance_font = lv_obj_get_style_text_font(page->distance_label,0);
    lv_point_t extent;
    lv_text_get_size(&extent,text,distance_font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
    if(extent.x > 295) {
        distance_font = &lv_font_montserrat_32;
        lv_obj_set_style_text_font(page->distance_label,distance_font,0);
        lv_text_get_size(&extent,text,distance_font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
    }
    int32_t baseline = 48 + distance_font->line_height - distance_font->base_line;
    report_label(card, 26+LV_MIN(extent.x,295),
                 baseline-lv_font_montserrat_24.line_height+lv_font_montserrat_24.base_line,
                 92,"km",&lv_font_montserrat_24,
                 UI_TOKEN_COLOR_TEXT_SECONDARY,LV_TEXT_ALIGN_LEFT);
    ui_page_line(card, 16, 122, 416, 1, UI_TOKEN_COLOR_DIVIDER);
    format_duration(page->model.duration_seconds, text, sizeof(text));
    hero_metric(card, 16, 136, "骑行时间", "DURATION", text);
    snprintf(text, sizeof(text), has_metric(page, UI_REPORT_HAS_DISTANCE) ? "%.1f km/h" : "--",
             page->model.average_speed_kmh);
    hero_metric(card, 230, 136, "平均速度", "AVG SPEED", text);
    ui_page_line(card, 220, 138, 1, 55, UI_TOKEN_COLOR_DIVIDER);
    snprintf(text, sizeof(text), has_metric(page, UI_REPORT_HAS_SPEED) ? "%.1f km/h" : "--",
             page->model.maximum_speed_kmh);
    hero_metric(card, 16, 210, "最高速度", "MAX SPEED", text);
    snprintf(text, sizeof(text), has_metric(page, UI_REPORT_HAS_ASCENT) ? "%lu m" : "--",
             (unsigned long)page->model.total_ascent_m);
    hero_metric(card, 230, 210, "累计爬升", "TOTAL ASCENT", text);
    ui_page_line(card, 220, 212, 1, 55, UI_TOKEN_COLOR_DIVIDER);
    return y + 286 + REPORT_SECTION_GAP;
}

static void add_performance_item(performance_item_t *items, uint8_t *count,
                                 const char *title, const char *subtitle,
                                 const char *unit, const char *format,
                                 int32_t value)
{
    performance_item_t *item = &items[(*count)++];
    item->title = title;
    item->subtitle = subtitle;
    item->unit = unit;
    snprintf(item->value, sizeof(item->value), format, value);
}

static int32_t create_performance(report_page_t *page, int32_t y)
{
    performance_item_t items[6];
    uint8_t count = 0U;
    if(page->model.has_heart_rate)
        add_performance_item(items, &count, "平均心率", "AVG HEART RATE",
                             "bpm", "%ld",
                             page->model.average_heart_rate);
    if(page->model.has_power)
        add_performance_item(items, &count, "平均功率", "AVG POWER",
                             "W", "%ld", page->model.average_power);
    if(page->model.has_cadence)
        add_performance_item(items, &count, "平均踏频", "AVG CADENCE",
                             "rpm", "%ld",
                             page->model.average_cadence);
    add_performance_item(items, &count, "热量", "CALORIES", "kcal", has_metric(page, UI_REPORT_HAS_CALORIES) ? "%ld" : "--",
                         (int32_t)page->model.calories_kcal);
    add_performance_item(items, &count, "最高海拔", "MAX ELEVATION", "m",
                         has_metric(page, UI_REPORT_HAS_ALTITUDE) ? "%ld" : "--", page->model.maximum_elevation_m);
    add_performance_item(items, &count, "总下降", "TOTAL DESCENT", "m",
                         has_metric(page, UI_REPORT_HAS_ALTITUDE) ? "%ld" : "--", (int32_t)page->model.total_descent_m);
    uint8_t rows = (uint8_t)((count + 2U) / 3U);
    int32_t height = 54 + rows * 96;
    lv_obj_t *card = report_card(page->content, y, height);
    section_title(card, "骑行表现", "PERFORMANCE");
    for(uint8_t index = 0U; index < count; index++) {
        int32_t column = index % 3U;
        int32_t row = index / 3U;
        lv_obj_t *cell = ui_page_box(card, 12 + column * 141,
                                     52 + row * 94, 132, 84,
                                     UI_TOKEN_COLOR_DIVIDER, 1);
        lv_obj_set_style_radius(cell, 6, 0);
        report_label(cell, 10, 8, 112, items[index].title,
                     UI_FONT_PAGE_TEXT,
                     UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
        report_label(cell, 10, 29, 112, items[index].subtitle,
                     &lv_font_montserrat_12,
                     UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
        report_label(cell, 10, 50, 76, items[index].value,
                     &lv_font_montserrat_20,
                     UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
        report_label(cell, 79, 56, 43, items[index].unit,
                     &lv_font_montserrat_12,
                     UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_RIGHT);
    }
    return y + height + REPORT_SECTION_GAP;
}

static void metric_header(lv_obj_t *card, int32_t x, int32_t y,
                          int32_t width, const char *title,
                          const char *subtitle, const char *value)
{
    report_label(card, x, y, width, title, UI_FONT_PAGE_TEXT,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    report_label(card, x, y + 20, width, subtitle, &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    report_label(card, x, y + 39, width, value, &lv_font_montserrat_20,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
}

static void chart_style(lv_obj_t *chart)
{
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(0x5C6264), 0);
    lv_obj_set_style_border_width(chart, 1, 0);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x3B4143), LV_PART_MAIN);
    lv_obj_set_style_line_opa(chart, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
}

static void create_chart_u16(report_page_t *page, lv_obj_t *card, int32_t y,
                             const uint16_t *values, uint16_t count,
                             int32_t maximum, lv_color_t color)
{
    const ui_ride_report_trace_t *trace = &page->model.trace;
    ui_ride_field_t field = values == trace->speed_tenths ? UI_RIDE_FIELD_SPEED :
        values == trace->heart_rate ? UI_RIDE_FIELD_HEART_RATE :
        values == trace->power ? UI_RIDE_FIELD_POWER :
        values == trace->cadence ? UI_RIDE_FIELD_CADENCE : UI_RIDE_FIELD_ALTITUDE;
    unsigned valid_count = 0;
    int32_t minimum = 0;
    for(unsigned i = 0; i < count; ++i) {
        if(!(trace->valid_fields[i] & UI_RIDE_FIELD_BIT(field))) continue;
        ++valid_count;
        if(field == UI_RIDE_FIELD_ALTITUDE && trace->elevation_m[i] < minimum) minimum = trace->elevation_m[i];
    }
    if(count < 2U || valid_count < 2) {
        report_label(card, 20, y + 42, 408,
                     "暂无趋势数据 / NO TRACE DATA",
                     UI_FONT_PAGE_TEXT,
                     UI_TOKEN_COLOR_TEXT_SECONDARY,
                     LV_TEXT_ALIGN_CENTER);
        return;
    }
    lv_obj_t *chart = lv_chart_create(card);
    lv_obj_set_pos(chart, 38, y);
    lv_obj_set_size(chart, 388, 92);
    lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(chart, count);
    lv_chart_set_div_line_count(chart, 4, 5);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X, 0,
        page->model.duration_seconds > 0 && page->model.duration_seconds <= INT32_MAX ?
        page->model.duration_seconds : 1);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, minimum,
                       maximum > 0 ? maximum : 100);
    chart_style(chart);
    lv_chart_series_t *series = lv_chart_add_series(
        chart, color, LV_CHART_AXIS_PRIMARY_Y);
    for(uint16_t index = 0U; index < count; index++) {
        int32_t value = field == UI_RIDE_FIELD_ALTITUDE ? trace->elevation_m[index] : values[index];
        if(!(trace->valid_fields[index] & UI_RIDE_FIELD_BIT(field))) value = LV_CHART_POINT_NONE;
        lv_chart_set_next_value2(chart, series, trace->time_seconds[index], value);
    }
    report_label(card, 20, y + 95, 406,
        trace->time_is_estimated ? "估算时间轴" : "横轴：有效记录时间 (s)",
        UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_RIGHT);
}

static int32_t create_speed(report_page_t *page, int32_t y)
{
    lv_obj_t *card = report_card(page->content, y, 270);
    section_title(card, "速度", "SPEED");
    char text[32];
    snprintf(text, sizeof(text), has_metric(page, UI_REPORT_HAS_DISTANCE) ? "%.1f km/h" : "--",
             page->model.average_speed_kmh);
    metric_header(card, 18, 56, 185, "平均速度", "AVG SPEED", text);
    snprintf(text, sizeof(text), has_metric(page, UI_REPORT_HAS_SPEED) ? "%.1f km/h" : "--",
             page->model.maximum_speed_kmh);
    metric_header(card, 230, 56, 190, "最高速度", "MAX SPEED", text);
    ui_page_line(card, 216, 58, 1, 66, UI_TOKEN_COLOR_DIVIDER);
    report_label(card, 18, 132, 120, "速度 (km/h)",
                 UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY,
                 LV_TEXT_ALIGN_LEFT);
    int32_t maximum = (int32_t)(page->model.maximum_speed_kmh * 10.0f);
    if(maximum < 100) maximum = 100;
    create_chart_u16(page, card, 148, page->model.trace.speed_tenths,
                     page->model.trace.point_count,
                     maximum + maximum / 5, UI_COLOR_YELLOW);
    return y + 270 + REPORT_SECTION_GAP;
}

static int32_t create_sensor_chart(report_page_t *page, int32_t y,
                                   const char *title, const char *subtitle,
                                   const char *average_title,
                                   const char *maximum_title,
                                   const char *minimum_title,
                                   const char *unit,
                                   uint16_t average, uint16_t maximum,
                                   uint16_t minimum,
                                   const uint16_t *values,
                                   lv_color_t color)
{
    lv_obj_t *card = report_card(page->content, y, 270);
    section_title(card, title, subtitle);
    char text[32];
    snprintf(text, sizeof(text), "%u %s", average, unit);
    metric_header(card, 18, 56, minimum > 0U ? 120 : 185,
                  average_title, "AVERAGE", text);
    snprintf(text, sizeof(text), "%u %s", maximum,
             unit);
    metric_header(card, minimum > 0U ? 158 : 230, 56,
                  minimum > 0U ? 120 : 190, maximum_title, "MAXIMUM", text);
    if(minimum > 0U) {
        snprintf(text, sizeof(text), "%u %s", minimum, unit);
        metric_header(card, 300, 56, 126, minimum_title, "MINIMUM", text);
    }
    report_label(card, 18, 132, 180, unit, &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    int32_t chart_max = maximum > 0U ? maximum + maximum / 5U :
                                      average * 2U;
    if(chart_max < 10) chart_max = 10;
    create_chart_u16(page, card, 148, values, page->model.trace.point_count,
                     chart_max, color);
    return y + 270 + REPORT_SECTION_GAP;
}

static uint32_t zone_total(const uint32_t *seconds)
{
    uint32_t total = 0U;
    for(uint8_t index = 0U; index < UI_RIDE_REPORT_ZONE_COUNT; index++)
        total += seconds[index];
    return total;
}

static int32_t create_zones(report_page_t *page, int32_t y,
                            const char *title, const char *subtitle,
                            const uint32_t *seconds, bool power)
{
    static const char * const heart_names[UI_RIDE_REPORT_ZONE_COUNT] = {
        "热身", "燃脂", "有氧", "无氧", "极限"
    };
    static const char * const power_names[UI_RIDE_REPORT_ZONE_COUNT] = {
        "恢复", "耐力", "节奏", "阈值", "冲刺"
    };
    static const uint32_t colors[UI_RIDE_REPORT_ZONE_COUNT] = {
        0xBFC4C6, 0xB5D93D, 0xFFE600, 0xFF9D00, 0xFF3B3B
    };
    uint32_t total = zone_total(seconds);
    if(total == 0U) return y;
    lv_obj_t *card = report_card(page->content, y, 292);
    section_title(card, title, subtitle);
    for(uint8_t index = 0U; index < UI_RIDE_REPORT_ZONE_COUNT; index++) {
        int32_t row_y = 55 + index * 45;
        const char *name = power ? power_names[index] : heart_names[index];
        report_label(card, 16, row_y, 62, name, UI_FONT_PAGE_TEXT,
                     UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
        lv_obj_t *bar = lv_bar_create(card);
        lv_obj_set_pos(bar, 82, row_y + 5);
        lv_obj_set_size(bar, 208, 13);
        lv_bar_set_range(bar, 0, 100);
        uint32_t percent = seconds[index] * 100U / total;
        lv_bar_set_value(bar, (int32_t)percent, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, UI_TOKEN_COLOR_SEGMENT_INACTIVE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(colors[index]),
                                  LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        char text[24];
        format_short_time(seconds[index], text, sizeof(text));
        report_label(card, 302, row_y, 74, text, &lv_font_montserrat_14,
                     UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_RIGHT);
        snprintf(text, sizeof(text), "%lu%%", (unsigned long)percent);
        report_label(card, 380, row_y, 48, text, &lv_font_montserrat_14,
                     UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_RIGHT);
    }
    return y + 292 + REPORT_SECTION_GAP;
}

static int32_t create_elevation(report_page_t *page, int32_t y)
{
    lv_obj_t *card = report_card(page->content, y, 330);
    section_title(card, "海拔", "ELEVATION");
    char text[32];
    snprintf(text, sizeof(text), has_metric(page, UI_REPORT_HAS_ASCENT) ? "%lu m" : "--",
             (unsigned long)page->model.total_ascent_m);
    metric_header(card, 16, 58, 126, "累计爬升", "TOTAL ASCENT", text);
    snprintf(text, sizeof(text), has_metric(page, UI_REPORT_HAS_ALTITUDE) ? "%lu m" : "--",
             (unsigned long)page->model.total_descent_m);
    metric_header(card, 160, 58, 126, "累计下降", "TOTAL DESCENT", text);
    snprintf(text, sizeof(text), has_metric(page, UI_REPORT_HAS_ALTITUDE) ? "%ld m" : "--",
             (long)page->model.maximum_elevation_m);
    metric_header(card, 304, 58, 126, "最高海拔", "MAX ELEVATION", text);
    report_label(card, 16, 138, 120, "海拔 (m)",
                 UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY,
                 LV_TEXT_ALIGN_LEFT);
    uint16_t converted[UI_RIDE_REPORT_POINT_MAX];
    int32_t maximum = page->model.maximum_elevation_m;
    for(uint16_t index = 0U; index < page->model.trace.point_count; index++) {
        int32_t value = page->model.trace.elevation_m[index];
        converted[index] = value > 0 ? (uint16_t)value : 0U;
        if(value > maximum) maximum = value;
    }
    create_chart_u16(page, card, 156, converted, page->model.trace.point_count,
                     maximum + maximum / 5, UI_COLOR_YELLOW);
    return y + 330 + REPORT_SECTION_GAP;
}

static void energy_cell(lv_obj_t *card, int32_t x, int32_t y,
                        const char *title, const char *subtitle,
                        const char *value)
{
    lv_obj_t *cell = ui_page_box(card, x, y, 198, 76,
                                 UI_TOKEN_COLOR_DIVIDER, 1);
    lv_obj_set_style_radius(cell, 6, 0);
    report_label(cell, 12, 8, 174, title, UI_FONT_PAGE_TEXT,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    report_label(cell, 12, 28, 174, subtitle, &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    report_label(cell, 12, 47, 174, value, &lv_font_montserrat_20,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
}

static int32_t create_energy(report_page_t *page, int32_t y)
{
    int32_t height = page->model.has_temperature ? 228 : 142;
    lv_obj_t *card = report_card(page->content, y, height);
    section_title(card, "能量与环境", "");
    report_label(card, 138, 15, 260, "ENERGY & ENVIRONMENT",
                 &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    char text[32];
    snprintf(text, sizeof(text), has_metric(page, UI_REPORT_HAS_CALORIES) ? "%lu kcal" : "--",
             (unsigned long)page->model.calories_kcal);
    energy_cell(card, 16, 54, "热量消耗", "CALORIES", text);
    if(has_metric(page, UI_REPORT_HAS_SPEED))
        format_duration(page->model.moving_time_seconds, text, sizeof(text));
    else snprintf(text, sizeof(text), "--");
    energy_cell(card, 234, 54, "运动时间", "MOVING TIME", text);
    if(page->model.has_temperature) {
        snprintf(text, sizeof(text), "%.1f C",
                 page->model.average_temperature_tenths_c / 10.0f);
        energy_cell(card, 16, 140, "平均温度", "AVG TEMPERATURE", text);
    }
    return y + height + REPORT_SECTION_GAP;
}

static lv_obj_t *summary_rule(lv_obj_t *card, int32_t y, const char *title,
                         const char *subtitle, const char *description)
{
    lv_obj_t *row = ui_page_box(card, 14, y, 420, 62,
                                UI_TOKEN_COLOR_DIVIDER, 1);
    lv_obj_set_style_radius(row, 5, 0);
    ui_page_line(row, 12, 10, 3, 42, UI_COLOR_YELLOW);
    report_label(row, 26, 8, 170, title, UI_FONT_PAGE_TEXT,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    report_label(row, 150, 11, 244, subtitle, &lv_font_montserrat_12,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    return report_label(row, 26, 34, 374, description, UI_FONT_PAGE_TEXT,
                 UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
}

static const char *completion_text(bool saved)
{
    return saved ? "骑行已结束，记录已保存" : "骑行已结束，记录尚未保存";
}

static int32_t create_summary(report_page_t *page, int32_t y)
{
    bool long_ride = page->model.distance_km >= 50.0f;
    bool climbing = page->model.total_ascent_m >= 1000U;
    uint32_t heart_total = zone_total(
        page->model.trace.heart_rate_zone_seconds);
    bool aerobic = heart_total > 0U &&
        page->model.trace.heart_rate_zone_seconds[2] * 100U /
        heart_total >= 30U;
    /* Save state is required even when a distance/climbing/zone summary exists. */
    uint8_t rules = 1U + (long_ride ? 1U : 0U) + (climbing ? 1U : 0U) +
                    (aerobic ? 1U : 0U);
    int32_t height = 58 + rules * 70;
    lv_obj_t *card = report_card(page->content, y, height);
    section_title(card, "骑行总结", "RIDE SUMMARY");
    int32_t row_y = 54;
    char description[96];
    if(long_ride) {
        snprintf(description, sizeof(description),
                 "本次距离 %.1f km",
                 page->model.distance_km);
        summary_rule(card, row_y, "长距离", "LONG RIDE", description);
        row_y += 70;
    }
    if(climbing) {
        snprintf(description, sizeof(description),
                 "本次累计爬升 %lu m",
                 (unsigned long)page->model.total_ascent_m);
        summary_rule(card, row_y, "爬升挑战", "CLIMBING", description);
        row_y += 70;
    }
    if(aerobic) {
        snprintf(description,sizeof(description),"Z3 占有效心率时间 %lu%%",
                 (unsigned long)((uint64_t)page->model.trace.heart_rate_zone_seconds[2]*100U/heart_total));
        summary_rule(card,row_y,"区间分布","HEART RATE ZONES",description);
        row_y += 70;
    }
    page->completion_status=summary_rule(card,row_y,"骑行完成","RIDE COMPLETE",
                                        completion_text(page->model.saved));
    return y + height + REPORT_SECTION_GAP;
}

static void action_event(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    report_page_t *page = lv_event_get_user_data(event);
    if(page == NULL || page->callback == NULL) return;
    ui_sound_play(UI_SFX_BACK);
    page->callback(page->mode == UI_RIDE_REPORT_SUMMARY_MODE ?
                   UI_PAGE_ACTION_HOME : UI_PAGE_ACTION_BACK,
                   page->user_data);
}

static int32_t create_done(report_page_t *page, int32_t y)
{
    if(page->mode != UI_RIDE_REPORT_SUMMARY_MODE) return y;
    if(page->done_button != NULL) return y;
    lv_obj_t *button = ui_page_box(page->screen, 16, UI_PAGE_ACTION_Y, 448, UI_PAGE_ACTION_HEIGHT,
                                   UI_COLOR_YELLOW, 2);
    page->done_button = button;
    lv_obj_set_style_bg_color(button, UI_COLOR_YELLOW, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_transform_scale(button, 248, LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, action_event, LV_EVENT_CLICKED, page);
    ui_align_label_center(report_label(button, 0, 0, 448, "完成", UI_FONT_PAGE_TEXT,
                 UI_COLOR_BG, LV_TEXT_ALIGN_CENTER));
    return y;
}

static void rebuild(report_page_t *page)
{
    if(page == NULL || page->content == NULL) return;
    lv_obj_clean(page->content);
    page->distance_label = NULL;
    page->completion_status = NULL;
    int32_t y = 0;
    y = create_map(page, y);
    y = create_identity(page, y);
    y = create_hero(page, y);
    y = create_performance(page, y);
    y = create_speed(page, y);
    if(page->model.has_heart_rate) {
        y = create_sensor_chart(page, y, "心率", "HEART RATE",
                                "平均心率", "最高心率", "最低心率", "bpm",
                                page->model.average_heart_rate,
                                page->model.maximum_heart_rate,
                                page->model.minimum_heart_rate,
                                page->model.trace.heart_rate,
                                UI_COLOR_YELLOW);
        y = create_zones(page, y, "心率区间", "HEART RATE ZONES",
                         page->model.trace.heart_rate_zone_seconds, false);
    }
    if(page->model.has_power) {
        y = create_sensor_chart(page, y, "功率", "POWER",
                                "平均功率", "最大功率", "", "W",
                                page->model.average_power,
                                page->model.maximum_power, 0U,
                                page->model.trace.power, UI_COLOR_YELLOW);
    }
    if(page->model.has_cadence) {
        y = create_sensor_chart(page, y, "踏频", "CADENCE",
                                "平均踏频", "最大踏频", "", "rpm",
                                page->model.average_cadence,
                                page->model.maximum_cadence, 0U,
                                page->model.trace.cadence, UI_COLOR_YELLOW);
    }
    if(page->model.has_power)
        y = create_zones(page, y, "功率区间", "POWER ZONES",
                         page->model.trace.power_zone_seconds, true);
    y = create_elevation(page, y);
    y = create_energy(page, y);
    y = create_summary(page, y);
    y = create_done(page, y);
    page->content_height = (uint32_t)y;
    lv_obj_set_height(page->content, y);
    lv_obj_scroll_to_y(page->viewport, 0, LV_ANIM_OFF);
}

lv_obj_t *ui_ride_report_page_create(ui_ride_report_mode_t mode,
                                     ui_page_action_cb_t callback,
                                     void *user_data)
{
    if(mode >= UI_RIDE_REPORT_MODE_COUNT) return NULL;
    report_page_t *page = &s_pages[mode];
    memset(page, 0, sizeof(*page));
    page->mode = mode;
    page->callback = callback;
    page->user_data = user_data;
    page->screen = ui_page_screen_create();
    ui_page_status_create(&page->status, page->screen);

    lv_obj_t *back = lv_obj_create(page->screen);
    lv_obj_remove_style_all(back);
    lv_obj_set_pos(back, 14, 89);
    lv_obj_set_size(back, 52, 48);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_transform_scale(back, 242, LV_STATE_PRESSED);
    lv_obj_add_event_cb(back, action_event, LV_EVENT_CLICKED, page);
    ui_align_label_center(report_label(back, 0, 0, 52, LV_SYMBOL_LEFT, LV_FONT_DEFAULT,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER));
    report_label(page->screen, 100, 84, 280, "骑行报告", UI_FONT_PAGE_TEXT,
                 UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER);
    report_label(page->screen, 100, 114, 280, "RIDE REPORT",
                 &lv_font_montserrat_14, UI_TOKEN_COLOR_TEXT_SECONDARY,
                 LV_TEXT_ALIGN_CENTER);

    page->viewport = lv_obj_create(page->screen);
    lv_obj_remove_style_all(page->viewport);
    lv_obj_set_pos(page->viewport, 16, REPORT_VIEWPORT_Y);
    lv_obj_set_size(page->viewport, 448, mode == UI_RIDE_REPORT_SUMMARY_MODE ?
        UI_PAGE_ACTION_Y - 12 - REPORT_VIEWPORT_Y : REPORT_VIEWPORT_HEIGHT);
    lv_obj_set_scroll_dir(page->viewport, LV_DIR_VER);
    ui_page_scrollbar_style(page->viewport);
    lv_obj_set_style_pad_all(page->viewport, 0, 0);
    lv_obj_set_style_pad_right(page->viewport, 0, 0);
    page->content = lv_obj_create(page->viewport);
    lv_obj_remove_style_all(page->content);
    lv_obj_set_pos(page->content, 0, 0);
    lv_obj_set_size(page->content, 448, 1);
    lv_obj_clear_flag(page->content, LV_OBJ_FLAG_SCROLLABLE);
    (void)create_done(page, 0);
    return page->screen;
}

bool ui_ride_report_page_set_model(ui_ride_report_mode_t mode,
                                   const ui_ride_report_model_t *model)
{
    if(mode >= UI_RIDE_REPORT_MODE_COUNT ||
       !ui_ride_report_model_valid(model)) return false;
    report_page_t *page = &s_pages[mode];
    if(page->screen == NULL) return false;
    page->model = *model;
    if(page->active) rebuild(page);
    return true;
}

void ui_ride_report_page_set_saved(ui_ride_report_mode_t mode,bool saved)
{
    if(mode >= UI_RIDE_REPORT_MODE_COUNT) return;
    report_page_t *page=&s_pages[mode];
    page->model.saved=saved;
    if(page->completion_status != NULL)
        lv_label_set_text(page->completion_status,completion_text(saved));
}

static void distance_frame(void *target, int32_t tenths)
{
    char text[24];
    snprintf(text, sizeof(text), "%ld.%01ld",
             (long)(tenths / 10), (long)(tenths % 10));
    lv_label_set_text((lv_obj_t *)target, text);
}

void ui_ride_report_page_set_active(ui_ride_report_mode_t mode, bool active)
{
    if(mode >= UI_RIDE_REPORT_MODE_COUNT) return;
    report_page_t *page = &s_pages[mode];
    page->active = active;
    if(!active) {
        if(page->distance_label != NULL)
            lv_anim_delete(page->distance_label, NULL);
        if(page->viewport != NULL)
            lv_anim_delete(page->viewport, NULL);
        if(page->content != NULL) {
            lv_obj_clean(page->content);
            lv_obj_set_height(page->content, 1);
        }
        page->distance_label = NULL;
        page->completion_status = NULL;
        page->content_height = 0U;
        return;
    }
    if(page->content_height == 0U) rebuild(page);
    if(page->distance_label == NULL) return;
    if(mode == UI_RIDE_REPORT_SUMMARY_MODE && has_metric(page, UI_REPORT_HAS_DISTANCE)) {
        double tenths = (double)page->model.distance_km * 10.0 + 0.5;
        if(tenths <= INT32_MAX)
            ui_motion_value(page->distance_label, distance_frame, 0, (int32_t)tenths,
                            620U, lv_anim_path_ease_out, NULL, NULL);
    }
}

bool ui_ride_report_page_handle_input(ui_ride_report_mode_t mode,
                                      ui_input_event_t input)
{
    if(mode >= UI_RIDE_REPORT_MODE_COUNT) return false;
    report_page_t *page = &s_pages[mode];
    if(!page->active || page->viewport == NULL) return false;
    if(input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW) {
        int32_t maximum = (int32_t)page->content_height -
                          lv_obj_get_height(page->viewport);
        if(maximum < 0) maximum = 0;
        lv_point_t scroll_end;
        lv_obj_get_scroll_end(page->viewport,&scroll_end);
        int32_t target = scroll_end.y +
            (input == UI_INPUT_ENCODER_CW ? REPORT_SCROLL_STEP :
                                            -REPORT_SCROLL_STEP);
        if(target < 0) target = 0;
        if(target > maximum) target = maximum;
        lv_obj_scroll_to_y(page->viewport, target, ui_motion_scroll_mode());
        ui_sound_play(UI_SFX_FOCUS);
        return true;
    }
    if(input == UI_INPUT_ENCODER_PRESS &&
       mode == UI_RIDE_REPORT_SUMMARY_MODE) {
        if(page->callback != NULL)
            page->callback(UI_PAGE_ACTION_HOME, page->user_data);
        ui_sound_play(UI_SFX_CONFIRM);
        return true;
    }
    return false;
}

bool ui_ride_report_page_contains_scroll_point(ui_ride_report_mode_t mode,
                                               int32_t x, int32_t y)
{
    if(mode >= UI_RIDE_REPORT_MODE_COUNT) return false;
    lv_obj_t *viewport = s_pages[mode].viewport;
    if(viewport == NULL) return false;
    lv_area_t area;
    lv_obj_get_coords(viewport, &area);
    return x >= area.x1 && x <= area.x2 &&
           y >= area.y1 && y <= area.y2;
}

int32_t ui_ride_report_page_scroll_y(ui_ride_report_mode_t mode)
{
    if(mode >= UI_RIDE_REPORT_MODE_COUNT ||
       s_pages[mode].viewport == NULL) return 0;
    return lv_obj_get_scroll_y(s_pages[mode].viewport);
}

uint32_t ui_ride_report_page_content_height(ui_ride_report_mode_t mode)
{
    if(mode >= UI_RIDE_REPORT_MODE_COUNT) return 0U;
    return s_pages[mode].content_height;
}
