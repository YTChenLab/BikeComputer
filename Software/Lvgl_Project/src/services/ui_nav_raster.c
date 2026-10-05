#include "ui_nav_raster.h"
#include "ui/core/ui_concurrency.h"
#include <string.h>
enum { FREE,WRITING,READY,DISPLAY };
static struct {uint16_t *pixels;uint32_t id;uint8_t state;} slots[2];
static size_t capacity;
void ui_nav_raster_configure(uint16_t *a,uint16_t *b,size_t pixels)
{ui_concurrency_lock();memset(slots,0,sizeof(slots));slots[0].pixels=a;slots[1].pixels=b;capacity=pixels;ui_concurrency_unlock();}
bool ui_nav_raster_begin(uint32_t id,unsigned *slot,uint16_t **pixels)
{
    if(!id||!slot||!pixels)return false;bool found=false;ui_concurrency_lock();
    for(unsigned i=0;i<2;i++)if(slots[i].pixels&&capacity>=768*768&&(slots[i].state==FREE||slots[i].state==READY)){
        slots[i].state=WRITING;slots[i].id=id;*slot=i;*pixels=slots[i].pixels;found=true;break;}
    ui_concurrency_unlock();return found;
}
bool ui_nav_raster_publish(unsigned slot,uint32_t id)
{if(slot>=2)return false;ui_concurrency_lock();bool ok=slots[slot].state==WRITING&&slots[slot].id==id;if(ok)slots[slot].state=READY;ui_concurrency_unlock();return ok;}
void ui_nav_raster_abort(unsigned slot)
{if(slot>=2)return;ui_concurrency_lock();if(slots[slot].state!=DISPLAY)slots[slot].state=FREE;ui_concurrency_unlock();}
const uint16_t *ui_nav_raster_take(uint32_t id)
{
    const uint16_t *pixels=NULL;ui_concurrency_lock();
    for(unsigned i=0;i<2;i++)if(slots[i].id==id&&(slots[i].state==READY||slots[i].state==DISPLAY)){
        for(unsigned j=0;j<2;j++)if(j!=i&&slots[j].state==DISPLAY)slots[j].state=FREE;
        slots[i].state=DISPLAY;pixels=slots[i].pixels;break;}
    ui_concurrency_unlock();return pixels;
}
void ui_nav_raster_release(void)
{ui_concurrency_lock();for(unsigned i=0;i<2;i++)if(slots[i].state==DISPLAY)slots[i].state=FREE;ui_concurrency_unlock();}
