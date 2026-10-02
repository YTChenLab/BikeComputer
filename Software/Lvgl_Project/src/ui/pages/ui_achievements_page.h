#ifndef UI_ACHIEVEMENTS_PAGE_H
#define UI_ACHIEVEMENTS_PAGE_H

#include "../core/ui_input.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

lv_obj_t *ui_achievements_page_create(void);
void ui_achievements_page_set_active(bool active);
void ui_achievements_page_refresh(void);
bool ui_achievements_page_handle_input(ui_input_event_t input);
bool ui_achievements_page_detail_visible(void);
void ui_achievements_page_close_detail(void);
size_t ui_achievements_page_count(void);
int32_t ui_achievements_page_selected_index(void);
bool ui_achievements_page_contains_scroll_point(int32_t x, int32_t y);

#endif /* UI_ACHIEVEMENTS_PAGE_H */
