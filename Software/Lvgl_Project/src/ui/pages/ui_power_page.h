#ifndef UI_POWER_PAGE_H
#define UI_POWER_PAGE_H

#include "ui_page_action.h"
#include "../core/ui_input.h"
#include "lvgl.h"

#include <stdbool.h>

lv_obj_t *ui_power_page_create(ui_page_action_cb_t callback, void *user_data);
bool ui_power_page_handle_input(ui_input_event_t input);

#endif /* UI_POWER_PAGE_H */
