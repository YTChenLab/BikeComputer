#include "ui/core/ui_memory.h"
#include "ui_ride_detail_page.h"

#include "ui_ride_report_page.h"
#include "../models/ui_ride_report_model.h"

lv_obj_t *ui_ride_detail_page_create(ui_page_action_cb_t callback,
                                     void *user_data)
{
    return ui_ride_report_page_create(UI_RIDE_REPORT_DETAIL_MODE,
                                      callback, user_data);
}

void ui_ride_detail_page_set_record(const ui_ride_record_t *record)
{
    static UI_LARGE_RAM ui_ride_report_model_t report;
    if(ui_ride_report_model_from_record(&report, record))
        (void)ui_ride_report_page_set_model(UI_RIDE_REPORT_DETAIL_MODE,
                                            &report);
}

void ui_ride_detail_page_set_active(bool active)
{
    ui_ride_report_page_set_active(UI_RIDE_REPORT_DETAIL_MODE, active);
}

bool ui_ride_detail_page_handle_input(ui_input_event_t input)
{
    return ui_ride_report_page_handle_input(UI_RIDE_REPORT_DETAIL_MODE,
                                            input);
}

bool ui_ride_detail_page_contains_scroll_point(int32_t x, int32_t y)
{
    return ui_ride_report_page_contains_scroll_point(
        UI_RIDE_REPORT_DETAIL_MODE, x, y);
}
