#ifndef UI_NAV_GEOMETRY_H
#define UI_NAV_GEOMETRY_H

#include "ui_nav_map.h"

lv_point_precise_t ui_nav_geometry_transform_point(const ui_nav_map_t *map,
                                                    lv_point_precise_t point);
void ui_nav_geometry_attach(ui_nav_map_t *map);
void ui_nav_geometry_invalidate(ui_nav_map_t *map);
void ui_nav_geometry_draw_frame(lv_layer_t *layer,const lv_area_t *area,const ui_nav_map_frame_t *frame,float scale);

#endif /* UI_NAV_GEOMETRY_H */
