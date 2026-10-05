#ifndef UI_OFFLINE_ROUTE_H
#define UI_OFFLINE_ROUTE_H
#include "ui_offline_map.h"
#include "ui_navigation_service.h"
#define UI_OFFLINE_ROUTE_POINT_MAX 4096
typedef struct {ui_nav_point_t position;float cumulative_m;char road[48];} ui_offline_route_point_t;
typedef struct {uint32_t id,revision,count;float distance_m;char name[48];ui_offline_route_point_t points[UI_OFFLINE_ROUTE_POINT_MAX];} ui_offline_route_t;
ui_nav_result_code_t ui_offline_route_open(ui_offline_route_t *route,const ui_offline_map_io_t *io);
/* Route replay only. Actual GNSS/map matching is a separate board producer. */
void ui_offline_route_replay(const ui_offline_route_t *route,float travelled_m,uint32_t session_id,uint32_t now_ms,nav_model_t *model);
void ui_offline_route_window(const ui_offline_route_t *route,ui_nav_map_frame_t *frame);
#endif
