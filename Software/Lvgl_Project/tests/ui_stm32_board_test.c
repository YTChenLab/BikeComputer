#include "platform/stm32/ui_stm32_startup.h"
#include "platform/ui_platform_internal.h"
#include "ui/core/ui_memory.h"
#include "ui/core/ui_input_queue.h"
#include "services/ui_navigation_service.h"
#include "ui/core/ui_theme.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static UI_DMA_RAM uint8_t draw_a[480 * 40 * 2], draw_b[480 * 40 * 2];
static UI_DMA_RAM uint16_t framebuffer[480 * 854];
static UI_LARGE_RAM uint16_t raster_a[768 * 768], raster_b[768 * 768];
static uint32_t now_ms, locks, notifications, waits, flushes, faults;
static bool worker, isr, locked, async_mode, dma_owned;
static const uint8_t *dma_pixels;
static lv_area_t dma_area;
static uint32_t dma_stride;
static ui_input_event_t events[32];
static unsigned event_count;
static ui_command_t last_command;

static uint32_t clock_ms(void *context) { (void)context; return now_ms; }
static uint32_t clock_us(void *context) { (void)context; return now_ms * 1000U; }
static bool on_ui(void *context) { (void)context; return !worker; }
static bool in_isr(void *context) { (void)context; return isr; }
static void fault(const char *reason, void *context) { (void)context; (void)reason; ++faults; }
static void lock(void *context) { (void)context; assert(!isr && !locked); locked=true; ++locks; }
static void unlock(void *context) { (void)context; assert(locked); locked=false; }
static bool resources(void *context) { (void)context; return true; }
static bool command(const ui_command_t *request, void *context)
{ (void)context; last_command=*request; ++notifications; return true; }
static bool notify(void *context) { (void)context; ++notifications; return true; }
static void sound(ui_sound_event_t event,uint8_t level,void *context)
{ (void)event; (void)level; (void)context; ++notifications; }
static bool connect(ui_device_kind_t kind,void *context)
{ (void)kind; return notify(context); }
static ui_ride_history_result_t load(ui_ride_record_t *records,size_t capacity,size_t *count,void *context)
{ (void)records; (void)capacity; (void)context; *count=0; return UI_RIDE_HISTORY_EMPTY; }
static ui_ride_history_result_t store(const ui_ride_record_t *records,size_t count,void *context)
{ (void)records; (void)count; (void)context; return UI_RIDE_HISTORY_IO_ERROR; }
static int64_t utc(void *context) { (void)context; return 0; }
static bool pref_load(uint8_t *blob,void *context) { (void)blob; (void)context; return false; }
static bool pref_store(const uint8_t *blob,void *context) { (void)blob; (void)context; return false; }
static bool nav(const ui_nav_request_t *request,void *context) { (void)request; (void)context; return false; }
static bool map(const ui_nav_map_request_t *request,void *context) { (void)request; (void)context; return false; }
static bool search(const ui_nav_search_request_t *request,void *context) { (void)request; (void)context; return false; }
static void copy_pixels(const lv_area_t *area,const uint8_t *pixels,uint32_t stride)
{
    for(int y=area->y1;y<=area->y2;++y)
        memcpy(&framebuffer[y*480+area->x1],pixels+(y-area->y1)*stride,(area->x2-area->x1+1)*2U);
}
static ui_stm32_flush_status_t start_flush(const lv_area_t *area,const uint8_t *pixels,uint32_t stride,void *context)
{
    (void)context; assert(!worker && !isr && !dma_owned); ++flushes;
    if(!async_mode) { copy_pixels(area,pixels,stride); return UI_STM32_FLUSH_DONE; }
    dma_owned=true; dma_pixels=pixels; dma_area=*area; dma_stride=stride;
    return UI_STM32_FLUSH_ASYNC;
}
static bool wait_flush(void *context)
{
    (void)context; assert(!worker && !isr && dma_owned); ++waits;
    copy_pixels(&dma_area,dma_pixels,dma_stride);
    dma_owned=false; return true; /* Simulates DMA completed before notification wake. */
}
static void collect(const ui_input_message_t *message,void *context)
{ (void)context; assert(event_count<32); events[event_count++]=message->type; }
static void drain(void) { ui_input_queue_drain(32,collect,NULL); }

int main(void)
{
    const ui_stm32_service_port_t services={.dispatch_command=command,
        .request_history_page_worker=notify,.lock=lock,.unlock=unlock};
    const ui_ride_history_storage_t history={.load=load,.store=store,.now_utc=utc};
    const ui_preferences_port_t preferences={.load=pref_load,.store=pref_store,.request_worker=notify};
    const ui_nav_backend_t navigation={.dispatch=nav};
    const ui_nav_map_backend_t maps={.request=map};
    const ui_nav_search_backend_t places={.request=search};
    const ui_device_connection_port_t devices={.scan=notify,.connect=connect};
    ui_stm32_startup_t startup={
        .runtime={.now_ms=clock_ms,.now_us=clock_us,.is_ui_task=on_ui,.is_isr=in_isr,.fault=fault},
        .resources_ready=resources,
        .display={.width=480,.height=854,.draw_buffer_a=draw_a,.draw_buffer_b=draw_b,
            .draw_buffer_bytes=sizeof(draw_a),.start_flush=start_flush,.wait_flush_complete=wait_flush},
        .services=&services,.history=&history,.preferences=&preferences,
        .navigation=&navigation,.maps=&maps,.search=&places,.devices=&devices,
        .font20=&ui_font_misans_ui_20,.font24=&ui_font_misans_ui_24,
        .sound=sound,.raster_a=raster_a,.raster_b=raster_b,.raster_pixel_count=768*768,
    };
    assert(ui_stm32_startup(&startup));
    assert(!ui_stm32_startup(&startup));
    assert(!ui_navigation_service_demo_enabled());
    ui_navigation_service_set_demo_enabled(true);
    assert(!ui_navigation_service_demo_enabled());
    ui_weather_data_t weather; assert(ui_weather_model_get(&weather));
    assert(weather.quality_known && !weather.valid && weather.temperature_c==0);
    assert(ui_stm32_datetime_set_offset_minutes(480));
    ui_calendar_date_t date;
    assert(ui_platform_local_date(INT64_C(1709164800),&date));
    assert(date.year==2024 && date.month==2 && date.day==29 && date.hour==8);
    assert(!ui_stm32_datetime_set_offset_minutes(900));
    ui_physical_input_event_t input={.key=UI_PHYSICAL_KEY_BACK,.action=UI_PHYSICAL_KEY_DOUBLE_CLICK,.timestamp_ms=10};
    assert(ui_stm32_services_handle_input_event(&input));
    assert(ui_input_queue_pending()==0); /* No second UI input queue hop. */
    input.action=UI_PHYSICAL_KEY_LONG_START;
    assert(ui_stm32_services_handle_input_event(&input));
    input.action=UI_PHYSICAL_KEY_LONG_REPEAT;
    assert(ui_stm32_services_handle_input_event(&input));
    input.action=UI_PHYSICAL_KEY_LONG_END;
    assert(ui_stm32_services_handle_input_event(&input));
    unsigned commands_before=notifications;
    input.key=UI_PHYSICAL_KEY_POWER;
    for(input.action=UI_PHYSICAL_KEY_DOWN;input.action<=UI_PHYSICAL_KEY_LONG_END;++input.action)
        assert(ui_stm32_services_handle_input_event(&input));
    assert(ui_stm32_services_apply_display_state(false));
    assert(ui_stm32_services_apply_display_state(true));
    assert(notifications==commands_before); /* Feedback cannot dispatch POWER again. */
    input.action=UI_PHYSICAL_ENCODER_DELTA; input.encoder_delta=1;
    assert(!ui_stm32_services_handle_input_event(&input));
    input.key=UI_PHYSICAL_KEY_ENCODER;
    input.encoder_delta=INT32_MIN;
    assert(!ui_stm32_services_handle_input_event(&input));
    input.key=UI_PHYSICAL_KEY_BACK; input.action=UI_PHYSICAL_KEY_CLICK; input.encoder_delta=0;
    assert(ui_stm32_services_submit_touch(40,50,true,false));
    int32_t x,y; bool pressed;
    assert(ui_stm32_services_get_touch(&x,&y,&pressed) && x==40 && y==50 && pressed);
    assert(ui_stm32_services_submit_touch(60,70,true,false));
    assert(ui_stm32_services_submit_touch(60,70,false,false));
    drain(); assert(events[0]==UI_INPUT_TOUCH_BEGIN && events[1]==UI_INPUT_TOUCH_MOVE && events[2]==UI_INPUT_TOUCH_END);
    isr=true;
    assert(!ui_stm32_services_submit_touch(0,0,true,false));
    assert(!ui_stm32_services_handle_input_event(&input));
    assert(!ui_stm32_services_apply_display_state(true));
    isr=false; worker=true;
    assert(ui_stm32_ui_task_handler()==1);
    assert(!ui_stm32_services_get_touch(&x,&y,&pressed));
    assert(!ui_stm32_services_handle_input_event(&input));
    assert(!ui_stm32_services_apply_display_state(true));
    worker=false;
    ui_input_message_t message={.type=UI_INPUT_ENCODER_CW};
    for(unsigned i=0;i<UI_INPUT_QUEUE_MAX;++i) assert(ui_input_queue_post(&message));
    assert(!ui_input_queue_post(&message));
    event_count=0; drain();
    now_ms=2400; assert(lv_tick_get()==now_ms); ui_stm32_ui_task_handler();
    assert(flushes>0 && waits==0);
    async_mode=true; lv_obj_invalidate(lv_screen_active());
    now_ms+=20; ui_stm32_ui_task_handler(); assert(waits>0);
    assert(ui_stm32_services_complete_command(UINT32_C(31337),UI_COMMAND_STATUS_FAILED,-2000,NULL,0));
    now_ms+=6000; ui_stm32_ui_task_handler();
    now_ms+=40; ui_stm32_ui_task_handler();
    ui_stm32_diagnostics_t diagnostics; assert(ui_stm32_services_get_diagnostics(&diagnostics));
    assert(diagnostics.input_queue_drop>0 && diagnostics.command_late_ack>0 && diagnostics.command_timeout>0);
    assert(diagnostics.runtime.counters[UI_DIAG_CONTEXT_REJECTED]>=3);
    assert(diagnostics.runtime.counters[UI_DIAG_MAILBOX_FAILURE]>0);
    assert(diagnostics.runtime.counters[UI_DIAG_INPUT_INVALID]>=2);
    assert(diagnostics.runtime.handler_calls>=3 && faults==0 && locks>0 && !locked);
    puts("ui_stm32_board_test: PASS (startup, mutex, semantic input, POWER feedback, touch, worker/ISR rejection, DMA ownership, clock, diagnostics)");
    return 0;
}
