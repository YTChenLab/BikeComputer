#include "services/ui_offline_map.h"
#include "services/ui_offline_route.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {uint8_t *bytes;size_t length;} memory_t;
static bool read_at(void *c,uint32_t o,void *b,size_t n){memory_t *m=c;if(o>m->length||n>m->length-o)return false;memcpy(b,m->bytes+o,n);return true;}
static memory_t load(const char *path){FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);assert(n>0);rewind(f);memory_t m={malloc(n),n};assert(m.bytes&&fread(m.bytes,1,n,f)==(size_t)n);fclose(f);return m;}
int main(void)
{
 memory_t raw=load("assets/maps/xian/xian-urban.bmap");ui_offline_map_io_t io={read_at,raw.length,&raw};ui_offline_map_t map;assert(ui_offline_map_open(&map,&io)==UI_NAV_OK);
 memory_t rr=load("assets/maps/xian/xian-city.broute");ui_offline_map_io_t rio={read_at,rr.length,&rr};ui_offline_route_t *route=malloc(sizeof(*route));assert(route&&ui_offline_route_open(route,&rio)==UI_NAV_OK);
 assert(route->count>2&&route->distance_m>500);ui_nav_map_frame_t *f=malloc(sizeof(*f));assert(f);ui_offline_map_stats_t stats;
 ui_nav_map_request_t r={.id=1,.center=route->points[route->count/2].position,.meters_per_pixel=2,.width=504,.height=580};
 clock_t t=clock();assert(ui_offline_map_read(&map,&r,f,&stats)==UI_NAV_OK);double ms=(clock()-t)*1000./CLOCKS_PER_SEC;
 assert(f->road_count>10&&!f->demo&&f->origin.system==UI_NAV_COORD_WGS84&&stats.tiles>0);
 printf("real map: %.1f ms, %u tiles, %u bytes, %u roads, %u areas, %u labels; frame=%zu route=%zu\n",ms,stats.tiles,stats.bytes,f->road_count,f->area_count,f->label_count,sizeof(*f),sizeof(*route));
 unsigned turns=0;for(float travelled=0;travelled<=route->distance_m;travelled+=20){nav_model_t m;ui_offline_route_replay(route,travelled,9,100,&m);assert(m.position_valid&&m.guidance_valid&&m.session_id==9&&m.route_id==1);if(m.direction==NAV_MANEUVER_RIGHT||m.direction==NAV_MANEUVER_LEFT)turns++;}
 assert(turns>0);nav_model_t arrived;ui_offline_route_replay(route,route->distance_m,9,100,&arrived);assert(arrived.ui_state==NAV_UI_ARRIVED&&arrived.speed==0);
 r.route_id=f->route_id=1;r.route_revision=f->route_revision=1;ui_offline_route_window(route,f);assert(f->point_count>2&&f->point_count<=UI_NAV_WINDOW_POINT_MAX&&f->destination_valid);
 r.center=(ui_nav_point_t){31.2,121.5,UI_NAV_COORD_WGS84};assert(ui_offline_map_read(&map,&r,f,&stats)==UI_NAV_NO_MAP);
 r.center=route->points[0].position;r.center.system=UI_NAV_COORD_GCJ02;assert(ui_offline_map_read(&map,&r,f,&stats)==UI_NAV_UNSUPPORTED);
 raw.bytes[20]^=1;assert(ui_offline_map_open(&map,&io)==UI_NAV_CORRUPT_MAP);raw.bytes[20]^=1;
 assert(ui_offline_map_open(&map,&io)==UI_NAV_OK);r.center=route->points[0].position;assert(ui_offline_map_read(&map,&r,f,&stats)==UI_NAV_OK);
 /* Corrupt a nonempty tile actually read in the requested window. */
 int cx=((int32_t)(r.center.longitude*1e7)-map.west)/map.cell,cy=((int32_t)(r.center.latitude*1e7)-map.south)/map.cell;
 bool caught=false;for(int dy=-1;dy<=1&&!caught;dy++)for(int dx=-1;dx<=1&&!caught;dx++){
  unsigned tile=(cy+dy)*map.cols+cx+dx;if(tile>=map.cols*map.rows)continue;
  uint8_t *e=raw.bytes+128+tile*12;uint32_t offset=(uint32_t)e[0]|(uint32_t)e[1]<<8|(uint32_t)e[2]<<16|(uint32_t)e[3]<<24;if(!(e[4]|e[5]|e[6]|e[7]))continue;
  raw.bytes[offset+1]^=1;ui_nav_result_code_t result=ui_offline_map_read(&map,&r,f,&stats);raw.bytes[offset+1]^=1;if(result==UI_NAV_CORRUPT_MAP){caught=true;assert(!f->road_count);break;}
 }assert(caught);
 rr.bytes[rr.length-3]^=1;assert(ui_offline_route_open(route,&rio)==UI_NAV_NO_ROUTE);rr.bytes[rr.length-3]^=1;
 io.size=127;assert(ui_offline_map_open(&map,&io)==UI_NAV_CORRUPT_MAP);
 free(raw.bytes);free(rr.bytes);free(route);free(f);puts("ui_offline_map_test: PASS");return 0;
}
