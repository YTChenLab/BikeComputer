#include "services/ui_nav_search.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {FILE *file;bool corrupt;uint32_t flip;} input_t;
static bool read_at(void *context,uint32_t offset,void *bytes,size_t count){input_t *p=context;if(fseek(p->file,offset,SEEK_SET)||fread(bytes,1,count,p->file)!=count)return false;if(p->corrupt&&p->flip>=offset&&p->flip-offset<count)((uint8_t *)bytes)[p->flip-offset]^=1;return true;}
static bool stop(void *unused){(void)unused;return true;}
static bool dispatched(const ui_nav_search_request_t *r,void *context){*(uint32_t *)context=r->id;return true;}
int main(void)
{
 input_t input={.file=fopen("assets/maps/xian/xian-places.bpoi","rb")};assert(input.file);fseek(input.file,0,SEEK_END);ui_offline_map_io_t io={read_at,(uint32_t)ftell(input.file),&input};ui_offline_places_t places;
 assert(ui_offline_places_open(&places,&io)==UI_NAV_OK);input.corrupt=true;input.flip=28;assert(ui_offline_places_open(&places,&io)==UI_NAV_CORRUPT_MAP);input.flip=io.size-1;assert(ui_offline_places_open(&places,&io)==UI_NAV_CORRUPT_MAP);input.corrupt=false;
 ui_place_results_t out;ui_place_stats_t stats;ui_nav_point_t center={34.25794,108.934547,UI_NAV_COORD_WGS84};
 assert(ui_offline_places_query(&places,"xibeidaxue",center,true,&out,NULL,NULL,&stats)==UI_NAV_OK&&out.count);
 bool found=false;for(unsigned i=0;i<out.count;i++)if(strstr(out.items[i].name,"西北大学"))found=true;assert(found);
 assert(ui_offline_places_query(&places,"XAJTDX",center,true,&out,NULL,NULL,&stats)==UI_NAV_OK&&out.count);found=false;
 for(unsigned i=0;i<out.count;i++)if(strstr(out.items[i].name,"交通大学"))found=true;assert(found);
 assert(ui_offline_places_query(&places,"jiaotong",center,true,&out,NULL,NULL,&stats)==UI_NAV_OK&&out.count);
 assert(ui_offline_places_query(&places,"zhonglou",center,true,&out,NULL,NULL,&stats)==UI_NAV_OK&&out.count);
 found=false;for(unsigned i=0;i<out.count;i++)if(strstr(out.items[i].name,"钟楼"))found=true;assert(found);
 for(unsigned i=0;i<out.count;i++)for(unsigned j=0;j<i;j++)assert(out.items[i].id!=out.items[j].id);
 printf("place index: %u records, %u keys; query %u scanned, %u read bytes; result count=%u\n",places.records,places.keys,stats.scanned,stats.bytes,out.count);
 assert(ui_offline_places_query(&places,"zhonglou",center,true,&out,stop,NULL,&stats)==UI_NAV_CANCELLED);
 assert(ui_offline_places_query(&places,"zzzzzzzz",center,true,&out,NULL,NULL,&stats)==UI_NAV_OK&&!out.count);
 assert(ui_offline_places_query(&places,"z",center,true,&out,NULL,NULL,&stats)==UI_NAV_UNSUPPORTED);
 assert(ui_offline_places_query(&places,"钟楼",center,true,&out,NULL,NULL,&stats)==UI_NAV_UNSUPPORTED);
 io.size=127;assert(ui_offline_places_open(&places,&io)==UI_NAV_NO_MAP);fclose(input.file);
 uint32_t id=0;ui_nav_search_backend_t port={.request=dispatched,.context=&id};assert(ui_nav_search_register(&port));
 uint32_t first=ui_nav_search_request("zhonglou",center,true,100),second=ui_nav_search_request("jiaotong",center,true,200);assert(first!=second&&id==second);
 out=(ui_place_results_t){.request_id=first};assert(!ui_nav_search_submit(&out));out.request_id=second;assert(ui_nav_search_submit(&out));ui_nav_search_tick(220);bool busy;ui_nav_search_get(&out,NULL,&busy);assert(!busy&&out.request_id==second);
 uint32_t third=ui_nav_search_request("zhonglou",center,true,300);ui_nav_search_cancel();out.request_id=third;assert(!ui_nav_search_submit(&out));
 ui_nav_search_request("zhonglou",center,true,400);ui_nav_search_tick(5400);ui_nav_search_get(&out,NULL,&busy);assert(!busy&&out.code==UI_NAV_TIMEOUT);
 assert(ui_nav_search_register(NULL));ui_nav_search_request("zhonglou",center,true,5500);ui_nav_search_tick(5501);ui_nav_search_get(&out,NULL,&busy);assert(!busy&&out.code==UI_NAV_UNAVAILABLE);
 puts("ui_offline_places_test: PASS");return 0;
}
