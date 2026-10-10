#include "ui_offline_graph.h"
#include "ui_offline_tiles.h"
#include <math.h>
#include <string.h>
#include <float.h>
#include <stdio.h>
static uint32_t u32(const uint8_t *b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static float f32(const uint8_t *b){uint32_t u=u32(b);float f;memcpy(&f,&u,4);return f;}
static bool read_at(const ui_offline_graph_t *g,uint32_t offset,void *p,size_t n,ui_offline_graph_stats_t *stats)
{if(offset>g->io.size||n>g->io.size-offset)return false;if(stats)stats->bytes+=n;return g->io.read_at(g->io.context,offset,p,n);}
static double distance(ui_nav_point_t a,ui_nav_point_t b)
{return hypot((a.longitude-b.longitude)*111319.49*cos((a.latitude+b.latitude)*.00872664626),(a.latitude-b.latitude)*111319.49);}
typedef struct {ui_nav_point_t point;uint32_t first,count;} node_t;
static bool node(const ui_offline_graph_t *g,uint32_t id,node_t *n,ui_offline_graph_stats_t *stats)
{
    uint8_t b[16];if(id>=g->nodes||!read_at(g,g->node_offset+id*16,b,16,stats))return false;
    n->point=(ui_nav_point_t){(int32_t)u32(b+4)/1e7,(int32_t)u32(b)/1e7,UI_NAV_COORD_WGS84};n->first=u32(b+8);n->count=u32(b+12);
    return n->first<=g->edges&&n->count<=g->edges-n->first&&fabs(n->point.latitude)<=85&&fabs(n->point.longitude)<=180;
}
ui_nav_result_code_t ui_offline_graph_open(ui_offline_graph_t *g,const ui_offline_map_io_t *io)
{
    if(!g||!io||!io->read_at)return UI_NAV_NO_ROUTE;memset(g,0,sizeof(*g));uint8_t h[128];
    if(io->size<128||!io->read_at(io->context,0,h,128))return UI_NAV_NO_ROUTE;
    if(memcmp(h,"BGRF0001",8)||u32(h+8)!=1)return UI_NAV_UNSUPPORTED;
    uint32_t c=ui_offline_crc32(0xffffffff,h,60);c=ui_offline_crc32(c,h+64,64)^0xffffffff;
    if(c!=u32(h+60))return UI_NAV_CORRUPT_MAP;
    g->io=*io;g->revision=u32(h+12);g->nodes=u32(h+16);g->edges=u32(h+20);g->names=u32(h+24);g->node_offset=u32(h+28);g->edge_offset=u32(h+32);
    g->west=(int32_t)u32(h+36);g->south=(int32_t)u32(h+40);g->east=(int32_t)u32(h+44);g->north=(int32_t)u32(h+48);g->name_offset=u32(h+52);
    if(!g->nodes||g->nodes>500000||!g->edges||g->edges>4000000||!g->names||g->node_offset!=128||
       g->west>=g->east||g->south>=g->north||g->edge_offset!=128+g->nodes*16||g->name_offset!=g->edge_offset+g->edges*16||
       g->name_offset>io->size||g->names>(io->size-g->name_offset)/48||io->size!=g->name_offset+g->names*48)return UI_NAV_CORRUPT_MAP;
    uint8_t *b=g->validation_workspace;uint32_t offset=128;c=0xffffffff;
    while(offset<io->size){size_t n=io->size-offset;if(n>sizeof(g->validation_workspace))n=sizeof(g->validation_workspace);if(!io->read_at(io->context,offset,b,n))return UI_NAV_CORRUPT_MAP;c=ui_offline_crc32(c,b,n);offset+=n;}
    return (c^0xffffffff)==u32(h+56)?UI_NAV_OK:UI_NAV_CORRUPT_MAP;
}
size_t ui_offline_graph_workspace_size(const ui_offline_graph_t *g){return g?(size_t)g->nodes*24:0;}
static void heap_up(uint32_t *heap,uint32_t *pos,const float *priority,uint32_t index)
{while(index){uint32_t parent=(index-1)/2;if(priority[heap[parent]]<=priority[heap[index]])break;uint32_t swap=heap[parent];heap[parent]=heap[index];heap[index]=swap;pos[heap[parent]]=parent;pos[heap[index]]=index;index=parent;}}
static uint32_t heap_pop(uint32_t *heap,uint32_t *pos,const float *priority,uint32_t *count)
{
    uint32_t result=heap[0];pos[result]=UINT32_MAX;
    if(--*count){heap[0]=heap[*count];pos[heap[0]]=0;uint32_t p=0;
        for(;;){uint32_t a=p*2+1;if(a>=*count)break;uint32_t b=a+1,k=b<*count&&priority[heap[b]]<priority[heap[a]]?b:a;
            if(priority[heap[p]]<=priority[heap[k]])break;uint32_t v=heap[p];heap[p]=heap[k];heap[k]=v;pos[heap[p]]=p;pos[heap[k]]=k;p=k;}}
    return result;
}
ui_nav_result_code_t ui_offline_graph_plan(const ui_offline_graph_t *g,ui_nav_point_t start,ui_nav_point_t end,
    void *workspace,size_t bytes,ui_offline_route_t *route,bool (*cancelled)(void *),void *context,ui_offline_graph_stats_t *stats)
{
    if(!g||!workspace||!route||bytes<ui_offline_graph_workspace_size(g)||(uintptr_t)workspace%4)return UI_NAV_FAILED;
    memset(route,0,sizeof(*route));ui_offline_graph_stats_t local={0};if(!stats)stats=&local;memset(stats,0,sizeof(*stats));
    if(start.system!=UI_NAV_COORD_WGS84||end.system!=UI_NAV_COORD_WGS84)return UI_NAV_UNSUPPORTED;
    if(!isfinite(start.latitude)||!isfinite(start.longitude)||!isfinite(end.latitude)||!isfinite(end.longitude))return UI_NAV_FAILED;
    if(start.longitude*1e7<g->west||start.longitude*1e7>g->east||start.latitude*1e7<g->south||start.latitude*1e7>g->north)return UI_NAV_NO_POSITION;
    if(end.longitude*1e7<g->west||end.longitude*1e7>g->east||end.latitude*1e7<g->south||end.latitude*1e7>g->north)return UI_NAV_NO_MAP;
    float *cost=workspace,*priority=cost+g->nodes;uint32_t *parent=(uint32_t *)(priority+g->nodes),*names=parent+g->nodes,*heap=names+g->nodes,*pos=heap+g->nodes;
    uint32_t from=0,to=0;double best_start=DBL_MAX,best_end=DBL_MAX;
    for(uint32_t i=0;i<g->nodes;i++){
        if((i&1023)==0&&cancelled&&cancelled(context)){stats->cancelled=true;return UI_NAV_CANCELLED;}
        node_t n;if(!node(g,i,&n,stats))return UI_NAV_CORRUPT_MAP;
        double a=distance(start,n.point),b=distance(end,n.point);
        if(n.count&&a<best_start){best_start=a;from=i;}if(b<best_end){best_end=b;to=i;}
        cost[i]=priority[i]=FLT_MAX;parent[i]=names[i]=pos[i]=UINT32_MAX;
    }
    stats->start_snap_m=best_start;stats->end_snap_m=best_end;
    if(best_start>180)return UI_NAV_NO_POSITION;if(best_end>180||from==to)return UI_NAV_NO_ROUTE;
    node_t target;if(!node(g,to,&target,stats))return UI_NAV_CORRUPT_MAP;
    uint32_t count=1;heap[0]=from;pos[from]=0;cost[from]=priority[from]=0;
    while(count){
        if((stats->expanded&127)==0&&cancelled&&cancelled(context)){stats->cancelled=true;return UI_NAV_CANCELLED;}
        uint32_t current=heap_pop(heap,pos,priority,&count);if(current==to)break;
        stats->expanded++;node_t n;if(!node(g,current,&n,stats))return UI_NAV_CORRUPT_MAP;
        for(uint32_t i=0;i<n.count;i++){
            uint8_t b[16];if(!read_at(g,g->edge_offset+(n.first+i)*16,b,16,stats))return UI_NAV_CORRUPT_MAP;
            uint32_t next=u32(b),name=u32(b+12);float length=f32(b+4),weight=f32(b+8);
            if(next>=g->nodes||name>=g->names||!isfinite(length)||!isfinite(weight)||length<=0||weight<length||weight>100000)return UI_NAV_CORRUPT_MAP;
            float proposed=cost[current]+weight;if(proposed>=cost[next])continue;
            node_t v;if(!node(g,next,&v,stats))return UI_NAV_CORRUPT_MAP;
            cost[next]=proposed;priority[next]=proposed+(float)distance(v.point,target.point)*.99f;parent[next]=current;names[next]=name;
            if(pos[next]==UINT32_MAX){if(count>=g->nodes)return UI_NAV_FAILED;pos[next]=count;heap[count++]=next;}
            heap_up(heap,pos,priority,pos[next]);
        }
    }
    if(parent[to]==UINT32_MAX)return UI_NAV_NO_ROUTE;
    /* Reuse the now-idle heap to reconstruct the exact road path. */
    uint32_t path_count=0,current=to;
    while(current!=from){if(path_count>=g->nodes)return UI_NAV_CORRUPT_MAP;heap[path_count++]=current;current=parent[current];if(current==UINT32_MAX)return UI_NAV_CORRUPT_MAP;}
    heap[path_count++]=from;if(path_count>UI_OFFLINE_ROUTE_POINT_MAX)return UI_NAV_UNSUPPORTED;
    route->id=2;route->revision=g->revision;route->count=path_count;snprintf(route->name,sizeof(route->name),"地图选点路线");
    for(uint32_t i=0;i<path_count;i++){
        node_t n;uint32_t id=heap[path_count-1-i];if(!node(g,id,&n,stats))return UI_NAV_CORRUPT_MAP;
        ui_offline_route_point_t *p=&route->points[i];p->position=n.point;
        p->cumulative_m=i?route->points[i-1].cumulative_m+(float)distance(route->points[i-1].position,p->position):0;
        uint32_t next_name=names[heap[i+1<path_count?path_count-2-i:path_count-1-i]];
        if(next_name==UINT32_MAX)next_name=0;
        if(!read_at(g,g->name_offset+next_name*48,p->road,48,stats)||!memchr(p->road,0,48))return UI_NAV_CORRUPT_MAP;
    }
    route->distance_m=route->points[path_count-1].cumulative_m;return UI_NAV_OK;
}
