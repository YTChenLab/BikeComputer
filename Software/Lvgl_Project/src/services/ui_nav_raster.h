#ifndef UI_NAV_RASTER_H
#define UI_NAV_RASTER_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* Two caller-owned buffers. The storage task never writes the displayed
 * buffer. The UI adopts a completed buffer only between LVGL draw cycles. */
void ui_nav_raster_configure(uint16_t *first,uint16_t *second,size_t pixels);
bool ui_nav_raster_begin(uint32_t request_id,unsigned *slot,uint16_t **pixels);
bool ui_nav_raster_publish(unsigned slot,uint32_t request_id);
void ui_nav_raster_abort(unsigned slot);
const uint16_t *ui_nav_raster_take(uint32_t request_id);
void ui_nav_raster_release(void);
#endif
