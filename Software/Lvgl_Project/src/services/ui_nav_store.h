#ifndef UI_NAV_STORE_H
#define UI_NAV_STORE_H
#include "ui_nav_session.h"
#include "ui_navigation_service.h"

/* Explicit board capabilities. Zero means no supported product operation. */
enum {
    UI_NAV_CAP_ROUTES=1u, UI_NAV_CAP_PLACES=2u, UI_NAV_CAP_PLAN=4u,
    UI_NAV_CAP_PICK=8u, UI_NAV_CAP_IMPORT_ROUTE=16u, UI_NAV_CAP_PHONE=32u,
    UI_NAV_CAP_MAPS=64u, UI_NAV_CAP_IMPORT_MAP=128u, UI_NAV_CAP_REROUTE=256u
};
typedef enum { UI_NAV_PHASE_FAR,UI_NAV_PHASE_APPROACH,UI_NAV_PHASE_NEAR,
    UI_NAV_PHASE_NOW,UI_NAV_PHASE_PASSED } ui_nav_phase_t;
typedef struct {
    ui_nav_session_t session;
    nav_model_t guidance;
    uint32_t revision,capabilities;
    ui_nav_phase_t phase;
    bool guidance_matches,busy;
} ui_nav_snapshot_t;
/* Producer-safe capability registration; existing mailboxes own async data. */
void ui_nav_store_set_capabilities(uint32_t capabilities);
uint32_t ui_nav_store_capabilities(void);
/* UI thread: compose only coherent session/guidance, never copies map geometry. */
bool ui_nav_store_get(ui_nav_snapshot_t *snapshot);
uint32_t ui_nav_store_command(const ui_nav_request_t *request,uint32_t now_ms);
#endif
