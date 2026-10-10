#include "ui_offline_route.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t u32(const uint8_t *b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static float f32(const uint8_t *b){uint32_t v=u32(b);float f;memcpy(&f,&v,4);return f;}
static uint32_t crc(uint32_t c,const uint8_t *p,size_t n){while(n--){c^=*p++;for(unsigned b=0;b<8;b++)c=(c>>1)^(0xEDB88320u&-(c&1));}return c;}
ui_nav_result_code_t ui_offline_route_open(ui_offline_route_t *r,const ui_offline_map_io_t *io)
{
 if(!r||!io||!io->read_at)return UI_NAV_NO_ROUTE;memset(r,0,sizeof(*r));uint8_t b[128];
 if(io->size<128||!io->read_at(io->context,0,b,128))return UI_NAV_NO_ROUTE;
 if(memcmp(b,"BRTE0001",8)||u32(b+8)!=1)return UI_NAV_UNSUPPORTED;
 unsigned count=u32(b+20);float length=f32(b+16);uint32_t expected=u32(b+72),c=0xffffffff;
 if(count<2||count>UI_OFFLINE_ROUTE_POINT_MAX||io->size!=128+count*60||!isfinite(length)||length<=0||!memchr(b+24,0,48)||!u32(b+12))return UI_NAV_NO_ROUTE;
 r->id=1;r->revision=u32(b+12);r->distance_m=length;memcpy(r->name,b+24,48);
 for(unsigned i=0;i<count;i++){
  if(!io->read_at(io->context,128+i*60,b,60))goto failed;c=crc(c,b,60);
  ui_offline_route_point_t *p=&r->points[i];p->position=(ui_nav_point_t){(int32_t)u32(b+4)/1e7,(int32_t)u32(b)/1e7,UI_NAV_COORD_WGS84};p->cumulative_m=f32(b+8);memcpy(p->road,b+12,48);
  if(!isfinite(p->cumulative_m)||p->cumulative_m<0||p->cumulative_m>length+1||fabs(p->position.latitude)>85||fabs(p->position.longitude)>180||!memchr(p->road,0,48)||(i&&p->cumulative_m<r->points[i-1].cumulative_m))goto failed;
 }
 if((c^0xffffffff)!=expected||r->points[0].cumulative_m!=0||fabs(r->points[count-1].cumulative_m-length)>1)goto failed;
 r->count=count;return UI_NAV_OK;
failed:memset(r,0,sizeof(*r));return UI_NAV_NO_ROUTE;
}
void ui_offline_route_window(const ui_offline_route_t *r,ui_nav_map_frame_t *f)
{
 if(!r||r->count<2||f->route_id!=r->id||f->route_revision!=r->revision)return;
 f->destination_valid=true;f->destination=r->points[r->count-1].position;
 /* Uniform decimation only for the overview; follow uses the nearest bounded
  * continuous range. Full route stays external to the LVGL model. */
 unsigned start=0,end=r->count-1;if(r->count>UI_NAV_WINDOW_POINT_MAX){
  float best=1e30;unsigned nearest=0;for(unsigned i=0;i<r->count;i++){float d=hypot((r->points[i].position.longitude-f->origin.longitude)*92000,(r->points[i].position.latitude-f->origin.latitude)*111319);if(d<best){best=d;nearest=i;}}
  start=nearest>32?nearest-32:0;end=start+UI_NAV_WINDOW_POINT_MAX-1;if(end>=r->count){end=r->count-1;start=end-UI_NAV_WINDOW_POINT_MAX+1;}
 }
 for(unsigned i=start;i<=end;i++)f->points[f->point_count++]=r->points[i].position;
}
