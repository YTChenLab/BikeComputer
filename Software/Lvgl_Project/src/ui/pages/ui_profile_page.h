#ifndef UI_PROFILE_PAGE_H
#define UI_PROFILE_PAGE_H

#include "ui_page_action.h"
#include "lvgl.h"

#include <stdbool.h>

lv_obj_t *ui_profile_page_create(ui_page_action_cb_t callback,
                                 void *user_data);

#endif /* UI_PROFILE_PAGE_H */
