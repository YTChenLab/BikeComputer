#ifndef UI_OFFLINE_GRAPH_H
#define UI_OFFLINE_GRAPH_H
#include "ui_offline_route.h"
typedef struct {
    ui_offline_map_io_t io;uint32_t revision,nodes,edges,names,node_offset,edge_offset,name_offset;
    int32_t west,south,east,north;
} ui_offline_graph_t;
typedef struct {uint32_t expanded,bytes;float start_snap_m,end_snap_m;bool cancelled;} ui_offline_graph_stats_t;
ui_nav_result_code_t ui_offline_graph_open(ui_offline_graph_t *graph,const ui_offline_map_io_t *io);
size_t ui_offline_graph_workspace_size(const ui_offline_graph_t *graph);
/* Real road-graph A*, caller-owned scratch memory and route. No LVGL calls,
 * no malloc, no direct-line fallback. Conservative cycling access is baked
 * into the graph; complex turn restrictions remain a separate extension. */
ui_nav_result_code_t ui_offline_graph_plan(const ui_offline_graph_t *graph,
    ui_nav_point_t start,ui_nav_point_t end,void *workspace,size_t bytes,
    ui_offline_route_t *route,bool (*cancelled)(void *context),void *context,
    ui_offline_graph_stats_t *stats);
#endif
