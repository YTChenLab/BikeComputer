#include "ui_control_center.h"

#include "../core/ui_background.h"
#include "../core/ui_colors.h"
#include "../core/ui_sizes.h"
#include "../core/ui_theme.h"
#include "../pages/ui_page_common.h"

#include <stdint.h>
#include <stdio.h>

#define SLIDER_TOP       194
#define SLIDER_HEIGHT    208
#define SLIDER_BOTTOM    (SLIDER_TOP + SLIDER_HEIGHT)
#define SOUND_LEFT       16
#define SOUND_RIGHT      116
#define BRIGHTNESS_LEFT  128
#define BRIGHTNESS_RIGHT 228

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *tiles[4];
    lv_obj_t *state_labels[4];
    lv_obj_t *icon_backgrounds[4];
    lv_obj_t *sound_fill;
    lv_obj_t *brightness_fill;
    ui_control_center_action_cb_t callback;
    void *user_data;
} ui_control_center_context_t;

static ui_control_center_context_t s_center;

static void action_event(lv_event_t *event)
{
    if(s_center.callback != NULL) {
        s_center.callback((ui_control_center_action_t)(uintptr_t)
                          lv_event_get_user_data(event), s_center.user_data);
    }
}

static lv_obj_t *symbol(lv_obj_t *parent, int32_t x, int32_t y,
                        int32_t width, int32_t height, const char *text,
                        const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y + (height - font->line_height) / 2);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_WHITE, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static lv_obj_t *icon_circle(lv_obj_t *parent, int32_t x, int32_t y,
                             const char *icon)
{
    lv_obj_t *circle = ui_page_box(parent, x, y, 56, 56, UI_COLOR_WHITE, 0);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, UI_COLOR_DECORATION, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    symbol(circle, 0, 0, 56, 56, icon, &lv_font_montserrat_24);
    return circle;
}

static lv_obj_t *connection_card(int32_t x, const char *icon,
                                 const char *title,
                                 ui_control_center_action_t action,
                                 lv_obj_t **icon_background,
                                 lv_obj_t **state_label)
{
    lv_obj_t *card = ui_page_card(s_center.root, x, 78, 216, 96);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, action_event, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)action);
    *icon_background = icon_circle(card, 14, 20, icon);
    ui_page_label(card, 82, 20, 120, title, UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_LEFT);
    *state_label = ui_page_label(card, 82, 53, 126, "", UI_FONT_PAGE_TEXT,
                                 LV_TEXT_ALIGN_LEFT);
    return card;
}

static void vertical_slider(int32_t x, const char *icon, lv_obj_t **fill)
{
    lv_obj_t *track = ui_page_card(s_center.root, x, SLIDER_TOP, 100,
                                   SLIDER_HEIGHT);
    lv_obj_set_style_clip_corner(track, true, 0);
    *fill = ui_page_box(track, 0, 0, 100, SLIDER_HEIGHT, UI_COLOR_WHITE, 0);
    lv_obj_set_style_bg_color(*fill, UI_COLOR_YELLOW, 0);
    lv_obj_set_style_bg_opa(*fill, LV_OPA_COVER, 0);
    symbol(track, 22, SLIDER_HEIGHT - 72, 56, 56, icon,
           &lv_font_montserrat_24);
}

void ui_control_center_create(ui_control_center_action_cb_t callback,
                              void *user_data)
{
    s_center.callback = callback;
    s_center.user_data = user_data;
    s_center.root = ui_page_box(lv_layer_top(), 0, -UI_SCREEN_H,
                                UI_SCREEN_W, UI_SCREEN_H, UI_COLOR_BG, 0);
    ui_background_apply(s_center.root);
    lv_obj_add_flag(s_center.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_center.root, LV_OBJ_FLAG_HIDDEN);

    ui_page_label(s_center.root, 20, 20, 250, "控制中心", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_LEFT);
    ui_page_label(s_center.root, 270, 22, 190, "上滑收起", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_RIGHT);
    lv_obj_t *handle = ui_page_box(s_center.root, 190, 52, 100, 4,
                                   UI_COLOR_WHITE, 0);
    lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);

    s_center.tiles[0] = connection_card(16, LV_SYMBOL_WIFI, "WiFi",
                                       UI_CONTROL_CENTER_ACTION_WIFI,
                                       &s_center.icon_backgrounds[0],
                                       &s_center.state_labels[0]);
    lv_obj_set_style_text_font(s_center.state_labels[0],
                               &lv_font_montserrat_16, 0);
    s_center.tiles[1] = connection_card(248, LV_SYMBOL_BLUETOOTH, "蓝牙",
                                       UI_CONTROL_CENTER_ACTION_BLUETOOTH,
                                       &s_center.icon_backgrounds[1],
                                       &s_center.state_labels[1]);
    vertical_slider(16, LV_SYMBOL_VOLUME_MAX, &s_center.sound_fill);
    vertical_slider(128, LV_SYMBOL_EYE_OPEN, &s_center.brightness_fill);
    ui_page_label(s_center.root, 16, 410, 100, "声音", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(s_center.root, 128, 410, 100, "亮度", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);

    s_center.tiles[2] = ui_page_card(s_center.root, 248, 194, 216, 96);
    lv_obj_add_flag(s_center.tiles[2], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_center.tiles[2], action_event, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)UI_CONTROL_CENTER_ACTION_GPS);
    s_center.icon_backgrounds[2] = icon_circle(s_center.tiles[2], 14, 20,
                                               LV_SYMBOL_GPS);
    s_center.state_labels[2] = ui_page_label(s_center.tiles[2], 82, 25, 126,
                                             "", UI_FONT_PAGE_TEXT,
                                             LV_TEXT_ALIGN_LEFT);

    s_center.tiles[3] = ui_page_card(s_center.root, 248, 306, 216, 96);
    lv_obj_add_flag(s_center.tiles[3], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_center.tiles[3], action_event, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)UI_CONTROL_CENTER_ACTION_POWER_SAVE);
    s_center.icon_backgrounds[3] = icon_circle(s_center.tiles[3], 14, 20,
                                               LV_SYMBOL_BATTERY_3);
    s_center.state_labels[3] = ui_page_label(s_center.tiles[3], 82, 25, 126,
                                             "", UI_FONT_PAGE_TEXT,
                                             LV_TEXT_ALIGN_LEFT);

    ui_page_label(s_center.root, 20, 468, 440,
                  "SYSTEM QUICK CONTROLS",
                  &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
}

lv_obj_t *ui_control_center_root(void)
{
    return s_center.root;
}

void ui_control_center_set_state(const ui_control_center_state_t *state)
{
    if(state == NULL || s_center.root == NULL) return;
    const char *gps_signal = state->satellites >= 8 ? "信号良好" :
                             (state->satellites >= 4 ? "信号一般" :
                                                       "暂无信号");
    char gps_text[40];
    snprintf(gps_text, sizeof(gps_text), "%s\n%s",
             state->gps ? "GPS 已开启" : "GPS 未开启",
             state->gps ? gps_signal : "暂无信号");
    lv_obj_set_style_text_font(s_center.state_labels[0],
                               state->wifi ? &lv_font_montserrat_16 :
                                             UI_FONT_PAGE_TEXT, 0);
    lv_label_set_text(s_center.state_labels[0],
                      state->wifi ? "Bike-SpeedView" : "未开启");
    lv_label_set_text(s_center.state_labels[1],
                      state->bluetooth ? "骑行传感器" : "未开启");
    lv_label_set_text(s_center.state_labels[2], gps_text);
    lv_label_set_text(s_center.state_labels[3],
                      state->power_save ? "省电模式\n已开启" :
                                          "省电模式\n已关闭");

    const bool enabled[] = {
        state->wifi, state->bluetooth, state->gps, state->power_save
    };
    for(uint8_t i = 0; i < 4; i++)
        lv_obj_set_style_bg_color(s_center.icon_backgrounds[i],
                                  enabled[i] ? UI_COLOR_YELLOW :
                                               UI_COLOR_DECORATION, 0);

    uint8_t sound = state->sound_level > 100 ? 100 : state->sound_level;
    uint8_t brightness = state->brightness_level > 100 ?
                         100 : state->brightness_level;
    int32_t sound_height = SLIDER_HEIGHT * sound / 100;
    int32_t brightness_height = SLIDER_HEIGHT * brightness / 100;
    lv_obj_set_pos(s_center.sound_fill, 0, SLIDER_HEIGHT - sound_height);
    lv_obj_set_height(s_center.sound_fill, sound_height);
    lv_obj_set_pos(s_center.brightness_fill, 0,
                   SLIDER_HEIGHT - brightness_height);
    lv_obj_set_height(s_center.brightness_fill, brightness_height);
}

ui_control_center_slider_t ui_control_center_slider_at(int32_t screen_x,
                                                       int32_t screen_y)
{
    if(s_center.root == NULL) return UI_CONTROL_CENTER_SLIDER_NONE;
    int32_t local_y = screen_y - lv_obj_get_y(s_center.root);
    if(local_y < SLIDER_TOP || local_y > SLIDER_BOTTOM)
        return UI_CONTROL_CENTER_SLIDER_NONE;
    if(screen_x >= SOUND_LEFT && screen_x < SOUND_RIGHT)
        return UI_CONTROL_CENTER_SLIDER_SOUND;
    if(screen_x >= BRIGHTNESS_LEFT && screen_x < BRIGHTNESS_RIGHT)
        return UI_CONTROL_CENTER_SLIDER_BRIGHTNESS;
    return UI_CONTROL_CENTER_SLIDER_NONE;
}

uint8_t ui_control_center_level_from_y(int32_t screen_y)
{
    int32_t root_y = s_center.root != NULL ? lv_obj_get_y(s_center.root) : 0;
    int32_t local_y = screen_y - root_y;
    int32_t value = (SLIDER_BOTTOM - local_y) * 100 / SLIDER_HEIGHT;
    if(value < 0) value = 0;
    if(value > 100) value = 100;
    return (uint8_t)value;
}
