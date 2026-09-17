#ifndef UI_SUMMARY_PAGE_H
#define UI_SUMMARY_PAGE_H

#include "ui_page_action.h"
#include "../core/ui_input.h"
#include "../models/ui_ride_summary_model.h"
#include "lvgl.h"

#include <stdbool.h>

lv_obj_t *ui_summary_page_create(ui_page_action_cb_t callback,
                                 void *user_data);
void ui_summary_page_set_data(const ui_ride_summary_t *summary);
void ui_summary_page_set_active(bool active);
bool ui_summary_page_handle_input(ui_input_event_t input);

#endif /* UI_SUMMARY_PAGE_H */
