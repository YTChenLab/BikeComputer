#include "ui_nav_map_provider.h"
#include "ui/core/ui_concurrency.h"
#include <math.h>
#include <string.h>
static ui_nav_map_backend_t s_backend;
static struct {
    ui_nav_map_request_t request;
    ui_nav_map_frame_t current, pending;
    uint32_t next, version, started;
    bool busy, pending_valid;
} s;
static bool valid(ui_nav_point_t p)
{ return isfinite(p.latitude) && isfinite(p.longitude) && fabs(p.latitude)<=90 && fabs(p.longitude)<=180 && p.system<=UI_NAV_COORD_GCJ02; }
void ui_nav_map_provider_init(void)
{ ui_concurrency_lock(); memset(&s,0,sizeof(s)); s.next=1; s.current.status=UI_NAV_NO_MAP; ui_concurrency_unlock(); }
bool ui_nav_map_provider_register(const ui_nav_map_backend_t *backend)
{
    if(backend && !backend->request) return false;
    ui_concurrency_lock(); s_backend=backend?*backend:(ui_nav_map_backend_t){0}; ui_concurrency_unlock(); return true;
}
void ui_nav_map_provider_cancel(void)
{
    ui_concurrency_lock(); uint32_t id=s.busy?s.request.id:0;
    s.busy=s.pending_valid=false; ui_nav_map_backend_t backend=s_backend; ui_concurrency_unlock();
    if(id && backend.cancel) backend.cancel(id,backend.context);
}
uint32_t ui_nav_map_provider_request(const ui_nav_map_request_t *request,uint32_t now_ms)
{
    if(!request || !valid(request->center) || !isfinite(request->meters_per_pixel) ||
       request->meters_per_pixel<0.05f || request->meters_per_pixel>10000 ||
       !request->width || !request->height || !isfinite(request->heading) || request->purpose>UI_NAV_MAP_PICK) return 0;
    ui_nav_map_provider_cancel();
    ui_concurrency_lock(); s.request=*request; if(!s.next) s.next=1;
    s.request.id=s.next++; s.started=now_ms; s.busy=true;
    ui_nav_map_request_t copy=s.request; ui_nav_map_backend_t backend=s_backend; ui_concurrency_unlock();
    if(!backend.request || !backend.request(&copy,backend.context)) {
        ui_nav_map_frame_t empty={.request_id=copy.id,.route_id=copy.route_id,
            .route_revision=copy.route_revision,.status=UI_NAV_NO_MAP};
        ui_nav_map_provider_submit(&empty);
    }
    return copy.id;
}
bool ui_nav_map_provider_submit(const ui_nav_map_frame_t *frame)
{
    if(!frame || frame->road_count>UI_NAV_MAP_ROAD_MAX || frame->point_count>UI_NAV_WINDOW_POINT_MAX ||
       frame->area_count>UI_NAV_MAP_AREA_MAX || frame->label_count>UI_NAV_MAP_LABEL_MAX ||
       frame->status>UI_NAV_CANCELLED) return false;
    if(frame->status==UI_NAV_OK || (frame->status==UI_NAV_NO_MAP && frame->point_count>=2)) {
        if((!frame->map_revision && (frame->road_count || frame->area_count || frame->label_count)) || !valid(frame->origin) || !isfinite(frame->meters_per_pixel) ||
           frame->meters_per_pixel<0.05f || frame->meters_per_pixel>10000) return false;
        if(frame->raster_id && (!frame->map_revision || frame->raster_id!=frame->request_id ||
           !isfinite(frame->raster_latitude_ref) || fabsf(frame->raster_latitude_ref)>85)) return false;
        if(frame->destination_valid && (!valid(frame->destination)||frame->destination.system!=frame->origin.system)) return false;
        for(unsigned i=0;i<frame->point_count;i++) if(!valid(frame->points[i]) || frame->points[i].system!=frame->origin.system) return false;
        for(unsigned i=0;i<frame->road_count;i++) if(!valid(frame->roads[i].a) || !valid(frame->roads[i].b) ||
            frame->roads[i].a.system!=frame->origin.system || frame->roads[i].b.system!=frame->origin.system || frame->roads[i].level>2) return false;
        for(unsigned i=0;i<frame->area_count;i++) {
            const ui_nav_area_t *area=&frame->areas[i];
            if(area->count<3 || area->count>UI_NAV_MAP_AREA_POINT_MAX || area->kind>UI_NAV_AREA_WATER) return false;
            float sign=0;
            for(unsigned j=0;j<area->count;j++) {
                ui_nav_local_point_t a=area->points[j],b=area->points[(j+1)%area->count],c=area->points[(j+2)%area->count];
                if(!isfinite(a.east_m)||!isfinite(a.north_m)||fabs(a.east_m)>1000000||fabs(a.north_m)>1000000) return false;
                float cross=(b.east_m-a.east_m)*(c.north_m-b.north_m)-(b.north_m-a.north_m)*(c.east_m-b.east_m);
                if(cross && sign && (cross>0)!=(sign>0)) return false;
                if(cross) sign=cross;
            }
            if(!sign) return false;
        }
        for(unsigned i=0;i<frame->label_count;i++) {
            ui_nav_local_point_t p=frame->labels[i].point;
            if(!isfinite(p.east_m)||!isfinite(p.north_m)||fabs(p.east_m)>1000000||fabs(p.north_m)>1000000||
                frame->labels[i].kind>7 || !memchr(frame->labels[i].text,0,sizeof(frame->labels[i].text))) return false;
        }
    }
    ui_concurrency_lock(); bool accept=s.busy && !s.pending_valid && frame->request_id==s.request.id &&
        frame->route_id==s.request.route_id && frame->route_revision==s.request.route_revision &&
        ((frame->status!=UI_NAV_OK && !(frame->status==UI_NAV_NO_MAP && frame->point_count>=2)) || frame->origin.system==s.request.center.system);
    if(accept) { s.pending=*frame; s.pending_valid=true; }
    ui_concurrency_unlock(); return accept;
}
bool ui_nav_map_provider_process(uint32_t now_ms)
{
    ui_concurrency_lock();
    if(s.busy && !s.pending_valid && (uint32_t)(now_ms-s.started)>=5000) {
        s.pending=(ui_nav_map_frame_t){.request_id=s.request.id,.route_id=s.request.route_id,
            .route_revision=s.request.route_revision,.status=UI_NAV_TIMEOUT}; s.pending_valid=true;
    }
    bool changed=s.pending_valid; uint32_t cancel_id=0;
    if(changed) { s.current=s.pending; s.pending_valid=s.busy=false; s.version++;
        if(s.current.status==UI_NAV_TIMEOUT) cancel_id=s.request.id; }
    ui_nav_map_backend_t backend=s_backend; ui_concurrency_unlock();
    if(cancel_id && backend.cancel) backend.cancel(cancel_id,backend.context);
    return changed;
}
bool ui_nav_map_provider_get(ui_nav_map_frame_t *frame,uint32_t *version)
{
    if(!frame) return false; ui_concurrency_lock(); *frame=s.current;
    if(version) *version=s.version; ui_concurrency_unlock(); return true;
}
bool ui_nav_map_provider_busy(void)
{ ui_concurrency_lock(); bool busy=s.busy; ui_concurrency_unlock(); return busy; }
