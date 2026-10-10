#include "ui/core/ui_memory.h"
#include <stdatomic.h>
#include "ui_offline_map.h"
#include <math.h>
#include <string.h>
#include <limits.h>
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int32_t i32(const uint8_t *p){uint32_t n=u32(p);return n<=INT32_MAX?(int32_t)n:(int32_t)((int64_t)n-4294967296LL);}
static uint32_t crc(uint32_t c,const uint8_t *p,size_t n){while(n--){c^=*p++;for(unsigned b=0;b<8;b++)c=(c>>1)^(0xEDB88320u&-(c&1));}return c;}
static bool read_at(const ui_offline_map_t *m,uint32_t o,void *b,size_t n){return o<=m->io.size&&n<=m->io.size-o&&m->io.read_at(m->io.context,o,b,n);}
ui_nav_result_code_t ui_offline_map_open(ui_offline_map_t *m,const ui_offline_map_io_t *io)
{
 if(!m||!io||!io->read_at)return UI_NAV_NO_MAP;memset(m,0,sizeof(*m));m->io=*io;uint8_t h[128];
 if(!read_at(m,0,h,sizeof(h)))return UI_NAV_CORRUPT_MAP;
 if(memcmp(h,"BMAP0001",8)||u32(h+8)!=1)return UI_NAV_UNSUPPORTED;
 if((crc(crc(0xffffffff,h,44),h+48,80)^0xffffffff)!=u32(h+44)||!memchr(h+48,0,16)||!memchr(h+64,0,48))return UI_NAV_CORRUPT_MAP;
 memcpy(m->version_name,h+48,16);memcpy(m->name,h+64,48);
 m->revision=u32(h+12);m->west=i32(h+16);m->south=i32(h+20);m->east=i32(h+24);m->north=i32(h+28);m->cell=u32(h+32);m->cols=u32(h+36);m->rows=u32(h+40);
 if(!m->revision||!m->cell||!m->cols||!m->rows||m->cols>512||m->rows>512||m->west>=m->east||m->south>=m->north||
    m->west< -1800000000||m->east>1800000000||m->south< -900000000||m->north>900000000||
    128ull+(uint64_t)m->cols*m->rows*12>io->size||
    (int64_t)m->west+(uint64_t)m->cell*m->cols!=m->east||(int64_t)m->south+(uint64_t)m->cell*m->rows!=m->north)return UI_NAV_CORRUPT_MAP;
 return UI_NAV_OK;
}
static ui_nav_point_t point(int32_t x,int32_t y){return (ui_nav_point_t){y/1e7,x/1e7,UI_NAV_COORD_WGS84};}
static ui_nav_local_point_t local(ui_nav_point_t o,ui_nav_point_t p){return (ui_nav_local_point_t){(p.longitude-o.longitude)*111319.49*cos(o.latitude*.01745329252),(p.latitude-o.latitude)*111319.49};}
static float road_score(const ui_nav_road_t *r,ui_nav_point_t o,float radius)
{ui_nav_local_point_t a=local(o,r->a),b=local(o,r->b);float dx=b.east_m-a.east_m,dy=b.north_m-a.north_m,d=dx*dx+dy*dy,t=d?-(a.east_m*dx+a.north_m*dy)/d:0;t=fmaxf(0,fminf(1,t));return r->level*radius+hypotf(a.east_m+t*dx,a.north_m+t*dy);}
static float area_score(const ui_nav_area_t *a,float radius)
{float x=0,y=0;for(unsigned i=0;i<a->count;i++){x+=a->points[i].east_m;y+=a->points[i].north_m;}return (a->kind==UI_NAV_AREA_BUILDING?radius*3:0)+hypotf(x/a->count,y/a->count);}
static UI_LARGE_RAM float rs[UI_NAV_MAP_ROAD_MAX],as[UI_NAV_MAP_AREA_MAX],ls[UI_NAV_MAP_LABEL_MAX];
static atomic_flag s_map_busy=ATOMIC_FLAG_INIT;
static ui_nav_result_code_t read_window(const ui_offline_map_t *m,const ui_nav_map_request_t *r,ui_nav_map_frame_t *f,ui_offline_map_stats_t *stats)
{
 if(!m||!r||!f||!stats)return UI_NAV_FAILED;memset(f,0,sizeof(*f));memset(stats,0,sizeof(*stats));
 f->request_id=r->id;f->route_id=r->route_id;f->route_revision=r->route_revision;f->map_revision=m->revision;f->origin=r->center;f->meters_per_pixel=r->meters_per_pixel;
 if(r->center.system!=UI_NAV_COORD_WGS84)return f->status=UI_NAV_UNSUPPORTED;
 if(!isfinite(r->center.latitude)||!isfinite(r->center.longitude)||!isfinite(r->meters_per_pixel)||r->meters_per_pixel<.05||!r->width||!r->height||fabs(r->center.latitude)>85)return f->status=UI_NAV_FAILED;
 double mx=r->center.longitude*1e7,my=r->center.latitude*1e7;
 if(mx<m->west||mx>m->east||my<m->south||my>m->north)return f->status=UI_NAV_NO_MAP;
 double halfx=r->width*r->meters_per_pixel*.5,halfy=r->height*r->meters_per_pixel*.5;
 double dx=halfx*1e7/(111319.49*cos(r->center.latitude*.01745329252)),dy=halfy*1e7/111319.49;
 int32_t west=(int32_t)fmax(m->west,mx-dx),east=(int32_t)fmin(m->east,mx+dx),south=(int32_t)fmax(m->south,my-dy),north=(int32_t)fmin(m->north,my+dy);
 int x1=(west-m->west)/m->cell,x2=(east-m->west)/m->cell,y1=(south-m->south)/m->cell,y2=(north-m->south)/m->cell;
 if(x2>=(int)m->cols)x2=m->cols-1;if(y2>=(int)m->rows)y2=m->rows-1;
 float radius=fmax(100,hypot(halfx,halfy)),screen_mpp=fmax(r->width*r->meters_per_pixel/464,r->height*r->meters_per_pixel/540);
 unsigned area_kinds[3]={0};
 /* Reject huge requests rather than freezing a board on a whole-province read. */
 if((x2-x1+1)*(y2-y1+1)>256)return f->status=UI_NAV_UNSUPPORTED;
 for(int y=y1;y<=y2;y++)for(int x=x1;x<=x2;x++){
  uint8_t entry[12],b[116];uint32_t ti=y*m->cols+x;if(!read_at(m,128+ti*12,entry,12))goto corrupt;
  uint32_t pos=u32(entry),n=u32(entry+4),expected=u32(entry+8),c=0xffffffff;
  if(pos<128+m->cols*m->rows*12||pos>m->io.size||n>m->io.size-pos||n>8*1024*1024)goto corrupt;
  stats->tiles++;stats->bytes+=n;
  while(n){
   if(n<68||!read_at(m,pos,b,68))goto corrupt;uint8_t kind=b[0],level=b[1];unsigned count=b[2]|b[3]<<8;
   if(kind>3||level>2||(kind==0?count!=2:(count<3||count>6))||!memchr(b+20,0,48))goto corrupt;
   unsigned len=68+count*8;if(n<len||!read_at(m,pos+68,b+68,count*8))goto corrupt;c=crc(c,b,len);pos+=len;n-=len;
   if(i32(b+4)>east||i32(b+12)<west||i32(b+8)>north||i32(b+16)<south)continue;
   ui_nav_point_t p[6];for(unsigned i=0;i<count;i++){
    int32_t px=i32(b+68+i*8),py=i32(b+72+i*8);if(px< -1800000000||px>1800000000||py< -900000000||py>900000000)goto corrupt;p[i]=point(px,py);
   }
   if(kind==0){
    if((screen_mpp>12&&level>0)||(screen_mpp>5&&level>1))continue;
            ui_nav_road_t road={p[0],p[1],level};bool duplicate=false;for(unsigned i=0;i<f->road_count;i++)if(f->roads[i].a.latitude==road.a.latitude&&f->roads[i].a.longitude==road.a.longitude&&f->roads[i].b.latitude==road.b.latitude&&f->roads[i].b.longitude==road.b.longitude)duplicate=true;
    if(duplicate)continue;stats->roads_seen++;float score=road_score(&road,f->origin,radius);unsigned slot=f->road_count;
    if(slot==UI_NAV_MAP_ROAD_MAX){stats->roads_limited=true;slot=0;for(unsigned i=1;i<f->road_count;i++)if(rs[i]>rs[slot])slot=i;if(score>=rs[slot])continue;}else f->road_count++;
    f->roads[slot]=road;rs[slot]=score;
   }else{
    if(kind==1&&screen_mpp>6)continue;ui_nav_area_t area={.count=count,.kind=kind-1};for(unsigned i=0;i<count;i++)area.points[i]=local(f->origin,p[i]);
    bool duplicate=false;for(unsigned i=0;i<f->area_count;i++)if(f->areas[i].count==count&&!memcmp(f->areas[i].points,area.points,count*sizeof(area.points[0])))duplicate=true;
    if(duplicate)continue;stats->areas_seen++;float score=area_score(&area,radius);unsigned slot=f->area_count;
    unsigned quota=area.kind==UI_NAV_AREA_BUILDING?128:32;
    if(area_kinds[area.kind]>=quota){stats->areas_limited=true;slot=UI_NAV_MAP_AREA_MAX;for(unsigned i=0;i<f->area_count;i++)if(f->areas[i].kind==area.kind&&(slot==UI_NAV_MAP_AREA_MAX||as[i]>as[slot]))slot=i;if(slot==UI_NAV_MAP_AREA_MAX||score>=as[slot])continue;}
    else {if(slot==UI_NAV_MAP_AREA_MAX)continue;f->area_count++;area_kinds[area.kind]++;}
    f->areas[slot]=area;as[slot]=score;
   }
   if(b[20]&&(kind!=1)){bool duplicate=false;for(unsigned i=0;i<f->label_count;i++)if(!strcmp(f->labels[i].text,(char*)b+20))duplicate=true;
    if(!duplicate){ui_nav_local_point_t q={0};for(unsigned i=0;i<count;i++){ui_nav_local_point_t a=local(f->origin,p[i]);q.east_m+=a.east_m/count;q.north_m+=a.north_m/count;}
     if(fabsf(q.east_m)>halfx||fabsf(q.north_m)>halfy)continue;float score=hypotf(q.east_m,q.north_m)+(kind?radius*.2f:level*radius*.15f);unsigned slot=f->label_count;
     if(slot==UI_NAV_MAP_LABEL_MAX){slot=0;for(unsigned i=1;i<f->label_count;i++)if(ls[i]>ls[slot])slot=i;if(score>=ls[slot])continue;}else f->label_count++;
     ui_nav_map_label_t *l=&f->labels[slot];l->point=q;l->kind=kind?kind-1:0;memcpy(l->text,b+20,48);ls[slot]=score;}}
  }
  if((c^0xffffffff)!=expected)goto corrupt;
 }
 /* Paint local roads first and main roads last. Areas before roads in renderer. */
 for(unsigned i=0;i<f->road_count;i++)for(unsigned j=i+1;j<f->road_count;j++)if(f->roads[j].level>f->roads[i].level){ui_nav_road_t t=f->roads[i];f->roads[i]=f->roads[j];f->roads[j]=t;}
 for(unsigned i=0;i<f->area_count;i++)for(unsigned j=i+1;j<f->area_count;j++)if(f->areas[j].kind>f->areas[i].kind){ui_nav_area_t t=f->areas[i];f->areas[i]=f->areas[j];f->areas[j]=t;}
 return f->status=UI_NAV_OK;
corrupt:f->road_count=f->area_count=f->label_count=0;return f->status=UI_NAV_CORRUPT_MAP;
}

ui_nav_result_code_t ui_offline_map_read(const ui_offline_map_t *map,const ui_nav_map_request_t *request,ui_nav_map_frame_t *frame,ui_offline_map_stats_t *stats)
{
 if(atomic_flag_test_and_set_explicit(&s_map_busy,memory_order_acquire))return UI_NAV_UNAVAILABLE;
 ui_nav_result_code_t result=read_window(map,request,frame,stats);
 atomic_flag_clear_explicit(&s_map_busy,memory_order_release);return result;
}
