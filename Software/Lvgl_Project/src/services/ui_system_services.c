#include "ui_system_services.h"

#include <string.h>

static ui_system_services_backend_t s_backend;

void ui_system_services_register(const ui_system_services_backend_t *backend)
{
    if(backend == NULL) {
        ui_system_services_reset();
        return;
    }
    s_backend = *backend;
}

void ui_system_services_reset(void)
{
    memset(&s_backend, 0, sizeof(s_backend));
}

bool ui_system_services_request_gnss_fix(void)
{
    return s_backend.request_gnss_fix != NULL &&
           s_backend.request_gnss_fix(s_backend.context);
}

bool ui_system_services_save_ride(void)
{
    return s_backend.save_ride != NULL &&
           s_backend.save_ride(s_backend.context);
}

bool ui_system_services_retry_save(void)
{
    if(s_backend.retry_save != NULL)
        return s_backend.retry_save(s_backend.context);
    return ui_system_services_save_ride();
}

void ui_system_services_tick(uint32_t elapsed_ms)
{
    if(elapsed_ms > 0 && s_backend.tick != NULL)
        s_backend.tick(elapsed_ms, s_backend.context);
}
