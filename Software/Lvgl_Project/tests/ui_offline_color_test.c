#include "services/ui_offline_tiles.h"
#include "services/ui_offline_graph.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {FILE *file;uint32_t flip;bool corrupt;} input_t;
static bool read_at(void *context,uint32_t offset,void *bytes,size_t count)
{
    input_t *f=context;if(fseek(f->file,offset,SEEK_SET)||fread(bytes,1,count,f->file)!=count)return false;
    if(f->corrupt&&f->flip>=offset&&f->flip-offset<count)((uint8_t *)bytes)[f->flip-offset]^=1;return true;
}
static ui_offline_map_io_t open(input_t *in,const char *name)
{memset(in,0,sizeof(*in));in->file=fopen(name,"rb");assert(in->file);fseek(in->file,0,SEEK_END);long bytes=ftell(in->file);assert(bytes>0);return (ui_offline_map_io_t){read_at,(uint32_t)bytes,in};}
static bool cancel_now(void *context){(void)context;return true;}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(void)
{
    input_t file;ui_offline_map_io_t io=open(&file,"assets/maps/xian/xian-color.btiles");ui_offline_tiles_t tiles;
    assert(ui_offline_tiles_open(&tiles,&io)==UI_NAV_OK);file.corrupt=true;file.flip=24;
    assert(ui_offline_tiles_open(&tiles,&io)==UI_NAV_CORRUPT_MAP);file.corrupt=false;assert(ui_offline_tiles_open(&tiles,&io)==UI_NAV_OK);
    uint16_t *pixels=malloc(768*768*2),*cached=malloc(24*256*256*2);ui_nav_map_frame_t *frame=malloc(sizeof(*frame));assert(pixels&&cached&&frame);
    ui_offline_tile_cache_t cache;ui_offline_tile_cache_init(&cache,cached,24);ui_offline_tiles_stats_t cold,warm;
    ui_nav_map_request_t r={.id=12,.center={34.261,108.944,UI_NAV_COORD_WGS84},.meters_per_pixel=3,.width=464,.height=540};
    clock_t started=clock();assert(ui_offline_tiles_window(&tiles,&cache,&r,pixels,frame,&cold)==UI_NAV_OK);double cold_ms=(clock()-started)*1000./CLOCKS_PER_SEC;
    assert(frame->raster_id==12&&cold.misses>=9&&cold.bytes>1000);
    uint16_t first=pixels[0];bool detail=false;for(unsigned i=1;i<768*768;i++)if(pixels[i]!=first){detail=true;break;}assert(detail);
    started=clock();assert(ui_offline_tiles_window(&tiles,&cache,&r,pixels,frame,&warm)==UI_NAV_OK);double warm_ms=(clock()-started)*1000./CLOCKS_PER_SEC;
    assert(warm.bytes==0&&warm.misses==0&&warm.hits==cold.tiles);
    assert(frame->label_count>0&&frame->label_count<=UI_NAV_MAP_LABEL_MAX);
    printf("color tiles: cold %.1f ms, warm %.1f ms, %u cold bytes, %u tiles; cached read bytes=%u\n",cold_ms,warm_ms,cold.bytes,cold.tiles,warm.bytes);
    uint8_t entry[16];assert(read_at(&file,tiles.directory+cache.entries[0].key*16,entry,16));
    file.corrupt=true;file.flip=u32(entry);ui_offline_tile_cache_init(&cache,cached,24);
    assert(ui_offline_tiles_window(&tiles,&cache,&r,pixels,frame,&warm)==UI_NAV_CORRUPT_MAP);
    file.flip=tiles.label_offset+tiles.label_bytes-1;assert(ui_offline_tiles_open(&tiles,&io)==UI_NAV_CORRUPT_MAP);
    file.corrupt=false;assert(ui_offline_tiles_open(&tiles,&io)==UI_NAV_OK);
    for(unsigned level=0;level<tiles.level_count;level++){
        r.meters_per_pixel=tiles.levels[level].mpp;
        assert(ui_offline_tiles_window(&tiles,&cache,&r,pixels,frame,&warm)==UI_NAV_OK);
        assert(frame->meters_per_pixel==r.meters_per_pixel);
    }
    r.meters_per_pixel=4.5f;assert(ui_offline_tiles_window(&tiles,&cache,&r,pixels,frame,&warm)==UI_NAV_OK);
    assert(frame->meters_per_pixel>=6.f); /* The lower LOD cannot cover a 540px zoomed-out viewport. */
    r.center=(ui_nav_point_t){34.2499678,108.9232174,UI_NAV_COORD_WGS84};r.meters_per_pixel=3;
    assert(ui_offline_tiles_window(&tiles,&cache,&r,pixels,frame,&warm)==UI_NAV_OK);
    bool school=false;for(unsigned i=0;i<frame->label_count;i++)if(frame->labels[i].kind==3&&strstr(frame->labels[i].text,"西北大学"))school=true;
    assert(school); /* Real OSM way center; not an invented public entrance. */
    r.center.system=UI_NAV_COORD_GCJ02;assert(ui_offline_tiles_window(&tiles,&cache,&r,pixels,frame,&warm)==UI_NAV_UNSUPPORTED);
    r.center=(ui_nav_point_t){31.2,121.5,UI_NAV_COORD_WGS84};assert(ui_offline_tiles_window(&tiles,&cache,&r,pixels,frame,&warm)==UI_NAV_NO_MAP);
    io.size=127;assert(ui_offline_tiles_open(&tiles,&io)==UI_NAV_NO_MAP);fclose(file.file);free(pixels);free(cached);free(frame);
    ui_offline_graph_t graph;io=open(&file,"assets/maps/xian/xian-cycle.bgraph");assert(ui_offline_graph_open(&graph,&io)==UI_NAV_OK);
    file.corrupt=true;file.flip=io.size-10;assert(ui_offline_graph_open(&graph,&io)==UI_NAV_CORRUPT_MAP);file.corrupt=false;assert(ui_offline_graph_open(&graph,&io)==UI_NAV_OK);
    size_t bytes=ui_offline_graph_workspace_size(&graph);void *workspace=malloc(bytes);ui_offline_route_t *a=malloc(sizeof(*a)),*b=malloc(sizeof(*b));assert(workspace&&a&&b);
    ui_nav_point_t start={34.25794,108.934547,UI_NAV_COORD_WGS84},end={34.2699991,108.964295,UI_NAV_COORD_WGS84};ui_offline_graph_stats_t stats;
    started=clock();assert(ui_offline_graph_plan(&graph,start,end,workspace,bytes,a,NULL,NULL,&stats)==UI_NAV_OK);
    printf("A*: %.1f ms, %u expanded, %u points, %.0f m, workspace=%zu bytes, end snap=%.1f m\n",(clock()-started)*1000./CLOCKS_PER_SEC,stats.expanded,a->count,a->distance_m,bytes,stats.end_snap_m);
    assert(a->id==2&&a->count>2&&a->distance_m>1000&&a->points[0].cumulative_m==0);
    for(unsigned i=1;i<a->count;i++)assert(a->points[i].cumulative_m>a->points[i-1].cumulative_m);
    end=(ui_nav_point_t){34.262,108.948,UI_NAV_COORD_WGS84};assert(ui_offline_graph_plan(&graph,start,end,workspace,bytes,b,NULL,NULL,&stats)==UI_NAV_OK);
    assert(a->distance_m!=b->distance_m&&a->points[a->count-1].position.longitude!=b->points[b->count-1].position.longitude);
    /* The formerly missing SUST main campus must route to Weiyang, not Taihua. */
    end=(ui_nav_point_t){34.3811545,108.97182845,UI_NAV_COORD_WGS84};
    assert(ui_offline_graph_plan(&graph,start,end,workspace,bytes,b,NULL,NULL,&stats)==UI_NAV_OK);
    assert(b->points[b->count-1].position.latitude>34.35&&stats.end_snap_m<=180);
    printf("SUST Weiyang: %.0f m, %u points, endpoint snap %.1f m\n",b->distance_m,b->count,stats.end_snap_m);
    assert(ui_offline_graph_plan(&graph,start,end,workspace,bytes,b,cancel_now,NULL,&stats)==UI_NAV_CANCELLED);
    end.system=UI_NAV_COORD_GCJ02;assert(ui_offline_graph_plan(&graph,start,end,workspace,bytes,b,NULL,NULL,&stats)==UI_NAV_UNSUPPORTED);
    end=(ui_nav_point_t){31.2,121.5,UI_NAV_COORD_WGS84};assert(ui_offline_graph_plan(&graph,start,end,workspace,bytes,b,NULL,NULL,&stats)==UI_NAV_NO_MAP);
    assert(ui_offline_graph_plan(&graph,start,start,workspace,bytes,b,NULL,NULL,&stats)==UI_NAV_NO_ROUTE);
    free(a);free(b);free(workspace);fclose(file.file);puts("ui_offline_color_test: PASS");return 0;
}
