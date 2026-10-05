#include "ui_offline_places.h"
#include <math.h>
#include <string.h>
#define RECORD_BYTES 160u
#define KEY_BYTES 68u
#define SCAN_LIMIT 8192u
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t crc(uint32_t c,const uint8_t *p,size_t n){while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^((c&1)?0xedb88320u:0);}return c;}
static bool read(const ui_offline_places_t *p,uint32_t at,void *out,size_t n,ui_place_stats_t *s)
{if(at>p->io.size||n>p->io.size-at)return false;if(s)s->bytes+=(uint32_t)n;return p->io.read_at(p->io.context,at,out,n);}
ui_nav_result_code_t ui_offline_places_open(ui_offline_places_t *p,const ui_offline_map_io_t *io)
{
 if(!p||!io||!io->read_at||io->size<128)return UI_NAV_NO_MAP;
 uint8_t h[128],b[256];if(!io->read_at(io->context,0,h,128))return UI_NAV_FAILED;
 if(memcmp(h,"BPOI0001",8)||u32(h+8)!=1)return UI_NAV_UNSUPPORTED;
 uint32_t c=crc(crc(~0u,h,40),h+44,84)^~0u;
 if(c!=u32(h+40)||u32(h+44)||u32(h+32)!=io->size)return UI_NAV_CORRUPT_MAP;
 uint32_t count=u32(h+16),keys=u32(h+20),records=u32(h+24),index=u32(h+28);
 if(!count||count>2000000||!keys||keys>20000000||records!=128||index!=(uint64_t)records+(uint64_t)count*RECORD_BYTES||(uint64_t)index+(uint64_t)keys*KEY_BYTES!=io->size)return UI_NAV_CORRUPT_MAP;
 c=~0u;for(uint32_t at=128;at<io->size;){size_t n=io->size-at;if(n>sizeof(b))n=sizeof(b);if(!io->read_at(io->context,at,b,n))return UI_NAV_FAILED;c=crc(c,b,n);at+=(uint32_t)n;}
 if((c^~0u)!=u32(h+36))return UI_NAV_CORRUPT_MAP;
 /* CRC alone does not guarantee a sorted, NUL-terminated or in-bounds index. */
 char previous[64]={0};for(uint32_t i=0;i<keys;i++){
  if(!io->read_at(io->context,index+i*KEY_BYTES,b,KEY_BYTES))return UI_NAV_FAILED;
  if(!memchr(b,0,64)||!b[0]||u32(b+64)>=count||strcmp(previous,(char *)b)>0)return UI_NAV_CORRUPT_MAP;
  for(unsigned j=0;b[j];j++)if(!((b[j]>='a'&&b[j]<='z')||(b[j]>='0'&&b[j]<='9')))return UI_NAV_CORRUPT_MAP;
  memcpy(previous,b,64);
 }
 *p=(ui_offline_places_t){*io,u32(h+12),count,keys,records,index};return UI_NAV_OK;
}
ui_nav_result_code_t ui_offline_places_query(const ui_offline_places_t *p,const char *query,ui_nav_point_t center,bool valid,ui_place_results_t *out,bool (*cancelled)(void *),void *context,ui_place_stats_t *stats)
{
 if(!out)return UI_NAV_FAILED;memset(out,0,sizeof(*out));if(stats)memset(stats,0,sizeof(*stats));
 char q[64]={0};size_t n=0;if(!p||!query)return out->code=UI_NAV_UNAVAILABLE;
 for(;query[n]&&n<63;n++){unsigned char c=query[n];if(c>='A'&&c<='Z')c+=32;if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')))return out->code=UI_NAV_UNSUPPORTED;q[n]=(char)c;}
 if(query[n]||n<2)return out->code=UI_NAV_UNSUPPORTED;
 if(valid&&(!isfinite(center.latitude)||!isfinite(center.longitude)||fabs(center.latitude)>85||fabs(center.longitude)>180||center.system!=UI_NAV_COORD_WGS84))return out->code=UI_NAV_UNSUPPORTED;
 uint8_t key[68],record[160];uint32_t low=0,high=p->keys;
 while(low<high){uint32_t mid=low+(high-low)/2;if(!read(p,p->keys_offset+mid*KEY_BYTES,key,68,stats)||!memchr(key,0,64))return out->code=UI_NAV_CORRUPT_MAP;if(strcmp((char *)key,q)<0)low=mid+1;else high=mid;}
 double scores[8]={0};uint32_t scanned=0;
 for(uint32_t i=low;i<p->keys;i++){
  if(cancelled&&cancelled(context))return out->code=UI_NAV_CANCELLED;
  if(!read(p,p->keys_offset+i*KEY_BYTES,key,68,stats)||!memchr(key,0,64)||u32(key+64)>=p->records)return out->code=UI_NAV_CORRUPT_MAP;
  if(strncmp((char *)key,q,n))break;
  if(scanned++>=SCAN_LIMIT){out->limited=true;break;}
  if(!read(p,p->records_offset+u32(key+64)*RECORD_BYTES,record,160,stats)||!memchr(record+16,0,64)||!memchr(record+80,0,64)||record[12]>7||!u32(record))return out->code=UI_NAV_CORRUPT_MAP;
  ui_place_t item={.id=u32(record),.point={(int32_t)u32(record+8)/1e7,(int32_t)u32(record+4)/1e7,UI_NAV_COORD_WGS84},.kind=record[12],.entrance=record[13]!=0};
  if(fabs(item.point.latitude)>85||fabs(item.point.longitude)>180)return out->code=UI_NAV_CORRUPT_MAP;
  memcpy(item.name,record+16,64);memcpy(item.detail,record+80,64);
  double distance=valid?hypot((item.point.latitude-center.latitude)*111319.49,(item.point.longitude-center.longitude)*111319.49*cos(center.latitude*.01745329252)):0;
  double score=(strcmp((char *)key,q)?1000000:0)+(item.kind==0?10000:0)+fmin(500000,distance);
  unsigned duplicate=out->count;for(unsigned j=0;j<out->count;j++)if(out->items[j].id==item.id)duplicate=j;
  if(duplicate<out->count){if(scores[duplicate]<=score)continue;for(unsigned j=duplicate;j+1<out->count;j++){out->items[j]=out->items[j+1];scores[j]=scores[j+1];}out->count--;}
  unsigned at=0;while(at<out->count&&(scores[at]<score||(scores[at]==score&&out->items[at].id<item.id)))at++;
  if(at>=8)continue;if(out->count<8)out->count++;
  for(unsigned j=out->count-1;j>at;j--){out->items[j]=out->items[j-1];scores[j]=scores[j-1];}out->items[at]=item;scores[at]=score;
 }
 if(stats){stats->scanned=scanned;stats->limited=out->limited;}return out->code=UI_NAV_OK;
}
