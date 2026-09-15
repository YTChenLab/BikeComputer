#ifndef UI_STM32_SERVICES_H
#define UI_STM32_SERVICES_H

#include "ui/core/ui_event.h"
#include "ui/models/ui_ride_data_model.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool (*request_gnss_fix)(void *context);
    bool (*save_ride)(void *context);
    bool (*retry_save)(void *context);
    void (*tick)(uint32_t elapsed_ms, void *context);
    void (*lock)(void *context);
    void (*unlock)(void *context);
    void *context;
} ui_stm32_service_port_t;

/* Call once before GNSS/storage/sensor producer tasks are started. */
bool ui_stm32_services_register(const ui_stm32_service_port_t *port);

/* Task-context producer APIs. ISR code must notify a task first. */
bool ui_stm32_services_publish_event(const ui_event_t *event);
void ui_stm32_services_submit_ride_data(const ui_ride_data_t *data);

#endif /* UI_STM32_SERVICES_H */
