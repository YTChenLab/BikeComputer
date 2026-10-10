#include "services/ui_offline_route.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

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
