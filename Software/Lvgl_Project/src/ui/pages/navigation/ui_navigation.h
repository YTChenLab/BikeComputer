#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#include "lvgl.h"
#include "../../core/ui_input.h"

#include <stdbool.h>
#include "services/ui_nav_session.h"
#include "services/ui_offline_places.h"
#include "../../components/navigation/ui_nav_map.h"

typedef enum { UI_NAV_CARD_MAP, UI_NAV_CARD_DOCK } ui_navigation_card_t;
/* Outer card selection is separate from the controls inside either card. */
ui_navigation_card_t ui_navigation_selected_card(void);
bool ui_navigation_card_entered(void);
void ui_navigation_create(lv_obj_t *screen);
void ui_navigation_set_active(bool active);
bool ui_navigation_contains_point(int32_t x, int32_t y);
bool ui_navigation_handle_input(ui_input_event_t input);
void ui_navigation_demo_next_state(void);
void ui_navigation_cancel_pointer(void);
void ui_navigation_handle_pointer(const ui_input_pointer_event_t *event);
bool ui_navigation_handle_back(void);
void ui_navigation_set_exit_callback(void (*callback)(void));
void ui_navigation_open_routes(void);
void ui_navigation_set_routes_callback(void (*callback)(ui_nav_list_t));
void ui_navigation_open_routes_kind(ui_nav_list_t kind);
void ui_navigation_open_maps(void);
const ui_nav_map_t *ui_navigation_map(void);
void ui_navigation_open_menu(void);
void ui_navigation_set_overview(bool overview);
bool ui_navigation_overview(void);
void ui_navigation_toggle_direction(void);
bool ui_navigation_north_up(void);
void ui_navigation_toggle_prompts(void);
bool ui_navigation_prompts_enabled(void);
void ui_navigation_open_phone(void);
void ui_navigation_import_route(void);
void ui_navigation_preview_destination(const ui_nav_item_t *place);
void ui_navigation_preview_route(const ui_nav_item_t *route);
void ui_navigation_open_search(void);
void ui_navigation_close_search(void);
void ui_navigation_search_layout(bool open);
/* Candidate expansion and search entry share one composed map geometry. */
void ui_navigation_search_candidates(int32_t progress);
void ui_navigation_search_selected(const ui_place_t *place);

#endif /* UI_NAVIGATION_H */
