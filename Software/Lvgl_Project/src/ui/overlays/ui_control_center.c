#include "ui_control_center.h"

#include "../core/ui_motion.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CC_CARD_W             216
#define CC_CARD_H              78
#define CC_LEFT                16
#define CC_RIGHT              248
#define CC_CONNECTION_Y        62
#define CC_SLIDER_X            16
#define CC_SLIDER_W           448
#define CC_SLIDER_H            62
#define CC_SOUND_Y            152
#define CC_BRIGHTNESS_Y       224
#define CC_TRACK_X            116
#define CC_TRACK_Y             30
#define CC_TRACK_W            282
#define CC_TRACK_H              4
#define CC_THUMB_SIZE          12
#define CC_STATUS_Y           306
#define CC_STATUS_W           104
#define CC_STATUS_H            96
#define CC_STATUS_GAP           8
#define CC_FOCUS_COUNT          7

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *top_cap;
    lv_obj_t *title;
    lv_obj_t *hint;
    lv_obj_t *handle;
    lv_obj_t *focus_capsule;
    lv_obj_t *controls[UI_CONTROL_CENTER_ACTION_COUNT];
    lv_obj_t *titles[UI_CONTROL_CENTER_ACTION_COUNT];
    lv_obj_t *state_labels[UI_CONTROL_CENTER_ACTION_COUNT];
    lv_obj_t *icon_backgrounds[UI_CONTROL_CENTER_ACTION_COUNT];
    lv_obj_t *sound_fill;
    lv_obj_t *sound_thumb;
    lv_obj_t *sound_value;
    lv_obj_t *brightness_fill;
    lv_obj_t *brightness_thumb;
    lv_obj_t *brightness_value;
    lv_obj_t *battery_card;
    lv_obj_t *battery_icon;
    lv_obj_t *battery_title;
    lv_obj_t *battery_state;
    ui_control_center_state_t state;
    ui_control_center_feedback_t feedback[UI_CONTROL_CENTER_ACTION_COUNT];
    ui_control_center_action_cb_t callback;
    void *user_data;
    uint8_t focus_index;
    ui_control_center_mode_t mode;
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

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
} control_rect_t;

static const control_rect_t s_compact_rects[4] = {
    {12, 48, 108, 88}, {128, 48, 108, 88},
    {244, 48, 108, 88}, {360, 48, 108, 88}
};

static const control_rect_t s_expanded_rects[4] = {
    {16, 62, 216, 78}, {248, 62, 216, 78},
    {16, 152, 448, 62}, {16, 224, 448, 62}
};

static int32_t lerp_value(int32_t from, int32_t to, int32_t progress)
{
    return from + (to - from) * progress / 1000;
}

static int32_t expansion_progress(int32_t height)
{
    if(height <= UI_CONTROL_CENTER_COMPACT_HEIGHT) return 0;
    if(height >= UI_CONTROL_CENTER_PANEL_HEIGHT) return 1000;
    return (height - UI_CONTROL_CENTER_COMPACT_HEIGHT) * 1000 /
           (UI_CONTROL_CENTER_PANEL_HEIGHT -
            UI_CONTROL_CENTER_COMPACT_HEIGHT);
}

static int32_t staged_progress(int32_t progress, int32_t start)
{
    if(progress <= start) return 0;
    return LV_CLAMP(0, (progress - start) * 1000 / (1000 - start), 1000);
}

static lv_opa_t morph_text_opacity(int32_t progress,
                                    int32_t reveal_start)
{
    if(progress < 200)
        return (lv_opa_t)(LV_OPA_COVER * (200 - progress) / 200);
    if(progress <= reveal_start) return LV_OPA_TRANSP;
    return (lv_opa_t)(LV_OPA_COVER *
                      (progress - reveal_start) / (1000 - reveal_start));
}

static void slider_width_animation(void *object, int32_t value)
{
    lv_obj_set_width((lv_obj_t *)object, value);
}

static void slider_x_animation(void *object, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)object, value);
}

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
    lv_obj_set_style_radius(object, UI_TOKEN_RADIUS_SMALL, 0);
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
                             int32_t size, const char *icon)
{
    lv_obj_t *circle = box(parent, x, y, size, size);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(circle, 0, 0);
    lv_obj_set_style_bg_color(circle, UI_TOKEN_COLOR_DECORATION, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *icon_label = label(circle, 0, 0, size, icon,
                                 &lv_font_montserrat_20,
                                 LV_TEXT_ALIGN_CENTER);
    lv_obj_center(icon_label);
    return circle;
}

static bool action_enabled(ui_control_center_action_t action)
{
    if(action == UI_CONTROL_CENTER_ACTION_WIFI)
        return s_center.state.wifi_enabled;
    if(action == UI_CONTROL_CENTER_ACTION_BLUETOOTH)
        return s_center.state.bluetooth_enabled;
    if(action == UI_CONTROL_CENTER_ACTION_GPS)
        return s_center.state.gps;
    if(action == UI_CONTROL_CENTER_ACTION_POWER_SAVE)
        return s_center.state.power_save;
    if(action == UI_CONTROL_CENTER_ACTION_TOUCH_LOCK)
        return s_center.state.touch_locked;
    return true;
}

static void update_visuals(bool animate_sliders);

static void set_object_visible(lv_obj_t *object, bool visible)
{
    if(object == NULL) return;
    if(visible) lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static void apply_control_geometry(uint8_t index, int32_t progress)
{
    ui_control_center_action_t action = s_focus_order[index];
    lv_obj_t *control = s_center.controls[action];
    const control_rect_t *compact = &s_compact_rects[index];
    const control_rect_t *expanded = &s_expanded_rects[index];
    int32_t layout_progress = progress;
    int32_t vertical_progress = progress;
    if(index < 2U) {
        /* Keep the connection cards compact until the sliders have moved below. */
        layout_progress = staged_progress(progress, 250);
    }
    else if(index == 2U) {
        layout_progress = staged_progress(progress, 250);
        vertical_progress = LV_CLAMP(0, progress * 4, 1000);
    }
    else {
        /* The brightness row waits until the sound row has cleared its path. */
        layout_progress = staged_progress(progress, 650);
        vertical_progress = LV_CLAMP(0, progress * 2, 1000);
    }
    int32_t width = lerp_value(compact->width, expanded->width, layout_progress);
    int32_t size_progress = layout_progress;
    if(index >= 2U) size_progress = vertical_progress;
    int32_t height = lerp_value(compact->height, expanded->height,
                                size_progress);
    int32_t y = lerp_value(compact->y, expanded->y, vertical_progress);
    if(index == 3U) {
        if(progress <= 500)
            y = lerp_value(compact->y, 238, vertical_progress);
        else
            y = lerp_value(238, expanded->y,
                           LV_CLAMP(0, (progress - 500) * 1000 / 300, 1000));
    }
    lv_obj_set_pos(control,
                   lerp_value(compact->x, expanded->x, layout_progress), y);
    lv_obj_set_size(control, width, height);
    lv_obj_set_style_radius(control, lerp_value(22, 8, progress), 0);
    lv_obj_set_style_bg_opa(control,
                            (lv_opa_t)(LV_OPA_COVER * progress / 1000), 0);

    lv_obj_t *icon = s_center.icon_backgrounds[action];
    int32_t icon_size = index < 2U ? lerp_value(40, 50, layout_progress) : 40;
    int32_t compact_icon_x = (compact->width - 40) / 2;
    int32_t expanded_icon_x = index < 2U ? 12 : 10;
    int32_t expanded_icon_y = index < 2U ? 14 : 11;
    lv_obj_set_pos(icon,
                   lerp_value(compact_icon_x, expanded_icon_x, layout_progress),
                   lerp_value(8, expanded_icon_y, layout_progress));
    lv_obj_set_size(icon, icon_size, icon_size);
    lv_obj_t *icon_label = lv_obj_get_child(icon, 0);
    lv_obj_set_width(icon_label, icon_size);
    lv_obj_center(icon_label);

    lv_obj_t *title = s_center.titles[action];
    if(index < 2U) {
        lv_obj_set_pos(title, lerp_value(0, 74, layout_progress),
                       lerp_value(50, 11, layout_progress));
        lv_obj_set_width(title, lerp_value(compact->width, 128,
                                           layout_progress));
        lv_obj_set_style_text_align(
            title, progress < 500 ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT,
            0);
        lv_obj_t *state = s_center.state_labels[action];
        lv_obj_set_pos(state, lerp_value(0, 74, layout_progress),
                       lerp_value(70, 40, layout_progress));
        lv_obj_set_width(state, lerp_value(compact->width, 128,
                                           layout_progress));
        lv_obj_set_style_text_align(
            state, progress < 500 ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT,
            0);
        lv_opa_t text_opa = morph_text_opacity(progress, 925);
        lv_obj_set_style_text_opa(title, text_opa, 0);
        lv_obj_set_style_text_opa(state, text_opa, 0);
    }
    else {
        bool compact_layout = progress < 200;
        bool slider_track_visible = progress == 0 ||
                                    (progress >= 300 &&
                                     (index == 2U || layout_progress >= 250));
        lv_obj_set_pos(title, compact_layout ? 0 : 60,
                       compact_layout ? 50 : 8);
        lv_obj_set_width(title, lerp_value(compact->width, 76,
                                           layout_progress));
        lv_obj_set_style_text_align(
            title, progress < 500 ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT,
            0);
        lv_obj_t *value = index == 2U ? s_center.sound_value :
                                        s_center.brightness_value;
        lv_obj_set_pos(value, compact_layout ? 0 :
                       lerp_value(0, 388, layout_progress),
                       compact_layout ? 70 : 8);
        lv_obj_set_width(value, lerp_value(compact->width, 48,
                                           layout_progress));
        lv_obj_set_style_text_align(
            value, progress < 500 ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_RIGHT,
            0);
        lv_opa_t text_opa = morph_text_opacity(
            progress, index == 2U ? 775 : 895);
        lv_obj_set_style_text_opa(title, text_opa, 0);
        lv_obj_set_style_text_opa(value, text_opa, 0);
        for(uint8_t child = 3U; child <= 5U; child++)
            set_object_visible(lv_obj_get_child(control, child),
                               slider_track_visible);
    }
}

static void apply_geometry(int32_t height)
{
    height = LV_CLAMP(UI_CONTROL_CENTER_COMPACT_HEIGHT, height,
                      UI_CONTROL_CENTER_PANEL_HEIGHT);
    int32_t progress = expansion_progress(height);
    lv_obj_set_height(s_center.panel, height);
    lv_obj_set_y(s_center.handle, height - 13);
    lv_label_set_text(s_center.title,
                      progress < 500 ? "快捷控制" : "系统控制");
    lv_label_set_text(s_center.hint,
                      progress < 500 ? "继续下拉展开" : "上滑关闭");

    for(uint8_t index = 0U; index < 4U; index++)
        apply_control_geometry(index, progress);

    bool details_visible = progress >= 850;
    for(uint8_t action = UI_CONTROL_CENTER_ACTION_GPS;
        action <= UI_CONTROL_CENTER_ACTION_TOUCH_LOCK; action++)
        set_object_visible(s_center.controls[action], details_visible);
    set_object_visible(s_center.battery_card, details_visible);

    set_object_visible(s_center.focus_capsule, progress < 500);
    if(progress < 500) {
        uint8_t compact_index = s_center.focus_index < 4U ?
                                s_center.focus_index : 0U;
        const control_rect_t *rect = &s_compact_rects[compact_index];
        lv_obj_set_pos(s_center.focus_capsule, rect->x, rect->y);
        lv_obj_set_size(s_center.focus_capsule, rect->width, rect->height);
    }
    update_visuals(false);
}

static bool pointer_on_slider_track(ui_control_center_action_t action)
{
    if(action != UI_CONTROL_CENTER_ACTION_SOUND &&
       action != UI_CONTROL_CENTER_ACTION_BRIGHTNESS)
        return false;
    lv_indev_t *indev = lv_indev_active();
    if(indev == NULL || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER)
        return false;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    ui_control_center_slider_t slider =
        ui_control_center_slider_at(point.x, point.y);
    return (action == UI_CONTROL_CENTER_ACTION_SOUND &&
            slider == UI_CONTROL_CENTER_SLIDER_SOUND) ||
           (action == UI_CONTROL_CENTER_ACTION_BRIGHTNESS &&
            slider == UI_CONTROL_CENTER_SLIDER_BRIGHTNESS);
}

static void action_event(lv_event_t *event)
{
    ui_control_center_action_t action =
        (ui_control_center_action_t)(uintptr_t)lv_event_get_user_data(event);
    if(action >= UI_CONTROL_CENTER_ACTION_COUNT) return;
    lv_event_code_t code = lv_event_get_code(event);
    if(code == LV_EVENT_PRESSED) {
        if(s_center.icon_backgrounds[action] != NULL)
            ui_motion_toggle_press(s_center.icon_backgrounds[action]);
        lv_obj_set_style_border_color(lv_event_get_target_obj(event),
                                      UI_TOKEN_COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(lv_event_get_target_obj(event),
                                      UI_TOKEN_BORDER_FOCUS, 0);
        return;
    }
    if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if(s_center.icon_backgrounds[action] != NULL)
            ui_motion_toggle_release(s_center.icon_backgrounds[action]);
        update_visuals(true);
        return;
    }
    if(code != LV_EVENT_CLICKED ||
       s_center.feedback[action] == UI_CONTROL_CENTER_FEEDBACK_PENDING)
        return;
    if(pointer_on_slider_track(action)) return;
    s_center.feedback[action] = UI_CONTROL_CENTER_FEEDBACK_IDLE;
    if(s_center.mode == UI_CONTROL_CENTER_MODE_EXPANDED &&
       (action == UI_CONTROL_CENTER_ACTION_SOUND ||
        action == UI_CONTROL_CENTER_ACTION_BRIGHTNESS)) {
        s_center.focus_index = action == UI_CONTROL_CENTER_ACTION_SOUND ?
                               2U : 3U;
        s_center.adjusting = true;
    }
    else if(s_center.callback != NULL)
        s_center.callback(action, -1, s_center.user_data);
    update_visuals(true);
}

static void connection_card(int32_t x, const char *icon,
                            const char *title,
                            ui_control_center_action_t action)
{
    lv_obj_t *card = box(s_center.panel, x, CC_CONNECTION_Y,
                         CC_CARD_W, CC_CARD_H);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, action_event, LV_EVENT_ALL,
                        (void *)(uintptr_t)action);
    s_center.controls[action] = card;
    s_center.icon_backgrounds[action] = icon_circle(card, 12, 14, 50, icon);
    s_center.titles[action] = label(
        card, 74, 11, 128, title, UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_LEFT);
    s_center.state_labels[action] = label(
        card, 74, 40, 128, "", UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(s_center.state_labels[action], LV_LABEL_LONG_DOT);
}

static void slider_row(int32_t y, const char *title, const char *icon,
                       ui_control_center_action_t action,
                       lv_obj_t **fill, lv_obj_t **thumb,
                       lv_obj_t **value_label)
{
    lv_obj_t *row = box(s_center.panel, CC_SLIDER_X, y,
                        CC_SLIDER_W, CC_SLIDER_H);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, action_event, LV_EVENT_ALL,
                        (void *)(uintptr_t)action);
    s_center.controls[action] = row;
    s_center.icon_backgrounds[action] = icon_circle(row, 10, 11, 40, icon);
    s_center.titles[action] = label(
        row, 60, 8, 76, title, UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_LEFT);
    *value_label = label(row, 388, 8, 48, "0%", UI_FONT_PAGE_TEXT,
                         LV_TEXT_ALIGN_RIGHT);

    lv_obj_t *track = box(row, CC_TRACK_X, CC_TRACK_Y,
                          CC_TRACK_W, CC_TRACK_H);
    lv_obj_set_style_radius(track, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_bg_color(track, UI_TOKEN_COLOR_SEGMENT_INACTIVE, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);
    *fill = box(row, CC_TRACK_X, CC_TRACK_Y, 0, CC_TRACK_H);
    lv_obj_set_style_radius(*fill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(*fill, 0, 0);
    lv_obj_set_style_bg_color(*fill, UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_clear_flag(*fill, LV_OBJ_FLAG_CLICKABLE);
    *thumb = box(row, CC_TRACK_X - CC_THUMB_SIZE / 2,
                 CC_TRACK_Y - (CC_THUMB_SIZE - CC_TRACK_H) / 2,
                 CC_THUMB_SIZE, CC_THUMB_SIZE);
    lv_obj_set_style_radius(*thumb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(*thumb, 0, 0);
    lv_obj_set_style_bg_color(*thumb, UI_TOKEN_COLOR_TEXT_PRIMARY, 0);
    lv_obj_clear_flag(*thumb, LV_OBJ_FLAG_CLICKABLE);
}

static void status_card(uint8_t column, const char *icon, const char *title,
                        ui_control_center_action_t action, bool clickable)
{
    int32_t x = CC_LEFT + column * (CC_STATUS_W + CC_STATUS_GAP);
    lv_obj_t *card = box(s_center.panel, x, CC_STATUS_Y,
                         CC_STATUS_W, CC_STATUS_H);
    lv_obj_t *icon_bg = icon_circle(card, 32, 8, 40, icon);
    lv_obj_t *title_label = label(
        card, 4, 51, 96, title, UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *state = label(card, 4, 73, 96, "", UI_FONT_PAGE_TEXT,
                            LV_TEXT_ALIGN_CENTER);
    if(clickable) {
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, action_event, LV_EVENT_ALL,
                            (void *)(uintptr_t)action);
        s_center.controls[action] = card;
        s_center.icon_backgrounds[action] = icon_bg;
        s_center.titles[action] = title_label;
        s_center.state_labels[action] = state;
    }
    else {
        s_center.battery_card = card;
        s_center.battery_icon = icon_bg;
        s_center.battery_title = title_label;
        s_center.battery_state = state;
    }
}

static void update_slider(lv_obj_t *fill, lv_obj_t *thumb,
                          lv_obj_t *value_label, uint8_t level,
                          bool animate)
{
    int32_t width = CC_TRACK_W * clamp_level(level) / 100;
    int32_t center = CC_TRACK_X + width;
    int32_t thumb_x = center - CC_THUMB_SIZE / 2;
    bool animated = animate && s_center.root != NULL &&
                    !lv_obj_has_flag(s_center.root, LV_OBJ_FLAG_HIDDEN);
    if(animated) {
        ui_motion_value(fill, slider_width_animation,
                        lv_obj_get_width(fill), width,
                        UI_TOKEN_MOTION_VALUE_STEP_MS,
                        lv_anim_path_ease_out, NULL, NULL);
        ui_motion_value(thumb, slider_x_animation,
                        lv_obj_get_x(thumb), thumb_x,
                        UI_TOKEN_MOTION_VALUE_STEP_MS,
                        lv_anim_path_ease_out, NULL, NULL);
    }
    else {
        ui_motion_cancel(fill, slider_width_animation);
        ui_motion_cancel(thumb, slider_x_animation);
        lv_obj_set_width(fill, width);
        lv_obj_set_x(thumb, thumb_x);
    }
    char value[8];
    snprintf(value, sizeof(value), "%u%%", (unsigned)level);
    lv_label_set_text(value_label, value);
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
    const char *signal = s_center.state.satellites >= 8U ? "良好" :
                         (s_center.state.satellites >= 4U ? "一般" : "无信号");
    lv_label_set_text(s_center.state_labels[UI_CONTROL_CENTER_ACTION_GPS],
                      override != NULL ? override :
                      (s_center.state.gps ? signal : "关闭"));

    override = feedback_text(UI_CONTROL_CENTER_ACTION_POWER_SAVE);
    lv_label_set_text(
        s_center.state_labels[UI_CONTROL_CENTER_ACTION_POWER_SAVE],
        override != NULL ? override :
        (s_center.state.power_save ? "开启" : "标准"));

    override = feedback_text(UI_CONTROL_CENTER_ACTION_TOUCH_LOCK);
    lv_label_set_text(
        s_center.state_labels[UI_CONTROL_CENTER_ACTION_TOUCH_LOCK],
        override != NULL ? override :
        (s_center.state.touch_locked ? "开启" : "关闭"));

    char battery[16];
    snprintf(battery, sizeof(battery), "%u%%%s",
             (unsigned)s_center.state.battery_percent,
             s_center.state.battery_charging ? "+" : "");
    lv_label_set_text(s_center.battery_state, battery);
}

static void update_visuals(bool animate_sliders)
{
    ui_control_center_action_t selected = s_focus_order[s_center.focus_index];
    bool compact = ui_control_center_panel_height() <
                   (UI_CONTROL_CENTER_COMPACT_HEIGHT +
                    UI_CONTROL_CENTER_PANEL_HEIGHT) / 2;
    for(uint8_t action = 0; action < UI_CONTROL_CENTER_ACTION_COUNT; action++) {
        lv_obj_t *control = s_center.controls[action];
        if(control == NULL) continue;
        bool focused = selected == (ui_control_center_action_t)action;
        bool failed = s_center.feedback[action] == UI_CONTROL_CENTER_FEEDBACK_FAILED;
        bool pending = s_center.feedback[action] == UI_CONTROL_CENTER_FEEDBACK_PENDING;
        lv_color_t color = failed ? UI_TOKEN_COLOR_CRITICAL :
            ((focused || pending) ? UI_TOKEN_COLOR_ACCENT :
                                    UI_TOKEN_COLOR_SURFACE_BORDER);
        lv_obj_set_style_border_color(control, color, 0);
        lv_obj_set_style_border_width(
            control, compact && action < UI_CONTROL_CENTER_ACTION_GPS ? 0 :
            (focused || pending || failed ? UI_TOKEN_BORDER_FOCUS :
                                            UI_TOKEN_BORDER_THIN), 0);
        if(s_center.icon_backgrounds[action] != NULL) {
            lv_obj_set_style_bg_color(s_center.icon_backgrounds[action],
                action_enabled((ui_control_center_action_t)action) ?
                UI_TOKEN_COLOR_ACCENT : lv_color_hex(0x253033), 0);
        }
    }
    update_state_text();
    update_slider(s_center.sound_fill, s_center.sound_thumb,
                  s_center.sound_value, s_center.state.sound_level,
                  animate_sliders);
    update_slider(s_center.brightness_fill, s_center.brightness_thumb,
                  s_center.brightness_value, s_center.state.brightness_level,
                  animate_sliders);
    if(compact) {
        lv_color_t capsule_color =
            s_center.feedback[selected] == UI_CONTROL_CENTER_FEEDBACK_FAILED ?
            UI_TOKEN_COLOR_CRITICAL : UI_TOKEN_COLOR_ACCENT;
        lv_obj_set_style_border_color(s_center.focus_capsule,
                                      capsule_color, 0);
    }
}

void ui_control_center_create(ui_control_center_action_cb_t callback,
                              void *user_data)
{
    memset(&s_center, 0, sizeof(s_center));
    s_center.callback = callback;
    s_center.user_data = user_data;
    s_center.root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_center.root);
    lv_obj_set_pos(s_center.root, 0, -UI_TOKEN_SCREEN_HEIGHT);
    lv_obj_set_size(s_center.root, UI_TOKEN_SCREEN_WIDTH,
                    UI_TOKEN_SCREEN_HEIGHT);
    lv_obj_set_style_bg_opa(s_center.root, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_center.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_center.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_center.root, LV_OBJ_FLAG_HIDDEN);

    s_center.panel = box(s_center.root, 0, 0, UI_TOKEN_SCREEN_WIDTH,
                         UI_CONTROL_CENTER_COMPACT_HEIGHT);
    lv_obj_set_style_bg_color(s_center.panel, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_radius(s_center.panel, 24, 0);
    lv_obj_set_style_clip_corner(s_center.panel, true, 0);
    lv_obj_set_style_border_side(s_center.panel, LV_BORDER_SIDE_BOTTOM, 0);
    s_center.top_cap = box(s_center.panel, 0, 0, UI_TOKEN_SCREEN_WIDTH, 28);
    lv_obj_set_style_radius(s_center.top_cap, 0, 0);
    lv_obj_set_style_border_width(s_center.top_cap, 0, 0);
    lv_obj_clear_flag(s_center.top_cap, LV_OBJ_FLAG_CLICKABLE);
    s_center.title = label(s_center.panel, 16, 12, 250, "快捷控制",
                           UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_LEFT);
    s_center.hint = label(s_center.panel, 270, 15, 194,
                          "继续下拉展开", UI_FONT_PAGE_TEXT,
                          LV_TEXT_ALIGN_RIGHT);
    s_center.handle = box(s_center.panel, 204,
                          UI_CONTROL_CENTER_COMPACT_HEIGHT - 13, 72, 5);
    lv_obj_set_style_radius(s_center.handle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_center.handle, 0, 0);
    lv_obj_set_style_bg_color(s_center.handle,
                              UI_TOKEN_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_bg_opa(s_center.handle, LV_OPA_60, 0);
    lv_obj_clear_flag(s_center.handle, LV_OBJ_FLAG_CLICKABLE);

    s_center.focus_capsule = box(s_center.panel, 12, 48, 108, 88);
    lv_obj_set_style_radius(s_center.focus_capsule, 22, 0);
    lv_obj_set_style_bg_opa(s_center.focus_capsule, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_center.focus_capsule,
                                  UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_center.focus_capsule,
                                  UI_TOKEN_BORDER_FOCUS, 0);
    lv_obj_clear_flag(s_center.focus_capsule,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    connection_card(CC_LEFT, LV_SYMBOL_WIFI, "WiFi",
                    UI_CONTROL_CENTER_ACTION_WIFI);
    connection_card(CC_RIGHT, LV_SYMBOL_BLUETOOTH, "蓝牙",
                    UI_CONTROL_CENTER_ACTION_BLUETOOTH);
    slider_row(CC_SOUND_Y, "声音", LV_SYMBOL_VOLUME_MAX,
               UI_CONTROL_CENTER_ACTION_SOUND, &s_center.sound_fill,
               &s_center.sound_thumb, &s_center.sound_value);
    slider_row(CC_BRIGHTNESS_Y, "亮度", LV_SYMBOL_EYE_OPEN,
               UI_CONTROL_CENTER_ACTION_BRIGHTNESS,
               &s_center.brightness_fill, &s_center.brightness_thumb,
               &s_center.brightness_value);
    status_card(0, LV_SYMBOL_GPS, "GPS", UI_CONTROL_CENTER_ACTION_GPS, true);
    status_card(1, LV_SYMBOL_BATTERY_3, "省电",
                UI_CONTROL_CENTER_ACTION_POWER_SAVE, true);
    status_card(2, LV_SYMBOL_KEYBOARD, "触摸锁",
                UI_CONTROL_CENTER_ACTION_TOUCH_LOCK, true);
    status_card(3, LV_SYMBOL_BATTERY_3, "电量",
                UI_CONTROL_CENTER_ACTION_COUNT, false);
    s_center.mode = UI_CONTROL_CENTER_MODE_COMPACT;
    s_center.adjusting = false;
    apply_geometry(UI_CONTROL_CENTER_COMPACT_HEIGHT);
}

lv_obj_t *ui_control_center_root(void) { return s_center.root; }
lv_obj_t *ui_control_center_panel(void) { return s_center.panel; }
lv_obj_t *ui_control_center_focus_capsule(void)
{
    return s_center.focus_capsule;
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
    s_center.state.bluetooth_name[sizeof(s_center.state.bluetooth_name) - 1U] = '\0';
    s_center.state.sound_level = clamp_level(s_center.state.sound_level);
    s_center.state.brightness_level = clamp_level(s_center.state.brightness_level);
    s_center.state.battery_percent = clamp_level(s_center.state.battery_percent);
    update_visuals(true);
}

void ui_control_center_set_feedback(ui_control_center_action_t action,
                                    ui_control_center_feedback_t feedback)
{
    if(action >= UI_CONTROL_CENTER_ACTION_COUNT ||
       feedback > UI_CONTROL_CENTER_FEEDBACK_FAILED) return;
    s_center.feedback[action] = feedback;
    if(feedback == UI_CONTROL_CENTER_FEEDBACK_PENDING &&
       s_center.icon_backgrounds[action] != NULL)
        ui_motion_pulse(s_center.icon_backgrounds[action], 264,
                        UI_TOKEN_MOTION_TOGGLE_MS);
    update_visuals(true);
}

void ui_control_center_prepare_show(void)
{
    s_center.focus_index = 0U;
    s_center.adjusting = false;
    update_visuals(true);
}

void ui_control_center_preview_level(ui_control_center_slider_t slider,
                                     uint8_t level)
{
    level = clamp_level(level);
    if(slider == UI_CONTROL_CENTER_SLIDER_SOUND)
        s_center.state.sound_level = level;
    else if(slider == UI_CONTROL_CENTER_SLIDER_BRIGHTNESS)
        s_center.state.brightness_level = level;
    lv_obj_t *thumb = slider == UI_CONTROL_CENTER_SLIDER_SOUND ?
                      s_center.sound_thumb : s_center.brightness_thumb;
    ui_motion_pulse(thumb, 272, UI_TOKEN_MOTION_VALUE_STEP_MS * 2U);
    update_visuals(true);
}

bool ui_control_center_handle_input(ui_input_event_t input)
{
    if(input != UI_INPUT_ENCODER_CW && input != UI_INPUT_ENCODER_CCW &&
       input != UI_INPUT_ENCODER_PRESS) return false;
    ui_control_center_action_t action = s_focus_order[s_center.focus_index];
    if(input == UI_INPUT_ENCODER_PRESS) {
        if(s_center.adjusting) {
            (void)ui_control_center_exit_adjustment(true);
            return true;
        }
        if(s_center.mode == UI_CONTROL_CENTER_MODE_EXPANDED &&
           (action == UI_CONTROL_CENTER_ACTION_SOUND ||
            action == UI_CONTROL_CENTER_ACTION_BRIGHTNESS)) {
            s_center.adjusting = true;
            lv_obj_t *thumb = action == UI_CONTROL_CENTER_ACTION_SOUND ?
                              s_center.sound_thumb : s_center.brightness_thumb;
            ui_motion_pulse(thumb, 272,
                            UI_TOKEN_MOTION_CAPSULE_PRESS_MS);
            return true;
        }
        if(s_center.feedback[action] != UI_CONTROL_CENTER_FEEDBACK_PENDING &&
           s_center.callback != NULL) {
            s_center.feedback[action] = UI_CONTROL_CENTER_FEEDBACK_IDLE;
            s_center.callback(action, -1, s_center.user_data);
            update_visuals(true);
        }
        return true;
    }
    if(s_center.adjusting) {
        int32_t delta = input == UI_INPUT_ENCODER_CW ? 5 : -5;
        uint8_t current = action == UI_CONTROL_CENTER_ACTION_SOUND ?
                          s_center.state.sound_level :
                          s_center.state.brightness_level;
        ui_control_center_preview_level(
            action == UI_CONTROL_CENTER_ACTION_SOUND ?
            UI_CONTROL_CENTER_SLIDER_SOUND :
            UI_CONTROL_CENTER_SLIDER_BRIGHTNESS,
            clamp_level((int32_t)current + delta));
        return true;
    }
    uint8_t focus_count = s_center.mode == UI_CONTROL_CENTER_MODE_COMPACT ?
                          4U : CC_FOCUS_COUNT;
    int32_t next = (int32_t)s_center.focus_index +
                   (input == UI_INPUT_ENCODER_CW ? 1 : -1);
    if(next < 0) next = focus_count - 1;
    if(next >= focus_count) next = 0;
    s_center.focus_index = (uint8_t)next;
    if(s_center.mode == UI_CONTROL_CENTER_MODE_COMPACT) {
        const control_rect_t *rect = &s_compact_rects[s_center.focus_index];
        ui_motion_focus_capsule(s_center.focus_capsule, rect->x, rect->width);
    }
    update_visuals(true);
    return true;
}

ui_control_center_action_t ui_control_center_selected_action(void)
{
    return s_focus_order[s_center.focus_index];
}

bool ui_control_center_is_adjusting(void) { return s_center.adjusting; }

void ui_control_center_set_mode(ui_control_center_mode_t mode)
{
    if(mode != UI_CONTROL_CENTER_MODE_COMPACT &&
       mode != UI_CONTROL_CENTER_MODE_EXPANDED) return;
    s_center.mode = mode;
    s_center.adjusting = false;
    if(mode == UI_CONTROL_CENTER_MODE_COMPACT && s_center.focus_index >= 4U)
        s_center.focus_index = 0U;
    apply_geometry(mode == UI_CONTROL_CENTER_MODE_COMPACT ?
                   UI_CONTROL_CENTER_COMPACT_HEIGHT :
                   UI_CONTROL_CENTER_PANEL_HEIGHT);
}

ui_control_center_mode_t ui_control_center_mode(void)
{
    return s_center.mode;
}

void ui_control_center_set_panel_height(int32_t height)
{
    apply_geometry(height);
}

int32_t ui_control_center_panel_height(void)
{
    return s_center.panel != NULL ? lv_obj_get_height(s_center.panel) : 0;
}

void ui_control_center_focus_action(ui_control_center_action_t action)
{
    for(uint8_t index = 0U; index < CC_FOCUS_COUNT; index++) {
        if(s_focus_order[index] != action) continue;
        s_center.focus_index = index;
        update_visuals(true);
        return;
    }
}

bool ui_control_center_exit_adjustment(bool commit)
{
    if(!s_center.adjusting) return false;
    ui_control_center_action_t action = s_focus_order[s_center.focus_index];
    s_center.adjusting = false;
    if(commit && s_center.callback != NULL) {
        int32_t value = action == UI_CONTROL_CENTER_ACTION_SOUND ?
                        s_center.state.sound_level :
                        s_center.state.brightness_level;
        s_center.callback(action, value, s_center.user_data);
    }
    update_visuals(true);
    return true;
}

ui_control_center_slider_t ui_control_center_slider_at(int32_t screen_x,
                                                       int32_t screen_y)
{
    if(s_center.root == NULL) return UI_CONTROL_CENTER_SLIDER_NONE;
    int32_t local_x = screen_x - lv_obj_get_x(s_center.root);
    int32_t local_y = screen_y - lv_obj_get_y(s_center.root);
    int32_t track_start = CC_SLIDER_X + CC_TRACK_X;
    int32_t track_end = track_start + CC_TRACK_W;
    if(local_x < track_start || local_x > track_end)
        return UI_CONTROL_CENTER_SLIDER_NONE;
    if(local_y >= CC_SOUND_Y && local_y <= CC_SOUND_Y + CC_SLIDER_H)
        return UI_CONTROL_CENTER_SLIDER_SOUND;
    if(local_y >= CC_BRIGHTNESS_Y && local_y <= CC_BRIGHTNESS_Y + CC_SLIDER_H)
        return UI_CONTROL_CENTER_SLIDER_BRIGHTNESS;
    return UI_CONTROL_CENTER_SLIDER_NONE;
}

uint8_t ui_control_center_level_from_x(int32_t screen_x)
{
    int32_t root_x = s_center.root != NULL ? lv_obj_get_x(s_center.root) : 0;
    int32_t local_x = screen_x - root_x;
    int32_t track_start = CC_SLIDER_X + CC_TRACK_X;
    return clamp_level((local_x - track_start) * 100 / CC_TRACK_W);
}
