#ifndef UI_SYSTEM_SERVICES_H
#define UI_SYSTEM_SERVICES_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool (*request_gnss_fix)(void *context);
    bool (*save_ride)(void *context);
    bool (*retry_save)(void *context);
    void (*tick)(uint32_t elapsed_ms, void *context);
    void *context;
} ui_system_services_backend_t;

void ui_system_services_register(const ui_system_services_backend_t *backend);
void ui_system_services_reset(void);
bool ui_system_services_request_gnss_fix(void);
bool ui_system_services_save_ride(void);
bool ui_system_services_retry_save(void);
void ui_system_services_tick(uint32_t elapsed_ms);

#endif /* UI_SYSTEM_SERVICES_H */
