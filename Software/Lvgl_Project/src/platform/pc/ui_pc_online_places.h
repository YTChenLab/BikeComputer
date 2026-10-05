#ifndef UI_PC_ONLINE_PLACES_H
#define UI_PC_ONLINE_PLACES_H
#include "services/ui_online_places.h"
#include "services/ui_nav_search.h"
typedef struct {bool enabled,valid;char key[33];} ui_pc_online_places_config_t;
/* Missing config leaves network OFF. Never logs keys, queries or responses. */
void ui_pc_online_places_load(ui_pc_online_places_config_t *config,const char *path);
ui_place_online_status_t ui_pc_online_places_query(const ui_pc_online_places_config_t *config,
 const ui_nav_search_request_t *request,ui_place_results_t *out,bool (*cancelled)(void *),void *context);
#endif
