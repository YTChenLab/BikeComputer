#include "services/ui_online_places.h"
#include "platform/pc/ui_pc_online_places.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *body="{\"status\":\"1\",\"tips\":["
 "{\"id\":\"testA\",\"name\":\"测试小区\",\"adcode\":\"610113\",\"location\":\"108.944574,34.2584\",\"address\":\"测试路1号\"},"
 "{\"id\":\"testA\",\"name\":\"重复小区\",\"adcode\":\"610113\",\"location\":\"108.944574,34.2584\"},"
 "{\"id\":\"other\",\"name\":\"临潼小区\",\"adcode\":\"610115\",\"location\":\"109.21,34.36\"},"
 "{\"id\":\"noPoint\",\"name\":\"没有坐标\",\"adcode\":\"610113\",\"location\":[]},"
 "{\"id\":\"badPoint\",\"name\":\"错误坐标\",\"adcode\":\"610113\",\"location\":\"NaN,34.3\"},"
 "{\"id\":\"testB\",\"name\":\"\\u6d4b\\u8bd5\\u82d1\",\"adcode\":\"610116\",\"location\":\"108.91,34.17\",\"address\":[]}] }";
static void parse_tests(void)
{
 ui_place_results_t r;assert(ui_online_places_amap_parse(body,strlen(body),&r)==UI_PLACE_ONLINE_OK);
 assert(r.count==2&&r.items[0].online&&!r.items[0].entrance&&!r.items[0].navigation_point_valid);
 assert(!strcmp(r.items[0].name,"测试小区")&&strstr(r.items[0].detail,"高德 · 雁塔区"));
 assert(!strcmp(r.items[1].name,"测试苑")&&r.items[1].point.system==UI_NAV_COORD_WGS84);
 assert(fabs(r.items[0].point.longitude-108.944574)>.001); /* Reject treating GCJ as raw WGS. */
 assert(ui_online_places_amap_parse(body,strlen(body)-2,&r)==UI_PLACE_ONLINE_FAILED&&r.count==0);
 const char *key_error="{\"status\":\"0\",\"infocode\":\"10001\",\"tips\":[]}";
 assert(ui_online_places_amap_parse(key_error,strlen(key_error),&r)==UI_PLACE_ONLINE_KEY_ERROR&&r.count==0);
 const char *duplicate="{\"status\":\"1\",\"status\":\"0\",\"tips\":[]}";
 assert(ui_online_places_amap_parse(duplicate,strlen(duplicate),&r)==UI_PLACE_ONLINE_FAILED);
 const char *bad_number="{\"status\":\"1\",\"tips\":[],\"unknown\":01}";
 assert(ui_online_places_amap_parse(bad_number,strlen(bad_number),&r)==UI_PLACE_ONLINE_FAILED);
 assert(ui_online_places_amap_parse(body,UI_ONLINE_RESPONSE_MAX+1,&r)==UI_PLACE_ONLINE_FAILED);
 const char *invalid="{\"status\":\"1\",\"tips\":[{\"id\":\"bad\",\"name\":\"\\u0000\",\"adcode\":\"610113\",\"location\":\"108.94,34.26\"}]}";
 assert(ui_online_places_amap_parse(invalid,strlen(invalid),&r)==UI_PLACE_ONLINE_OK&&r.count==0);
 ui_nav_point_t input={34.26,108.94,UI_NAV_COORD_WGS84},gcj,wgs;
 assert(ui_online_places_wgs_to_gcj(input,&gcj)&&gcj.system==UI_NAV_COORD_GCJ02);
 assert(ui_online_places_gcj_to_wgs(gcj,&wgs)&&fabs(wgs.longitude-input.longitude)<1e-7&&fabs(wgs.latitude-input.latitude)<1e-7);
 assert(!ui_online_places_gcj_to_wgs(input,&wgs));gcj.longitude=NAN;assert(!ui_online_places_gcj_to_wgs(gcj,&wgs));
}
static void merge_tests(void)
{
 ui_place_results_t offline={.request_id=45,.count=8},online;
 for(unsigned i=0;i<8;i++){offline.items[i]=(ui_place_t){.id=i+1,.point={34.26,108.94+i*.01,UI_NAV_COORD_WGS84}};snprintf(offline.items[i].name,64,"离线 %u",i);}
 assert(ui_online_places_amap_parse(body,strlen(body),&online)==UI_PLACE_ONLINE_OK);
 online.items[0].id=offline.items[0].id;ui_online_places_merge(&offline,&online,UI_PLACE_ONLINE_OK);
 assert(offline.count==8&&offline.limited&&offline.request_id==45&&offline.items[1].online&&offline.items[1].id!=offline.items[0].id);
 ui_place_results_t old=offline;ui_online_places_merge(&offline,&online,UI_PLACE_ONLINE_TIMEOUT);
 assert(offline.count==old.count&&offline.online_status==UI_PLACE_ONLINE_TIMEOUT&&!memcmp(offline.items,old.items,sizeof(old.items)));
 offline=(ui_place_results_t){.code=UI_NAV_NO_MAP};ui_online_places_merge(&offline,&online,UI_PLACE_ONLINE_OK);
 assert(offline.code==UI_NAV_OK&&offline.count==2);
 online=(ui_place_results_t){.count=1};online.items[0]=offline.items[0];
 ui_online_places_merge(&offline,&online,UI_PLACE_ONLINE_OK);assert(offline.count==2);
}
static bool cancelled(void *context){(void)context;return true;}
static void configuration_tests(void)
{
 ui_pc_online_places_config_t config;ui_place_results_t out;ui_nav_search_request_t request={.query="test"};
 ui_pc_online_places_load(&config,"missing-config.ini");assert(!config.enabled);
 assert(ui_pc_online_places_query(&config,&request,&out,NULL,NULL)==UI_PLACE_ONLINE_OFF);
 FILE *file=fopen("online-test.ini","wb");assert(file);fputs("enabled=1\namap_key=\n",file);fclose(file);
 ui_pc_online_places_load(&config,"online-test.ini");assert(config.enabled&&!config.valid);
 assert(ui_pc_online_places_query(&config,&request,&out,NULL,NULL)==UI_PLACE_ONLINE_KEY_ERROR);
 file=fopen("online-test.ini","wb");assert(file);fputs("\xef\xbb\xbf" "enabled=0\namap_key=0123456789abcdef0123456789abcdef\n",file);fclose(file);
 ui_pc_online_places_load(&config,"online-test.ini");assert(!config.enabled&&config.valid);
 assert(ui_pc_online_places_query(&config,&request,&out,NULL,NULL)==UI_PLACE_ONLINE_OFF);
 config.enabled=true;assert(ui_pc_online_places_query(&config,&request,&out,cancelled,NULL)==UI_PLACE_ONLINE_FAILED); /* No network. */
 file=fopen("online-test.ini","wb");assert(file);fputs("enabled=1\nenabled=0\namap_key=0123456789abcdef0123456789abcdef\n",file);fclose(file);
 ui_pc_online_places_load(&config,"online-test.ini");assert(config.enabled&&!config.valid);assert(!remove("online-test.ini"));
}
int main(int argc,char **argv)
{
 if(argc==2&&!strcmp(argv[1],"--probe-transport")){
  ui_pc_online_places_config_t config={.enabled=true,.valid=true,.key="00000000000000000000000000000000"};
  ui_nav_search_request_t request={.query="xian"};ui_place_results_t result;
  ui_place_online_status_t status=ui_pc_online_places_query(&config,&request,&result,NULL,NULL);
  printf("HTTPS invalid-key probe: status=%u, count=%u\n",(unsigned)status,result.count);
  return status==UI_PLACE_ONLINE_KEY_ERROR?0:1;
 }
 if(argc==4&&!strcmp(argv[1],"--query-file")){
  ui_pc_online_places_config_t config;ui_pc_online_places_load(&config,argv[2]);
  if(!config.enabled||!config.valid){puts("Configuration is disabled or invalid; no request sent.");return 2;}
  ui_nav_search_request_t request={0};FILE *file=fopen(argv[3],"rb");if(!file)return 2;
  size_t size=fread(request.query,1,sizeof(request.query),file);fclose(file);if(size==sizeof(request.query))return 2;
  while(size&&(request.query[size-1]=='\r'||request.query[size-1]=='\n'))size--;request.query[size]=0;
  ui_place_results_t result;ui_place_online_status_t status=ui_pc_online_places_query(&config,&request,&result,NULL,NULL);
  printf("Online result: status=%u, count=%u\n",(unsigned)status,result.count);
  for(unsigned i=0;i<result.count;i++)printf("%s | %s | WGS84 %.7f,%.7f\n",result.items[i].name,result.items[i].detail,result.items[i].point.longitude,result.items[i].point.latitude);
  return status==UI_PLACE_ONLINE_OK?0:1;
 }
 parse_tests();merge_tests();configuration_tests();puts("ui_online_places_test: PASS");return 0;
}
