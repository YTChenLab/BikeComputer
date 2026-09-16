#include "ui_control_center.h"

#include "../core/ui_background.h"
#include "../core/ui_motion.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CC_CARD_W          216
#define CC_CARD_H           96
#define CC_LEFT             16
#define CC_RIGHT           248
#define CC_CONNECTION_Y     78
#define CC_SLIDER_TOP      194
#define CC_SLIDER_HEIGHT   230
#define CC_SLIDER_BOTTOM   (CC_SLIDER_TOP + CC_SLIDER_HEIGHT)
#define CC_SOUND_LEFT       16
#define CC_SOUND_RIGHT     116
#define CC_BRIGHT_LEFT     128
#define CC_BRIGHT_RIGHT    228
#define CC_DETAIL_TOP      194
#define CC_DETAIL_H        106
#define CC_DETAIL_GAP       18
#define CC_STATUS_TOP      458
#define CC_STATUS_H         92
#define CC_FOCUS_COUNT       7
#define CC_LEVEL_STEP        5

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *controls[UI_CONTROL_CENTER_ACTION_COUNT];
    lv_obj_t *state_labels[UI_CONTROL_CENTER_ACTION_COUNT];
    lv_obj_t *icon_backgrounds[UI_CONTROL_CENTER_ACTION_COUNT];
    lv_obj_t *sound_fill;
    lv_obj_t *brightness_fill;
    lv_obj_t *battery_state;
    ui_control_center_state_t state;
    ui_control_center_feedback_t feedback[UI_CONTROL_CENTER_ACTION_COUNT];
    ui_control_center_action_cb_t callback;
    void *user_data;
    uint8_t focus_index;
    bool adjusting;
} ui_control_center_context_t;

static const ui_control_center_action_t s_focus_order[CC_FOCUS_COUNT] = {
    UI_CONTROL_CENTER_ACTION_WIFI,
    UI_CONTROL_CENTER_ACTION_BLUETOOTH,
    UI_CONTROL_CENTER_ACTION_SOUND,
    UI_CONTROL_CENTER_ACTION_BRIGHTNESS,
    UI_CONTROL_CENTER_ACTION_GPS,
    UI_CONTROL_CENTER_ACTION_POWER_SAVE,
    UI_CONTROL_CENTER_ACTION_TOUCH_LOCK
};

static ui_control_center_context_t s_center;

static uint8_t clamp_level(int32_t value)
{
    if(value < 0) return 0U;
    if(value > 100) return 100U;
    return (uint8_t)value;
}

static lv_obj_t *box(lv_obj_t *parent, int32_t x, int32_t y,
                     int32_t width, int32_t height)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(object, UI_TOKEN_COLOR_SURFACE_BORDER, 0);
    lv_obj_set_style_border_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(object, UI_TOKEN_BORDER_THIN, 0);
    lv_obj_set_style_radius(object, UI_TOKEN_RADIUS_SURFACE, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

static lv_obj_t *label(lv_obj_t *parent, int32_t x, int32_t y,
                       int32_t width, const char *text,
                       const lv_font_t *font, lv_text_align_t align)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, width);
    lv_obj_set_style_text_color(object, UI_TOKEN_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_align(object, align, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

static lv_obj_t *icon_circle(lv_obj_t *parent, int32_t x, int32_t y,
                             const char *icon)
{
    lv_obj_t *circle = box(parent, x, y, 58, 58);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(circle, 0, 0);
    lv_obj_set_style_bg_color(circle, UI_TOKEN_COLOR_DECORATION, 0);
    label(circle, 0, 16, 58, icon, &lv_font_montserrat_24,
          LV_TEXT_ALIGN_CENTER);
    return circle;
}

static bool action_enabled(ui_control_center_action_t action)
{
    switch(action) {
        case UI_CONTROL_CENTER_ACTION_WIFI:
            return s_center.state.wifi_enabled;
        case UI_CONTROL_CENTER_ACTION_BLUETOOTH:
            return s_center.state.bluetooth_enabled;
        case UI_CONTROL_CENTER_ACTION_GPS:
            return s_center.state.gps;
        case UI_CONTROL_CENTER_ACTION_POWER_SAVE:
            return s_center.state.power_save;
        case UI_CONTROL_CENTER_ACTION_TOUCH_LOCK:
            return s_center.state.touch_locked;
        default:
            return true;
    }
}

static void update_visuals(void);

static void action_event(lv_event_t *event)
{
    ui_control_center_action_t action =
        (ui_control_center_action_t)(uintptr_t)lv_event_get_user_data(event);
    if(action >= UI_CONTROL_CENTER_ACTION_COUNT) return;
    lv_event_code_t code = lv_event_get_code(event);
    if(code == LV_EVENT_PRESSED) {
        if(s_center.icon_backgrounds[action] != NULL)
            ui_motion_button_press(s_center.icon_backgrounds[action]);
        lv_obj_set_style_border_color(lv_event_get_target_obj(event),
                                      UI_TOKEN_COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(lv_event_get_target_obj(event),
                                      UI_TOKEN_BORDER_FOCUS, 0);
        return;
    }
    if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if(s_center.icon_backgrounds[action] != NULL)
            ui_motion_button_release(s_center.icon_backgrounds[action]);
        update_visuals();
        return;
    }
    if(code != LV_EVENT_CLICKED ||
       s_center.feedback[action] == UI_CONTROL_CENTER_FEEDBACK_PENDING)
        return;
    s_center.feedback[action] = UI_CONTROL_CENTER_FEEDBACK_IDLE;
    if(s_center.callback != NULL)
        s_center.callback(action, -1, s_center.user_data);
    update_visuals();
}

static lv_obj_t *connection_card(int32_t x, const char *icon,
                                 const char *title,
                                 ui_control_center_action_t action)
{
    lv_obj_t *card = box(s_center.root, x, CC_CONNECTION_Y,
                         CC_CARD_W, CC_CARD_H);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, action_event, LV_EVENT_ALL,
                        (void *)(uintptr_t)action);
    s_center.controls[action] = card;
    s_center.icon_backgrounds[action] = icon_circle(card, 14, 19, icon);
    label(card, 82, 16, 120, title, UI_FONT_PAGE_TEXT,
          LV_TEXT_ALIGN_LEFT);
    s_center.state_labels[action] = label(
        card, 82, 50, 120, "", &lv_font_montserrat_14,
        LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(s_center.state_labels[action], LV_LABEL_LONG_DOT);
    return card;
}

static void vertical_slider(int32_t x, const char *title, const char *icon,
                            ui_control_center_action_t action,
                            lv_obj_t **fill)
{
    lv_obj_t *track = box(s_center.root, x, CC_SLIDER_TOP, 100,
                          CC_SLIDER_HEIGHT);
    lv_obj_set_style_clip_corner(track, true, 0);
    *fill = box(track, 0, CC_SLIDER_HEIGHT, 100, 0);
    lv_obj_set_style_radius(*fill, 0, 0);
    lv_obj_set_style_border_width(*fill, 0, 0);
    lv_obj_set_style_bg_color(*fill, UI_TOKEN_COLOR_ACCENT, 0);
    label(track, 8, 12, 84, title, &lv_font_montserrat_14,
          LV_TEXT_ALIGN_CENTER);
    s_center.icon_backgrounds[action] = icon_circle(
        track, 21, CC_SLIDER_HEIGHT - 76, icon);
    lv_obj_set_style_bg_color(s_center.icon_backgrounds[action],
                              lv_color_hex(0x171B1B), 0);
    s_center.controls[action] = track;
}

static lv_obj_t *detail_card(int32_t y, const char *icon,
                             const char *title,
                             ui_control_center_action_t action)
{
    lv_obj_t *card = box(s_center.root, CC_RIGHT, y,
                         CC_CARD_W, CC_DETAIL_H);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, action_event, LV_EVENT_ALL,
                        (void *)(uintptr_t)action);
    s_center.controls[action] = card;
    s_center.icon_backgrounds[action] = icon_circle(card, 14, 24, icon);
    label(card, 82, 18, 122, title, UI_FONT_PAGE_TEXT,
          LV_TEXT_ALIGN_LEFT);
    s_center.state_labels[action] = label(
        card, 82, 52, 122, "", &lv_font_montserrat_14,
        LV_TEXT_ALIGN_LEFT);
    return card;
}

static void status_cards(void)
{
    lv_obj_t *touch = box(s_center.root, CC_LEFT, CC_STATUS_TOP,
                          CC_CARD_W, CC_STATUS_H);
    lv_obj_add_flag(touch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(touch, action_event, LV_EVENT_ALL,
                        (void *)(uintptr_t)
                        UI_CONTROL_CENTER_ACTION_TOUCH_LOCK);
    s_center.controls[UI_CONTROL_CENTER_ACTION_TOUCH_LOCK] = touch;
    s_center.icon_backgrounds[UI_CONTROL_CENTER_ACTION_TOUCH_LOCK] =
        icon_circle(touch, 14, 17, LV_SYMBOL_KEYBOARD);
    label(touch, 82, 14, 122, "触摸锁定", UI_FONT_PAGE_TEXT,
          LV_TEXT_ALIGN_LEFT);
    s_center.state_labels[UI_CONTROL_CENTER_ACTION_TOUCH_LOCK] = label(
        touch, 82, 49, 122, "", &lv_font_montserrat_14,
        LV_TEXT_ALIGN_LEFT);

    lv_obj_t *battery = box(s_center.root, CC_RIGHT, CC_STATUS_TOP,
                            CC_CARD_W, CC_STATUS_H);
    icon_circle(battery, 14, 17, LV_SYMBOL_BATTERY_3);
    label(battery, 82, 14, 122, "电池状态", UI_FONT_PAGE_TEXT,
          LV_TEXT_ALIGN_LEFT);
    s_center.battery_state = label(
        battery, 82, 49, 122, "", &lv_font_montserrat_14,
        LV_TEXT_ALIGN_LEFT);
}

static void update_slider(lv_obj_t *fill, uint8_t level)
{
    int32_t height = CC_SLIDER_HEIGHT * clamp_level(level) / 100;
    lv_obj_set_y(fill, CC_SLIDER_HEIGHT - height);
    lv_obj_set_height(fill, height);
}

static const char *feedback_text(ui_control_center_action_t action)
{
    if(s_center.feedback[action] == UI_CONTROL_CENTER_FEEDBACK_PENDING)
        return "处理中";
    if(s_center.feedback[action] == UI_CONTROL_CENTER_FEEDBACK_FAILED)
        return "操作失败";
    return NULL;
}

static void update_state_text(void)
{
    const char *override = feedback_text(UI_CONTROL_CENTER_ACTION_WIFI);
    const char *wifi = !s_center.state.wifi_enabled ? "未开启" :
        (s_center.state.wifi_connected ?
         (s_center.state.wifi_name[0] != '\0' ?
          s_center.state.wifi_name : "已连接") : "等待连接");
    lv_label_set_text(s_center.state_labels[UI_CONTROL_CENTER_ACTION_WIFI],
                      override != NULL ? override : wifi);

    override = feedback_text(UI_CONTROL_CENTER_ACTION_BLUETOOTH);
    const char *bluetooth = !s_center.state.bluetooth_enabled ? "未开启" :
        (s_center.state.bluetooth_connected ?
         (s_center.state.bluetooth_name[0] != '\0' ?
          s_center.state.bluetooth_name : "已连接") : "等待连接");
    lv_label_set_text(
        s_center.state_labels[UI_CONTROL_CENTER_ACTION_BLUETOOTH],
        override != NULL ? override : bluetooth);

    override = feedback_text(UI_CONTROL_CENTER_ACTION_GPS);
    const char *signal = s_center.state.satellites >= 8U ? "信号良好" :
                         (s_center.state.satellites >= 4U ?
                          "信号一般" : "暂无信号");
    lv_label_set_text(s_center.state_labels[UI_CONTROL_CENTER_ACTION_GPS],
                      override != NULL ? override :
                      (s_center.state.gps ? signal : "未开启"));

    override = feedback_text(UI_CONTROL_CENTER_ACTION_POWER_SAVE);
    lv_label_set_text(
        s_center.state_labels[UI_CONTROL_CENTER_ACTION_POWER_SAVE],
        override != NULL ? override :
        (s_center.state.power_save ? "已开启" : "已关闭"));

    override = feedback_text(UI_CONTROL_CENTER_ACTION_TOUCH_LOCK);
    lv_label_set_text(
        s_center.state_labels[UI_CONTROL_CENTER_ACTION_TOUCH_LOCK],
        override != NULL ? override :
        (s_center.state.touch_locked ? "已开启" : "已关闭"));

    char battery[32];
    snprintf(battery, sizeof(battery), "%u%% · %s",
             (unsigned)s_center.state.battery_percent,
             s_center.state.battery_charging ? "充电中" : "未充电");
    lv_label_set_text(s_center.battery_state, battery);
}

static void update_visuals(void)
{
    ui_control_center_action_t selected = s_focus_order[s_center.focus_index];
    for(uint8_t action = 0; action < UI_CONTROL_CENTER_ACTION_COUNT;
        action++) {
        lv_obj_t *control = s_center.controls[action];
        if(control == NULL) continue;
        bool focused = selected == (ui_control_center_action_t)action;
        bool failed = s_center.feedback[action] ==
                      UI_CONTROL_CENTER_FEEDBACK_FAILED;
        bool pending = s_center.feedback[action] ==
                       UI_CONTROL_CENTER_FEEDBACK_PENDING;
        lv_color_t color = failed ? UI_TOKEN_COLOR_CRITICAL :
            ((focused || pending) ? UI_TOKEN_COLOR_ACCENT :
             UI_TOKEN_COLOR_SURFACE_BORDER);
        lv_obj_set_style_border_color(control, color, 0);
        lv_obj_set_style_border_width(
            control, focused || pending || failed ?
            UI_TOKEN_BORDER_FOCUS : UI_TOKEN_BORDER_THIN, 0);
        if(s_center.icon_backgrounds[action] != NULL) {
            lv_obj_set_style_bg_color(
                s_center.icon_backgrounds[action],
                action_enabled((ui_control_center_action_t)action) ?
                UI_TOKEN_COLOR_ACCENT : lv_color_hex(0x253033), 0);
        }
    }
    update_state_text();
    update_slider(s_center.sound_fill, s_center.state.sound_level);
    update_slider(s_center.brightness_fill,
                  s_center.state.brightness_level);
}

void ui_control_center_create(ui_control_center_action_cb_t callback,
                              void *user_data)
{
    memset(&s_center, 0, sizeof(s_center));
    s_center.callback = callback;
    s_center.user_data = user_data;
    s_center.root = box(lv_layer_top(), 0, -UI_TOKEN_SCREEN_HEIGHT,
                        UI_TOKEN_SCREEN_WIDTH, UI_TOKEN_SCREEN_HEIGHT);
    lv_obj_set_style_radius(s_center.root, 0, 0);
    lv_obj_set_style_border_width(s_center.root, 0, 0);
    ui_background_apply(s_center.root);
    lv_obj_add_flag(s_center.root, LV_OBJ_FLAG_HIDDEN);

    label(s_center.root, 20, 20, 250, "控制中心", UI_FONT_PAGE_TEXT,
          LV_TEXT_ALIGN_LEFT);
    label(s_center.root, 270, 22, 190, "上滑收起", UI_FONT_PAGE_TEXT,
          LV_TEXT_ALIGN_RIGHT);
    lv_obj_t *handle = box(s_center.root, 190, 52, 100, 4);
    lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_set_style_bg_color(handle, UI_TOKEN_COLOR_TEXT_PRIMARY, 0);

    connection_card(CC_LEFT, LV_SYMBOL_WIFI, "WiFi",
                    UI_CONTROL_CENTER_ACTION_WIFI);
    connection_card(CC_RIGHT, LV_SYMBOL_BLUETOOTH, "蓝牙",
                    UI_CONTROL_CENTER_ACTION_BLUETOOTH);
    vertical_slider(CC_SOUND_LEFT, "SOUND", LV_SYMBOL_VOLUME_MAX,
                    UI_CONTROL_CENTER_ACTION_SOUND,
                    &s_center.sound_fill);
    vertical_slider(CC_BRIGHT_LEFT, "DISPLAY", LV_SYMBOL_EYE_OPEN,
                    UI_CONTROL_CENTER_ACTION_BRIGHTNESS,
                    &s_center.brightness_fill);
    detail_card(CC_DETAIL_TOP, LV_SYMBOL_GPS, "卫星定位",
                UI_CONTROL_CENTER_ACTION_GPS);
    detail_card(CC_DETAIL_TOP + CC_DETAIL_H + CC_DETAIL_GAP,
                LV_SYMBOL_BATTERY_3, "省电模式",
                UI_CONTROL_CENTER_ACTION_POWER_SAVE);
    status_cards();
    label(s_center.root, 20, 578, 440,
          "SYSTEM QUICK CONTROLS",
          &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    label(s_center.root, 20, 798, 440,
          "向上滑动关闭 · 右键返回",
          UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_CENTER);
    update_visuals();
}

lv_obj_t *ui_control_center_root(void)
{
    return s_center.root;
}

lv_obj_t *ui_control_center_control(ui_control_center_action_t action)
{
    return action < UI_CONTROL_CENTER_ACTION_COUNT ?
           s_center.controls[action] : NULL;
}

void ui_control_center_set_state(const ui_control_center_state_t *state)
{
    if(state == NULL || s_center.root == NULL) return;
    s_center.state = *state;
    s_center.state.wifi_name[sizeof(s_center.state.wifi_name) - 1U] = '\0';
    s_center.state.bluetooth_name[
        sizeof(s_center.state.bluetooth_name) - 1U] = '\0';
    s_center.state.sound_level = clamp_level(s_center.state.sound_level);
    s_center.state.brightness_level =
        clamp_level(s_center.state.brightness_level);
    s_center.state.battery_percent =
        clamp_level(s_center.state.battery_percent);
    update_visuals();
}

void ui_control_center_set_feedback(ui_control_center_action_t action,
                                    ui_control_center_feedback_t feedback)
{
    if(action >= UI_CONTROL_CENTER_ACTION_COUNT ||
       feedback > UI_CONTROL_CENTER_FEEDBACK_FAILED) return;
    s_center.feedback[action] = feedback;
    update_visuals();
}

void ui_control_center_prepare_show(void)
{
    s_center.focus_index = 0U;
    s_center.adjusting = false;
    update_visuals();
}

void ui_control_center_preview_level(ui_control_center_slider_t slider,
                                     uint8_t level)
{
    level = clamp_level(level);
    if(slider == UI_CONTROL_CENTER_SLIDER_SOUND)
        s_center.state.sound_level = level;
    else if(slider == UI_CONTROL_CENTER_SLIDER_BRIGHTNESS)
        s_center.state.brightness_level = level;
    update_visuals();
}

bool ui_control_center_handle_input(ui_input_event_t input)
{
    if(input != UI_INPUT_ENCODER_CW &&
       input != UI_INPUT_ENCODER_CCW &&
       input != UI_INPUT_ENCODER_PRESS) return false;
    ui_control_center_action_t action = s_focus_order[s_center.focus_index];
    if(input == UI_INPUT_ENCODER_PRESS) {
        if(action == UI_CONTROL_CENTER_ACTION_SOUND ||
           action == UI_CONTROL_CENTER_ACTION_BRIGHTNESS) {
            s_center.adjusting = !s_center.adjusting;
            update_visuals();
            return true;
        }
        if(s_center.feedback[action] != UI_CONTROL_CENTER_FEEDBACK_PENDING &&
           s_center.callback != NULL) {
            s_center.feedback[action] = UI_CONTROL_CENTER_FEEDBACK_IDLE;
            s_center.callback(action, -1, s_center.user_data);
            update_visuals();
        }
        return true;
    }

    int32_t direction = input == UI_INPUT_ENCODER_CW ? 1 : -1;
    if(s_center.adjusting &&
       (action == UI_CONTROL_CENTER_ACTION_SOUND ||
        action == UI_CONTROL_CENTER_ACTION_BRIGHTNESS)) {
        uint8_t current = action == UI_CONTROL_CENTER_ACTION_SOUND ?
            s_center.state.sound_level : s_center.state.brightness_level;
        uint8_t next = clamp_level((int32_t)current +
                                   direction * CC_LEVEL_STEP);
        ui_control_center_preview_level(
            action == UI_CONTROL_CENTER_ACTION_SOUND ?
            UI_CONTROL_CENTER_SLIDER_SOUND :
            UI_CONTROL_CENTER_SLIDER_BRIGHTNESS, next);
        if(s_center.callback != NULL)
            s_center.callback(action, next, s_center.user_data);
        return true;
    }

    int32_t next = (int32_t)s_center.focus_index + direction;
    if(next < 0) next = CC_FOCUS_COUNT - 1;
    if(next >= CC_FOCUS_COUNT) next = 0;
    s_center.focus_index = (uint8_t)next;
    update_visuals();
    return true;
}

ui_control_center_action_t ui_control_center_selected_action(void)
{
    return s_focus_order[s_center.focus_index];
}

bool ui_control_center_is_adjusting(void)
{
    return s_center.adjusting;
}

ui_control_center_slider_t ui_control_center_slider_at(int32_t screen_x,
                                                       int32_t screen_y)
{
    if(s_center.root == NULL) return UI_CONTROL_CENTER_SLIDER_NONE;
    int32_t local_y = screen_y - lv_obj_get_y(s_center.root);
    if(local_y < CC_SLIDER_TOP || local_y > CC_SLIDER_BOTTOM)
        return UI_CONTROL_CENTER_SLIDER_NONE;
    if(screen_x >= CC_SOUND_LEFT && screen_x < CC_SOUND_RIGHT)
        return UI_CONTROL_CENTER_SLIDER_SOUND;
    if(screen_x >= CC_BRIGHT_LEFT && screen_x < CC_BRIGHT_RIGHT)
        return UI_CONTROL_CENTER_SLIDER_BRIGHTNESS;
    return UI_CONTROL_CENTER_SLIDER_NONE;
}

uint8_t ui_control_center_level_from_y(int32_t screen_y)
{
    int32_t root_y = s_center.root != NULL ? lv_obj_get_y(s_center.root) : 0;
    int32_t local_y = screen_y - root_y;
    return clamp_level((CC_SLIDER_BOTTOM - local_y) * 100 /
                       CC_SLIDER_HEIGHT);
}
