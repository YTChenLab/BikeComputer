#include "ui_summary_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_motion.h"
#include "../core/ui_sound.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SUMMARY_METRIC_COUNT 8U
#define SUMMARY_REVEAL_COUNT 10U
#define SUMMARY_HERO_Y 158
#define SUMMARY_METRIC_START_Y 312
#define SUMMARY_METRIC_ROW_STEP 80
#define SUMMARY_COMPLETE_Y 712

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *value;
    lv_obj_t *unit;
} summary_metric_t;

static ui_status_bar_t s_status;
static ui_page_action_cb_t s_callback;
static void *s_user_data;
static lv_obj_t *s_hero;
static lv_obj_t *s_distance;
static lv_obj_t *s_save_badge;
static lv_obj_t *s_complete_button;
static summary_metric_t s_metrics[SUMMARY_METRIC_COUNT];
static lv_obj_t *s_reveal[SUMMARY_REVEAL_COUNT];
static int32_t s_reveal_y[SUMMARY_REVEAL_COUNT];
static ui_ride_summary_t s_data;
static bool s_active;

static lv_obj_t *label(lv_obj_t *parent, int32_t x, int32_t y,
                       int32_t width, const char *text,
                       const lv_font_t *font, lv_color_t color,
                       lv_text_align_t align)
{
    lv_obj_t *object = ui_page_label(parent, x, y, width, text, font, align);
    lv_obj_set_style_text_color(object, color, 0);
    lv_label_set_long_mode(object, LV_LABEL_LONG_CLIP);
    return object;
}

static void complete_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    if(code == LV_EVENT_PRESSED) ui_motion_button_press(target);
    else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
        ui_motion_button_release(target);
    else if(code == LV_EVENT_CLICKED && s_callback != NULL) {
        ui_sound_play(UI_SFX_CONFIRM);
        s_callback(UI_PAGE_ACTION_HOME, s_user_data);
    }
}

static void distance_frame(void *target, int32_t hundredths)
{
    char text[24];
    snprintf(text, sizeof(text), "%ld.%02ld",
             (long)(hundredths / 100), (long)(hundredths % 100));
    lv_label_set_text((lv_obj_t *)target, text);
}

static void make_metric(lv_obj_t *screen, uint8_t index,
                        int32_t x, int32_t y,
                        const char *title, const char *unit)
{
    summary_metric_t *metric = &s_metrics[index];
    metric->root = ui_page_card(screen, x, y, 216, 74);
    lv_obj_set_style_radius(metric->root, UI_TOKEN_RADIUS_MEDIUM, 0);
    label(metric->root, 14, 7, 188, title, UI_FONT_PAGE_TEXT,
          UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    metric->value = label(metric->root, 14, 27, 140, "--",
                          &ui_font_misans_num_48, UI_COLOR_WHITE,
                          LV_TEXT_ALIGN_LEFT);
    metric->unit = label(metric->root, 151, 43, 53, unit,
                         &lv_font_montserrat_16,
                         UI_TOKEN_COLOR_TEXT_SECONDARY,
                         LV_TEXT_ALIGN_RIGHT);
}

lv_obj_t *ui_summary_page_create(ui_page_action_cb_t callback,
                                 void *user_data)
{
    s_callback = callback;
    s_user_data = user_data;
    s_active = false;
    memset(&s_data, 0, sizeof(s_data));
    lv_obj_t *screen = ui_page_screen_create();
    ui_page_status_create(&s_status, screen);

    label(screen, 24, 88, 270, "骑行完成", UI_FONT_PAGE_TEXT,
          UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
    label(screen, 24, 116, 270, "RIDE COMPLETE", &lv_font_montserrat_18,
          UI_COLOR_YELLOW, LV_TEXT_ALIGN_LEFT);
    label(screen, 326, 98, 128, "GOOD RIDE", &lv_font_montserrat_14,
          UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_RIGHT);
    ui_page_line(screen, 20, 145, 440, 1, UI_TOKEN_COLOR_SURFACE_BORDER);

    s_hero = ui_page_card(screen, 16, SUMMARY_HERO_Y, 448, 142);
    lv_obj_set_style_border_color(s_hero, UI_COLOR_YELLOW, 0);
    label(s_hero, 18, 14, 220, "TOTAL DISTANCE", &lv_font_montserrat_16,
          UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_distance = label(s_hero, 18, 45, 300, "0.00",
                       &ui_font_misans_metric_60, UI_COLOR_WHITE,
                       LV_TEXT_ALIGN_LEFT);
    label(s_hero, 309, 83, 70, "KM", &lv_font_montserrat_18,
          UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_save_badge = label(s_hero, 330, 17, 94, "SAVED",
                         &lv_font_montserrat_14, UI_COLOR_YELLOW,
                         LV_TEXT_ALIGN_RIGHT);

    static const char * const titles[SUMMARY_METRIC_COUNT] = {
        "骑行时间", "平均速度", "最大速度", "累计爬升",
        "平均心率", "平均功率", "平均踏频", "保存状态"
    };
    static const char * const units[SUMMARY_METRIC_COUNT] = {
        "", "KM/H", "KM/H", "M", "BPM", "W", "RPM", ""
    };
    for(uint8_t index = 0; index < SUMMARY_METRIC_COUNT; index++) {
        int32_t x = 16 + (index % 2U) * 226;
        int32_t y = SUMMARY_METRIC_START_Y +
                    (int32_t)(index / 2U) * SUMMARY_METRIC_ROW_STEP;
        make_metric(screen, index, x, y, titles[index], units[index]);
    }

    /* Duration and save state are textual values and use the complete UI
     * font instead of the large numeric-only font. */
    lv_obj_set_style_text_font(s_metrics[0].value, UI_FONT_PAGE_TEXT, 0);
    lv_obj_set_y(s_metrics[0].value, 37);
    lv_obj_set_width(s_metrics[0].value, 188);
    lv_obj_set_style_text_font(s_metrics[7].value,
                               &lv_font_montserrat_16, 0);
    lv_obj_set_y(s_metrics[7].value, 39);
    lv_obj_set_width(s_metrics[7].value, 188);
    lv_obj_set_style_text_color(s_metrics[7].value, UI_COLOR_YELLOW, 0);

    s_complete_button = ui_page_box(screen, 16, SUMMARY_COMPLETE_Y, 448, 72,
                                    UI_COLOR_YELLOW, 2);
    lv_obj_set_style_bg_color(s_complete_button, UI_COLOR_YELLOW, 0);
    lv_obj_set_style_bg_opa(s_complete_button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_complete_button, UI_TOKEN_RADIUS_MEDIUM, 0);
    lv_obj_add_flag(s_complete_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_complete_button, complete_event, LV_EVENT_ALL, NULL);
    label(s_complete_button, 24, 16, 270, "完成", UI_FONT_PAGE_TEXT,
          UI_COLOR_BG, LV_TEXT_ALIGN_LEFT);
    label(s_complete_button, 24, 43, 270, "DONE", &lv_font_montserrat_14,
          UI_COLOR_BG, LV_TEXT_ALIGN_LEFT);
    label(s_complete_button, 390, 22, 34, LV_SYMBOL_RIGHT,
          LV_FONT_DEFAULT, UI_COLOR_BG, LV_TEXT_ALIGN_RIGHT);

    s_reveal[0] = s_hero;
    for(uint8_t index = 0; index < SUMMARY_METRIC_COUNT; index++)
        s_reveal[index + 1U] = s_metrics[index].root;
    s_reveal[9] = s_complete_button;

    /* The page has not completed its first LVGL layout pass here. Reading
     * object coordinates with lv_obj_get_y() can therefore return 0 and the
     * reveal animation would permanently stack every card at the top. Keep
     * animation destinations tied to the same deterministic page geometry
     * used when the objects are created. */
    s_reveal_y[0] = SUMMARY_HERO_Y;
    for(uint8_t index = 0; index < SUMMARY_METRIC_COUNT; index++) {
        s_reveal_y[index + 1U] = SUMMARY_METRIC_START_Y +
                                 (int32_t)(index / 2U) *
                                 SUMMARY_METRIC_ROW_STEP;
    }
    s_reveal_y[9] = SUMMARY_COMPLETE_Y;

    ui_summary_page_set_data(&s_data);
    return screen;
}

void ui_summary_page_set_data(const ui_ride_summary_t *summary)
{
    if(summary == NULL || s_distance == NULL) return;
    s_data = *summary;
    char text[32];
    snprintf(text, sizeof(text), "%.2f", summary->distance_km);
    lv_label_set_text(s_distance, text);

    uint32_t hours = summary->duration_seconds / 3600U;
    uint32_t minutes = (summary->duration_seconds / 60U) % 60U;
    uint32_t seconds = summary->duration_seconds % 60U;
    snprintf(text, sizeof(text), "%02lu:%02lu:%02lu",
             (unsigned long)hours, (unsigned long)minutes,
             (unsigned long)seconds);
    lv_label_set_text(s_metrics[0].value, text);
    snprintf(text, sizeof(text), "%.1f", summary->average_speed_kmh);
    lv_label_set_text(s_metrics[1].value, text);
    snprintf(text, sizeof(text), "%.1f", summary->maximum_speed_kmh);
    lv_label_set_text(s_metrics[2].value, text);
    snprintf(text, sizeof(text), "%lu", (unsigned long)summary->ascent_m);
    lv_label_set_text(s_metrics[3].value, text);
    snprintf(text, sizeof(text), "%u", summary->average_heart_rate);
    lv_label_set_text(s_metrics[4].value, text);
    snprintf(text, sizeof(text), "%u", summary->average_power);
    lv_label_set_text(s_metrics[5].value, text);
    snprintf(text, sizeof(text), "%u", summary->average_cadence);
    lv_label_set_text(s_metrics[6].value, text);
    lv_label_set_text(s_metrics[7].value,
                      summary->saved ? "SAVED LOCALLY" : "SAVE PENDING");
    lv_label_set_text(s_save_badge, summary->saved ? "SAVED" : "PENDING");
}

void ui_summary_page_set_active(bool active)
{
    if(s_active == active) return;
    s_active = active;
    if(!active) return;

    for(uint8_t index = 0; index < SUMMARY_REVEAL_COUNT; index++) {
        uint32_t delay = index == 0U ? 0U : 90U + (index - 1U) * 45U;
        ui_motion_translate_fade_delayed(
            s_reveal[index], s_reveal_y[index] + 10, s_reveal_y[index],
            LV_OPA_TRANSP, LV_OPA_COVER, 260U, delay, NULL, NULL);
    }
    int32_t hundredths = (int32_t)(s_data.distance_km * 100.0f + 0.5f);
    if(hundredths < 0) hundredths = 0;
    ui_motion_value(s_distance, distance_frame, 0, hundredths, 620U,
                    lv_anim_path_ease_out, NULL, NULL);
}

bool ui_summary_page_handle_input(ui_input_event_t input)
{
    if(input == UI_INPUT_ENCODER_PRESS) {
        if(s_callback != NULL) {
            ui_sound_play(UI_SFX_CONFIRM);
            s_callback(UI_PAGE_ACTION_HOME, s_user_data);
        }
        return true;
    }
    if(input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW) {
        ui_sound_play(UI_SFX_FOCUS);
        return true;
    }
    return false;
}
