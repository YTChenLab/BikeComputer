#include "ui_stm32_display.h"
#include "ui/core/ui_runtime.h"
#include <stdint.h>
#include <stdlib.h>

static ui_stm32_display_port_t s_port;
static lv_display_t *s_display;
static bool s_in_flight;
static void wait_flush(lv_display_t *display)
{
    if(!s_in_flight) return;
    if(!ui_runtime_require_ui() || !s_port.wait_flush_complete ||
       !s_port.wait_flush_complete(s_port.context)) {
        ui_runtime_count(UI_DIAG_DISPLAY_FAILURE);
        ui_runtime_fault("DMA flush did not complete; buffer remains owned");
        abort(); /* Never release/reuse a buffer still owned by DMA. */
    }
    s_in_flight = false;
    lv_display_flush_ready(display);
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    if(!ui_runtime_require_ui()) { ui_runtime_fault("flush outside UiTask"); abort(); }
    uint32_t stride = lv_display_get_buf_active(display)->header.stride;
    ui_stm32_flush_status_t status = s_port.start_flush(area, pixels, stride, s_port.context);
    if(status == UI_STM32_FLUSH_ASYNC) {
        s_in_flight = true;
        if(!s_port.wait_flush_complete) {
            ui_runtime_count(UI_DIAG_DISPLAY_FAILURE);
            ui_runtime_fault("Async flush requires a completion wait port"); abort();
        }
        return;
    }
    if(status != UI_STM32_FLUSH_DONE) ui_runtime_count(UI_DIAG_DISPLAY_FAILURE);
    /* DONE/FAILED both promise that no DMA owns pixels. */
    lv_display_flush_ready(display);
}
lv_display_t *ui_stm32_display_create(const ui_stm32_display_port_t *port)
{
    if(s_display || !ui_runtime_require_ui() || !port || !port->start_flush ||
       port->width != 480 || port->height != 854 || !port->draw_buffer_a || !port->draw_buffer_b ||
       port->draw_buffer_a == port->draw_buffer_b ||
       ((uintptr_t)port->draw_buffer_a & 31U) || ((uintptr_t)port->draw_buffer_b & 31U) ||
       port->draw_buffer_bytes < (size_t)port->width * 2U ||
       port->draw_buffer_bytes >= (size_t)port->width * port->height * 2U ||
       port->draw_buffer_bytes > UINT32_MAX) return NULL;
    uintptr_t a = (uintptr_t)port->draw_buffer_a, b = (uintptr_t)port->draw_buffer_b;
    if((a < b ? b - a : a - b) < port->draw_buffer_bytes) return NULL;
    s_port = *port;
    s_display = lv_display_create(port->width, port->height);
    if(!s_display) return NULL;
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_display, port->draw_buffer_a, port->draw_buffer_b,
                          (uint32_t)port->draw_buffer_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_display, flush);
    lv_display_set_flush_wait_cb(s_display, wait_flush);
    return s_display;
}
