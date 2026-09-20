#ifndef UI_PROFILE_PAGE_H
#define UI_PROFILE_PAGE_H

#include "ui_page_action.h"
#include "../core/ui_input.h"
#include "lvgl.h"

#include <stdbool.h>

lv_obj_t *ui_profile_page_create(ui_page_action_cb_t callback,
                                 void *user_data);
void ui_profile_page_set_active(bool active);
void ui_profile_page_refresh(void);
bool ui_profile_page_handle_input(ui_input_event_t input);
int32_t ui_profile_page_selected_index(void);

#endif /* UI_PROFILE_PAGE_H */
