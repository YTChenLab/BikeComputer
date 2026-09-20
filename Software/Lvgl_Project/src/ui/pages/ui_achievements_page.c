#include "ui_achievements_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_sound.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"
#include "../models/ui_ride_history_model.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define ACHIEVEMENT_COUNT 6U

typedef struct
{
    const char *title;
    const char *detail;
} achievement_item_t;

typedef struct
{
    lv_obj_t *screen;
    lv_obj_t *list;
    lv_obj_t *rows[ACHIEVEMENT_COUNT];
    lv_obj_t *badges[ACHIEVEMENT_COUNT];
    lv_obj_t *badge_labels[ACHIEVEMENT_COUNT];
    lv_obj_t *progress_labels[ACHIEVEMENT_COUNT];
    lv_obj_t *summary_status;
    lv_obj_t *summary_percent;
    size_t count;
    int32_t selected;
    bool active;
} achievements_context_t;

static achievements_context_t s_achievements;

static const achievement_item_t s_items[ACHIEVEMENT_COUNT] = {
    {"首骑完成", "Complete your first ride"},
    {"百公里", "Reach 100 km total distance"},
    {"爬升达人", "Reach 2,000 m total ascent"},
    {"十次骑行", "Complete ten saved rides"},
    {"探索者", "Reach 500 km total distance"},
    {"耐力骑手", "Reach 50 hours total time"}
};

static lv_obj_t *achievement_label(lv_obj_t *parent, int32_t x, int32_t y,
                                   int32_t width, const char *text,
                                   const lv_font_t *font, lv_color_t color,
                                   lv_text_align_t align)
{
    lv_obj_t *label = ui_page_label(parent, x, y, width, text, font, align);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

static void update_focus(void)
{
    for(size_t index = 0U; index < s_achievements.count; index++) {
        bool focused = s_achievements.active &&
                       s_achievements.selected == (int32_t)index;
        lv_obj_set_style_border_color(
            s_achievements.rows[index],
            focused ? UI_TOKEN_COLOR_ACCENT : UI_TOKEN_COLOR_SURFACE_BORDER,
            0);
        lv_obj_set_style_border_width(
            s_achievements.rows[index],
            focused ? UI_TOKEN_BORDER_FOCUS : UI_TOKEN_BORDER_THIN, 0);
        lv_obj_set_style_shadow_color(s_achievements.rows[index],
                                      UI_TOKEN_COLOR_ACCENT, 0);
        lv_obj_set_style_shadow_width(s_achievements.rows[index],
                                      focused ? 8 : 0, 0);
        lv_obj_set_style_shadow_opa(s_achievements.rows[index],
                                    focused ? LV_OPA_30 : LV_OPA_TRANSP, 0);
    }
}

static void row_clicked(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    s_achievements.selected = (int32_t)(uintptr_t)
                             lv_event_get_user_data(event);
    update_focus();
    ui_sound_play(UI_SFX_PRESS);
}

static lv_obj_t *create_row(size_t index, const achievement_item_t *item)
{
    lv_obj_t *row = lv_obj_create(s_achievements.list);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, 432);
    lv_obj_set_height(row, 92);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x080D10), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(row, UI_TOKEN_COLOR_SURFACE_BORDER, 0);
    lv_obj_set_style_border_opa(row, LV_OPA_70, 0);
    lv_obj_set_style_border_width(row, UI_TOKEN_BORDER_THIN, 0);
    lv_obj_set_style_radius(row, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_set_style_transform_scale(row, 250, LV_STATE_PRESSED);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row, row_clicked, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)index);

    lv_obj_t *badge = ui_page_box(row, 14, 18, 56, 56,
                                  UI_TOKEN_COLOR_SURFACE_BORDER, 1);
    s_achievements.badges[index] = badge;
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x1B2228), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    s_achievements.badge_labels[index] = achievement_label(
        badge, 0, 14, 56, "--", &lv_font_montserrat_16,
        UI_TOKEN_COLOR_TEXT_TERTIARY, LV_TEXT_ALIGN_CENTER);

    achievement_label(row, 86, 13, 210, item->title, UI_FONT_PAGE_TEXT,
                      UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    achievement_label(row, 86, 43, 230, item->detail, &lv_font_montserrat_12,
                      UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_achievements.progress_labels[index] = achievement_label(
        row, 300, 32, 116, "--", &lv_font_montserrat_16,
        UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_RIGHT);
    return row;
}

lv_obj_t *ui_achievements_page_create(void)
{
    memset(&s_achievements, 0, sizeof(s_achievements));
    s_achievements.selected = -1;
    s_achievements.screen = ui_page_screen_create();

    static ui_status_bar_t status;
    ui_page_status_create(&status, s_achievements.screen);
    ui_page_title(s_achievements.screen, "成就", "ACHIEVEMENTS");

    lv_obj_t *summary = ui_page_card(s_achievements.screen, 16, 158, 448, 88);
    achievement_label(summary, 16, 14, 200, "骑行里程碑", UI_FONT_PAGE_TEXT,
                      UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_achievements.summary_status = achievement_label(
        summary, 16, 48, 270, "0 unlocked / 6 in progress",
        &lv_font_montserrat_14, UI_TOKEN_COLOR_TEXT_SECONDARY,
        LV_TEXT_ALIGN_LEFT);
    s_achievements.summary_percent = achievement_label(
        summary, 326, 25, 70, "0", &ui_font_misans_num_48,
        UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_RIGHT);
    achievement_label(summary, 398, 48, 30, "%", &lv_font_montserrat_18,
                      UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_RIGHT);

    s_achievements.list = lv_obj_create(s_achievements.screen);
    lv_obj_remove_style_all(s_achievements.list);
    lv_obj_set_pos(s_achievements.list, 16, 260);
    lv_obj_set_size(s_achievements.list, 448, 566);
    lv_obj_set_flex_flow(s_achievements.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_achievements.list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_achievements.list, 8, 0);
    lv_obj_set_style_pad_bottom(s_achievements.list, 12, 0);
    lv_obj_set_scroll_dir(s_achievements.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_achievements.list, LV_SCROLLBAR_MODE_AUTO);

    s_achievements.count = ACHIEVEMENT_COUNT;
    for(size_t index = 0U; index < s_achievements.count; index++)
        s_achievements.rows[index] = create_row(index, &s_items[index]);
    ui_achievements_page_refresh();
    return s_achievements.screen;
}

void ui_achievements_page_refresh(void)
{
    if(s_achievements.screen == NULL) return;
    ui_ride_history_totals_t totals;
    ui_ride_history_model_totals(&totals);
    const double hours = (double)totals.total_duration_seconds / 3600.0;
    const bool unlocked[ACHIEVEMENT_COUNT] = {
        totals.ride_count >= 1U,
        totals.total_distance_km >= 100.0,
        totals.total_ascent_m >= 2000U,
        totals.ride_count >= 10U,
        totals.total_distance_km >= 500.0,
        totals.total_duration_seconds >= 50U * 3600U
    };
    char progress[ACHIEVEMENT_COUNT][32];
    snprintf(progress[0], sizeof(progress[0]), "%lu / 1 ride",
             (unsigned long)(totals.ride_count > 1U ? 1U :
                             totals.ride_count));
    snprintf(progress[1], sizeof(progress[1]), "%.0f / 100 km",
             totals.total_distance_km > 100.0 ? 100.0 :
             totals.total_distance_km);
    snprintf(progress[2], sizeof(progress[2]), "%llu / 2000 m",
             (unsigned long long)(totals.total_ascent_m > 2000U ? 2000U :
                                  totals.total_ascent_m));
    snprintf(progress[3], sizeof(progress[3]), "%lu / 10 rides",
             (unsigned long)(totals.ride_count > 10U ? 10U :
                             totals.ride_count));
    snprintf(progress[4], sizeof(progress[4]), "%.0f / 500 km",
             totals.total_distance_km > 500.0 ? 500.0 :
             totals.total_distance_km);
    snprintf(progress[5], sizeof(progress[5]), "%.0f / 50 h",
             hours > 50.0 ? 50.0 : hours);

    uint8_t unlocked_count = 0U;
    for(uint8_t index = 0U; index < ACHIEVEMENT_COUNT; index++) {
        if(unlocked[index]) unlocked_count++;
        lv_label_set_text(s_achievements.progress_labels[index],
                          progress[index]);
        lv_label_set_text(s_achievements.badge_labels[index],
                          unlocked[index] ? "OK" : "--");
        lv_obj_set_style_text_color(
            s_achievements.badge_labels[index],
            unlocked[index] ? UI_TOKEN_COLOR_ACCENT :
                              UI_TOKEN_COLOR_TEXT_TERTIARY, 0);
        lv_obj_set_style_border_color(
            s_achievements.badges[index],
            unlocked[index] ? UI_TOKEN_COLOR_ACCENT :
                              UI_TOKEN_COLOR_SURFACE_BORDER, 0);
        lv_obj_set_style_bg_color(
            s_achievements.badges[index],
            unlocked[index] ? lv_color_hex(0x493A13) :
                              lv_color_hex(0x1B2228), 0);
        lv_obj_set_style_text_color(
            s_achievements.progress_labels[index],
            unlocked[index] ? UI_TOKEN_COLOR_ACCENT :
                              UI_TOKEN_COLOR_TEXT_PRIMARY, 0);
    }
    char text[48];
    snprintf(text, sizeof(text), "%u unlocked / %u in progress",
             unlocked_count, ACHIEVEMENT_COUNT - unlocked_count);
    lv_label_set_text(s_achievements.summary_status, text);
    snprintf(text, sizeof(text), "%u",
             (unsigned)(unlocked_count * 100U / ACHIEVEMENT_COUNT));
    lv_label_set_text(s_achievements.summary_percent, text);
}

void ui_achievements_page_set_active(bool active)
{
    s_achievements.active = active;
    if(active) ui_achievements_page_refresh();
    if(active && s_achievements.count > 0U)
        s_achievements.selected = 0;
    update_focus();
}

bool ui_achievements_page_handle_input(ui_input_event_t input)
{
    if(!s_achievements.active || s_achievements.count == 0U) return false;
    if(input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW) {
        int32_t direction = input == UI_INPUT_ENCODER_CW ? 1 : -1;
        int32_t next = s_achievements.selected + direction;
        if(next < 0) next = 0;
        if(next >= (int32_t)s_achievements.count)
            next = (int32_t)s_achievements.count - 1;
        if(next != s_achievements.selected) {
            s_achievements.selected = next;
            update_focus();
            lv_obj_scroll_to_view(s_achievements.rows[next], LV_ANIM_ON);
            ui_sound_play(UI_SFX_FOCUS);
        }
        return true;
    }
    if(input == UI_INPUT_ENCODER_PRESS) {
        ui_sound_play(UI_SFX_PRESS);
        return true;
    }
    return false;
}

size_t ui_achievements_page_count(void)
{
    return s_achievements.count;
}

int32_t ui_achievements_page_selected_index(void)
{
    return s_achievements.selected;
}

bool ui_achievements_page_contains_scroll_point(int32_t x, int32_t y)
{
    if(s_achievements.list == NULL) return false;
    lv_area_t area;
    lv_obj_get_coords(s_achievements.list, &area);
    return x >= area.x1 && x <= area.x2 &&
           y >= area.y1 && y <= area.y2;
}
