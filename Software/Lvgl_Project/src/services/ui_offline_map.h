#ifndef UI_OFFLINE_MAP_H
#define UI_OFFLINE_MAP_H
#include "ui_nav_map_provider.h"
#include <stddef.h>
/* Board task supplies FatFS/QSPI read_at; core has no filesystem or LVGL calls. */
typedef struct {
 bool (*read_at)(void *context,uint32_t offset,void *bytes,size_t count);
 uint32_t size;void *context;
} ui_offline_map_io_t;
typedef struct {
 ui_offline_map_io_t io;uint32_t revision,cell,cols,rows;
 int32_t west,south,east,north;
 char version_name[16],name[48];
} ui_offline_map_t;
typedef struct {uint32_t tiles,bytes,roads_seen,areas_seen;bool roads_limited,areas_limited;} ui_offline_map_stats_t;
ui_nav_result_code_t ui_offline_map_open(ui_offline_map_t *map,const ui_offline_map_io_t *io);
/* Call on the storage worker. Working memory is bounded by the output frame;
 * input tiles stream in fixed record buffers; CRC checked before publication. */
ui_nav_result_code_t ui_offline_map_read(const ui_offline_map_t *map,const ui_nav_map_request_t *request,ui_nav_map_frame_t *frame,ui_offline_map_stats_t *stats);
#endif
