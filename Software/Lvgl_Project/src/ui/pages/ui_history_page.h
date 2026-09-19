#ifndef UI_HISTORY_PAGE_H
#define UI_HISTORY_PAGE_H

#include "../core/ui_input.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stddef.h>

lv_obj_t *ui_history_page_create(void);
void ui_history_page_set_active(bool active);
void ui_history_page_refresh(void);
bool ui_history_page_handle_input(ui_input_event_t input);
size_t ui_history_page_visible_count(void);
int32_t ui_history_page_selected_index(void);

#endif /* UI_HISTORY_PAGE_H */
