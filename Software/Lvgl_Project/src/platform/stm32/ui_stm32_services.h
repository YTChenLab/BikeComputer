#ifndef UI_STM32_SERVICES_H
#define UI_STM32_SERVICES_H

#include "ui/core/ui_event.h"
#include "ui/core/ui_input.h"
#include "ui/core/ui_hardware_input.h"
#include "ui/models/ui_ride_data_model.h"
#include "ui/models/ui_weather_model.h"
#include "services/ui_command_service.h"
#include "services/ui_navigation_service.h"
#include "services/ui_ride_history_repository.h"
#include "services/ui_state_adapter.h"
#include "services/ui_preferences.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    /* Legacy hooks are optional during migration to dispatch_command. */
    bool (*request_gnss_fix)(void *context);
    bool (*save_ride)(void *context);
    bool (*retry_save)(void *context);
    bool (*dispatch_command)(const ui_command_t *command, void *context);
    bool (*request_history_page_worker)(void *context);
    void (*tick)(uint32_t elapsed_ms, void *context);
    void (*lock)(void *context);
    void (*unlock)(void *context);
    void *context;
} ui_stm32_service_port_t;

/* Call once before GNSS/storage/sensor producer tasks are started. */
bool ui_stm32_services_register(const ui_stm32_service_port_t *port);

/* Register a board storage adapter before ui_app_init(). The adapter may use
 * FatFs, littlefs or another STM32 storage implementation; UI code remains
 * independent of the concrete filesystem. */
bool ui_stm32_services_register_history_storage(
    const ui_ride_history_storage_t *storage);

/* Task-context producer APIs. ISR code must notify a task first. */
bool ui_stm32_services_publish_event(const ui_event_t *event);
void ui_stm32_services_submit_ride_data(const ui_ride_data_t *data);
bool ui_stm32_services_submit_ride_patch(const ui_ride_data_t *patch, uint32_t fields);
bool ui_stm32_services_submit_weather(const ui_weather_data_t *weather);
bool ui_stm32_services_submit_preferences(const ui_preferences_t *preferences);
void ui_stm32_services_submit_navigation(const nav_model_t *model);
bool ui_stm32_services_submit_state(
    const ui_state_adapter_snapshot_t *snapshot,
    ui_state_adapter_mask_t fields);
bool ui_stm32_services_submit_input(const ui_input_message_t *message);
void ui_stm32_services_sample_inputs(uint32_t pressed_mask, int32_t encoder_delta,
                                    uint32_t now_ms);
/* Input-task setup only. Set entries to UI_INPUT_COUNT to disable them. */
bool ui_stm32_services_set_key_map(const ui_input_event_t short_events[UI_HW_KEY_COUNT],
    const ui_input_event_t long_events[UI_HW_KEY_COUNT]);

/* Task-context completion. Optional state is applied before result delivery
 * only for a matching successful command; late acknowledgements are ignored. */
bool ui_stm32_services_complete_command(
    uint32_t command_id,
    ui_command_status_t status,
    int32_t error_code,
    const ui_state_adapter_snapshot_t *state,
    ui_state_adapter_mask_t fields);

#endif /* UI_STM32_SERVICES_H */
