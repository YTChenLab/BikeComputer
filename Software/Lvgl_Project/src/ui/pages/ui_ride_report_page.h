#ifndef UI_RIDE_REPORT_PAGE_H
#define UI_RIDE_REPORT_PAGE_H

#include "ui_page_action.h"
#include "../core/ui_input.h"
#include "../models/ui_ride_report_model.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

lv_obj_t *ui_ride_report_page_create(ui_ride_report_mode_t mode,
                                     ui_page_action_cb_t callback,
                                     void *user_data);
bool ui_ride_report_page_set_model(ui_ride_report_mode_t mode,
                                   const ui_ride_report_model_t *model);
/* UI-thread update after storage confirmation; preserves scroll and content. */
void ui_ride_report_page_set_saved(ui_ride_report_mode_t mode, bool saved);
void ui_ride_report_page_set_active(ui_ride_report_mode_t mode, bool active);
bool ui_ride_report_page_handle_input(ui_ride_report_mode_t mode,
                                      ui_input_event_t input);
bool ui_ride_report_page_contains_scroll_point(ui_ride_report_mode_t mode,
                                               int32_t x, int32_t y);
int32_t ui_ride_report_page_scroll_y(ui_ride_report_mode_t mode);
uint32_t ui_ride_report_page_content_height(ui_ride_report_mode_t mode);

#endif /* UI_RIDE_REPORT_PAGE_H */
