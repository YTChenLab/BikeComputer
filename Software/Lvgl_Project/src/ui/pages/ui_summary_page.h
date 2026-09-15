#ifndef UI_SUMMARY_PAGE_H
#define UI_SUMMARY_PAGE_H

#include "ui_page_action.h"
#include "lvgl.h"

#include <stdbool.h>

lv_obj_t *ui_summary_page_create(ui_page_action_cb_t callback,
                                 void *user_data, bool wifi, bool bluetooth);
void ui_summary_page_set_connectivity(bool wifi, bool bluetooth);

#endif /* UI_SUMMARY_PAGE_H */
