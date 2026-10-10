#ifndef UI_STM32_DISPLAY_H
#define UI_STM32_DISPLAY_H
#include "lvgl.h"
#include <stddef.h>

typedef enum { UI_STM32_FLUSH_DONE, UI_STM32_FLUSH_ASYNC, UI_STM32_FLUSH_FAILED } ui_stm32_flush_status_t;
typedef struct {
    uint16_t width, height;
    void *draw_buffer_a, *draw_buffer_b;
    size_t draw_buffer_bytes; /* Each buffer: < full framebuffer, 32-byte aligned. */
    ui_stm32_flush_status_t (*start_flush)(const lv_area_t *area,
        const uint8_t *pixels, uint32_t stride_bytes, void *context);
    /* UiTask only: wait for DMA completion notification, with a BSP deadline.
     * True means the DMA no longer owns pixels. False is a fatal display fault.
     * ISR only gives a FromISR notification; it never calls flush_ready. */
    bool (*wait_flush_complete)(void *context);
    void *context;
} ui_stm32_display_port_t;
lv_display_t *ui_stm32_display_create(const ui_stm32_display_port_t *port);
#endif
