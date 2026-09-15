#ifndef UI_SETTINGS_PAGE_H
#define UI_SETTINGS_PAGE_H

#include "ui_page_action.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

lv_obj_t *ui_settings_page_create(ui_page_action_cb_t callback,
                                  void *user_data);
bool ui_settings_page_handle_key(uint32_t key);

#endif /* UI_SETTINGS_PAGE_H */
