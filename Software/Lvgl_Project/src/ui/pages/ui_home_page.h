#ifndef UI_HOME_PAGE_H
#define UI_HOME_PAGE_H

#include "ui_page_action.h"
#include "../core/ui_input.h"
#include "lvgl.h"

#include <stdbool.h>

lv_obj_t *ui_home_page_create(ui_page_action_cb_t callback,
                              void *user_data);
void ui_home_page_set_active(bool active);
bool ui_home_page_handle_input(ui_input_event_t input);

#endif /* UI_HOME_PAGE_H */
