#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#include "lvgl.h"
#include "../../components/navigation/ui_nav_model.h"
#include "../../core/ui_input.h"

#include <stdbool.h>

void ui_navigation_create(lv_obj_t *screen);
void ui_navigation_set_active(bool active);
bool ui_navigation_post_model(const nav_model_t *model);
bool ui_navigation_contains_point(int32_t x, int32_t y);
bool ui_navigation_handle_input(ui_input_event_t input);
const nav_model_t *ui_navigation_get_model(void);

#endif /* UI_NAVIGATION_H */
