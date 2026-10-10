#include "ui/core/ui_memory.h"
#include <stdatomic.h>
#include "ui_online_places.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fixed token budget and depth: do not trust HTTP response sizes or nesting. */
enum { J_STRING,J_OBJECT,J_ARRAY,J_PRIMITIVE,J_MAX=2048 };
typedef struct {size_t start,end;unsigned next,type;} token_t;
typedef struct {const char *text;size_t size,at;unsigned count;token_t tokens[J_MAX];char scratch_detail[512],scratch_name[256];} json_t;
static void spaces(json_t *j){while(j->at<j->size&&strchr(" \t\r\n",j->text[j->at]))j->at++;}
static bool json_number(const char *p,size_t n)
{
 size_t i=0;if(i<n&&p[i]=='-')i++;if(i==n)return false;
 if(p[i]=='0')i++;
 else{if(p[i]<'1'||p[i]>'9')return false;while(i<n&&isdigit((unsigned char)p[i]))i++;}
 if(i<n&&p[i]=='.'){size_t start=++i;while(i<n&&isdigit((unsigned char)p[i]))i++;if(i==start)return false;}
 if(i<n&&(p[i]=='e'||p[i]=='E')){i++;if(i<n&&(p[i]=='+'||p[i]=='-'))i++;size_t start=i;while(i<n&&isdigit((unsigned char)p[i]))i++;if(i==start)return false;}
 return i==n;
}
static bool value(json_t *j,unsigned depth,unsigned *index)
{
 spaces(j);if(depth>16||j->at>=j->size||j->count==J_MAX)return false;
 unsigned i=j->count++;*index=i;token_t *t=&j->tokens[i];t->start=j->at;char c=j->text[j->at++];
 if(c=='"'){
  t->type=J_STRING;t->start=j->at;
  while(j->at<j->size){unsigned char b=(unsigned char)j->text[j->at++];
   if(b=='"'){t->end=j->at-1;t->next=j->count;return true;}
   if(b<32)return false;
   if(b=='\\'){
    if(j->at>=j->size)return false;c=j->text[j->at++];
    if(c=='u'){for(unsigned k=0;k<4;k++)if(j->at>=j->size||!isxdigit((unsigned char)j->text[j->at++]))return false;}
    else if(!strchr("\"\\/bfnrt",c))return false;
   }
  }return false;
 }
 if(c=='{'||c=='['){
  t->type=c=='{'?J_OBJECT:J_ARRAY;char close=c=='{'?'}':']';spaces(j);
  if(j->at<j->size&&j->text[j->at]==close)j->at++;
  else for(;;){unsigned child;
   if(!value(j,depth+1,&child))return false;
   if(c=='{'){if(j->tokens[child].type!=J_STRING)return false;spaces(j);
    if(j->at>=j->size||j->text[j->at++]!=':'||!value(j,depth+1,&child))return false;}
   spaces(j);if(j->at>=j->size)return false;char separator=j->text[j->at++];
   if(separator==close)break;if(separator!=',')return false;
  }
 }else{
  t->type=J_PRIMITIVE;
  while(j->at<j->size&&!strchr(" \t\r\n,]}:",j->text[j->at]))j->at++;
  size_t n=j->at-t->start;char number[64];
  bool literal=(n==4&&(!memcmp(j->text+t->start,"null",4)||!memcmp(j->text+t->start,"true",4)))||
               (n==5&&!memcmp(j->text+t->start,"false",5));
  if(!literal){if(!n||n>=sizeof(number))return false;memcpy(number,j->text+t->start,n);number[n]=0;
   char *end;double v=strtod(number,&end);if(*end||!isfinite(v)||!json_number(j->text+t->start,n))return false;}
 }
 t->end=j->at;t->next=j->count;return true;
}
static unsigned property(const json_t *j,unsigned object,const char *key)
{
 if(j->tokens[object].type!=J_OBJECT)return J_MAX;unsigned found=J_MAX;
 for(unsigned i=object+1;i<j->tokens[object].next;){const token_t *t=&j->tokens[i];unsigned v=i+1;
  if(t->type!=J_STRING||v>=j->count)return J_MAX;
  if(t->end-t->start==strlen(key)&&!memcmp(j->text+t->start,key,strlen(key))){if(found!=J_MAX)return J_MAX;found=v;}
  i=j->tokens[v].next;
 }return found;
}
static unsigned hex4(const char *p)
{unsigned v=0;for(unsigned i=0;i<4;i++)v=v*16+(isdigit((unsigned char)p[i])?p[i]-'0':tolower((unsigned char)p[i])-'a'+10);return v;}
static bool string(const json_t *j,unsigned index,char *out,size_t cap)
{
 if(index>=j->count||j->tokens[index].type!=J_STRING||!cap)return false;
 const token_t *t=&j->tokens[index];size_t n=0;bool truncated=false;
 for(size_t p=t->start;p<t->end;){unsigned char b=(unsigned char)j->text[p++];unsigned cp=b;char bytes[4];unsigned count=1;
  if(b=='\\'){
   char esc=j->text[p++];if(esc=='u'){
    cp=hex4(j->text+p);p+=4;
    if(cp>=0xd800&&cp<=0xdbff){if(p+6>t->end||j->text[p]!='\\'||j->text[p+1]!='u')return false;
     unsigned low=hex4(j->text+p+2);if(low<0xdc00||low>0xdfff)return false;p+=6;cp=0x10000+((cp-0xd800)<<10)+low-0xdc00;}
    else if(cp>=0xdc00&&cp<=0xdfff)return false;
   }else{const char *escapes="\"\\/bfnrt",*chars="\"\\/\b\f\n\r\t";const char *e=strchr(escapes,esc);if(!e)return false;cp=(unsigned char)chars[e-escapes];}
  }else if(b>=128){
   unsigned continuation;b>=0xc2&&b<=0xdf?(continuation=1,cp=b&31):b>=0xe0&&b<=0xef?(continuation=2,cp=b&15):b>=0xf0&&b<=0xf4?(continuation=3,cp=b&7):(continuation=4);
   if(continuation==4||p+continuation>t->end)return false;
   for(unsigned k=0;k<continuation;k++){b=(unsigned char)j->text[p++];if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&63);}
   if((continuation==1&&cp<128)||(continuation==2&&cp<2048)||(continuation==3&&cp<65536)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
  }
  if(cp<32)return false; /* No embedded NUL or layout control characters. */
  if(cp<128)bytes[0]=(char)cp;
  else if(cp<2048){count=2;bytes[0]=(char)(0xc0|(cp>>6));bytes[1]=(char)(0x80|(cp&63));}
  else if(cp<65536){count=3;bytes[0]=(char)(0xe0|(cp>>12));bytes[1]=(char)(0x80|((cp>>6)&63));bytes[2]=(char)(0x80|(cp&63));}
  else{count=4;bytes[0]=(char)(0xf0|(cp>>18));bytes[1]=(char)(0x80|((cp>>12)&63));bytes[2]=(char)(0x80|((cp>>6)&63));bytes[3]=(char)(0x80|(cp&63));}
  if(!truncated&&n+count<cap){memcpy(out+n,bytes,count);n+=count;}else truncated=true;
 }out[n]=0;return true;
}
static bool field(const json_t *j,unsigned o,const char *key,char *out,size_t cap)
{return string(j,property(j,o,key),out,cap);}
static void utf8_copy(char *out,size_t cap,const char *text)
{size_t n=strlen(text);if(n>=cap)n=cap-1;while(n&&(text[n]&0xc0)==0x80)n--;memcpy(out,text,n);out[n]=0;}

/* Approximate GCJ-02 transform, deliberately not a surveyed entrance coordinate. */
static void delta(double x,double y,double *lat,double *lon)
{
 const double pi=3.14159265358979323846;
 *lat=-100+2*x+3*y+.2*y*y+.1*x*y+.2*sqrt(fabs(x));
 *lat+=(20*sin(6*x*pi)+20*sin(2*x*pi))*2/3;
 *lat+=(20*sin(y*pi)+40*sin(y*pi/3))*2/3;
 *lat+=(160*sin(y*pi/12)+320*sin(y*pi/30))*2/3;
 *lon=300+x+2*y+.1*x*x+.1*x*y+.1*sqrt(fabs(x));
 *lon+=(20*sin(6*x*pi)+20*sin(2*x*pi))*2/3;
 *lon+=(20*sin(x*pi)+40*sin(x*pi/3))*2/3;
 *lon+=(150*sin(x*pi/12)+300*sin(x*pi/30))*2/3;
}
bool ui_online_places_wgs_to_gcj(ui_nav_point_t p,ui_nav_point_t *out)
{
 if(!out||p.system!=UI_NAV_COORD_WGS84||!isfinite(p.latitude)||!isfinite(p.longitude)||p.longitude<72||p.longitude>138||p.latitude<1||p.latitude>56)return false;
 const double pi=3.14159265358979323846,a=6378245.,ee=.00669342162296594323;double lat,lon,rad=p.latitude*pi/180;
 delta(p.longitude-105,p.latitude-35,&lat,&lon);double magic=1-ee*sin(rad)*sin(rad),root=sqrt(magic);
 *out=(ui_nav_point_t){p.latitude+lat*180/((a*(1-ee)/(magic*root))*pi),p.longitude+lon*180/(a/root*cos(rad)*pi),UI_NAV_COORD_GCJ02};return true;
}
bool ui_online_places_gcj_to_wgs(ui_nav_point_t p,ui_nav_point_t *out)
{
 if(!out||p.system!=UI_NAV_COORD_GCJ02)return false;ui_nav_point_t guess=p;guess.system=UI_NAV_COORD_WGS84;
 for(unsigned i=0;i<6;i++){ui_nav_point_t transformed;if(!ui_online_places_wgs_to_gcj(guess,&transformed))return false;
  guess.latitude-=transformed.latitude-p.latitude;guess.longitude-=transformed.longitude-p.longitude;}
 *out=guess;return true;
}
static const char *district(const char *code)
{
 static const char *codes[]={"610102","610103","610104","610111","610112","610113","610116"};
 static const char *names[]={"新城区","碑林区","莲湖区","灞桥区","未央区","雁塔区","长安区"};
 for(unsigned i=0;i<7;i++)if(!strcmp(code,codes[i]))return names[i];return NULL;
}
static bool point(const char *text,ui_nav_point_t *out)
{
 char *end;double lon=strtod(text,&end);if(end==text||*end++!=',')return false;const char *start=end;double lat=strtod(start,&end);
 if(end==start||*end||!isfinite(lat)||!isfinite(lon))return false;
 ui_nav_point_t raw={lat,lon,UI_NAV_COORD_GCJ02};if(!ui_online_places_gcj_to_wgs(raw,out))return false;
 return out->longitude>=108.63&&out->longitude<=109.28&&out->latitude>=33.79&&out->latitude<=34.47;
}
static uint32_t identity(const char *id)
{uint32_t hash=2166136261u;for(const char *p="amap:";*p;p++)hash=(hash^(unsigned char)*p)*16777619u;for(;*id;id++)hash=(hash^(unsigned char)*id)*16777619u;return hash?hash:1;}
static ui_place_online_status_t parse_amap(const char *text,size_t size,ui_place_results_t *out,json_t *workspace)
{
 if(!out)return UI_PLACE_ONLINE_FAILED;*out=(ui_place_results_t){0};
 if(!text||!size||size>UI_ONLINE_RESPONSE_MAX||memchr(text,0,size))return UI_PLACE_ONLINE_FAILED;
 json_t *j=workspace; memset(j,0,sizeof(*j));j->text=text;j->size=size;unsigned root;if(!value(j,0,&root))return UI_PLACE_ONLINE_FAILED;spaces(j);
 if(j->at!=size||j->tokens[root].type!=J_OBJECT)return UI_PLACE_ONLINE_FAILED;
 char status[16],code[24];if(!field(j,root,"status",status,sizeof(status)))return UI_PLACE_ONLINE_FAILED;
 if(strcmp(status,"1")){
  if(field(j,root,"infocode",code,sizeof(code))&&(!strcmp(code,"10001")||!strcmp(code,"10004")||!strcmp(code,"10005")||!strcmp(code,"10006")||!strcmp(code,"10007")||!strcmp(code,"10009")||!strcmp(code,"10012")||!strcmp(code,"10013")))return UI_PLACE_ONLINE_KEY_ERROR;
  return UI_PLACE_ONLINE_FAILED;
 }
 unsigned tips=property(j,root,"tips");if(tips>=j->count||j->tokens[tips].type!=J_ARRAY)return UI_PLACE_ONLINE_FAILED;
 for(unsigned i=tips+1;i<j->tokens[tips].next;i=j->tokens[i].next){
  char id[128],adcode[24],location[128],address[256]="";char *name=j->scratch_name;ui_nav_point_t p;
  if(!field(j,i,"id",id,sizeof(id))||!id[0]||!field(j,i,"name",name,sizeof(j->scratch_name))||!name[0]||
     !field(j,i,"adcode",adcode,sizeof(adcode))||!district(adcode)||!field(j,i,"location",location,sizeof(location))||!point(location,&p))continue;
  bool duplicate=false;for(unsigned n=0;n<out->count;n++)if(out->items[n].id==identity(id))duplicate=true;if(duplicate)continue;
  if(out->count==UI_PLACE_RESULT_MAX){out->limited=true;continue;}
  field(j,i,"address",address,sizeof(address));ui_place_t *item=&out->items[out->count++];
  *item=(ui_place_t){.id=identity(id),.point=p,.kind=7,.online=true};utf8_copy(item->name,sizeof(item->name),name);
  char *detail=j->scratch_detail;snprintf(detail,sizeof(j->scratch_detail),"高德 · %s%s%s",district(adcode),address[0]?" · ":"",address);utf8_copy(item->detail,sizeof(item->detail),detail);
 }return UI_PLACE_ONLINE_OK;
}
static UI_LARGE_RAM json_t s_json_workspace;
static UI_LARGE_RAM ui_place_results_t s_merge_workspace;
static atomic_flag s_json_busy=ATOMIC_FLAG_INIT;
ui_place_online_status_t ui_online_places_amap_parse(const char *text,size_t size,ui_place_results_t *out)
{
 if(atomic_flag_test_and_set_explicit(&s_json_busy,memory_order_acquire))return UI_PLACE_ONLINE_FAILED;
 ui_place_online_status_t status=parse_amap(text,size,out,&s_json_workspace);
 atomic_flag_clear_explicit(&s_json_busy,memory_order_release);return status;
}

static bool same(const ui_place_t *a,const ui_place_t *b)
{double x=(a->point.longitude-b->point.longitude)*92000,y=(a->point.latitude-b->point.latitude)*111319;return !strcmp(a->name,b->name)&&x*x+y*y<120*120;}
void ui_online_places_merge(ui_place_results_t *offline,const ui_place_results_t *online,ui_place_online_status_t status)
{
 if(!offline)return;offline->online_status=(uint8_t)status;if(status!=UI_PLACE_ONLINE_OK||!online)return;
 if(atomic_flag_test_and_set_explicit(&s_json_busy,memory_order_acquire)){offline->online_status=UI_PLACE_ONLINE_FAILED;return;}
 ui_place_results_t *merged=&s_merge_workspace;memset(merged,0,sizeof(*merged));
 merged->request_id=offline->request_id;merged->online_status=(uint8_t)status;merged->limited=offline->limited||online->limited;
 for(unsigned index=0;index<UI_PLACE_RESULT_MAX;index++)for(unsigned source=0;source<2;source++){
  const ui_place_results_t *r=source?online:offline;if(index>=r->count)continue;ui_place_t p=r->items[index];bool duplicate=false;
  for(unsigned i=0;i<merged->count;i++)if(same(&merged->items[i],&p))duplicate=true;if(duplicate)continue;
  if(merged->count==UI_PLACE_RESULT_MAX){merged->limited=true;continue;}
  bool collision;do{collision=false;for(unsigned i=0;i<merged->count;i++)if(merged->items[i].id==p.id){p.id++;if(!p.id)p.id=1;collision=true;}}while(collision);
  merged->items[merged->count++]=p;
 }
 if(!merged->count&&offline->code!=UI_NAV_OK)merged->code=offline->code;*offline=*merged;
 atomic_flag_clear_explicit(&s_json_busy,memory_order_release);
}
