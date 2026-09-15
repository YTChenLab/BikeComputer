#ifndef UI_PAGE_COMMON_H
#define UI_PAGE_COMMON_H

#include "../components/ui_status_bar.h"
#include "ui_page_action.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

lv_obj_t *ui_page_screen_create(void);
lv_obj_t *ui_page_box(lv_obj_t *parent, int32_t x, int32_t y,
                      int32_t width, int32_t height,
                      lv_color_t border, uint8_t border_width);
lv_obj_t *ui_page_card(lv_obj_t *parent, int32_t x, int32_t y,
                       int32_t width, int32_t height);
lv_obj_t *ui_page_label(lv_obj_t *parent, int32_t x, int32_t y,
                        int32_t width, const char *text,
                        const lv_font_t *font, lv_text_align_t align);
void ui_page_line(lv_obj_t *parent, int32_t x, int32_t y,
                  int32_t width, int32_t height, lv_color_t color);
void ui_page_title(lv_obj_t *screen, const char *title, const char *subtitle);
lv_obj_t *ui_page_button(lv_obj_t *parent, int32_t x, int32_t y,
                         int32_t width, int32_t height, const char *text,
                         lv_event_cb_t event_cb, void *user_data);
void ui_page_status_registry_init(void);
void ui_page_status_registry_deinit(void);
bool ui_page_status_register(ui_status_bar_t *status);
bool ui_page_status_unregister(ui_status_bar_t *status);
void ui_page_status_create(ui_status_bar_t *status, lv_obj_t *screen);

#endif /* UI_PAGE_COMMON_H */
