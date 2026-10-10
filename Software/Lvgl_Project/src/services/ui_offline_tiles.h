#ifndef UI_OFFLINE_TILES_H
#define UI_OFFLINE_TILES_H
#include "ui_offline_map.h"
#define UI_OFFLINE_TILE_SIZE 256
#define UI_OFFLINE_RASTER_SIZE 768
#define UI_OFFLINE_TILE_LEVEL_MAX 10
#define UI_OFFLINE_TILE_CACHE_MAX 32
typedef struct { float mpp; uint32_t cols,rows,first; } ui_offline_tile_level_t;
typedef struct {
    ui_offline_map_io_t io;
    uint8_t validation_workspace[1024];
    uint32_t revision,directory,total,label_offset,label_bytes;
    int32_t west,south,east,north;
    float latitude_ref,meters_lon,meters_lat;
    uint32_t level_count;
    ui_offline_tile_level_t levels[UI_OFFLINE_TILE_LEVEL_MAX];
} ui_offline_tiles_t;
typedef struct {float x,y;uint8_t kind;char text[48];} ui_offline_tile_label_t;
typedef struct { uint32_t key,age; bool valid;uint8_t label_count;ui_offline_tile_label_t labels[16]; } ui_offline_tile_cache_entry_t;
typedef struct {
    uint8_t decode_workspace[4096];
    uint16_t *pixels; unsigned slots; uint32_t clock;
    ui_offline_tile_cache_entry_t entries[UI_OFFLINE_TILE_CACHE_MAX];
} ui_offline_tile_cache_t;
typedef struct { uint32_t bytes,hits,misses,tiles; float mpp; } ui_offline_tiles_stats_t;
uint32_t ui_offline_crc32(uint32_t state,const void *data,size_t count);
ui_nav_result_code_t ui_offline_tiles_open(ui_offline_tiles_t *tiles,const ui_offline_map_io_t *io);
void ui_offline_tile_cache_init(ui_offline_tile_cache_t *cache,uint16_t *pixels,unsigned slots);
/* Storage-task only. Output and cache buffers are supplied by the caller, so a
 * board can use SDRAM/PSRAM without allocating from the LVGL UI heap. */
ui_nav_result_code_t ui_offline_tiles_window(const ui_offline_tiles_t *tiles,
    ui_offline_tile_cache_t *cache,const ui_nav_map_request_t *request,
    uint16_t *pixels,ui_nav_map_frame_t *frame,ui_offline_tiles_stats_t *stats);
#endif
