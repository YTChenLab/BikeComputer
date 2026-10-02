#ifndef UI_SAVED_ROUTES_PAGE_H
#define UI_SAVED_ROUTES_PAGE_H

#include "lvgl.h"
#include "ui_page_action.h"

lv_obj_t *ui_saved_routes_page_create(ui_page_action_cb_t callback,
                                      void *user_data);

#endif /* UI_SAVED_ROUTES_PAGE_H */
