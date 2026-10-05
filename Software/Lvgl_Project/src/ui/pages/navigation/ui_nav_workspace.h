#ifndef UI_NAV_WORKSPACE_H
#define UI_NAV_WORKSPACE_H
#include "lvgl.h"
#include "../../core/ui_input.h"
#include <stdbool.h>
#include "../../components/navigation/ui_nav_map.h"
#include "services/ui_nav_session.h"
typedef enum {
    UI_NAV_VIEW_CENTER, UI_NAV_VIEW_DESTINATION, UI_NAV_VIEW_LIST,
    UI_NAV_VIEW_PREVIEW, UI_NAV_VIEW_PICK, UI_NAV_VIEW_LIVE,
    UI_NAV_VIEW_MENU, UI_NAV_VIEW_STOP, UI_NAV_VIEW_DELETE,
    UI_NAV_VIEW_MAPS, UI_NAV_VIEW_ARRIVAL, UI_NAV_VIEW_PHONE,UI_NAV_VIEW_MAP_DETAIL, UI_NAV_VIEW_ERROR, UI_NAV_VIEW_MAP_VERSIONS, UI_NAV_VIEW_STORAGE
} ui_nav_view_t;
void ui_nav_workspace_create(lv_obj_t *parent, void (*live_changed)(bool live), void (*exit_page)(void));
void ui_nav_workspace_bind_map(ui_nav_map_t *map);
void ui_nav_workspace_set_controls_active(bool active);
void ui_nav_workspace_open_maps(void);
void ui_nav_workspace_rebuild(void);
bool ui_nav_workspace_map_visible(void);
int32_t ui_nav_workspace_height(void);
void ui_nav_workspace_set_active(bool active);
void ui_nav_workspace_refresh(void);
bool ui_nav_workspace_handle_input(ui_input_event_t input);
bool ui_nav_workspace_back(void);
bool ui_nav_workspace_contains_point(int32_t x,int32_t y);
void ui_nav_workspace_cancel_pointer(void);
void ui_nav_workspace_handle_pointer(const ui_input_pointer_event_t *event);
void ui_nav_workspace_open_menu(void);
void ui_nav_workspace_open_routes(void);
void ui_nav_workspace_preview_destination(const ui_nav_item_t *place);
void ui_nav_workspace_preview_search_destination(const ui_nav_item_t *place,bool mapped_entrance,bool online);
void ui_nav_workspace_suspend(bool suspend);
void ui_nav_workspace_preview_route(const ui_nav_item_t *route);
void ui_nav_workspace_open_phone(void);
void ui_nav_workspace_import_route(void);
void ui_nav_workspace_stop(void);
/* The product application coordinates recording before authorizing guidance. */
void ui_nav_workspace_set_start_callback(void (*callback)(void));
/* Return true when the application owns stop-recording/save coordination. */
void ui_nav_workspace_set_finish_callback(bool (*callback)(void));
bool ui_nav_workspace_start(void);
lv_obj_t *ui_nav_workspace_root(void);
ui_nav_view_t ui_nav_workspace_view(void);
#endif
