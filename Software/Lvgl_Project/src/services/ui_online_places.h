#ifndef UI_ONLINE_PLACES_H
#define UI_ONLINE_PLACES_H
#include "ui_offline_places.h"
#define UI_ONLINE_RESPONSE_MAX 65536u
typedef enum { UI_PLACE_ONLINE_OFF,UI_PLACE_ONLINE_OK,UI_PLACE_ONLINE_FAILED,
 UI_PLACE_ONLINE_TIMEOUT,UI_PLACE_ONLINE_KEY_ERROR } ui_place_online_status_t;
/* Bounded JSON input; returned points are approximate WGS84, never entrances. */
ui_place_online_status_t ui_online_places_amap_parse(const char *json,size_t size,ui_place_results_t *out);
void ui_online_places_merge(ui_place_results_t *offline,const ui_place_results_t *online,
                           ui_place_online_status_t status);
bool ui_online_places_gcj_to_wgs(ui_nav_point_t gcj,ui_nav_point_t *wgs);
bool ui_online_places_wgs_to_gcj(ui_nav_point_t wgs,ui_nav_point_t *gcj);
#endif
