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
static float bearing(ui_nav_point_t a,ui_nav_point_t b)
{float angle=atan2((b.longitude-a.longitude)*cos(a.latitude*.01745329252),b.latitude-a.latitude)*57.295779513;return angle<0?angle+360:angle;}
static float turn_angle(const ui_offline_route_t *r,unsigned i)
{float d=bearing(r->points[i].position,r->points[i+1].position)-bearing(r->points[i-1].position,r->points[i].position);while(d>180)d-=360;while(d< -180)d+=360;return d;}
static nav_maneuver_t turn(float d){return fabsf(d)>150?NAV_MANEUVER_U_TURN:d>25?NAV_MANEUVER_RIGHT:d< -25?NAV_MANEUVER_LEFT:NAV_MANEUVER_STRAIGHT;}
void ui_offline_route_replay(const ui_offline_route_t *r,float travelled,uint32_t id,uint32_t now,nav_model_t *m)
{
 memset(m,0,sizeof(*m));if(!r||r->count<2||!isfinite(travelled))return;
 travelled=fmaxf(0,fminf(r->distance_m,travelled));unsigned i=0;while(i+1<r->count-1&&r->points[i+1].cumulative_m<=travelled)i++;
 const ui_offline_route_point_t *a=&r->points[i],*b=&r->points[i+1];float span=b->cumulative_m-a->cumulative_m,t=span>0?(travelled-a->cumulative_m)/span:0;
 m->latitude=a->position.latitude+t*(b->position.latitude-a->position.latitude);m->longitude=a->position.longitude+t*(b->position.longitude-a->position.longitude);m->coordinate_system=UI_NAV_COORD_WGS84;
 m->heading=bearing(a->position,b->position);m->heading_valid=m->position_valid=m->guidance_valid=m->speed_valid=m->distance_valid=true;
 m->sampled_at_ms=now;m->stale_after_ms=2500;m->session_bound=true;m->session_id=id;m->route_id=r->id;m->route_revision=r->revision;
 m->speed=travelled>=r->distance_m?0:18;m->distance=(r->distance_m-travelled)/1000.f;m->zoom=16;m->demo=true;
 unsigned j=i+1;while(j+1<r->count&&fabsf(turn_angle(r,j))<30)j++;
 m->step_id=j+1;m->direction=j==r->count-1?NAV_MANEUVER_ARRIVE:turn(turn_angle(r,j));m->direction_distance=(uint32_t)fmaxf(0,r->points[j].cumulative_m-travelled);
 m->junction_valid=true;m->junction_latitude=r->points[j].position.latitude;m->junction_longitude=r->points[j].position.longitude;
 const char *road=r->points[j].road[0]?r->points[j].road:"沿路线骑行";snprintf(m->road_name,sizeof(m->road_name),"%s",road);
 if(j+1<r->count){m->exit_position_valid=true;m->exit_latitude=r->points[j+1].position.latitude;m->exit_longitude=r->points[j+1].position.longitude;
  unsigned k=j+1;while(k+1<r->count&&fabsf(turn_angle(r,k))<30)k++;
  m->next_valid=true;m->next_direction=k==r->count-1?NAV_MANEUVER_ARRIVE:turn(turn_angle(r,k));m->next_distance_m=(uint32_t)(r->points[k].cumulative_m-r->points[j].cumulative_m);
 }
 m->ui_state=m->direction_distance<=8?NAV_UI_NOW:m->direction_distance<=40?NAV_UI_IMMINENT:m->direction_distance<=160?NAV_UI_APPROACH:NAV_UI_CRUISE;
 if(travelled>=r->distance_m){m->ui_state=NAV_UI_ARRIVED;m->direction=NAV_MANEUVER_ARRIVE;m->direction_distance=0;m->next_valid=false;}
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
