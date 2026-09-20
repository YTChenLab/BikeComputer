#include "ui_history_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_sound.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"
#include "../models/ui_ride_history_model.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define HISTORY_VISIBLE_MAX 50U
#define HISTORY_ROW_HEIGHT  112
#define HISTORY_ROW_GAP       8

typedef struct
{
    lv_obj_t *screen;
    lv_obj_t *list;
    lv_obj_t *empty;
    lv_obj_t *ride_count;
    lv_obj_t *total_distance;
    lv_obj_t *total_time;
    lv_obj_t *rows[HISTORY_VISIBLE_MAX];
    ui_status_bar_t status;
    size_t visible_count;
    int32_t selected;
    bool active;
    ui_page_action_cb_t callback;
    void *user_data;
} ui_history_page_context_t;

static ui_history_page_context_t s_history;

static lv_obj_t *history_label(lv_obj_t *parent, int32_t x, int32_t y,
                               int32_t width, const char *text,
                               const lv_font_t *font, lv_color_t color,
                               lv_text_align_t align)
{
    lv_obj_t *label = ui_page_label(parent, x, y, width, text, font, align);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
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

static void update_focus(void)
{
    for(size_t index = 0U; index < s_history.visible_count; index++) {
        bool focused = s_history.active &&
                       s_history.selected == (int32_t)index;
        lv_obj_set_style_border_color(
            s_history.rows[index],
            focused ? UI_TOKEN_COLOR_ACCENT :
                      UI_TOKEN_COLOR_SURFACE_BORDER, 0);
        lv_obj_set_style_border_width(
            s_history.rows[index],
            focused ? UI_TOKEN_BORDER_FOCUS : UI_TOKEN_BORDER_THIN, 0);
    }
}

static void row_clicked(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    s_history.selected = (int32_t)(uintptr_t)lv_event_get_user_data(event);
    update_focus();
    ui_sound_play(UI_SFX_PRESS);
    if(s_history.callback != NULL)
        s_history.callback(UI_PAGE_ACTION_RIDE_DETAIL, s_history.user_data);
}

static lv_obj_t *create_row(size_t index, const ui_ride_record_t *record)
{
    lv_obj_t *row = lv_obj_create(s_history.list);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, 432);
    lv_obj_set_height(row, HISTORY_ROW_HEIGHT);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x080D10), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(row, UI_TOKEN_COLOR_SURFACE_BORDER, 0);
    lv_obj_set_style_border_opa(row, LV_OPA_50, 0);
    lv_obj_set_style_border_width(row, UI_TOKEN_BORDER_THIN, 0);
    lv_obj_set_style_radius(row, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_set_style_transform_scale(row, 250, LV_STATE_PRESSED);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row, row_clicked, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)index);

    char text[40];
    format_date(record->finished_at_utc, text, sizeof(text));
    history_label(row, 14, 10, 240, text, &lv_font_montserrat_14,
                  UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    snprintf(text, sizeof(text), "%.1f km", record->distance_km);
    history_label(row, 270, 8, 146, text, &lv_font_montserrat_20,
                  UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_RIGHT);

    uint32_t hours = record->duration_seconds / 3600U;
    uint32_t minutes = (record->duration_seconds / 60U) % 60U;
    snprintf(text, sizeof(text), "%02lu:%02lu",
             (unsigned long)hours, (unsigned long)minutes);
    history_label(row, 14, 45, 112, text, &lv_font_montserrat_20,
                  UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    history_label(row, 14, 75, 112, "DURATION",
                  &lv_font_montserrat_12,
                  UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    snprintf(text, sizeof(text), "%.1f km/h", record->average_speed_kmh);
    history_label(row, 140, 45, 132, text, &lv_font_montserrat_20,
                  UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    history_label(row, 140, 75, 132, "AVG SPEED",
                  &lv_font_montserrat_12,
                  UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    snprintf(text, sizeof(text), "%lu m", (unsigned long)record->ascent_m);
    history_label(row, 286, 45, 130, text, &lv_font_montserrat_20,
                  UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_RIGHT);
    history_label(row, 286, 75, 130, "ASCENT", &lv_font_montserrat_12,
                  UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_RIGHT);
    return row;
}

lv_obj_t *ui_history_page_create(ui_page_action_cb_t callback,
                                 void *user_data)
{
    memset(&s_history, 0, sizeof(s_history));
    s_history.callback = callback;
    s_history.user_data = user_data;
    s_history.selected = -1;
    s_history.screen = ui_page_screen_create();
    ui_page_status_create(&s_history.status, s_history.screen);
    ui_page_title(s_history.screen, "骑行历史", "RIDE HISTORY");

    lv_obj_t *summary = ui_page_card(s_history.screen, 16, 158, 448, 106);
    history_label(summary, 14, 10, 100, "RIDES",
                  &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
                  LV_TEXT_ALIGN_LEFT);
    history_label(summary, 132, 10, 140, "DISTANCE",
                  &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
                  LV_TEXT_ALIGN_LEFT);
    history_label(summary, 288, 10, 140, "TIME",
                  &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
                  LV_TEXT_ALIGN_LEFT);
    s_history.ride_count = history_label(
        summary, 14, 41, 100, "0", &ui_font_misans_num_48,
        UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_LEFT);
    s_history.total_distance = history_label(
        summary, 132, 51, 140, "0 km", &lv_font_montserrat_20,
        UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_history.total_time = history_label(
        summary, 288, 51, 140, "0 h", &lv_font_montserrat_20,
        UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);

    s_history.list = lv_obj_create(s_history.screen);
    lv_obj_remove_style_all(s_history.list);
    lv_obj_set_pos(s_history.list, 16, 278);
    lv_obj_set_size(s_history.list, 448, 556);
    lv_obj_set_flex_flow(s_history.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_history.list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_history.list, HISTORY_ROW_GAP, 0);
    lv_obj_set_style_pad_bottom(s_history.list, 12, 0);
    lv_obj_set_scroll_dir(s_history.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_history.list, LV_SCROLLBAR_MODE_AUTO);

    s_history.empty = history_label(
        s_history.screen, 28, 470, 424,
        "暂无骑行记录\n完成并保存一次骑行后将在这里显示",
        UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY,
        LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(s_history.empty, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(s_history.empty, 72);
    lv_obj_set_style_text_line_space(s_history.empty, 12, 0);
    ui_history_page_refresh();
    return s_history.screen;
}

void ui_history_page_refresh(void)
{
    if(s_history.list == NULL) return;
    lv_obj_clean(s_history.list);
    memset(s_history.rows, 0, sizeof(s_history.rows));
    size_t total_count = ui_ride_history_model_count();
    s_history.visible_count = total_count > HISTORY_VISIBLE_MAX ?
                              HISTORY_VISIBLE_MAX : total_count;
    ui_ride_history_totals_t totals;
    ui_ride_history_model_totals(&totals);
    char text[40];
    snprintf(text, sizeof(text), "%lu", (unsigned long)totals.ride_count);
    lv_label_set_text(s_history.ride_count, text);
    snprintf(text, sizeof(text), "%.0f km", totals.total_distance_km);
    lv_label_set_text(s_history.total_distance, text);
    snprintf(text, sizeof(text), "%llu h",
             (unsigned long long)(totals.total_duration_seconds / 3600U));
    lv_label_set_text(s_history.total_time, text);

    for(size_t index = 0U; index < s_history.visible_count; index++) {
        ui_ride_record_t record;
        if(ui_ride_history_model_get_newest(index, &record))
            s_history.rows[index] = create_row(index, &record);
    }
    if(s_history.visible_count == 0U) {
        lv_obj_clear_flag(s_history.empty, LV_OBJ_FLAG_HIDDEN);
        s_history.selected = -1;
    }
    else {
        lv_obj_add_flag(s_history.empty, LV_OBJ_FLAG_HIDDEN);
        if(s_history.selected < 0 ||
           s_history.selected >= (int32_t)s_history.visible_count)
            s_history.selected = 0;
    }
    update_focus();
}

void ui_history_page_set_active(bool active)
{
    s_history.active = active;
    if(active) {
        ui_history_page_refresh();
        if(s_history.visible_count > 0U) s_history.selected = 0;
    }
    update_focus();
}

bool ui_history_page_handle_input(ui_input_event_t input)
{
    if(!s_history.active || s_history.visible_count == 0U) return false;
    if(input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW) {
        int32_t direction = input == UI_INPUT_ENCODER_CW ? 1 : -1;
        int32_t next = s_history.selected + direction;
        if(next < 0) next = 0;
        if(next >= (int32_t)s_history.visible_count)
            next = (int32_t)s_history.visible_count - 1;
        if(next != s_history.selected) {
            s_history.selected = next;
            update_focus();
            lv_obj_scroll_to_view(s_history.rows[next], LV_ANIM_ON);
            ui_sound_play(UI_SFX_FOCUS);
        }
        return true;
    }
    if(input == UI_INPUT_ENCODER_PRESS) {
        ui_sound_play(UI_SFX_PRESS);
        if(s_history.callback != NULL)
            s_history.callback(UI_PAGE_ACTION_RIDE_DETAIL,
                               s_history.user_data);
        return true;
    }
    return false;
}

size_t ui_history_page_visible_count(void)
{
    return s_history.visible_count;
}

int32_t ui_history_page_selected_index(void)
{
    return s_history.selected;
}

bool ui_history_page_selected_record(ui_ride_record_t *record)
{
    if(record == NULL || s_history.selected < 0 ||
       s_history.selected >= (int32_t)s_history.visible_count)
        return false;
    return ui_ride_history_model_get_newest((size_t)s_history.selected,
                                            record);
}

bool ui_history_page_contains_scroll_point(int32_t x, int32_t y)
{
    if(s_history.list == NULL) return false;
    lv_area_t area;
    lv_obj_get_coords(s_history.list, &area);
    return x >= area.x1 && x <= area.x2 &&
           y >= area.y1 && y <= area.y2;
}
