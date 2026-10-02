#ifndef UI_BOARD_ADAPTER_EXAMPLE_H
#define UI_BOARD_ADAPTER_EXAMPLE_H
#include "platform/stm32/ui_stm32_services.h"
#include "services/ui_device_info.h"

/* This example is glue for actual board services, not a HAL implementation.
 * Queue hooks must copy commands and return immediately on the UI task. */
typedef struct {
    ui_stm32_service_port_t services;
    ui_ride_history_storage_t history;
    ui_preferences_port_t preferences;
} bike_ui_ports_t;

/* After LVGL/display/RTOS queues are ready; before ui_app_init and producers. */
bool bike_ui_attach_ports(const bike_ui_ports_t *ports);
/* Fusion-task entry. Return value validates the contract, not render completion. */
bool bike_ui_publish_snapshot(const ui_ride_data_t *snapshot);
/* Storage-task entries, selected by the queued job type. */
/* If false, retain retry_result and retry its ACK; do not repeat the I/O. */
bool bike_ui_save_worker(uint32_t command_id, ui_command_result_t *retry_result);
void bike_ui_history_page_worker(void);
void bike_ui_preferences_worker(void);
#endif
