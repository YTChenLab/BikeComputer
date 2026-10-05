#ifndef UI_HOME_PAGE_H
#define UI_HOME_PAGE_H

#include "ui_page_action.h"
#include "../core/ui_input.h"
#include "lvgl.h"

#include <stdbool.h>

lv_obj_t *ui_home_page_create(ui_page_action_cb_t callback,
                              void *user_data);
void ui_home_page_set_active(bool active);
void ui_home_page_animate_enter(bool reduced_motion,
                                lv_anim_completed_cb_t completed, void *user_data);
void ui_home_page_cancel_enter(void);
bool ui_home_page_is_entering(void);
bool ui_home_page_refresh_feedback(void);
bool ui_home_page_handle_input(ui_input_event_t input);

#endif /* UI_HOME_PAGE_H */
