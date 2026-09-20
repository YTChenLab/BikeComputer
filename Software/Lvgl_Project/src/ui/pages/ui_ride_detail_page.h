#ifndef UI_RIDE_DETAIL_PAGE_H
#define UI_RIDE_DETAIL_PAGE_H

#include "ui_page_action.h"
#include "../core/ui_input.h"
#include "../models/ui_ride_history_model.h"
#include "lvgl.h"

#include <stdbool.h>

lv_obj_t *ui_ride_detail_page_create(ui_page_action_cb_t callback,
                                     void *user_data);
void ui_ride_detail_page_set_record(const ui_ride_record_t *record);
void ui_ride_detail_page_set_active(bool active);
bool ui_ride_detail_page_handle_input(ui_input_event_t input);
bool ui_ride_detail_page_contains_scroll_point(int32_t x, int32_t y);

#endif /* UI_RIDE_DETAIL_PAGE_H */
