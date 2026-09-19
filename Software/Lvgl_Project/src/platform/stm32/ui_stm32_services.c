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
    ui_stm32_service_port_t *port = context;
    if(port->tick != NULL) port->tick(elapsed_ms, port->context);
}

static bool dispatch_command(const ui_command_t *command, void *context)
{
    ui_stm32_service_port_t *port = context;
    return port->dispatch_command(command, port->context);
}

bool ui_stm32_services_register(const ui_stm32_service_port_t *port)
{
    if(port == NULL || port->dispatch_command == NULL ||
       port->lock == NULL ||
       port->unlock == NULL) return false;

    s_port = *port;
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
        .context = &s_port
    };
    (void)ui_command_service_register_backend(&command_backend);
    return true;
}

bool ui_stm32_services_register_history_storage(
    const ui_ride_history_storage_t *storage)
{
    return ui_ride_history_repository_register_storage(storage);
}

bool ui_stm32_services_publish_event(const ui_event_t *event)
{
    return ui_event_bridge_post(event);
}

void ui_stm32_services_submit_ride_data(const ui_ride_data_t *data)
{
    ui_ride_data_model_submit(data);
}

void ui_stm32_services_submit_navigation(const nav_model_t *model)
{
    ui_navigation_service_submit(model);
}

bool ui_stm32_services_submit_state(
    const ui_state_adapter_snapshot_t *snapshot,
    ui_state_adapter_mask_t fields)
{
    return ui_state_adapter_submit(snapshot, fields);
}

bool ui_stm32_services_submit_input(const ui_input_message_t *message)
{
    return ui_input_queue_post(message);
}

bool ui_stm32_services_complete_command(
    uint32_t command_id,
    ui_command_status_t status,
    int32_t error_code,
    const ui_state_adapter_snapshot_t *state,
    ui_state_adapter_mask_t fields)
{
    if(fields != 0U &&
       (state == NULL || !ui_state_adapter_submit(state, fields)))
        return false;
    return ui_command_service_complete(command_id, status, error_code);
}
