#ifndef UI_SAVED_ROUTES_PAGE_H
#define UI_SAVED_ROUTES_PAGE_H

#include "lvgl.h"
#include "ui_page_action.h"
#include "services/ui_nav_session.h"
#include "../core/ui_input.h"

lv_obj_t *ui_saved_routes_page_create(ui_page_action_cb_t callback,
                                      void *user_data);

void ui_saved_routes_page_set_kind(ui_nav_list_t kind);
void ui_saved_routes_page_set_active(bool active);
ui_nav_list_t ui_saved_routes_page_kind(void);
bool ui_saved_routes_page_selected(ui_nav_item_t *item);
bool ui_saved_routes_page_handle_input(ui_input_event_t input);

#endif /* UI_SAVED_ROUTES_PAGE_H */
