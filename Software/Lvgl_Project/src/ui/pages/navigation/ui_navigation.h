#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#include "lvgl.h"
#include "../../core/ui_input.h"

#include <stdbool.h>

void ui_navigation_create(lv_obj_t *screen);
void ui_navigation_set_active(bool active);
bool ui_navigation_contains_point(int32_t x, int32_t y);
bool ui_navigation_handle_input(ui_input_event_t input);
void ui_navigation_demo_next_state(void);

#endif /* UI_NAVIGATION_H */
