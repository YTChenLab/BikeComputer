#ifndef UI_HISTORY_PAGE_H
#define UI_HISTORY_PAGE_H

#include "../core/ui_input.h"
#include "ui_page_action.h"
#include "../models/ui_ride_history_model.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

lv_obj_t *ui_history_page_create(ui_page_action_cb_t callback,
                                 void *user_data);
void ui_history_page_set_active(bool active);
void ui_history_page_refresh(void);
void ui_history_page_tick(void);
bool ui_history_page_handle_input(ui_input_event_t input);
size_t ui_history_page_visible_count(void);
int32_t ui_history_page_selected_index(void);
bool ui_history_page_selected_record(ui_ride_record_t *record);
bool ui_history_page_contains_scroll_point(int32_t x, int32_t y);

#endif /* UI_HISTORY_PAGE_H */
