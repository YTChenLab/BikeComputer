#ifndef UI_DOCK_VIEW_H
#define UI_DOCK_VIEW_H

#include "lvgl.h"
#include "../../core/ui_input.h"

#include <stdbool.h>

void ui_dock_view_init(void);
void ui_dock_view_deinit(void);
bool ui_dock_view_is_visible(void);
lv_obj_t *ui_dock_view_root(void);
bool ui_dock_view_handle_input(ui_input_event_t input);

#endif /* UI_DOCK_VIEW_H */
