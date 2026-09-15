#include "ui_page_common.h"

#include "../core/ui_background.h"
#include "../core/ui_colors.h"
#include "../core/ui_sizes.h"
#include "../core/ui_theme.h"

lv_obj_t *ui_page_screen_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, UI_SCREEN_W, UI_SCREEN_H);
    ui_background_apply(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
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
    lv_obj_set_style_bg_opa(card, LV_OPA_90, 0);
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
                  title, UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_LEFT);
    ui_page_label(screen, UI_LEFT + 18, 112, UI_CONTENT_W - 36,
                  subtitle, UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_LEFT);
    ui_page_line(screen, UI_LEFT, 143, UI_CONTENT_W, 1, UI_COLOR_WHITE);
}

lv_obj_t *ui_page_button(lv_obj_t *parent, int32_t x, int32_t y,
                         int32_t width, int32_t height, const char *text,
                         lv_event_cb_t event_cb, void *user_data)
{
    lv_obj_t *button = ui_page_box(parent, x, y, width, height,
                                   UI_COLOR_WHITE, 1);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, user_data);
    ui_page_label(button, 6, (height - 20) / 2, width - 12, text,
                  UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_CENTER);
    return button;
}

void ui_page_status_create(ui_status_bar_t *status, lv_obj_t *screen,
                           bool wifi, bool bluetooth)
{
    ui_status_bar_create(status, screen);
    ui_status_bar_set_satellites(status, 12);
    ui_status_bar_set_time(status, 10, 24);
    ui_status_bar_set_battery(status, 82);
    ui_page_status_set_connectivity(status, wifi, bluetooth);
}

void ui_page_status_set_connectivity(ui_status_bar_t *status,
                                     bool wifi, bool bluetooth)
{
    if(status == NULL || status->root == NULL) return;
    ui_status_bar_set_wifi(status, wifi);
    ui_status_bar_set_bluetooth(status, bluetooth);
}
