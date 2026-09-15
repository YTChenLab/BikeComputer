#ifndef UI_DOCK_VIEW_H
#define UI_DOCK_VIEW_H

#include "lvgl.h"

#include <stdbool.h>

void ui_dock_view_init(void);
void ui_dock_view_deinit(void);
bool ui_dock_view_is_visible(void);
lv_obj_t *ui_dock_view_root(void);

#endif /* UI_DOCK_VIEW_H */
