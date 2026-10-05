#include "ui_pc_offline_navigation.h"
#include "services/ui_offline_route.h"
#include "services/ui_offline_tiles.h"
#include "services/ui_offline_graph.h"
#include "services/ui_nav_raster.h"
#include "services/ui_nav_store.h"
#include "services/ui_nav_search.h"
#include "ui/core/ui_concurrency.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#define PC_TILE_CACHE_SLOTS 24
typedef struct {FILE *file;uint8_t buffer[65536];uint32_t start,count;} file_io_t;
static bool read_file(void *ctx,uint32_t offset,void *bytes,size_t count)
{
    file_io_t *f=ctx;uint8_t *p=bytes;
    while(count){
        if(offset<f->start||offset>=f->start+f->count){
            if(fseek(f->file,offset,SEEK_SET)!=0)return false;
            f->start=offset;f->count=fread(f->buffer,1,sizeof(f->buffer),f->file);if(!f->count)return false;
        }
        size_t n=f->start+f->count-offset;if(n>count)n=count;
        memcpy(p,f->buffer+offset-f->start,n);p+=n;offset+=n;count-=n;
    }
    return true;
}
static bool open_file(const char *path,file_io_t *f,ui_offline_map_io_t *io)
{
    memset(f,0,sizeof(*f));f->file=fopen(path,"rb");if(!f->file)return false;
    if(fseek(f->file,0,SEEK_END)){fclose(f->file);f->file=NULL;return false;}
    long n=ftell(f->file);if(n<0||(unsigned long)n>UINT32_MAX){fclose(f->file);f->file=NULL;return false;}
    *io=(ui_offline_map_io_t){read_file,(uint32_t)n,f};return true;
}
static struct {
    char map_path[512],graph_path[512],places_path[512],map_date[16],map_name[48];
    ui_offline_places_t places;bool places_verified,search_running;ui_nav_search_request_t search_request;
    ui_offline_route_t route,base_route;
    ui_offline_tiles_t tiles;ui_offline_graph_t graph;
    file_io_t tile_file;ui_offline_tile_cache_t cache;
    uint16_t *cache_pixels,*raster[2];
    ui_nav_result_code_t map_status,route_status,tile_status,graph_status;
    uint32_t map_size,revision,session,producer,epoch,route_serial;
    float travelled,multiplier;uint32_t model_elapsed;
    bool pending,used,stopping,plan_ready,planning;
    ui_nav_request_t request;ui_nav_result_t plan_result;ui_offline_route_t *planned;
    unsigned workers,map_workers;ui_offline_map_stats_t stats;
    ui_pc_offline_performance_t performance;
} s;
static double milliseconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return (double)t.QuadPart*1000/f.QuadPart;
#else
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000.+t.tv_nsec/1e6;
#endif
}
static ui_nav_item_t route_item(const ui_offline_route_t *route)
{
    ui_nav_item_t i={.id=route->id,.revision=route->revision,.point=route->points[route->count?route->count-1:0].position,
        .distance_m=(uint32_t)route->distance_m,.distance_valid=route->count>1,
        .duration_s=(uint32_t)(route->distance_m/5),.duration_valid=route->count>1,
        .start_allowed=route->count>1,.saved=route->id==1,.coverage_known=true,.coverage_complete=s.map_status==UI_NAV_OK};
    snprintf(i.name,sizeof(i.name),"%s",route->name);snprintf(i.detail,sizeof(i.detail),route->id==1?"西安城区骑行路线":"按所选目的地规划的骑行路线");
    if(route->count){i.thumbnail_count=16;for(unsigned k=0;k<16;k++)i.thumbnail[k]=route->points[k*(route->count-1)/15].position;}
    return i;
}
static ui_nav_item_t map_item(void)
{
    ui_nav_item_t i={.id=100,.revision=s.revision,.size_kib=(s.map_size+1023)/1024,.size_valid=s.map_size>0,.saved=true,
        .map_status=s.map_status==UI_NAV_OK?UI_NAV_MAP_READY:s.map_status==UI_NAV_NO_MAP?UI_NAV_MAP_MISSING:UI_NAV_MAP_CORRUPT};
    snprintf(i.name,sizeof(i.name),"%s",s.map_name[0]?s.map_name:"西安城区基础地图");
    snprintf(i.version_name,sizeof(i.version_name),"%s",s.map_date[0]?s.map_date:"未知");
    snprintf(i.detail,sizeof(i.detail),"(c) OpenStreetMap contributors");snprintf(i.coverage,sizeof(i.coverage),"西安城区 · 彩色离线地图");return i;
}
typedef struct {bool plan;ui_nav_request_t command;ui_nav_point_t start;ui_nav_map_request_t map;ui_offline_route_t route;} job_t;
static bool cancelled(void *context)
{
    job_t *job=context;ui_concurrency_lock();bool stop=s.stopping||!s.pending||s.request.id!=job->command.id;ui_concurrency_unlock();return stop;
}
static void plan_worker(job_t *job)
{
    double begin=milliseconds();ui_nav_result_t result={.request_id=job->command.id,.code=UI_NAV_NO_ROUTE};
    ui_offline_route_t *route=calloc(1,sizeof(*route));file_io_t file;ui_offline_map_io_t io;ui_offline_graph_stats_t stats={0};
    if(route&&open_file(s.graph_path,&file,&io)){
        ui_offline_graph_t graph=s.graph;graph.io=io;
        if(io.size!=s.graph.io.size)result.code=UI_NAV_CORRUPT_MAP;
        else {
            size_t bytes=ui_offline_graph_workspace_size(&graph);void *workspace=malloc(bytes);
            result.code=workspace?ui_offline_graph_plan(&graph,job->start,job->command.destination.point,workspace,bytes,route,cancelled,job,&stats):UI_NAV_FAILED;free(workspace);
        }
        fclose(file.file);
    }
    if(result.code==UI_NAV_OK&&job->command.destination.name[0])snprintf(route->name,sizeof(route->name),"%s",job->command.destination.name);
    ui_concurrency_lock();s.performance.planning_ms=(float)(milliseconds()-begin);s.performance.planning_expanded=stats.expanded;
    s.performance.planning_workspace_bytes=(uint32_t)ui_offline_graph_workspace_size(&s.graph);
    if(!s.stopping&&s.pending&&s.request.id==job->command.id){s.plan_result=result;s.planned=route;route=NULL;s.plan_ready=true;}
    s.workers--;ui_concurrency_unlock();free(route);free(job);
}
static void map_worker(job_t *job)
{
    double begin=milliseconds();ui_nav_map_request_t r=job->map;
    ui_nav_map_frame_t *frame=calloc(1,sizeof(*frame));ui_offline_map_stats_t stats={0};ui_offline_tiles_stats_t raster_stats={0};
    unsigned slot=0;bool acquired=false;uint16_t *pixels=NULL;
    if(frame){
        ui_nav_result_code_t code=UI_NAV_NO_MAP;
        if(r.purpose==UI_NAV_MAP_OVERVIEW&&r.route_id==job->route.id&&job->route.count>1){
            double west=180,east=-180,south=90,north=-90;
            for(unsigned i=0;i<job->route.count;i++){ui_nav_point_t p=job->route.points[i].position;west=fmin(west,p.longitude);east=fmax(east,p.longitude);south=fmin(south,p.latitude);north=fmax(north,p.latitude);}
            r.center=(ui_nav_point_t){(north+south)/2,(west+east)/2,UI_NAV_COORD_WGS84};
            r.meters_per_pixel=fmax(.75,fmax((east-west)*92000/(464-100),(north-south)*111319/(540-100)));
        }
        if(s.tile_status==UI_NAV_OK){
            acquired=ui_nav_raster_begin(r.id,&slot,&pixels);code=acquired?ui_offline_tiles_window(&s.tiles,&s.cache,&r,pixels,frame,&raster_stats):UI_NAV_UNAVAILABLE;
            if(code==UI_NAV_OK)ui_nav_raster_publish(slot,r.id);
        }else if(s.tile_status!=UI_NAV_NO_MAP)code=s.tile_status;
        else {
            file_io_t input;ui_offline_map_io_t io;ui_offline_map_t map;
            if(open_file(s.map_path,&input,&io)){code=ui_offline_map_open(&map,&io);if(code==UI_NAV_OK)code=ui_offline_map_read(&map,&r,frame,&stats);fclose(input.file);}
        }
        frame->request_id=r.id;frame->route_id=r.route_id;frame->route_revision=r.route_revision;frame->status=code;
        if(code==UI_NAV_OK){
            ui_offline_route_window(&job->route,frame);
            if(r.purpose==UI_NAV_MAP_OVERVIEW&&job->route.count>UI_NAV_WINDOW_POINT_MAX){
                frame->point_count=UI_NAV_WINDOW_POINT_MAX;
                for(unsigned k=0;k<UI_NAV_WINDOW_POINT_MAX;k++)frame->points[k]=job->route.points[k*(job->route.count-1)/(UI_NAV_WINDOW_POINT_MAX-1)].position;
            }
        }
        bool accepted=ui_nav_map_provider_submit(frame);if(acquired&&(!accepted||code!=UI_NAV_OK))ui_nav_raster_abort(slot);free(frame);
    }
    ui_concurrency_lock();s.stats=stats;s.performance.map_ms=(float)(milliseconds()-begin);
    s.performance.read_bytes=raster_stats.bytes;s.performance.cache_hits=raster_stats.hits;s.performance.cache_misses=raster_stats.misses;
    s.performance.tiles=raster_stats.tiles;s.performance.map_requests++;s.workers--;s.map_workers--;ui_concurrency_unlock();free(job);
}
#ifdef _WIN32
static DWORD WINAPI worker(void *p){job_t *job=p;if(job->plan)plan_worker(job);else map_worker(job);return 0;}
#else
static void *worker(void *p){job_t *job=p;if(job->plan)plan_worker(job);else map_worker(job);return NULL;}
#endif
static bool launch(job_t *job)
{
#ifdef _WIN32
    HANDLE thread=CreateThread(NULL,0,worker,job,0,NULL);if(!thread)return false;CloseHandle(thread);
#else
    pthread_t thread;if(pthread_create(&thread,NULL,worker,job))return false;pthread_detach(thread);
#endif
    return true;
}
static bool search_cancelled(void *context)
{uint32_t id=*(uint32_t *)context;ui_concurrency_lock();bool stop=s.stopping||s.search_request.id!=id;ui_concurrency_unlock();return stop;}
#ifdef _WIN32
static DWORD WINAPI search_worker(void *unused)
#else
static void *search_worker(void *unused)
#endif
{
 (void)unused;uint32_t handled=0;
 for(;;){
  ui_concurrency_lock();ui_nav_search_request_t r=s.search_request;
  if(s.stopping||!r.id||r.id==handled){s.search_running=false;s.workers--;ui_concurrency_unlock();break;}ui_concurrency_unlock();
  file_io_t file;ui_offline_map_io_t io;ui_place_results_t out={.code=UI_NAV_NO_MAP};
  if(open_file(s.places_path,&file,&io)){
   ui_offline_places_t places=s.places;
   if(!s.places_verified){out.code=ui_offline_places_open(&places,&io);if(out.code==UI_NAV_OK){s.places=places;s.places.io.context=NULL;s.places_verified=true;}}
   else {out.code=io.size==places.io.size?UI_NAV_OK:UI_NAV_CORRUPT_MAP;places.io=io;}
   if(out.code==UI_NAV_OK)ui_offline_places_query(&places,r.query,r.center,r.center_valid,&out,search_cancelled,&r.id,NULL);
   fclose(file.file);
  }
  out.request_id=r.id;ui_nav_search_submit(&out);handled=r.id;
 }
 return 0;
}
static bool search_request(const ui_nav_search_request_t *r,void *context)
{
 (void)context;ui_concurrency_lock();if(s.stopping){ui_concurrency_unlock();return false;}
 s.search_request=*r;if(s.search_running){ui_concurrency_unlock();return true;}s.search_running=true;s.workers++;ui_concurrency_unlock();
#ifdef _WIN32
 HANDLE thread=CreateThread(NULL,0,search_worker,NULL,0,NULL);if(thread){CloseHandle(thread);return true;}
#else
 pthread_t thread;if(!pthread_create(&thread,NULL,search_worker,NULL)){pthread_detach(thread);return true;}
#endif
 ui_concurrency_lock();s.search_running=false;s.workers--;ui_concurrency_unlock();return false;
}
static void search_cancel(uint32_t id,void *context)
{(void)context;ui_concurrency_lock();if(s.search_request.id==id)s.search_request.id=0;ui_concurrency_unlock();}
static bool dispatch(const ui_nav_request_t *r,void *context)
{
    (void)context;ui_concurrency_lock();s.request=*r;s.pending=true;s.plan_ready=false;ui_concurrency_unlock();
    if(r->kind!=UI_NAV_REQ_PLAN&&r->kind!=UI_NAV_REQ_REROUTE)return true;
    ui_nav_session_t session;ui_nav_session_get(&session);
    if(!session.position_valid||s.graph_status!=UI_NAV_OK){s.plan_result=(ui_nav_result_t){.request_id=r->id,.code=!session.position_valid?UI_NAV_NO_POSITION:s.graph_status};s.plan_ready=true;s.planning=true;return true;}
    job_t *job=calloc(1,sizeof(*job));if(!job)return false;job->plan=true;job->command=*r;job->start=session.position;
    if(r->kind==UI_NAV_REQ_REROUTE)job->command.destination=session.destination;
    ui_concurrency_lock();s.workers++;s.planning=true;ui_concurrency_unlock();if(launch(job))return true;
    ui_concurrency_lock();s.workers--;s.pending=s.planning=false;ui_concurrency_unlock();free(job);return false;
}
static void cancel(uint32_t id,void *context)
{(void)context;ui_concurrency_lock();if(s.request.id==id){s.pending=false;s.planning=false;free(s.planned);s.planned=NULL;s.plan_ready=false;}ui_concurrency_unlock();}
static void complete(void)
{
    ui_nav_request_t request;ui_nav_result_t result;ui_concurrency_lock();request=s.request;result=(ui_nav_result_t){.request_id=request.id,.code=UI_NAV_OK};
    if(s.planning){
        if(!s.plan_ready){ui_concurrency_unlock();return;}result=s.plan_result;
        if(result.code==UI_NAV_OK&&s.planned){s.route=*s.planned;s.route.revision=++s.route_serial;result.route=route_item(&s.route);s.route_status=UI_NAV_OK;s.travelled=0;}
        free(s.planned);s.planned=NULL;s.plan_ready=s.planning=s.pending=false;ui_concurrency_unlock();ui_nav_session_complete(&result);
        if(request.kind==UI_NAV_REQ_REROUTE&&result.code==UI_NAV_OK){ui_nav_session_t q;ui_nav_session_get(&q);q.route=result.route;q.state=UI_NAV_SESSION_ACTIVE;q.version=++s.producer;ui_nav_session_submit(&q);}return;
    }
    s.pending=false;ui_concurrency_unlock();
    switch(request.kind){
    case UI_NAV_REQ_LIST:case UI_NAV_REQ_MAP_CHECK:
        result.catalog.kind=request.list;result.catalog.offset=request.offset;
        if(request.list==UI_NAV_LIST_MAPS||request.list==UI_NAV_LIST_MAP_VERSIONS||request.list==UI_NAV_LIST_STORAGE){result.catalog.total=1;if(request.offset==0){result.catalog.count=1;result.catalog.items[0]=map_item();}}
        else if(request.list==UI_NAV_LIST_ROUTES||request.list==UI_NAV_LIST_RECENT_ROUTES){
            bool include_base=s.base_route.count>1&&(request.list!=UI_NAV_LIST_RECENT_ROUTES||s.used);
            result.catalog.total=include_base?1:0;if(s.route.id==2&&s.route_status==UI_NAV_OK)result.catalog.total++;
            for(uint32_t i=request.offset;i<result.catalog.total&&result.catalog.count<UI_NAV_CATALOG_MAX;i++)result.catalog.items[result.catalog.count++]=route_item(i==0&&include_base?&s.base_route:&s.route);
        }
        if(request.offset>result.catalog.total)result.catalog.offset=result.catalog.total;break;
    case UI_NAV_REQ_LOAD_ROUTE:
        if(request.destination.id==1&&s.base_route.count>1){s.route=s.base_route;s.route_status=UI_NAV_OK;result.route=route_item(&s.route);}
        else if(request.destination.id==s.route.id&&s.route_status==UI_NAV_OK)result.route=route_item(&s.route);else result.code=UI_NAV_NO_ROUTE;break;
    case UI_NAV_REQ_START:
        if(s.route_status!=UI_NAV_OK)result.code=UI_NAV_NO_ROUTE;else{result.session_id=++s.session;s.travelled=0;s.used=true;s.model_elapsed=0;}break;
    case UI_NAV_REQ_STOP:break;
    default:result.code=UI_NAV_UNSUPPORTED;break;
    }
    ui_nav_session_complete(&result);
}
static void tick(uint32_t elapsed,void *context)
{
    (void)context;ui_concurrency_lock();bool pending=s.pending;ui_concurrency_unlock();if(pending){complete();return;}
    ui_nav_session_t q;ui_nav_session_get(&q);if(s.route_status!=UI_NAV_OK)return;
    if(q.state==UI_NAV_SESSION_ACTIVE||q.state==UI_NAV_SESSION_ARRIVED){
        s.travelled=fminf(s.route.distance_m,s.travelled+elapsed*.005f*s.multiplier);s.model_elapsed+=elapsed;if(s.model_elapsed<500)return;
        s.model_elapsed=0;nav_model_t m;ui_offline_route_replay(&s.route,s.travelled,q.id,ui_nav_session_now(),&m);
        time_t now=s.epoch?s.epoch+(uint32_t)(s.travelled/5):time(NULL),eta=now+(time_t)((s.route.distance_m-s.travelled)/5);
        struct tm *t=localtime(&eta);if(t){m.eta_valid=true;m.eta_hour=t->tm_hour;m.eta_min=t->tm_min;}
        ui_navigation_service_submit(&m);q.position_valid=true;q.position=(ui_nav_point_t){m.latitude,m.longitude,UI_NAV_COORD_WGS84};q.source=UI_NAV_SOURCE_REPLAY;
        q.position_sampled_at_ms=m.sampled_at_ms;q.position_stale_after_ms=2500;q.elapsed_valid=q.travelled_valid=true;
        q.elapsed_s=(uint32_t)(s.travelled/5);q.travelled_m=(uint32_t)s.travelled;q.ascent_valid=false;if(s.travelled>=s.route.distance_m)q.state=UI_NAV_SESSION_ARRIVED;
        q.version=++s.producer;ui_nav_session_submit(&q);
    }else if(!q.position_valid&&s.base_route.count){q.position=s.base_route.points[0].position;q.position_valid=true;q.source=UI_NAV_SOURCE_REPLAY;q.position_sampled_at_ms=ui_nav_session_now();q.position_stale_after_ms=0;q.version=++s.producer;ui_nav_session_submit(&q);}
}
static bool request_map(const ui_nav_map_request_t *request,void *context)
{
    (void)context;ui_concurrency_lock();bool allowed=!s.stopping&&s.map_workers==0;if(allowed){s.workers++;s.map_workers++;}ui_concurrency_unlock();if(!allowed)return false;
    job_t *job=calloc(1,sizeof(*job));if(job){job->map=*request;job->route=s.route;if(launch(job))return true;free(job);}
    ui_concurrency_lock();s.workers--;s.map_workers--;ui_concurrency_unlock();return false;
}
bool ui_pc_offline_navigation_register(const char *directory)
{
    if(!directory)return false;ui_pc_offline_navigation_shutdown();memset(&s,0,sizeof(s));s.multiplier=1;
    char path[512];snprintf(s.map_path,sizeof(s.map_path),"%s/xian-urban.bmap",directory);snprintf(s.graph_path,sizeof(s.graph_path),"%s/xian-cycle.bgraph",directory);snprintf(s.places_path,sizeof(s.places_path),"%s/xian-places.bpoi",directory);
    file_io_t input;ui_offline_map_io_t io;ui_offline_map_t map;s.map_status=UI_NAV_NO_MAP;s.route_status=UI_NAV_NO_ROUTE;s.graph_status=UI_NAV_NO_ROUTE;s.tile_status=UI_NAV_NO_MAP;
    if(open_file(s.map_path,&input,&io)){s.map_status=ui_offline_map_open(&map,&io);s.map_size=io.size;if(s.map_status==UI_NAV_OK){s.revision=map.revision;memcpy(s.map_date,map.version_name,16);memcpy(s.map_name,map.name,48);}fclose(input.file);}
    snprintf(path,sizeof(path),"%s/xian-city.broute",directory);if(open_file(path,&input,&io)){s.route_status=ui_offline_route_open(&s.base_route,&io);s.route=s.base_route;fclose(input.file);}
    snprintf(path,sizeof(path),"%s/xian-color.btiles",directory);
    if(open_file(path,&s.tile_file,&io)){
        s.tile_status=ui_offline_tiles_open(&s.tiles,&io);
        if(s.tile_status==UI_NAV_OK){s.cache_pixels=malloc(PC_TILE_CACHE_SLOTS*256*256*2);s.raster[0]=malloc(768*768*2);s.raster[1]=malloc(768*768*2);
            if(!s.cache_pixels||!s.raster[0]||!s.raster[1])s.tile_status=UI_NAV_UNAVAILABLE;
            else{ui_offline_tile_cache_init(&s.cache,s.cache_pixels,PC_TILE_CACHE_SLOTS);ui_nav_raster_configure(s.raster[0],s.raster[1],768*768);s.map_status=UI_NAV_OK;s.map_size=io.size;}}
    }
    if(open_file(s.graph_path,&input,&io)){s.graph_status=ui_offline_graph_open(&s.graph,&io);s.graph.io.context=NULL;fclose(input.file);}
    s.route_serial=s.graph.revision>s.route.revision?s.graph.revision:s.route.revision;
    uint32_t capabilities=UI_NAV_CAP_MAPS|(s.route_status==UI_NAV_OK?UI_NAV_CAP_ROUTES:0);if(s.graph_status==UI_NAV_OK)capabilities|=UI_NAV_CAP_PLAN|UI_NAV_CAP_REROUTE;
    ui_nav_store_set_capabilities(capabilities);ui_navigation_service_register_demo_step(NULL);ui_navigation_service_set_demo_enabled(true);
    ui_nav_backend_t port={dispatch,cancel,tick,NULL};ui_nav_map_backend_t maps={.request=request_map};ui_nav_session_register_backend(&port);ui_nav_map_provider_register(&maps);
    ui_nav_search_backend_t search={search_request,search_cancel,NULL};ui_nav_search_register(&search);
    return s.map_status==UI_NAV_OK&&s.route_status==UI_NAV_OK;
}
void ui_pc_offline_navigation_set_replay_speed(float multiplier){if(isfinite(multiplier)&&multiplier>=0&&multiplier<=100)s.multiplier=multiplier;}
void ui_pc_offline_navigation_set_epoch(uint32_t epoch){s.epoch=epoch;}
bool ui_pc_offline_navigation_stats(ui_offline_map_stats_t *stats)
{if(!stats)return false;ui_concurrency_lock();*stats=s.stats;bool idle=!s.workers;ui_concurrency_unlock();return idle;}
bool ui_pc_offline_navigation_performance(ui_pc_offline_performance_t *out)
{if(!out)return false;ui_concurrency_lock();*out=s.performance;bool idle=!s.workers;ui_concurrency_unlock();return idle;}
void ui_pc_offline_navigation_shutdown(void)
{
    ui_concurrency_lock();s.stopping=true;ui_concurrency_unlock();for(;;){ui_concurrency_lock();unsigned n=s.workers;ui_concurrency_unlock();if(!n)break;
#ifdef _WIN32
        Sleep(1);
#else
        struct timespec t={0,1000000};nanosleep(&t,NULL);
#endif
    }
    ui_nav_raster_configure(NULL,NULL,0);free(s.cache_pixels);s.cache_pixels=NULL;for(unsigned i=0;i<2;i++){free(s.raster[i]);s.raster[i]=NULL;}
    free(s.planned);s.planned=NULL;if(s.tile_file.file){fclose(s.tile_file.file);s.tile_file.file=NULL;}
    ui_nav_search_register(NULL);
}
