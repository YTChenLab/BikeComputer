#ifndef UI_SYSTEM_PAGE_H
#define UI_SYSTEM_PAGE_H

#include "ui_page_action.h"
#include "lvgl.h"

#include <stdbool.h>

lv_obj_t *ui_system_page_create(ui_page_action_cb_t callback,
                                void *user_data);

#endif /* UI_SYSTEM_PAGE_H */
