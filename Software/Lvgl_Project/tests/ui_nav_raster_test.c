#include "services/ui_nav_raster.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
int main(void)
{
    uint16_t *a=calloc(768*768,2),*b=calloc(768*768,2),*write;unsigned slot,other;
    assert(a&&b);ui_nav_raster_configure(a,b,768*768);
    assert(!ui_nav_raster_begin(0,&slot,&write));
    assert(ui_nav_raster_begin(1,&slot,&write));assert(write==a);write[0]=42;
    assert(!ui_nav_raster_publish(slot,2));assert(!ui_nav_raster_take(1));
    assert(ui_nav_raster_publish(slot,1));const uint16_t *display=ui_nav_raster_take(1);assert(display==a&&display[0]==42);
    assert(ui_nav_raster_begin(2,&other,&write));assert(write==b&&other!=slot);write[0]=84;
    assert(!ui_nav_raster_begin(3,&other,&write));ui_nav_raster_abort(slot);assert(ui_nav_raster_take(1)==display);
    assert(ui_nav_raster_publish(other,2));assert(display[0]==42);assert(ui_nav_raster_take(2)==b);display=b;
    /* Superseded READY buffers are reusable, DISPLAY buffers are protected. */
    for(uint32_t id=3;id<1000;id++){
        assert(ui_nav_raster_begin(id,&slot,&write));assert(write!=display);
        assert(ui_nav_raster_publish(slot,id));const uint16_t *current=ui_nav_raster_take(id);assert(current==write);
        display=current;
    }
    ui_nav_raster_release();ui_nav_raster_configure(NULL,NULL,0);assert(!ui_nav_raster_begin(1000,&slot,&write));
    free(a);free(b);puts("ui_nav_raster_test: PASS");return 0;
}
