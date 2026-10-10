/* Compile-only ABI measurement. Never link this object into firmware. */
#include "platform/stm32/ui_stm32_startup.h"
#include "services/ui_offline_graph.h"
#include "services/ui_offline_tiles.h"
#include "services/ui_online_places.h"
#include "ui/models/ui_ride_report_model.h"
#define PROBE(type) const unsigned char board_size_##type[sizeof(type)] = {0}
PROBE(ui_ride_data_t);
PROBE(nav_model_t);
PROBE(ui_nav_session_t);
PROBE(ui_nav_result_t);
PROBE(ui_nav_catalog_t);
PROBE(ui_nav_map_frame_t);
PROBE(ui_place_results_t);
PROBE(ui_offline_graph_t);
PROBE(ui_offline_tiles_t);
PROBE(ui_offline_tile_cache_t);
PROBE(ui_offline_route_t);
PROBE(ui_ride_record_t);
PROBE(ui_ride_summary_t);
PROBE(ui_ride_report_model_t);
PROBE(ui_ride_history_checkpoint_t);
const unsigned char board_size_framebuffer_rgb565[480 * 854 * 2] = {0};
const unsigned char board_size_draw_buffer_40_lines[480 * 40 * 2] = {0};
const unsigned char board_size_raster_rgb565[768 * 768 * 2] = {0};
