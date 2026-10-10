#include "ui/core/ui_memory.h"
#include "ui_nav_session.h"
#include "ui/core/ui_concurrency.h"
#include <math.h>
#include <string.h>

static UI_LARGE_RAM struct {
    ui_nav_session_t session, pending_session;
    ui_nav_catalog_t catalog;
    ui_nav_catalog_t pending_catalog;
    ui_nav_request_t request;
    ui_nav_result_t result, pending;
    ui_nav_session_state_t previous;
    uint32_t next_id, started, last_tick, result_version, catalog_version;
    uint32_t producer_revision,retired_session;
    uint32_t catalog_producer_revision,pending_catalog_revision;
    bool busy, pending_valid, session_pending;
} s;
static ui_nav_backend_t s_backend;
static bool point_valid(ui_nav_point_t p)
{
    return isfinite(p.latitude) && isfinite(p.longitude) && fabs(p.latitude) <= 90 &&
        fabs(p.longitude) <= 180 && p.system <= UI_NAV_COORD_GCJ02;
}
static void clean_item(ui_nav_item_t *item)
{
    if(item->thumbnail_count>16)item->thumbnail_count=0;
    for(unsigned i=0;i<item->thumbnail_count;i++)if(!point_valid(item->thumbnail[i])){item->thumbnail_count=0;break;}
    item->name[sizeof(item->name)-1] = 0;
    item->detail[sizeof(item->detail)-1] = 0;
    item->version_name[sizeof(item->version_name)-1]=0; item->coverage[sizeof(item->coverage)-1]=0;
}
void ui_nav_session_init(void)
{
    ui_concurrency_lock();
    memset(&s, 0, sizeof(s));
    s.next_id = 1;
    s.session.version = 1;
    ui_concurrency_unlock();
}
bool ui_nav_session_register_backend(const ui_nav_backend_t *backend)
{
    if(backend && !backend->dispatch) return false;
    ui_concurrency_lock();
    if(s.busy) { ui_concurrency_unlock(); return false; }
    s_backend = backend ? *backend : (ui_nav_backend_t){0};
    ui_concurrency_unlock();
    return true;
}
bool ui_nav_session_cancel_request(uint32_t id)
{ui_concurrency_lock();bool match=s.busy&&s.request.id==id;ui_concurrency_unlock();if(match)ui_nav_session_cancel();return match;}
bool ui_nav_session_discard_preview(void)
{
 ui_concurrency_lock();bool allowed=!s.busy&&!s.session_pending&&s.session.state==UI_NAV_SESSION_PREVIEW;
 if(allowed){s.session.state=UI_NAV_SESSION_IDLE;memset(&s.session.route,0,sizeof(s.session.route));memset(&s.session.destination,0,sizeof(s.session.destination));s.session.version++;}
 ui_concurrency_unlock();return allowed;
}
void ui_nav_session_cancel(void)
{
    ui_concurrency_lock();
    uint32_t id = s.busy ? s.request.id : 0;
    ui_nav_backend_t backend = s_backend;
    if(s.busy) {
        if(s.pending_catalog.request_id==id) s.pending_catalog_revision=s.catalog_producer_revision;
        s.busy = s.pending_valid = false;
        s.session.state = s.previous;
        s.session.version++;
        s.result = (ui_nav_result_t){.request_id=id, .code=UI_NAV_CANCELLED};
        s.result_version++;
    }
    ui_concurrency_unlock();
    if(id && backend.cancel) backend.cancel(id, backend.context);
}
uint32_t ui_nav_session_request(const ui_nav_request_t *request, uint32_t now_ms)
{
    if(!request || request->kind > UI_NAV_REQ_IMPORT_MAP || request->list>UI_NAV_LIST_RECENT_ROUTES ||
       (request->kind == UI_NAV_REQ_PLAN && !point_valid(request->destination.point))) return 0;
    ui_nav_session_cancel();
    ui_concurrency_lock();
    s.request = *request;
    if(!s.next_id) s.next_id = 1;
    s.request.id = s.next_id++;
    s.request.session_id = s.session.id;
    s.request.source=s.session.source;
    s.request.route_revision = request->route_revision ? request->route_revision : s.session.route.revision;
    s.request.timeout_ms = request->timeout_ms ? request->timeout_ms : 15000;
    clean_item(&s.request.destination);
    s.previous = s.session.state;
    bool active=s.previous==UI_NAV_SESSION_ACTIVE||s.previous==UI_NAV_SESSION_ARRIVED||s.previous==UI_NAV_SESSION_STOPPING;
    s.started = now_ms;
    s.last_tick = now_ms;
    s.busy = true;
    if(!active && (request->kind == UI_NAV_REQ_PLAN || request->kind == UI_NAV_REQ_LOAD_ROUTE)) {
        s.previous=UI_NAV_SESSION_IDLE;
        memset(&s.session.route,0,sizeof(s.session.route));
        s.session.destination = request->destination;
        s.session.state = UI_NAV_SESSION_PREPARING;
    } else if(request->kind == UI_NAV_REQ_START || request->kind == UI_NAV_REQ_RESTORE)
        s.session.state = UI_NAV_SESSION_STARTING;
    else if(request->kind == UI_NAV_REQ_STOP) s.session.state = UI_NAV_SESSION_STOPPING;
    s.session.version++;
    ui_nav_request_t copy = s.request;
    bool allowed=!(active && (copy.kind==UI_NAV_REQ_PLAN||copy.kind==UI_NAV_REQ_LOAD_ROUTE || copy.kind==UI_NAV_REQ_IMPORT_ROUTE)) &&
        (copy.kind!=UI_NAV_REQ_START || (s.previous==UI_NAV_SESSION_PREVIEW && s.session.route.start_allowed)) &&
        (copy.kind!=UI_NAV_REQ_STOP || active) &&
        (copy.kind!=UI_NAV_REQ_RESTORE || s.previous==UI_NAV_SESSION_RECOVERABLE);
    ui_nav_backend_t backend = s_backend;
    ui_concurrency_unlock();
    if(!allowed || !backend.dispatch || !backend.dispatch(&copy, backend.context)) {
        static UI_LARGE_RAM ui_nav_result_t failure;
        memset(&failure,0,sizeof(failure)); failure.request_id=copy.id; failure.code=allowed?UI_NAV_UNAVAILABLE:UI_NAV_NO_ROUTE;
        ui_nav_session_complete(&failure);
    }
    return copy.id;
}
bool ui_nav_session_complete(const ui_nav_result_t *result)
{
    if(!result || result->code > UI_NAV_CANCELLED || result->catalog.count > UI_NAV_CATALOG_MAX) return false;
    ui_concurrency_lock();
    bool catalog_request=s.request.kind==UI_NAV_REQ_LIST || s.request.kind==UI_NAV_REQ_MAP_CHECK || s.request.kind==UI_NAV_REQ_MAP_UPDATE || s.request.kind==UI_NAV_REQ_IMPORT_MAP;
    bool accept = s.busy && result->request_id == s.request.id && !s.pending_valid &&
        (!catalog_request || result->code!=UI_NAV_OK || (result->catalog.kind==s.request.list &&
        (!result->catalog.storage_valid || result->catalog.storage_free_kib<=result->catalog.storage_total_kib) &&
        result->catalog.offset<=result->catalog.total && result->catalog.count<=result->catalog.total-result->catalog.offset));
    for(unsigned i=0; accept && i<result->catalog.count; i++) if(result->catalog.items[i].map_status>UI_NAV_MAP_CORRUPT ||
        (result->catalog.items[i].progress_valid && result->catalog.items[i].progress_percent>100)) accept=false;
    if(accept) { s.pending = *result; s.pending_valid = true; }
    ui_concurrency_unlock();
    return accept;
}
bool ui_nav_session_submit(const ui_nav_session_t *session)
{
    if(!session || session->state > UI_NAV_SESSION_RECOVERABLE ||
       session->source > UI_NAV_SOURCE_REPLAY ||
       (session->position_valid && !point_valid(session->position)) ||
       ((session->state == UI_NAV_SESSION_ACTIVE || session->state == UI_NAV_SESSION_ARRIVED) &&
        (!session->id || !session->route.id || !session->route.revision))) return false;
    ui_concurrency_lock();
    uint32_t latest = s.session_pending ? s.pending_session.version : s.producer_revision;
    bool accept = session->version > latest && !(session->id && session->id==s.retired_session);
    if(accept) { s.pending_session = *session; s.session_pending = true; }
    ui_concurrency_unlock();
    return accept;
}
bool ui_nav_session_submit_catalog(const ui_nav_catalog_t *catalog,uint32_t revision)
{
    if(!catalog || catalog->kind>UI_NAV_LIST_RECENT_ROUTES || catalog->count>UI_NAV_CATALOG_MAX || !revision ||
        catalog->offset>catalog->total || catalog->count>catalog->total-catalog->offset ||
        (catalog->storage_valid && catalog->storage_free_kib>catalog->storage_total_kib)) return false;
    for(unsigned i=0;i<catalog->count;i++) if(catalog->items[i].map_status>UI_NAV_MAP_CORRUPT ||
        (catalog->items[i].progress_valid && catalog->items[i].progress_percent>100)) return false;
    ui_concurrency_lock(); bool accept=revision>s.catalog_producer_revision && revision>s.pending_catalog_revision &&
        (!catalog->request_id || (s.busy && catalog->request_id==s.request.id));
    if(accept) { s.pending_catalog=*catalog; s.pending_catalog_revision=revision; }
    ui_concurrency_unlock(); return accept;
}
void ui_nav_session_tick(uint32_t now_ms)
{
    ui_concurrency_lock();
    uint32_t elapsed = now_ms - s.last_tick;
    s.last_tick = now_ms;
    ui_nav_backend_t backend = s_backend;
    ui_concurrency_unlock();
    if(backend.tick) backend.tick(elapsed, backend.context);
    ui_concurrency_lock();
    bool progress=s.pending_catalog_revision>s.catalog_producer_revision && s.pending_catalog.request_id==s.request.id &&
        (s.request.kind==UI_NAV_REQ_MAP_CHECK || s.request.kind==UI_NAV_REQ_MAP_UPDATE);
    if(s.busy && !s.pending_valid && !progress && (uint32_t)(now_ms-s.started) >= s.request.timeout_ms) {
        s.pending = (ui_nav_result_t){.request_id=s.request.id, .code=UI_NAV_TIMEOUT};
        s.pending_valid = true;
    }
    uint32_t cancel_id = 0;
    if(s.pending_valid) {
        s.result = s.pending;
        clean_item(&s.result.route);
        s.result_version++;
        s.pending_valid = false;
        s.busy = false;
        if(s.result.code == UI_NAV_TIMEOUT) cancel_id = s.request.id;
        if(s.result.code != UI_NAV_OK) s.session.state = s.previous;
        else switch(s.request.kind) {
            case UI_NAV_REQ_PLAN: case UI_NAV_REQ_LOAD_ROUTE: case UI_NAV_REQ_IMPORT_ROUTE:
                if(!s.result.route.id || !s.result.route.revision || !point_valid(s.result.route.point)) {
                    s.result.code = UI_NAV_NO_ROUTE; s.session.state = s.previous;
                } else {
                    s.session.route = s.result.route;
                    s.session.destination = s.result.route;
                    s.session.source = s.result.route.demo ? UI_NAV_SOURCE_DEMO : s.request.list==UI_NAV_LIST_PHONE_ROUTES?UI_NAV_SOURCE_PHONE:UI_NAV_SOURCE_DEVICE;
                    s.session.state = UI_NAV_SESSION_PREVIEW;
                }
                break;
            case UI_NAV_REQ_START: case UI_NAV_REQ_RESTORE:
                if(!s.result.session_id || !s.session.route.id || !s.session.route.start_allowed) {
                    s.result.code = UI_NAV_NO_ROUTE; s.session.state = s.previous;
                } else { s.session.id = s.result.session_id; s.session.state = UI_NAV_SESSION_ACTIVE;
                    if(s.request.kind==UI_NAV_REQ_START) s.session.elapsed_valid=s.session.travelled_valid=s.session.ascent_valid=false;
                }
                break;
            case UI_NAV_REQ_STOP:
                s.retired_session=s.session.id;
                s.session.state = UI_NAV_SESSION_IDLE; s.session.id = 0;
                break;
            case UI_NAV_REQ_LIST: case UI_NAV_REQ_MAP_CHECK: case UI_NAV_REQ_MAP_UPDATE: case UI_NAV_REQ_IMPORT_MAP:
                s.catalog = s.result.catalog;
                s.catalog.request_id=s.result.request_id;
                for(unsigned i=0; i<s.catalog.count; i++) clean_item(&s.catalog.items[i]);
                s.catalog_version++;
                break;
            case UI_NAV_REQ_DELETE_ROUTE:
                if(s.request.route_id==s.session.route.id && s.session.state!=UI_NAV_SESSION_ACTIVE &&
                    s.session.state!=UI_NAV_SESSION_ARRIVED && s.session.state!=UI_NAV_SESSION_STOPPING) {
                    memset(&s.session.route,0,sizeof(s.session.route)); s.session.state=UI_NAV_SESSION_IDLE;
                }
                break;
            default: break;
        }
        s.session.version++;
    }
    if(s.session_pending) {
        if(s.pending_session.version > s.producer_revision &&
           !(s.pending_session.id && s.pending_session.id==s.retired_session)) {
            if(s.busy && (s.pending_session.id!=s.session.id || s.pending_session.source!=s.session.source ||
                s.pending_session.route.revision!=s.session.route.revision)) {
                cancel_id=s.request.id; s.busy=false; s.pending_valid=false;
                if(s.pending_catalog.request_id==cancel_id) s.pending_catalog_revision=s.catalog_producer_revision;
                s.result=(ui_nav_result_t){.request_id=cancel_id,.code=UI_NAV_CANCELLED}; s.result_version++;
            }
            s.producer_revision=s.pending_session.version;
            uint32_t version=s.session.version+1;
            s.session = s.pending_session;
            s.session.version=version;
            clean_item(&s.session.route); clean_item(&s.session.destination);
        }
        s.session_pending = false;
    }
    if(s.pending_catalog_revision>s.catalog_producer_revision) {
        s.catalog=s.pending_catalog; s.catalog_producer_revision=s.pending_catalog_revision;
        for(unsigned i=0;i<s.catalog.count;i++) clean_item(&s.catalog.items[i]);
        s.catalog_version++;
        if(s.busy && s.catalog.request_id==s.request.id &&
            (s.request.kind==UI_NAV_REQ_MAP_CHECK || s.request.kind==UI_NAV_REQ_MAP_UPDATE)) s.started=now_ms;
    }
    if(s.session.position_valid && s.session.position_stale_after_ms &&
       (uint32_t)(now_ms-s.session.position_sampled_at_ms)>s.session.position_stale_after_ms) {
        s.session.position_valid=false; s.session.version++;
    }
    ui_concurrency_unlock();
    if(cancel_id && backend.cancel) backend.cancel(cancel_id, backend.context);
}
bool ui_nav_session_get(ui_nav_session_t *out)
{
    if(!out) return false;
    ui_concurrency_lock(); *out = s.session; ui_concurrency_unlock(); return true;
}
bool ui_nav_session_get_catalog(ui_nav_catalog_t *out, uint32_t *version)
{
    if(!out) return false;
    ui_concurrency_lock(); *out = s.catalog; if(version) *version=s.catalog_version;
    ui_concurrency_unlock(); return true;
}
bool ui_nav_session_get_result(ui_nav_result_t *out, uint32_t *version)
{
    if(!out) return false;
    ui_concurrency_lock(); *out=s.result; if(version) *version=s.result_version;
    ui_concurrency_unlock(); return true;
}
bool ui_nav_session_busy(void)
{ ui_concurrency_lock(); bool busy=s.busy; ui_concurrency_unlock(); return busy; }
bool ui_nav_session_active(void)
{
    ui_concurrency_lock(); ui_nav_session_state_t state=s.session.state; ui_concurrency_unlock();
    return state == UI_NAV_SESSION_ACTIVE || state == UI_NAV_SESSION_STOPPING ||
        state == UI_NAV_SESSION_ARRIVED;
}
uint32_t ui_nav_session_now(void)
{ ui_concurrency_lock(); uint32_t now=s.last_tick; ui_concurrency_unlock(); return now; }
const char *ui_nav_result_text(ui_nav_result_code_t code)
{
    switch(code) {
        case UI_NAV_OK: return "已完成";
        case UI_NAV_UNAVAILABLE: return "导航服务暂不可用";
        case UI_NAV_NO_POSITION: return "等待有效定位后再试";
        case UI_NAV_NO_MAP: return "当前区域尚未安装地图";
        case UI_NAV_NO_ROUTE: return "没有可用路线，请重新选择";
        case UI_NAV_CORRUPT_MAP: return "地图校验失败，请重新导入";
        case UI_NAV_UNSUPPORTED: return "此功能暂不可用";
        case UI_NAV_TIMEOUT: return "请求超时，可以重试";
        case UI_NAV_CANCELLED: return "已取消";
        default: return "操作失败，可以重试";
    }
}
