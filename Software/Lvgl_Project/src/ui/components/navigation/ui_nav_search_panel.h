#ifndef UI_NAV_SEARCH_PANEL_H
#define UI_NAV_SEARCH_PANEL_H
#include "lvgl.h"
#include "ui/core/ui_input.h"
#include "services/ui_nav_search.h"
void ui_nav_search_panel_create(lv_obj_t *parent);
void ui_nav_search_panel_open(ui_nav_point_t center,bool valid);
void ui_nav_search_panel_close(bool animated);
bool ui_nav_search_panel_active(void);
bool ui_nav_search_panel_back(void);
bool ui_nav_search_panel_input(ui_input_event_t input);
const char *ui_nav_search_panel_query(void);
#endif
