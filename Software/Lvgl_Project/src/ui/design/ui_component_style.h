#ifndef UI_COMPONENT_STYLE_H
#define UI_COMPONENT_STYLE_H

#include "lvgl.h"

void ui_component_style_init(void);
void ui_component_style_apply_bottom_surface(lv_obj_t *object);
void ui_component_style_apply_surface_host(lv_obj_t *object);

#endif /* UI_COMPONENT_STYLE_H */
