#ifndef UI_STM32_STARTUP_H
#define UI_STM32_STARTUP_H
#include "ui_stm32_services.h"
#include "ui_stm32_display.h"
#include "ui/core/ui_text.h"
#include "ui/core/ui_sound.h"
#include "ui/core/ui_background.h"
#include "ui/pages/ui_devices_page.h"

typedef struct {
    ui_runtime_port_t runtime;
    bool (*resources_ready)(void *context); /* BSP readiness, not HAL initialization. */
    void *resources_context;
    ui_stm32_display_port_t display;
    const ui_stm32_service_port_t *services;
    const ui_ride_history_storage_t *history;
    const ui_preferences_port_t *preferences;
    const ui_nav_backend_t *navigation;
    const ui_nav_map_backend_t *maps;
    const ui_nav_search_backend_t *search;
    uint32_t navigation_capabilities; /* Existing UI_NAV_CAP_*; real supported operations only. */
    const ui_device_connection_port_t *devices;
    const ui_background_port_t *background; /* Optional bitmap theme resource. */
    const lv_font_t *font20, *font24;
    ui_sound_backend_cb_t sound;
    void *sound_context;
    uint16_t *raster_a, *raster_b;
    size_t raster_pixel_count;
} ui_stm32_startup_t;
typedef struct {
    ui_runtime_diagnostics_t runtime;
    uint32_t input_queue_drop, event_bridge_drop, event_forward_failure;
    uint32_t command_drop, command_timeout, command_late_ack;
} ui_stm32_diagnostics_t;

/* Once, on UiTask after BSP SDRAM/display/storage readiness, before producers.
 * A failed partial initialization requires a board reset, not a second app_init.
 * Backend requests only queue work; startup load/RTC callbacks are the exception. */
bool ui_stm32_startup(const ui_stm32_startup_t *startup);
/* UiTask only; caller waits/sleeps using the returned LVGL deadline. */
uint32_t ui_stm32_ui_task_handler(void);
bool ui_stm32_services_get_diagnostics(ui_stm32_diagnostics_t *diagnostics);
bool ui_stm32_datetime_set_offset_minutes(int minutes);
#endif
