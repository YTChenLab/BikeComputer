#ifndef UI_PC_OFFLINE_NAVIGATION_H
#define UI_PC_OFFLINE_NAVIGATION_H
#include <stdbool.h>
#include "services/ui_offline_map.h"
/* Main selects the installed package. Tests can still select the synthetic port. */
bool ui_pc_offline_navigation_register(const char *directory);
void ui_pc_offline_navigation_set_replay_speed(float multiplier);
/* Deterministic capture clock; zero restores host time. */
void ui_pc_offline_navigation_set_epoch(uint32_t epoch_utc);
void ui_pc_offline_navigation_shutdown(void);
bool ui_pc_offline_navigation_stats(ui_offline_map_stats_t *stats);
typedef struct {
 float map_ms,planning_ms;
 uint32_t read_bytes,cache_hits,cache_misses,tiles,map_requests;
 uint32_t planning_expanded,planning_workspace_bytes;
} ui_pc_offline_performance_t;
bool ui_pc_offline_navigation_performance(ui_pc_offline_performance_t *out);
#endif
