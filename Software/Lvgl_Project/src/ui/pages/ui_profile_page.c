#include "ui_profile_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_sound.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"
#include "../models/ui_home_data_model.h"
#include "../models/ui_ride_history_model.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    lv_obj_t *screen;
    lv_obj_t *name;
    lv_obj_t *tagline;
    lv_obj_t *distance;
    lv_obj_t *time;
    lv_obj_t *ascent;
    lv_obj_t *rides;
    lv_obj_t *achievements;
    lv_obj_t *ride_action_value;
    lv_obj_t *achievement_action_value;
    lv_obj_t *actions[3];
    ui_page_action_t action_ids[3];
    ui_page_action_cb_t callback;
    void *user_data;
    int32_t selected;
    bool active;
} profile_context_t;

static profile_context_t s_profile;
static ui_status_bar_t s_status;

static lv_obj_t *profile_label(lv_obj_t *parent, int32_t x, int32_t y,
                               int32_t width, const char *text,
                               const lv_font_t *font, lv_color_t color,
                               lv_text_align_t align)
{
    lv_obj_t *label = ui_page_label(parent, x, y, width, text, font, align);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

static void action_event(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    uint8_t index = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    if(index >= 3U) return;
    s_profile.selected = index;
    if(s_profile.callback != NULL)
        s_profile.callback(s_profile.action_ids[index], s_profile.user_data);
}

static void update_focus(void)
{
    for(uint8_t index = 0U; index < 3U; index++) {
        bool focused = s_profile.active && s_profile.selected == index;
        lv_obj_set_style_border_color(
            s_profile.actions[index],
            focused ? UI_TOKEN_COLOR_ACCENT : UI_TOKEN_COLOR_SURFACE_BORDER,
            0);
        lv_obj_set_style_border_width(
            s_profile.actions[index],
            focused ? UI_TOKEN_BORDER_FOCUS : UI_TOKEN_BORDER_THIN, 0);
        lv_obj_set_style_shadow_color(s_profile.actions[index],
                                      UI_TOKEN_COLOR_ACCENT, 0);
        lv_obj_set_style_shadow_width(s_profile.actions[index],
                                      focused ? 8 : 0, 0);
        lv_obj_set_style_shadow_opa(s_profile.actions[index],
                                    focused ? LV_OPA_30 : LV_OPA_TRANSP, 0);
    }
}

static lv_obj_t *profile_action(lv_obj_t *parent, int32_t y,
                                const char *title, const char *detail,
                                const char *value, uint8_t index,
                                ui_page_action_t action,
                                lv_obj_t **value_label)
{
    lv_obj_t *row = ui_page_box(parent, 16, y, 448, 62,
                                UI_TOKEN_COLOR_SURFACE_BORDER, 1);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x080D10), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_set_style_transform_scale(row, 250, LV_STATE_PRESSED);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, action_event, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)index);
    s_profile.actions[index] = row;
    s_profile.action_ids[index] = action;
    profile_label(row, 16, 9, 220, title, UI_FONT_PAGE_TEXT,
                  UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    profile_label(row, 16, 36, 250, detail, &lv_font_montserrat_12,
                  UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *label = profile_label(
        row, 300, 19, 112, value, &lv_font_montserrat_16,
        UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_RIGHT);
    if(value_label != NULL) *value_label = label;
    return row;
}

static void update_data(const ui_home_data_t *data)
{
    if(data == NULL || s_profile.screen == NULL) return;
    char text[48];
    lv_label_set_text(s_profile.name, data->user_name);
    lv_label_set_text(s_profile.tagline, data->tagline);
    snprintf(text, sizeof(text), "%lu",
             (unsigned long)data->total_distance_km);
    lv_label_set_text(s_profile.distance, text);
    snprintf(text, sizeof(text), "%lu h %02u m",
             (unsigned long)data->total_time_hours,
             (unsigned)data->total_time_minutes);
    lv_label_set_text(s_profile.time, text);
    snprintf(text, sizeof(text), "%lu m",
             (unsigned long)data->total_ascent_m);
    lv_label_set_text(s_profile.ascent, text);
}

static void home_data_changed(const ui_home_data_t *data, void *user_data)
{
    (void)user_data;
    update_data(data);
}

lv_obj_t *ui_profile_page_create(ui_page_action_cb_t callback,
                                 void *user_data)
{
    memset(&s_profile, 0, sizeof(s_profile));
    s_profile.callback = callback;
    s_profile.user_data = user_data;
    s_profile.selected = -1;
    s_profile.screen = ui_page_screen_create();
    ui_page_status_create(&s_status, s_profile.screen);
    ui_page_title(s_profile.screen, "个人主页", "PROFILE");

    lv_obj_t *identity = ui_page_card(s_profile.screen, 16, 158, 448, 116);
    lv_obj_t *avatar = ui_page_box(identity, 18, 22, 72, 72,
                                   UI_TOKEN_COLOR_ACCENT, 1);
    lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(avatar, lv_color_hex(0x273C48), 0);
    lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);
    profile_label(avatar, 0, 22, 72, "AC", &lv_font_montserrat_20,
                  UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER);
    s_profile.name = profile_label(identity, 112, 22, 250, "Alex Chen",
                                   &lv_font_montserrat_22,
                                   UI_TOKEN_COLOR_TEXT_PRIMARY,
                                   LV_TEXT_ALIGN_LEFT);
    s_profile.tagline = profile_label(identity, 112, 58, 280,
                                      "Good Roads, Better Days.",
                                      &lv_font_montserrat_14,
                                      UI_TOKEN_COLOR_TEXT_SECONDARY,
                                      LV_TEXT_ALIGN_LEFT);
    profile_label(identity, 360, 22, 68, "LV 12", &lv_font_montserrat_16,
                  UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_RIGHT);
    profile_label(identity, 340, 58, 88, "ACTIVE", &lv_font_montserrat_12,
                  UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_RIGHT);

    lv_obj_t *stats = ui_page_card(s_profile.screen, 16, 286, 448, 116);
    profile_label(stats, 16, 13, 120, "DISTANCE", &lv_font_montserrat_12,
                  UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    profile_label(stats, 166, 13, 120, "TIME", &lv_font_montserrat_12,
                  UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    profile_label(stats, 316, 13, 112, "ASCENT", &lv_font_montserrat_12,
                  UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_profile.distance = profile_label(stats, 16, 42, 105, "0",
                                       &ui_font_misans_metric_small_46,
                                       UI_TOKEN_COLOR_TEXT_PRIMARY,
                                       LV_TEXT_ALIGN_LEFT);
    profile_label(stats, 108, 66, 38, "km", &lv_font_montserrat_14,
                  UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_profile.time = profile_label(stats, 166, 50, 135, "0 h 00 m",
                                   &lv_font_montserrat_18,
                                   UI_TOKEN_COLOR_TEXT_PRIMARY,
                                   LV_TEXT_ALIGN_LEFT);
    s_profile.ascent = profile_label(stats, 316, 50, 112, "0 m",
                                     &lv_font_montserrat_18,
                                     UI_TOKEN_COLOR_TEXT_PRIMARY,
                                     LV_TEXT_ALIGN_LEFT);

    profile_action(s_profile.screen, 416, "成就", "ACHIEVEMENTS",
                   "0 / 6", 0, UI_PAGE_ACTION_ACHIEVEMENT,
                   &s_profile.achievement_action_value);
    profile_action(s_profile.screen, 486, "骑行次数", "TOTAL RIDES",
                   "0", 1, UI_PAGE_ACTION_HISTORY,
                   &s_profile.ride_action_value);
    profile_action(s_profile.screen, 556, "设置", "SYSTEM SETTINGS",
                   "OPEN", 2, UI_PAGE_ACTION_SETTINGS, NULL);

    lv_obj_t *footer = ui_page_card(s_profile.screen, 16, 634, 448, 90);
    profile_label(footer, 16, 14, 180, "PROFILE STATUS",
                  &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
                  LV_TEXT_ALIGN_LEFT);
    s_profile.rides = profile_label(footer, 16, 37, 164, "0 rides",
                                    &lv_font_montserrat_20,
                                    UI_TOKEN_COLOR_TEXT_PRIMARY,
                                    LV_TEXT_ALIGN_LEFT);
    s_profile.achievements = profile_label(footer, 218, 37, 190, "0 / 6",
                                           &lv_font_montserrat_20,
                                           UI_TOKEN_COLOR_ACCENT,
                                           LV_TEXT_ALIGN_RIGHT);

    ui_home_data_t data;
    if(ui_home_data_model_get(&data)) update_data(&data);
    (void)ui_home_data_model_subscribe(home_data_changed, NULL);
    ui_profile_page_refresh();
    return s_profile.screen;
}

void ui_profile_page_refresh(void)
{
    if(s_profile.screen == NULL) return;
    ui_ride_history_totals_t totals;
    ui_ride_history_model_totals(&totals);
    uint8_t unlocked = 0U;
    if(totals.ride_count >= 1U) unlocked++;
    if(totals.total_distance_km >= 100.0) unlocked++;
    if(totals.total_ascent_m >= 2000U) unlocked++;
    if(totals.ride_count >= 10U) unlocked++;
    if(totals.total_distance_km >= 500.0) unlocked++;
    if(totals.total_duration_seconds >= 50U * 3600U) unlocked++;
    char text[32];
    snprintf(text, sizeof(text), "%lu rides",
             (unsigned long)totals.ride_count);
    lv_label_set_text(s_profile.rides, text);
    lv_label_set_text(s_profile.ride_action_value, text);
    snprintf(text, sizeof(text), "%u / 6", unlocked);
    lv_label_set_text(s_profile.achievements, text);
    lv_label_set_text(s_profile.achievement_action_value, text);
}

void ui_profile_page_set_active(bool active)
{
    s_profile.active = active;
    if(active) ui_profile_page_refresh();
    s_profile.selected = active ? 0 : -1;
    update_focus();
}

bool ui_profile_page_handle_input(ui_input_event_t input)
{
    if(!s_profile.active) return false;
    if(input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW) {
        int32_t direction = input == UI_INPUT_ENCODER_CW ? 1 : -1;
        int32_t next = s_profile.selected + direction;
        if(next < 0) next = 0;
        if(next >= 3) next = 2;
        s_profile.selected = next;
        update_focus();
        ui_sound_play(UI_SFX_FOCUS);
        return true;
    }
    if(input == UI_INPUT_ENCODER_PRESS && s_profile.selected >= 0) {
        if(s_profile.callback != NULL)
            s_profile.callback(s_profile.action_ids[s_profile.selected],
                               s_profile.user_data);
        ui_sound_play(UI_SFX_PRESS);
        return true;
    }
    return false;
}

int32_t ui_profile_page_selected_index(void)
{
    return s_profile.selected;
}
