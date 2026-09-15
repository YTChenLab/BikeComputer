#include "ui_stm32_services.h"

#include "services/ui_system_services.h"
#include "ui/core/ui_concurrency.h"
#include "ui/core/ui_event_bridge.h"

static ui_stm32_service_port_t s_port;

static bool request_gnss_fix(void *context)
{
    ui_stm32_service_port_t *port = context;
    return port->request_gnss_fix(port->context);
}

static bool save_ride(void *context)
{
    ui_stm32_service_port_t *port = context;
    return port->save_ride(port->context);
}

static bool retry_save(void *context)
{
    ui_stm32_service_port_t *port = context;
    if(port->retry_save != NULL) return port->retry_save(port->context);
    return port->save_ride(port->context);
}

static void tick(uint32_t elapsed_ms, void *context)
{
    ui_stm32_service_port_t *port = context;
    if(port->tick != NULL) port->tick(elapsed_ms, port->context);
}

bool ui_stm32_services_register(const ui_stm32_service_port_t *port)
{
    if(port == NULL || port->request_gnss_fix == NULL ||
       port->save_ride == NULL || port->lock == NULL ||
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
    return true;
}

bool ui_stm32_services_publish_event(const ui_event_t *event)
{
    return ui_event_bridge_post(event);
}

void ui_stm32_services_submit_ride_data(const ui_ride_data_t *data)
{
    ui_ride_data_model_submit(data);
}
