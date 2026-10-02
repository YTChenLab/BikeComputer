#ifndef UI_PAGE_COMMON_H
#define UI_PAGE_COMMON_H

#include "../components/ui_status_bar.h"
#include "ui_page_action.h"
#include "../design/ui_design_tokens.h"
#include "../design/ui_alignment.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

#define UI_PAGE_STATUS_CAPACITY 16U

lv_obj_t *ui_page_screen_create(void);
lv_obj_t *ui_page_bottom_surface_slot_create(lv_obj_t *screen);
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
/* Shared scroll indicator; keep the thumb visible whenever content overflows. */
static inline void ui_page_scrollbar_style(lv_obj_t *viewport)
{
    if(viewport == NULL) return;
    lv_obj_set_scrollbar_mode(viewport, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(viewport, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(viewport, UI_TOKEN_COLOR_ACCENT, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(viewport, LV_OPA_70, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(viewport, LV_RADIUS_CIRCLE, LV_PART_SCROLLBAR);
    lv_obj_set_style_anim_duration(viewport, 180, 0);
}

#endif /* UI_PAGE_COMMON_H */
