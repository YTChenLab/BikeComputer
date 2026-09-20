#ifndef UI_RIDE_DETAIL_PAGE_H
#define UI_RIDE_DETAIL_PAGE_H

#include "../models/ui_ride_history_model.h"
#include "lvgl.h"

lv_obj_t *ui_ride_detail_page_create(void);
void ui_ride_detail_page_set_record(const ui_ride_record_t *record);

#endif /* UI_RIDE_DETAIL_PAGE_H */
