#ifndef UI_NAV_SEARCH_H
#define UI_NAV_SEARCH_H
#include "ui_offline_places.h"
typedef struct {uint32_t id;char query[UI_PLACE_QUERY_MAX];ui_nav_point_t center;bool center_valid;} ui_nav_search_request_t;
typedef struct {bool (*request)(const ui_nav_search_request_t *,void *);void (*cancel)(uint32_t,void *);void *context;} ui_nav_search_backend_t;
bool ui_nav_search_register(const ui_nav_search_backend_t *backend);
uint32_t ui_nav_search_request(const char *query,ui_nav_point_t center,bool valid,uint32_t now);
void ui_nav_search_cancel(void);
bool ui_nav_search_submit(const ui_place_results_t *result);
/* UI timer tick applies bounded pending copy and a 5s timeout, never file I/O. */
void ui_nav_search_tick(uint32_t now);
void ui_nav_search_get(ui_place_results_t *result,uint32_t *version,bool *busy);
#endif
