#include <string.h>
#include "ui_stm32_services.h"

#include "services/ui_command_service.h"
#include "services/ui_system_services.h"
#include "ui/core/ui_concurrency.h"
#include "ui/core/ui_event_bridge.h"
#include "ui/core/ui_input_queue.h"
#include "services/ui_navigation_service.h"
#include "services/ui_ride_history_repository.h"
#include "services/ui_state_adapter.h"

static ui_stm32_service_port_t s_port;
static struct { int32_t x,y; bool pressed; } s_touch;
static ui_stm32_device_status_t s_devices[5];
static uint8_t s_device_pending;
static bool checked(bool success)
{ if(!success) ui_runtime_count(UI_DIAG_MAILBOX_FAILURE); return success; }

bool ui_stm32_services_register_navigation(const ui_nav_backend_t *navigation,
    const ui_nav_map_backend_t *maps)
{
    if(!ui_runtime_require_ui()) return false;
    return ui_nav_session_register_backend(navigation) && ui_nav_map_provider_register(maps);
}
bool ui_stm32_services_complete_navigation(const ui_nav_result_t *result)
{
    if(!ui_runtime_require_task()) { ui_runtime_count(UI_DIAG_MAILBOX_FAILURE); return false; }
    if(!result || (result->route.demo && !ui_navigation_service_demo_enabled())) return checked(false);
    return checked(ui_nav_session_complete(result));
}
bool ui_stm32_services_submit_nav_session(const ui_nav_session_t *session)
{
    if(!ui_runtime_require_task()) return checked(false);
    if(!session || (session->source==UI_NAV_SOURCE_DEMO && !ui_navigation_service_demo_enabled())) return checked(false);
    return checked(ui_nav_session_submit(session));
}
bool ui_stm32_services_submit_map_frame(const ui_nav_map_frame_t *frame)
{
    if(!ui_runtime_require_task()) return checked(false);
    if(!frame || (frame->demo && !ui_navigation_service_demo_enabled())) return checked(false);
    return checked(ui_nav_map_provider_submit(frame));
}
bool ui_stm32_services_submit_nav_catalog(const ui_nav_catalog_t *catalog, uint32_t revision)
{
    if(!ui_runtime_require_task()) return checked(false);
    if(catalog && !ui_navigation_service_demo_enabled())
        for(unsigned i=0;i<catalog->count && i<UI_NAV_CATALOG_MAX;++i)
            if(catalog->items[i].demo) return checked(false);
    return checked(ui_nav_session_submit_catalog(catalog,revision));
}

bool ui_stm32_services_submit_preferences(const ui_preferences_t *preferences)
{
    if(!ui_runtime_require_task()) { ui_runtime_count(UI_DIAG_MAILBOX_FAILURE); return false; }
    return checked(ui_preferences_submit(preferences));
}

static bool request_gnss_fix(void *context)
{
    ui_stm32_service_port_t *port = context;
    return port->request_gnss_fix != NULL &&
           port->request_gnss_fix(port->context);
}

static bool save_ride(void *context)
{
    ui_stm32_service_port_t *port = context;
    return port->save_ride != NULL && port->save_ride(port->context);
}

static bool retry_save(void *context)
{
    ui_stm32_service_port_t *port = context;
    if(port->retry_save != NULL) return port->retry_save(port->context);
    return port->save_ride != NULL && port->save_ride(port->context);
}

static void tick(uint32_t elapsed_ms, void *context)
{
    (void)ui_ride_history_repository_process(NULL);
    ui_stm32_service_port_t *port = context;
    if(port->tick != NULL) port->tick(elapsed_ms, port->context);
}

static void cancel_command(uint32_t id,void *context)
{ ui_stm32_service_port_t *p=context; if(p->cancel_command) p->cancel_command(id,p->context); }

static bool dispatch_command(const ui_command_t *command, void *context)
{
    ui_stm32_service_port_t *port = context;
    if(command->type == UI_COMMAND_RIDE_SAVE && !ui_ride_history_repository_begin_commit()) return false;
    bool accepted = port->dispatch_command(command, port->context);
    if(!accepted && command->type == UI_COMMAND_RIDE_SAVE)
        (void)ui_ride_history_repository_abort_commit();
    return accepted;
}

bool ui_stm32_services_register(const ui_stm32_service_port_t *port)
{
    if(port == NULL || port->dispatch_command == NULL ||
       port->lock == NULL ||
       port->unlock == NULL) return false;

    if(!ui_runtime_require_ui()) return false;
#if defined(UI_TARGET_STM32)
    if(!ui_runtime_registered()) return false;
#endif
    s_port = *port;
    (void)ui_ride_history_repository_register_page_worker(s_port.request_history_page_worker,s_port.context);
    ui_navigation_service_set_demo_enabled(false);
    ui_concurrency_set_port(s_port.lock, s_port.unlock, s_port.context);
    const ui_system_services_backend_t backend = {
        .request_gnss_fix = request_gnss_fix,
        .save_ride = save_ride,
        .retry_save = retry_save,
        .tick = tick,
        .context = &s_port
    };
    ui_system_services_register(&backend);

    ui_command_service_init();
    const ui_command_backend_t command_backend = {
        .dispatch = dispatch_command,
        .cancel = cancel_command,
        .context = &s_port
    };
    (void)ui_command_service_register_backend(&command_backend);
    return true;
}

bool ui_stm32_services_register_history_storage(
    const ui_ride_history_storage_t *storage)
{
    return ui_runtime_require_ui() && ui_ride_history_repository_register_storage(storage);
}

bool ui_stm32_services_publish_event(const ui_event_t *event)
{
    if(!ui_runtime_require_task()) { ui_runtime_count(UI_DIAG_MAILBOX_FAILURE); return false; }
    return checked(ui_event_bridge_post(event));
}

void ui_stm32_services_submit_ride_data(const ui_ride_data_t *data)
{
    if(!ui_runtime_require_task()) { ui_runtime_count(UI_DIAG_MAILBOX_FAILURE); return; }
#if defined(UI_TARGET_STM32)
    if(!data || !data->quality_known) { checked(false); return; }
#endif
    (void)checked(ui_ride_data_model_submit_checked(data));
}

void ui_stm32_services_submit_navigation(const nav_model_t *model)
{
    if(!ui_runtime_require_task()) { ui_runtime_count(UI_DIAG_MAILBOX_FAILURE); return; }
    (void)checked(ui_navigation_service_submit_checked(model));
}

bool ui_stm32_services_submit_ride_patch(const ui_ride_data_t *patch, uint32_t fields)
{
    if(!ui_runtime_require_task()) { ui_runtime_count(UI_DIAG_MAILBOX_FAILURE); return false; }
    return checked(ui_ride_data_model_submit_patch(patch, fields));
}

bool ui_stm32_services_submit_weather(const ui_weather_data_t *weather)
{
    if(!ui_runtime_require_task()) { ui_runtime_count(UI_DIAG_MAILBOX_FAILURE); return false; }
    return checked(ui_weather_model_submit(weather));
}

bool ui_stm32_services_submit_state(
    const ui_state_adapter_snapshot_t *snapshot,
    ui_state_adapter_mask_t fields)
{
    if(!ui_runtime_require_task()) { ui_runtime_count(UI_DIAG_MAILBOX_FAILURE); return false; }
    return checked(ui_state_adapter_submit(snapshot, fields));
}

bool ui_stm32_services_complete_command(
    uint32_t command_id,
    ui_command_status_t status,
    int32_t error_code,
    const ui_state_adapter_snapshot_t *state,
    ui_state_adapter_mask_t fields)
{
    if(!ui_runtime_require_task()) { ui_runtime_count(UI_DIAG_MAILBOX_FAILURE); return false; }
    return checked(ui_command_service_complete_with_state(command_id, status,
                                                  error_code, state, fields));
}

bool ui_stm32_services_register_preferences(const ui_preferences_port_t *port)
{ return ui_runtime_require_ui() && ui_preferences_register(port); }
bool ui_stm32_services_register_search(const ui_nav_search_backend_t *backend)
{ return ui_runtime_require_ui() && ui_nav_search_register(backend); }
bool ui_stm32_services_complete_search(const ui_place_results_t *result)
{ return checked(ui_runtime_require_task() && ui_nav_search_submit(result)); }
bool ui_stm32_services_submit_nav_capabilities(uint32_t capabilities)
{
    if(!ui_runtime_require_task() || (capabilities & ~UINT32_C(511))) return checked(false);
    ui_nav_store_set_capabilities(capabilities);
    return true;
}
bool ui_stm32_services_submit_device_info(const ui_device_info_t *info)
{ return checked(ui_runtime_require_task() && ui_device_info_submit(info)); }
ui_ride_history_result_t ui_stm32_services_commit_ride_worker(void)
{ return ui_runtime_require_worker() ? ui_ride_history_repository_commit_worker() : UI_RIDE_HISTORY_INVALID_DATA; }
ui_ride_history_result_t ui_stm32_services_history_page_worker(void)
{ return ui_runtime_require_worker() ? ui_ride_history_repository_page_worker() : UI_RIDE_HISTORY_INVALID_DATA; }
bool ui_stm32_services_preferences_worker(void)
{ if(!ui_runtime_require_worker()) return false; ui_preferences_worker(); return true; }
bool ui_stm32_services_submit_touch(int32_t x,int32_t y,bool pressed,bool cancelled)
{
    if(!ui_runtime_require_task() || x < 0 || x >= 480 || y < 0 || y >= 854) return checked(false);
    ui_concurrency_lock();
    bool before = s_touch.pressed;
    bool moved = x != s_touch.x || y != s_touch.y;
    s_touch.x=x; s_touch.y=y; s_touch.pressed=pressed && !cancelled;
    ui_concurrency_unlock();
    ui_input_event_t type = cancelled ? UI_INPUT_TOUCH_CANCEL :
        pressed && !before ? UI_INPUT_TOUCH_BEGIN : !pressed && before ? UI_INPUT_TOUCH_END :
        pressed && moved ? UI_INPUT_TOUCH_MOVE : UI_INPUT_COUNT;
    if(type == UI_INPUT_COUNT) return true;
    ui_input_message_t message={.type=type,.x=x,.y=y};
    return checked(ui_input_queue_post(&message));
}
bool ui_stm32_services_get_touch(int32_t *x,int32_t *y,bool *pressed)
{
    if(!ui_runtime_require_ui() || !x || !y || !pressed) return false;
    ui_concurrency_lock(); *x=s_touch.x; *y=s_touch.y; *pressed=s_touch.pressed;
    ui_concurrency_unlock(); return true;
}
bool ui_stm32_services_submit_device_status(const ui_stm32_device_status_t *status)
{
    if(!ui_runtime_require_task() || !status || status->kind >= 5) return checked(false);
    ui_concurrency_lock(); s_devices[status->kind]=*status;
    s_devices[status->kind].status[63]=0; s_devices[status->kind].feedback[95]=0;
    s_device_pending |= (uint8_t)(1U << status->kind); ui_concurrency_unlock(); return true;
}
bool ui_stm32_services_take_device_status(ui_stm32_device_status_t *status)
{
    if(!ui_runtime_require_ui() || !status) return false;
    ui_concurrency_lock();
    for(unsigned i=0;i<5;++i) if(s_device_pending & (1U << i)) {
        *status=s_devices[i]; s_device_pending &= (uint8_t)~(1U << i);
        ui_concurrency_unlock(); return true;
    }
    ui_concurrency_unlock(); return false;
}
