#ifndef UI_OFFLINE_PLACES_H
#define UI_OFFLINE_PLACES_H
#include "ui_offline_map.h"
#define UI_PLACE_QUERY_MAX 64
#define UI_PLACE_RESULT_MAX 8
typedef struct {uint32_t id;ui_nav_point_t point;char name[64],detail[64];uint8_t kind;bool entrance;
 ui_nav_point_t navigation_point;bool navigation_point_valid,online;} ui_place_t;
typedef struct {uint32_t request_id;ui_nav_result_code_t code;uint8_t count;bool limited;uint8_t online_status;ui_place_t items[UI_PLACE_RESULT_MAX];} ui_place_results_t;
typedef struct {ui_offline_map_io_t io;uint32_t revision,records,keys,records_offset,keys_offset;} ui_offline_places_t;
typedef struct {uint32_t bytes,scanned;bool limited;} ui_place_stats_t;
/* Storage worker only, caller supplies FatFS read_at and file size. No malloc/LVGL. */
ui_nav_result_code_t ui_offline_places_open(ui_offline_places_t *places,const ui_offline_map_io_t *io);
ui_nav_result_code_t ui_offline_places_query(const ui_offline_places_t *places,const char *query,ui_nav_point_t center,bool center_valid,ui_place_results_t *results,bool (*cancelled)(void *),void *context,ui_place_stats_t *stats);
#endif
