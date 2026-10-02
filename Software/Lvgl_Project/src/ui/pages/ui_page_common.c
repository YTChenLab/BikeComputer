#include "ui_page_common.h"

#include "../core/ui_background.h"
#include "../core/ui_app_state.h"
#include "../core/ui_colors.h"
#include "../core/ui_sizes.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"

#include <stddef.h>
#include <string.h>

static ui_status_bar_t *s_status_bars[UI_PAGE_STATUS_CAPACITY];
static uint8_t s_status_bar_count;
static int32_t s_status_subscription = -1;

static void apply_status(ui_status_bar_t *status,
                         const ui_app_state_t *state)
{
    if(status == NULL || status->root == NULL || state == NULL) return;
    ui_status_bar_set_satellites(status,
                                 state->gps_enabled ? state->satellites : 0);
    ui_status_bar_set_time(status, state->clock_hour, state->clock_minute);
    ui_status_bar_set_battery(status, state->battery_percent);
    lv_obj_set_style_text_font(status->time_label, state->clock_valid ? UI_FONT_STATUS_EMPHASIS : &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(status->battery_label, state->battery_valid ? UI_FONT_STATUS_EMPHASIS : &lv_font_montserrat_20, 0);
    if(!state->clock_valid) lv_label_set_text(status->time_label, "--:--");
    if(!state->battery_valid) { lv_label_set_text(status->battery_label, "--%"); lv_obj_set_width(status->battery_fill, 0); }
    ui_status_bar_set_wifi(status,
                           state->wifi_enabled && state->wifi_connected);
    ui_status_bar_set_bluetooth(
        status, state->bluetooth_enabled && state->bluetooth_connected);
}

static void status_state_changed(const ui_app_state_t *state,
                                 ui_app_state_change_mask_t changed,
                                 void *user_data)
{
    (void)changed;
    (void)user_data;
    for(uint8_t index = 0; index < s_status_bar_count; index++)
        apply_status(s_status_bars[index], state);
}

void ui_page_status_registry_init(void)
{
    if(s_status_subscription > 0)
        (void)ui_app_state_unsubscribe(s_status_subscription);
    memset(s_status_bars, 0, sizeof(s_status_bars));
    s_status_bar_count = 0;
    s_status_subscription = ui_app_state_subscribe(
        UI_APP_STATE_CHANGE_STATUS | UI_APP_STATE_CHANGE_CONNECTIVITY,
        status_state_changed, NULL);
}

void ui_page_status_registry_deinit(void)
{
    if(s_status_subscription > 0)
        (void)ui_app_state_unsubscribe(s_status_subscription);
    s_status_subscription = -1;
    memset(s_status_bars, 0, sizeof(s_status_bars));
    s_status_bar_count = 0;
}

bool ui_page_status_register(ui_status_bar_t *status)
{
    if(status == NULL || status->root == NULL) return false;
    for(uint8_t index = 0; index < s_status_bar_count; index++) {
        if(s_status_bars[index] == status) return true;
    }
    if(s_status_bar_count >= UI_PAGE_STATUS_CAPACITY) return false;
    s_status_bars[s_status_bar_count++] = status;
    ui_app_state_t state;
    if(ui_app_state_get(&state)) apply_status(status, &state);
    return true;
}

bool ui_page_status_unregister(ui_status_bar_t *status)
{
    for(uint8_t index = 0; index < s_status_bar_count; index++) {
        if(s_status_bars[index] != status) continue;
        for(uint8_t next = index + 1U; next < s_status_bar_count; next++)
            s_status_bars[next - 1U] = s_status_bars[next];
        s_status_bar_count--;
        return true;
    }
    return false;
}

lv_obj_t *ui_page_screen_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, UI_SCREEN_W, UI_SCREEN_H);
    ui_background_apply(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

lv_obj_t *ui_page_bottom_surface_slot_create(lv_obj_t *screen)
{
    if(screen == NULL) return NULL;
    lv_obj_t *slot = lv_obj_create(screen);
    lv_obj_remove_style_all(slot);
    lv_obj_set_pos(slot, UI_BOTTOM_SURFACE_X,
                   UI_BOTTOM_SURFACE_NORMAL_Y);
    lv_obj_set_size(slot, UI_BOTTOM_SURFACE_WIDTH,
                    UI_BOTTOM_SURFACE_NORMAL_HEIGHT);
    lv_obj_clear_flag(slot,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(slot, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    return slot;
}

lv_obj_t *ui_page_box(lv_obj_t *parent, int32_t x, int32_t y,
                      int32_t width, int32_t height,
                      lv_color_t border, uint8_t border_width)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_70, 0);
    lv_obj_set_style_border_color(obj, border, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, border_width, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

lv_obj_t *ui_page_card(lv_obj_t *parent, int32_t x, int32_t y,
                       int32_t width, int32_t height)
{
    lv_obj_t *card = ui_page_box(parent, x, y, width, height,
                                 UI_COLOR_WHITE, 1);
    lv_obj_set_style_bg_color(card, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
    lv_obj_set_style_radius(card, 14, 0);
    return card;
}

lv_obj_t *ui_page_label(lv_obj_t *parent, int32_t x, int32_t y,
                        int32_t width, const char *text,
                        const lv_font_t *font, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_color(label, UI_COLOR_WHITE, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

void ui_page_line(lv_obj_t *parent, int32_t x, int32_t y,
                  int32_t width, int32_t height, lv_color_t color)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, width, height);
    lv_obj_set_style_bg_color(line, color, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_page_title(lv_obj_t *screen, const char *title, const char *subtitle)
{
    ui_page_label(screen, UI_LEFT + 18, 85, UI_CONTENT_W - 36,
                  title, &ui_font_misans_ui_24, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *secondary = ui_page_label(screen, UI_LEFT + 18, 119, UI_CONTENT_W - 36,
                  subtitle, &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_color(secondary, UI_TOKEN_COLOR_TEXT_SECONDARY, 0);
    ui_page_line(screen, UI_LEFT, 143, UI_CONTENT_W, 1, UI_COLOR_GRID_LINE);
}

lv_obj_t *ui_page_button(lv_obj_t *parent, int32_t x, int32_t y,
                         int32_t width, int32_t height, const char *text,
                         lv_event_cb_t event_cb, void *user_data)
{
    lv_obj_t *button = ui_page_box(parent, x, y, width, height,
                                   UI_COLOR_WHITE, 1);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *label = ui_page_label(button, 0, 0, width, text,
                                    UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_CENTER);
    ui_align_label_center(label);
    return button;
}

void ui_page_status_create(ui_status_bar_t *status, lv_obj_t *screen)
{
    ui_status_bar_create(status, screen);
    (void)ui_page_status_register(status);
}
