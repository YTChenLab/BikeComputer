#include "ui_stm32_startup.h"
#include "ui_stm32_input.h"
#include "ui/core/ui_app.h"
#include "ui/core/ui_event_bridge.h"
#include "ui/core/ui_input_queue.h"
#include "services/ui_nav_raster.h"
#include "services/ui_offline_tiles.h"
#include <stdint.h>

static bool s_started, s_attempted;
static bool overlap(const void *a,size_t a_bytes,const void *b,size_t b_bytes)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x < y ? y-x < a_bytes : x-y < b_bytes;
}
bool ui_stm32_startup(const ui_stm32_startup_t *p)
{
    if(s_attempted || !p || !p->resources_ready || !p->resources_ready(p->resources_context) ||
       !p->services || !p->services->dispatch_command || !p->services->lock || !p->services->unlock ||
       !p->services->request_history_page_worker || !p->history || !p->history->load ||
       !p->history->store || !p->history->now_utc || !p->preferences || !p->preferences->load ||
       !p->preferences->store || !p->preferences->request_worker ||
       !p->navigation || !p->navigation->dispatch || !p->maps || !p->maps->request ||
       !p->search || !p->search->request || !p->devices || !p->devices->scan || !p->devices->connect ||
       !p->font20 || !p->font24 || !p->sound || !p->raster_a || !p->raster_b ||
       p->raster_a == p->raster_b || p->raster_pixel_count < UI_OFFLINE_RASTER_SIZE * UI_OFFLINE_RASTER_SIZE ||
       ((uintptr_t)p->raster_a & 31U) || ((uintptr_t)p->raster_b & 31U) ||
       p->raster_pixel_count > SIZE_MAX/sizeof(uint16_t) ||
       (p->navigation_capabilities & ~UINT32_C(511)) ||
       !p->font20->get_glyph_dsc || !p->font24->get_glyph_dsc) return false;
    size_t raster_bytes=p->raster_pixel_count*sizeof(uint16_t);
    if(overlap(p->raster_a,raster_bytes,p->raster_b,raster_bytes) ||
       overlap(p->raster_a,raster_bytes,p->display.draw_buffer_a,p->display.draw_buffer_bytes) ||
       overlap(p->raster_a,raster_bytes,p->display.draw_buffer_b,p->display.draw_buffer_bytes) ||
       overlap(p->raster_b,raster_bytes,p->display.draw_buffer_a,p->display.draw_buffer_bytes) ||
       overlap(p->raster_b,raster_bytes,p->display.draw_buffer_b,p->display.draw_buffer_bytes)) return false;
    if(!ui_runtime_register(&p->runtime)) return false;
    s_attempted = true;
    if(!ui_stm32_services_register(p->services)) return false;
    if(!ui_stm32_services_register_history_storage(p->history) ||
       !ui_stm32_services_register_preferences(p->preferences) ||
       !ui_stm32_services_register_navigation(p->navigation,p->maps) ||
       !ui_stm32_services_register_search(p->search)) return false;
    if(!ui_stm32_services_submit_nav_capabilities(p->navigation_capabilities)) return false;
    ui_nav_raster_configure(p->raster_a,p->raster_b,p->raster_pixel_count);
    lv_init();
    lv_tick_set_cb(ui_runtime_now_ms); /* No additional lv_tick_inc source. */
    if(!ui_stm32_display_create(&p->display) || !ui_stm32_input_create_pointer()) return false;
    ui_text_register_fonts(p->font20,p->font24);
    ui_sound_set_backend(p->sound,p->sound_context);
    ui_devices_page_register_connection_port(p->devices);
    if(p->background && !ui_background_register_port(p->background)) return false;
    ui_app_init();
    ui_state_adapter_snapshot_t initial = {0};
    (void)ui_stm32_services_submit_state(&initial, UI_STATE_ADAPTER_CONNECTIVITY | UI_STATE_ADAPTER_GNSS);
    s_started = true;
    return true;
}
uint32_t ui_stm32_ui_task_handler(void)
{
    if(!s_started || !ui_runtime_require_ui()) return 1;
    ui_stm32_device_status_t status;
    while(ui_stm32_services_take_device_status(&status)) {
        if(status.status[0]) ui_devices_page_set_status((ui_device_kind_t)status.kind,status.status,status.color);
        if(status.feedback[0]) ui_devices_page_set_feedback(status.feedback);
    }
    uint32_t before = ui_runtime_now_us();
    uint32_t next = lv_timer_handler();
    ui_runtime_record_handler(ui_runtime_now_us() - before);
    return next;
}
bool ui_stm32_services_get_diagnostics(ui_stm32_diagnostics_t *out)
{
    if(!out || !ui_runtime_require_task()) return false;
    ui_runtime_get_diagnostics(&out->runtime);
    out->input_queue_drop = ui_input_queue_dropped();
    out->event_bridge_drop = ui_event_bridge_dropped_count();
    out->event_forward_failure = ui_event_bridge_forward_failures();
    out->command_drop = ui_command_service_dropped();
    out->command_timeout = ui_command_service_timeouts();
    out->command_late_ack = ui_command_service_unexpected_results();
    return true;
}
