#ifndef UI_NAV_SESSION_H
#define UI_NAV_SESSION_H
#include <stdbool.h>
#include <stdint.h>

/* Bounded UI metadata, never the full road network or full route. */
#define UI_NAV_CATALOG_MAX 8
typedef enum { UI_NAV_COORD_WGS84, UI_NAV_COORD_GCJ02 } ui_nav_coord_t;
typedef struct { double latitude, longitude; ui_nav_coord_t system; } ui_nav_point_t;
typedef enum {
    UI_NAV_SESSION_IDLE, UI_NAV_SESSION_PREPARING, UI_NAV_SESSION_PREVIEW,
    UI_NAV_SESSION_STARTING, UI_NAV_SESSION_ACTIVE, UI_NAV_SESSION_STOPPING,
    UI_NAV_SESSION_ARRIVED, UI_NAV_SESSION_RECOVERABLE
} ui_nav_session_state_t;
typedef enum { UI_NAV_SOURCE_DEVICE, UI_NAV_SOURCE_PHONE, UI_NAV_SOURCE_DEMO, UI_NAV_SOURCE_REPLAY } ui_nav_source_t;
typedef enum { UI_NAV_LIST_FAVORITES, UI_NAV_LIST_RECENT, UI_NAV_LIST_ROUTES, UI_NAV_LIST_MAPS,
    UI_NAV_LIST_PHONE_ROUTES,UI_NAV_LIST_MAP_VERSIONS,UI_NAV_LIST_STORAGE, UI_NAV_LIST_RECENT_ROUTES } ui_nav_list_t;
typedef enum {
    UI_NAV_REQ_LIST, UI_NAV_REQ_PLAN, UI_NAV_REQ_LOAD_ROUTE, UI_NAV_REQ_START,
    UI_NAV_REQ_STOP, UI_NAV_REQ_RESTORE, UI_NAV_REQ_DELETE_ROUTE,
    UI_NAV_REQ_MAP_CHECK, UI_NAV_REQ_MAP_UPDATE, UI_NAV_REQ_REROUTE,
    UI_NAV_REQ_IMPORT_ROUTE,UI_NAV_REQ_IMPORT_MAP
} ui_nav_request_kind_t;
typedef enum {
    UI_NAV_OK, UI_NAV_UNAVAILABLE, UI_NAV_NO_POSITION, UI_NAV_NO_MAP,
    UI_NAV_NO_ROUTE, UI_NAV_CORRUPT_MAP, UI_NAV_UNSUPPORTED, UI_NAV_FAILED, UI_NAV_TIMEOUT,
    UI_NAV_CANCELLED
} ui_nav_result_code_t;
typedef enum { UI_NAV_MAP_MISSING, UI_NAV_MAP_READY, UI_NAV_MAP_CHECKING,
    UI_NAV_MAP_UPDATING, UI_NAV_MAP_CORRUPT } ui_nav_map_status_t;
typedef struct {
    uint32_t id, revision;
    char name[64], detail[96];
    ui_nav_point_t point;
    uint32_t distance_m, duration_s, size_kib;
    bool distance_valid, duration_valid, start_allowed, demo, saved;
    bool size_valid,progress_valid;
    uint8_t progress_percent;
    ui_nav_map_status_t map_status;
    uint32_t ascent_m;
    bool ascent_valid,coverage_known,coverage_complete;
    char version_name[24],coverage[48];
    uint8_t thumbnail_count;
    ui_nav_point_t thumbnail[16];
} ui_nav_item_t;
typedef struct {
    ui_nav_list_t kind;
    uint32_t request_id;
    uint32_t offset, total;
    uint8_t count;
    ui_nav_item_t items[UI_NAV_CATALOG_MAX];
    bool storage_valid;
    uint32_t storage_total_kib,storage_free_kib;
} ui_nav_catalog_t;
typedef struct {
    uint32_t id, session_id, route_id, route_revision, offset;
    ui_nav_request_kind_t kind;
    ui_nav_source_t source;
    ui_nav_list_t list;
    ui_nav_item_t destination;
    uint32_t timeout_ms;
} ui_nav_request_t;
typedef struct {
    uint32_t request_id, session_id;
    ui_nav_result_code_t code;
    ui_nav_item_t route;
    ui_nav_catalog_t catalog;
} ui_nav_result_t;
typedef struct {
    uint32_t version, id;
    ui_nav_session_state_t state;
    ui_nav_source_t source;
    ui_nav_item_t destination, route;
    bool position_valid, phone_connected;
    ui_nav_point_t position;
    uint32_t position_sampled_at_ms,position_stale_after_ms;
    bool elapsed_valid,travelled_valid,ascent_valid;
    uint32_t elapsed_s,travelled_m,ascent_m;
} ui_nav_session_t;
typedef struct {
    bool (*dispatch)(const ui_nav_request_t *request, void *context);
    void (*cancel)(uint32_t request_id, void *context);
    void (*tick)(uint32_t elapsed_ms, void *context);
    void *context;
} ui_nav_backend_t;

/* Init clears UI mailboxes, preserving a previously registered board port. */
void ui_nav_session_init(void);
bool ui_nav_session_register_backend(const ui_nav_backend_t *backend);
/* UI-thread requests. Returns a stable id, including unsupported requests. */
uint32_t ui_nav_session_request(const ui_nav_request_t *request, uint32_t now_ms);
void ui_nav_session_cancel(void);
/* UI thread: leave an unstarted preview, preserving the current position. */
bool ui_nav_session_discard_preview(void);
bool ui_nav_session_cancel_request(uint32_t request_id);
/* Producer task context, copies data; never calls LVGL or blocks on I/O. */
bool ui_nav_session_complete(const ui_nav_result_t *result);
bool ui_nav_session_submit(const ui_nav_session_t *session);
/* Optional progress/list snapshots; monotonic producer revision, bounded copy. */
bool ui_nav_session_submit_catalog(const ui_nav_catalog_t *catalog,uint32_t revision);
void ui_nav_session_tick(uint32_t now_ms);
bool ui_nav_session_get(ui_nav_session_t *session);
bool ui_nav_session_get_catalog(ui_nav_catalog_t *catalog, uint32_t *version);
bool ui_nav_session_get_result(ui_nav_result_t *result, uint32_t *version);
bool ui_nav_session_busy(void);
bool ui_nav_session_active(void);
uint32_t ui_nav_session_now(void);
const char *ui_nav_result_text(ui_nav_result_code_t code);
#endif
