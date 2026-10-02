#include "ui_summary_page.h"

#include "ui_ride_report_page.h"
#include "../models/ui_ride_report_model.h"

lv_obj_t *ui_summary_page_create(ui_page_action_cb_t callback,
                                 void *user_data)
{
    return ui_ride_report_page_create(UI_RIDE_REPORT_SUMMARY_MODE,
                                      callback, user_data);
}

void ui_summary_page_set_data(const ui_ride_summary_t *summary)
{
    ui_ride_report_model_t report;
    if(ui_ride_report_model_from_summary(&report, summary))
        (void)ui_ride_report_page_set_model(UI_RIDE_REPORT_SUMMARY_MODE,
                                            &report);
}

void ui_summary_page_set_active(bool active)
{
    ui_ride_report_page_set_active(UI_RIDE_REPORT_SUMMARY_MODE, active);
}

void ui_summary_page_set_saved(bool saved)
{
    ui_ride_report_page_set_saved(UI_RIDE_REPORT_SUMMARY_MODE,saved);
}

bool ui_summary_page_handle_input(ui_input_event_t input)
{
    return ui_ride_report_page_handle_input(UI_RIDE_REPORT_SUMMARY_MODE,
                                            input);
}

bool ui_summary_page_contains_scroll_point(int32_t x, int32_t y)
{
    return ui_ride_report_page_contains_scroll_point(
        UI_RIDE_REPORT_SUMMARY_MODE, x, y);
}
