#include "ui/core/ui_memory.h"
#include "ui_nav_search.h"
#include "ui_online_places.h"
#include "ui/core/ui_concurrency.h"
#include <string.h>
#include <math.h>
static ui_nav_search_backend_t port;
static UI_LARGE_RAM struct {ui_place_results_t current,pending;uint32_t next,id,version,started;bool busy,ready;} s;
bool ui_nav_search_register(const ui_nav_search_backend_t *p){if(p&&!p->request)return false;ui_nav_search_cancel();ui_concurrency_lock();port=p?*p:(ui_nav_search_backend_t){0};s.current=(ui_place_results_t){0};s.version++;ui_concurrency_unlock();return true;}
void ui_nav_search_cancel(void){ui_concurrency_lock();uint32_t id=s.busy?s.id:0;s.busy=s.ready=false;s.id=0;ui_nav_search_backend_t p=port;ui_concurrency_unlock();if(id&&p.cancel)p.cancel(id,p.context);}
uint32_t ui_nav_search_request(const char *q,ui_nav_point_t c,bool valid,uint32_t now)
{
 if(!q||strlen(q)>=UI_PLACE_QUERY_MAX)return 0;ui_nav_search_cancel();ui_concurrency_lock();if(!s.next)s.next=1;
 ui_nav_search_request_t r={.id=s.next++,.center=c,.center_valid=valid};memcpy(r.query,q,strlen(q)+1);s.id=r.id;s.started=now;s.busy=true;s.current=(ui_place_results_t){.request_id=r.id};s.version++;
 ui_nav_search_backend_t p=port;ui_concurrency_unlock();if(!p.request||!p.request(&r,p.context)){static UI_LARGE_RAM ui_place_results_t result;memset(&result,0,sizeof(result));result.request_id=r.id;result.code=p.request?UI_NAV_FAILED:UI_NAV_UNAVAILABLE;ui_nav_search_submit(&result);}return r.id;
}
bool ui_nav_search_submit(const ui_place_results_t *r)
{
 if(!r||r->count>8||r->code>UI_NAV_CANCELLED||r->online_status>UI_PLACE_ONLINE_KEY_ERROR)return false;
 for(unsigned i=0;i<r->count;i++){const ui_place_t *p=&r->items[i];if(!p->id||p->kind>7||!memchr(p->name,0,64)||!memchr(p->detail,0,64)||!isfinite(p->point.latitude)||!isfinite(p->point.longitude)||fabs(p->point.latitude)>85||fabs(p->point.longitude)>180||p->point.system!=UI_NAV_COORD_WGS84)return false;}
 for(unsigned i=0;i<r->count;i++){const ui_place_t *p=&r->items[i];if(p->navigation_point_valid&&(!isfinite(p->navigation_point.latitude)||!isfinite(p->navigation_point.longitude)||fabs(p->navigation_point.latitude)>85||fabs(p->navigation_point.longitude)>180||p->navigation_point.system!=UI_NAV_COORD_WGS84))return false;}
 ui_concurrency_lock();bool accept=s.busy&&!s.ready&&r->request_id==s.id;if(accept){s.pending=*r;s.ready=true;}ui_concurrency_unlock();return accept;
}
void ui_nav_search_tick(uint32_t now){ui_concurrency_lock();bool timeout=s.busy&&!s.ready&&now-s.started>=5000;if(timeout){s.pending=(ui_place_results_t){.request_id=s.id,.code=UI_NAV_TIMEOUT};s.ready=true;}uint32_t id=s.id;ui_nav_search_backend_t p=port;
 if(s.ready){s.current=s.pending;s.busy=s.ready=false;s.version++;}ui_concurrency_unlock();if(timeout&&p.cancel)p.cancel(id,p.context);}
void ui_nav_search_get(ui_place_results_t *r,uint32_t *v,bool *busy){ui_concurrency_lock();if(r)*r=s.current;if(v)*v=s.version;if(busy)*busy=s.busy;ui_concurrency_unlock();}
