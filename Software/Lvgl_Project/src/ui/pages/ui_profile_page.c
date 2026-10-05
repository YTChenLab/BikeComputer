#include "ui_profile_page.h"

#include "ui_page_common.h"
#include "../core/ui_text.h"
#include "../core/ui_sound.h"
#include "../components/ui_liquid_selection.h"
#include "../core/ui_memory.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"
#include "../models/ui_home_data_model.h"
#include "../models/ui_ride_history_model.h"
#include "../models/ui_achievement_model.h"
#include "../models/ui_dashboard_layout.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>


#define PROFILE_ACTION_COUNT 8U
#define PROFILE_GRID_COLS 10U
#define PROFILE_GRID_ROWS 3U
#define PROFILE_GRID_CELL_COUNT (PROFILE_GRID_COLS * PROFILE_GRID_ROWS)

typedef struct {
    lv_obj_t *screen;
    ui_status_bar_t status;
    lv_obj_t *initial, *name, *tagline;
    lv_obj_t *total_distance, *total_time, *total_ascent, *ride_count;
    lv_obj_t *achievement_count, *achievement_title, *achievement_remaining;
    lv_obj_t *achievement_progress, *activity_summary;
    lv_obj_t *refresh_button, *activity_cells[PROFILE_GRID_CELL_COUNT];
    lv_obj_t *history_subtitle, *layout_subtitle;
    lv_obj_t *units[4];
    int64_t activity_now_utc;
    lv_obj_t *actions[PROFILE_ACTION_COUNT];
    ui_page_action_t action_ids[PROFILE_ACTION_COUNT];
    ui_page_action_cb_t callback;
    void *user_data;
    int32_t selected;
    unsigned preset;
    bool active;
    ui_liquid_selection_t selection;
} profile_context_t;

static profile_context_t s_profile;
static void update_focus(bool animate);

static lv_obj_t *profile_label(lv_obj_t *parent, int32_t x, int32_t y,
                               int32_t width, const char *text,
                               const lv_font_t *font, lv_color_t color,
                               lv_text_align_t align)
{
    lv_obj_t *label = ui_page_label(parent, x, y, width, text, font, align);
    lv_obj_set_style_text_color(label, color, 0);
    /* Keep the underlying text intact for dynamic values and let the
     * carefully sized profile slots clip only the visual overflow. */
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

static void profile_set_text(lv_obj_t *label, const char *text,
                             uint16_t sanitizer_size,
                             const lv_font_t *font)
{
    if(label == NULL || text == NULL) return;
    (void)ui_text_set_label(label, text, sanitizer_size);
    if(font != NULL) lv_obj_set_style_text_font(label, font, 0);
}

static bool profile_ascii_only(const char *text)
{
    if(text == NULL) return false;
    for(const unsigned char *p = (const unsigned char *)text; *p != 0; ++p)
        if(*p >= 0x80U) return false;
    return true;
}

static lv_obj_t *profile_surface(int32_t x, int32_t y, int32_t width,
                                 int32_t height, int32_t radius)
{
    lv_obj_t *surface = ui_page_box(s_profile.screen, x, y, width, height,
                                    UI_TOKEN_COLOR_SURFACE_OUTLINE, 1);
    lv_obj_set_style_bg_color(surface, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(surface, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(surface, radius, 0);
    lv_obj_set_style_clip_corner(surface, true, 0);
    return surface;
}

static void profile_action_event(lv_event_t *event)
{
    if(!s_profile.active) return;
    unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    if(index >= PROFILE_ACTION_COUNT) return;
    switch(lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            s_profile.selected = (int32_t)index;
            update_focus(true);
            break;
        case LV_EVENT_CLICKED:
            s_profile.selected = (int32_t)index;
            update_focus(true);
            if(s_profile.callback != NULL)
                s_profile.callback(s_profile.action_ids[index], s_profile.user_data);
            break;
        default: break;
    }
}

static void profile_register_action(lv_obj_t *object, unsigned index,
                                    ui_page_action_t action)
{
    if(object == NULL || index >= PROFILE_ACTION_COUNT) return;
    s_profile.actions[index] = object;
    s_profile.action_ids[index] = action;
    lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(object, profile_action_event, LV_EVENT_ALL,
                        (void *)(uintptr_t)index);
    /* Large cards stay at their native size: pressing must not allocate a
     * transformed card framebuffer just to show focus feedback. */
    if(index != 0U) lv_obj_set_style_radius(object, 24, 0);
    lv_obj_set_style_opa(object, LV_OPA_80, LV_STATE_PRESSED);
}

static lv_obj_t *profile_menu(int32_t x, int32_t y, int32_t width,
                                int32_t height, const char *symbol,
                                const char *title_text, const char *subtitle_text,
                                unsigned index, ui_page_action_t action)
{
    lv_obj_t *row = profile_surface(x, y, width, height, 10);
    lv_obj_t *icon = profile_label(row, 14, 0, 32, symbol,
        &lv_font_montserrat_22, UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    ui_align_center_y(icon, 14);
    lv_obj_t *title = profile_label(row, 58, 0, width - 92, title_text,
        UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *subtitle = profile_label(row, 58, 0, width - 92, subtitle_text,
        &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    ui_align_text_pair(row, title, subtitle, 58, width - 92, 4);
    lv_obj_t *arrow = profile_label(row, width - 30, 0, 20, LV_SYMBOL_RIGHT,
        &lv_font_montserrat_20, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER);
    ui_align_center_y(arrow, width - 30);
    profile_register_action(row, index, action);
    return subtitle;
}

static void update_focus(bool animate)
{
    if(s_profile.selected >= 0)
        ui_liquid_selection_select(&s_profile.selection,
                                   (unsigned)s_profile.selected, animate);
}

static void format_uint64(char *buffer, size_t capacity, uint64_t value)
{
    char raw[32];
    snprintf(raw, sizeof(raw), "%llu", (unsigned long long)value);
    size_t length = strlen(raw);
    size_t commas = length > 0U ? (length - 1U) / 3U : 0U;
    if(length + commas + 1U > capacity) {
        if(capacity > 0U) buffer[0] = '\0';
        return;
    }
    size_t out = 0U;
    for(size_t index = 0U; index < length; ++index) {
        if(index > 0U && (length - index) % 3U == 0U) buffer[out++] = ',';
        buffer[out++] = raw[index];
    }
    buffer[out] = '\0';
}

static void update_identity(const ui_home_data_t *data)
{
    if(data == NULL || s_profile.screen == NULL) return;
    profile_set_text(s_profile.name,
                     strcmp(data->user_name, "Alex Chen") == 0 ?
                     "骑行者" : data->user_name, 24, &ui_font_misans_ui_24);
    profile_set_text(s_profile.tagline, data->tagline, 20,
                     profile_ascii_only(data->tagline) ?
                     &lv_font_montserrat_16 : &ui_font_misans_ui_20);
    const char *display_name = strcmp(data->user_name, "Alex Chen") == 0 ?
                               "骑行者" : data->user_name;
    const unsigned char *name = (const unsigned char *)display_name;
    unsigned length = name[0] < 0x80 ? 1U :
        (name[0] & 0xE0) == 0xC0 ? 2U :
        (name[0] & 0xF0) == 0xE0 ? 3U : 4U;
    char initial[5] = {0};
    if(name[0] != 0 && strlen(display_name) >= length)
        memcpy(initial, name, length);
    profile_set_text(s_profile.initial, initial[0] != 0 ? initial : "骑", 24,
                     &ui_font_misans_ui_24);
}

static void draw_activity_grid(lv_obj_t *activity)
{
    /* Resolve the newly-created card before reading its actual width. */
    lv_obj_update_layout(activity);
    int32_t usable_width = lv_obj_get_width(activity) - 28;
    for(unsigned row = 0U; row < PROFILE_GRID_ROWS; ++row) {
        for(unsigned column = 0U; column < PROFILE_GRID_COLS; ++column) {
            /* Fill the card evenly; two-pixel dividers preserve day boundaries. */
            int32_t x = 14 + (int32_t)column * usable_width / (int32_t)PROFILE_GRID_COLS;
            int32_t next_x = 14 + (int32_t)(column + 1U) * usable_width / (int32_t)PROFILE_GRID_COLS;
            int32_t y = 48 + (int32_t)row * 18;
            lv_obj_t *cell = ui_page_box(activity, x, y, next_x - x - 2, 16,
                                         UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
            lv_obj_set_style_bg_color(cell, UI_TOKEN_COLOR_SEGMENT_INACTIVE, 0);
            lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(cell, 3, 0);
            s_profile.activity_cells[row * PROFILE_GRID_COLS + column] = cell;
        }
    }
}

void ui_profile_page_set_activity_time(int64_t now_utc)
{
    s_profile.activity_now_utc = now_utc;
}

static void update_activity_grid(void)
{
    double distances[PROFILE_GRID_CELL_COUNT] = {0};
    size_t recent_count = 0U, dated_count = 0U;
    double recent_distance = 0.0;
    bool distance_missing = false;
    size_t count = ui_ride_history_model_count();
    /* One record at a time: a 100-record trace array can exhaust an MCU UI
     * task stack.  Timestamps and current UTC are provided by the backend. */
    static UI_LARGE_RAM ui_ride_record_t record;
    for(size_t i = 0U; i < count && s_profile.activity_now_utc > 0; ++i) {
        if(!ui_ride_history_model_get_newest(i, &record)) continue;
        int64_t stamp = record.started_at_utc > 0 ? record.started_at_utc : record.finished_at_utc;
        if(stamp <= 0) continue;
        ++dated_count;
        int64_t age = s_profile.activity_now_utc - stamp;
        if(age < 0 || age >= 30LL * 86400LL) continue;
        unsigned day = 29U - (unsigned)(age / 86400LL);
        ++recent_count;
        bool known = !(record.metric_flags & UI_REPORT_METRICS_KNOWN) ||
                     (record.metric_flags & UI_REPORT_HAS_DISTANCE);
        if(known) { recent_distance += record.distance_km; distances[day] += record.distance_km; }
        else distance_missing = true;
        /* A saved activity lights its day even when distance is unavailable. */
        if(distances[day] == 0) distances[day] = 0.001;
    }
    for(unsigned i = 0U; i < PROFILE_GRID_CELL_COUNT; ++i)
        if(s_profile.activity_cells[i] != NULL)
            lv_obj_set_style_bg_color(s_profile.activity_cells[i], distances[i] >= 80.0 ?
                UI_TOKEN_COLOR_ACCENT : distances[i] > 0 ? lv_color_hex(0x8F7A00) :
                UI_TOKEN_COLOR_SEGMENT_INACTIVE, 0);
    char text[64];
    ui_ride_history_totals_t totals;
    ui_ride_history_model_totals(&totals);
    if(s_profile.activity_now_utc <= 0) snprintf(text, sizeof(text), "日期未校准");
    else if(count == 0U && totals.ride_count > 0U) snprintf(text, sizeof(text), "暂无可读活动");
    else if(count > 0U && dated_count == 0U) snprintf(text, sizeof(text), "记录日期缺失");
    else if(distance_missing) snprintf(text, sizeof(text), "%u 次 · -- km", (unsigned)recent_count);
    else snprintf(text, sizeof(text), "%u 次 · %.0f km", (unsigned)recent_count, recent_distance);
    profile_set_text(s_profile.activity_summary, text, 20, &ui_font_misans_ui_20);
    lv_label_set_long_mode(s_profile.activity_summary, LV_LABEL_LONG_DOT);
}

static void fit_metric(lv_obj_t *number, lv_obj_t *unit, const char *text,
                       int32_t width, int32_t baseline)
{
    const lv_font_t *fonts[] = {&lv_font_montserrat_32, &lv_font_montserrat_28,
        &lv_font_montserrat_24, &lv_font_montserrat_20, &lv_font_montserrat_16};
    const lv_font_t *font = fonts[4];
    for(unsigned i = 0U; i < 5U; ++i) {
        lv_point_t size;
        lv_text_get_size(&size, text, fonts[i], 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
        if(size.x <= width) { font = fonts[i]; break; }
    }
    lv_obj_set_width(number, width);
    lv_obj_set_style_text_font(number, font, 0);
    lv_obj_set_y(number, baseline - font->line_height + font->base_line);
    lv_label_set_text(number, text);
    lv_label_set_long_mode(number, LV_LABEL_LONG_DOT);
    const lv_font_t *unit_font = lv_obj_get_style_text_font(unit, 0);
    lv_obj_set_y(unit, baseline - unit_font->line_height + unit_font->base_line);
}

static void update_totals(const ui_ride_history_totals_t *totals)
{
    if(totals == NULL) return;
    char text[64];
    snprintf(text, sizeof(text), "%.0f", totals->total_distance_km);
    fit_metric(s_profile.total_distance, s_profile.units[0], text, 112, 64);
    format_uint64(text, sizeof(text), totals->total_duration_seconds / 3600U);
    fit_metric(s_profile.total_time, s_profile.units[1], text, 80, 64);
    format_uint64(text, sizeof(text), totals->total_ascent_m);
    fit_metric(s_profile.total_ascent, s_profile.units[2], text, 112, 138);
    snprintf(text, sizeof(text), "%lu", (unsigned long)totals->ride_count);
    fit_metric(s_profile.ride_count, s_profile.units[3], text, 100, 138);
}

static void update_achievements(const ui_ride_history_totals_t *totals)
{
    unsigned unlocked = 0U;
    size_t next = UI_ACHIEVEMENT_COUNT;
    ui_achievement_progress_t nearest = {0};
    for(size_t index = 0U; index < UI_ACHIEVEMENT_COUNT; ++index) {
        ui_achievement_progress_t progress;
        const ui_achievement_definition_t *definition = ui_achievement_definition(index);
        if(definition == NULL || !ui_achievement_evaluate(index, totals, &progress)) continue;
        if(progress.unlocked) ++unlocked;
        else if(next == UI_ACHIEVEMENT_COUNT || progress.progress_permille > nearest.progress_permille) {
            next = index;
            nearest = progress;
        }
    }
    char text[64];
    snprintf(text, sizeof(text), "%u / %u", unlocked, UI_ACHIEVEMENT_COUNT);
    lv_label_set_text(s_profile.achievement_count, text);
    if(next == UI_ACHIEVEMENT_COUNT) {
        profile_set_text(s_profile.achievement_title, "全部成就已达成", 24,
                         &ui_font_misans_ui_24);
        profile_set_text(s_profile.achievement_remaining, "已完成当前成就目录", 20,
                         &ui_font_misans_ui_20);
        lv_obj_set_width(s_profile.achievement_progress, 320);
    }
    else {
        const ui_achievement_definition_t *definition = ui_achievement_definition(next);
        profile_set_text(s_profile.achievement_title, definition->title, 24,
                         &ui_font_misans_ui_24);
        const char *unit = definition->category == UI_ACHIEVEMENT_RIDES ? "次" :
            definition->category == UI_ACHIEVEMENT_DISTANCE ? "km" :
            definition->category == UI_ACHIEVEMENT_ASCENT ? "m" : "h";
        if(definition->category == UI_ACHIEVEMENT_RIDES || definition->category == UI_ACHIEVEMENT_ASCENT)
            snprintf(text, sizeof(text), "还需 %.0f %s", nearest.remaining, unit);
        else snprintf(text, sizeof(text), "还需 %.1f %s", nearest.remaining, unit);
        profile_set_text(s_profile.achievement_remaining, text, 20,
                         &ui_font_misans_ui_20);
        lv_obj_set_width(s_profile.achievement_progress,
                         (int32_t)(320U * nearest.progress_permille / 1000U));
    }
}

void ui_profile_page_refresh(void)
{
    if(s_profile.screen == NULL) return;
    ui_ride_history_totals_t totals;
    ui_ride_history_model_totals(&totals);
    update_totals(&totals);
    update_achievements(&totals);
    lv_label_set_text(s_profile.history_subtitle,
                      totals.ride_count == 0U ? "暂无记录" : "RIDE HISTORY");
    lv_obj_set_style_text_font(s_profile.history_subtitle, totals.ride_count == 0U ?
                              &ui_font_misans_ui_20 : &lv_font_montserrat_12, 0);
    update_activity_grid();
    ui_profile_page_set_preset(s_profile.preset);
}

static void home_changed(const ui_home_data_t *data, void *user_data)
{
    (void)user_data;
    update_identity(data);
    ui_profile_page_refresh();
}

void ui_profile_page_set_preset(unsigned preset)
{
    s_profile.preset = preset;
    if(s_profile.layout_subtitle != NULL)
        profile_set_text(s_profile.layout_subtitle,
                         ui_dashboard_preset_name(preset), 20,
                         &ui_font_misans_ui_20);
}

lv_obj_t *ui_profile_page_create(ui_page_action_cb_t callback, void *user_data)
{
    memset(&s_profile, 0, sizeof(s_profile));
    s_profile.callback = callback;
    s_profile.user_data = user_data;
    s_profile.selected = -1;
    s_profile.screen = ui_page_screen_create();
    lv_obj_set_style_bg_color(s_profile.screen, UI_TOKEN_COLOR_CANVAS, 0);
    ui_page_status_create(&s_profile.status, s_profile.screen);

    /* Product layout: 16 px at the bottom, independent of diagnostics.
     * Major cards have 10 px gaps; action rows have 8 px gaps. */
    lv_obj_t *identity = profile_surface(10, 50, 460, 80, 12);
    lv_obj_t *avatar = ui_page_box(identity, 14, 10, 58, 58, UI_TOKEN_COLOR_SURFACE_BORDER, 1);
    lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(avatar, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);
    s_profile.initial = profile_label(avatar, 0, 0, 58, "骑", &ui_font_misans_ui_24, UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    ui_align_label_center(s_profile.initial);
    s_profile.name = profile_label(identity, 90, 12, 230, "骑行者", &ui_font_misans_ui_24, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_profile.tagline = profile_label(identity, 118, 48, 236, "Good Roads, Better Days.", &lv_font_montserrat_16, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    ui_page_line(identity, 90, 52, 20, 3, UI_TOKEN_COLOR_ACCENT);
    ui_page_line(identity, 356, 14, 1, 52, UI_TOKEN_COLOR_SURFACE_OUTLINE);
    profile_label(identity, 376, 22, 58, "LOCAL\nRIDER", &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);


    lv_obj_t *stats = profile_surface(10, 140, 460, 208, 14);
    profile_label(stats, 18, 14, 220, "骑行总数据", &ui_font_misans_ui_24, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    profile_label(stats, 244, 14, 104, "本机累计", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_RIGHT);
    s_profile.refresh_button = ui_page_box(stats, 356, 8, 82, 38,
                                           UI_TOKEN_COLOR_SURFACE_OUTLINE, 1);
    lv_obj_set_style_bg_color(s_profile.refresh_button, UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
    lv_obj_set_style_bg_opa(s_profile.refresh_button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_profile.refresh_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(s_profile.refresh_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *refresh_icon = profile_label(s_profile.refresh_button, 7, 9, 24, LV_SYMBOL_REFRESH,
                  &lv_font_montserrat_18, UI_TOKEN_COLOR_ACCENT,
                  LV_TEXT_ALIGN_CENTER);
    lv_obj_t *refresh_text = profile_label(s_profile.refresh_button, 31, 9, 44, "刷新",
                  UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_PRIMARY,
                  LV_TEXT_ALIGN_CENTER);
    lv_obj_set_size(refresh_icon, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_size(refresh_text, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_profile.refresh_button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_profile.refresh_button, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_profile.refresh_button, 3, 0);
    profile_register_action(s_profile.refresh_button, 0,
                            UI_PAGE_ACTION_PROFILE_REFRESH);
    lv_obj_t *matrix = ui_page_box(stats, 8, 54, 444, 148, UI_TOKEN_COLOR_SURFACE_OUTLINE, 1);
    lv_obj_set_style_radius(matrix, 12, 0);
    lv_obj_set_style_bg_color(matrix, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, 0);
    ui_page_line(matrix, 222, 10, 1, 128, UI_TOKEN_COLOR_DIVIDER);
    ui_page_line(matrix, 18, 74, 408, 1, UI_TOKEN_COLOR_DIVIDER);
    profile_label(matrix, 16, 22, 34, LV_SYMBOL_UP, &lv_font_montserrat_28, UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    profile_label(matrix, 70, 10, 126, "总里程", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_profile.total_distance = profile_label(matrix, 70, 36, 144, "0", &lv_font_montserrat_32, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_profile.units[0] = profile_label(matrix, 188, 50, 34, "km", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    profile_label(matrix, 238, 22, 34, LV_SYMBOL_REFRESH, &lv_font_montserrat_24, UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    profile_label(matrix, 292, 10, 126, "总骑行时间", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_profile.total_time = profile_label(matrix, 292, 36, 105, "0", &lv_font_montserrat_32, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_profile.units[1] = profile_label(matrix, 382, 50, 36, "小时", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    profile_label(matrix, 16, 96, 34, LV_SYMBOL_UP, &lv_font_montserrat_28, UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    profile_label(matrix, 70, 84, 126, "总爬升", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_profile.total_ascent = profile_label(matrix, 70, 91, 140, "0", &lv_font_montserrat_32, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_profile.units[2] = profile_label(matrix, 188, 105, 34, "m", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    profile_label(matrix, 238, 96, 34, LV_SYMBOL_LIST, &lv_font_montserrat_24, UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    profile_label(matrix, 292, 84, 126, "总骑行次数", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_profile.ride_count = profile_label(matrix, 292, 91, 100, "0", &lv_font_montserrat_32, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_profile.units[3] = profile_label(matrix, 382, 105, 34, "次", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *achievement = profile_surface(10, 358, 460, 130, 14);
    profile_label(achievement, 18, 14, 200, "成就进度", &ui_font_misans_ui_24, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *count = ui_page_box(achievement, 360, 12, 70, 30, UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
    lv_obj_set_style_bg_color(count, UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
    lv_obj_set_style_bg_opa(count, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(count, LV_RADIUS_CIRCLE, 0);
    s_profile.achievement_count = profile_label(count, 0, 0, 70, "0 / 16", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    ui_align_label_center(s_profile.achievement_count);
    profile_label(achievement, 432, 15, 18, LV_SYMBOL_RIGHT, &lv_font_montserrat_20, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *hero = ui_page_box(achievement, 22, 46, 62, 62, UI_TOKEN_COLOR_ACCENT, 3);
    lv_obj_set_style_radius(hero, 16, 0);
    lv_obj_set_style_bg_color(hero, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(hero, LV_OPA_COVER, 0);
    ui_align_label_center(profile_label(hero, 0, 0, 62, LV_SYMBOL_UP,
        &lv_font_montserrat_24, UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER));
    s_profile.achievement_title = profile_label(achievement, 100, 46, 300, "首骑完成", &ui_font_misans_ui_24, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_profile.achievement_remaining = profile_label(achievement, 100, 78, 300, "还需 1 次解锁下一个成就", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *track = ui_page_box(achievement, 100, 107, 320, 7, UI_TOKEN_COLOR_SEGMENT_INACTIVE, 0);
    lv_obj_set_style_bg_color(track, UI_TOKEN_COLOR_SEGMENT_INACTIVE, 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(track, 4, 0);
    s_profile.achievement_progress = ui_page_box(track, 0, 0, 0, 7, UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_color(s_profile.achievement_progress, UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(s_profile.achievement_progress, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_profile.achievement_progress, 4, 0);
    profile_register_action(achievement, 1, UI_PAGE_ACTION_ACHIEVEMENT);

    lv_obj_t *activity = profile_surface(10, 498, 460, 114, 14);
    profile_label(activity, 18, 12, 190, "最近 30 天活动", &ui_font_misans_ui_24, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_profile.activity_summary = profile_label(activity, 246, 15, 174, "共骑行 0 次 · 0 km", UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_RIGHT);
    profile_label(activity, 432, 14, 18, LV_SYMBOL_RIGHT, &lv_font_montserrat_20, UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER);
    draw_activity_grid(activity);
    profile_register_action(activity, 2, UI_PAGE_ACTION_HISTORY);

    s_profile.history_subtitle = profile_menu(10, 622, 224, 64,
        LV_SYMBOL_LIST, "历史记录", "RIDE HISTORY", 3, UI_PAGE_ACTION_HISTORY);
    profile_menu(246, 622, 224, 64, LV_SYMBOL_OK,
        "我的成就", "ACHIEVEMENTS", 4, UI_PAGE_ACTION_ACHIEVEMENT);
    s_profile.layout_subtitle = profile_menu(10, 694, 224, 64,
        LV_SYMBOL_LIST, "数据布局", "", 5, UI_PAGE_ACTION_DASHBOARD_LAYOUT);
    profile_menu(246, 694, 224, 64, LV_SYMBOL_WIFI,
        "设备与传感器", "DEVICES & SENSORS", 6, UI_PAGE_ACTION_DEVICES);
    profile_menu(10, UI_PAGE_ACTION_Y, 460, UI_PAGE_ACTION_HEIGHT,
        LV_SYMBOL_SETTINGS, "系统设置", "SETTINGS", 7, UI_PAGE_ACTION_SETTINGS);

    /* Only operable cards participate. Identity and lifetime statistics
     * remain read-only; inset badges, progress and activity cells retain
     * their own opaque backgrounds and colors. */
    ui_liquid_selection_init(&s_profile.selection, s_profile.screen, false, NULL);
    for(unsigned i = 0U; i < PROFILE_ACTION_COUNT; ++i)
        ui_liquid_selection_add(&s_profile.selection, s_profile.actions[i]);

    ui_home_data_t home;
    if(ui_home_data_model_get(&home)) update_identity(&home);
    ui_home_data_model_subscribe(home_changed, NULL);
    lv_label_set_long_mode(s_profile.name, LV_LABEL_LONG_DOT);
    lv_label_set_long_mode(s_profile.tagline, LV_LABEL_LONG_DOT);
    ui_profile_page_refresh();
    update_focus(false);
    return s_profile.screen;
}

void ui_profile_page_set_active(bool active)
{
    s_profile.active = active;
    if(active) ui_profile_page_refresh();
    s_profile.selected = active ? 0 : -1;
    ui_liquid_selection_set_active(&s_profile.selection, active);
    update_focus(false);
}

bool ui_profile_page_handle_input(ui_input_event_t input)
{
    if(!s_profile.active) return false;
    if(input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW) {
        int32_t next = s_profile.selected + (input == UI_INPUT_ENCODER_CW ? 1 : -1);
        if(next < 0) next = (int32_t)PROFILE_ACTION_COUNT - 1;
        if(next >= (int32_t)PROFILE_ACTION_COUNT) next = 0;
        s_profile.selected = next;
        update_focus(true);
        ui_sound_play(UI_SFX_FOCUS);
        return true;
    }
    if(input == UI_INPUT_ENCODER_PRESS && s_profile.selected >= 0) {
        if(s_profile.callback != NULL)
            s_profile.callback(s_profile.action_ids[s_profile.selected], s_profile.user_data);
        ui_sound_play(UI_SFX_PRESS);
        return true;
    }
    return false;
}

int32_t ui_profile_page_selected_index(void)
{
    return s_profile.selected;
}
