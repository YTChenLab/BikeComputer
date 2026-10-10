#ifndef UI_STM32_SERVICES_H
#define UI_STM32_SERVICES_H

#include "ui/core/ui_event.h"
#include "ui/core/ui_input.h"
#include "ui/core/ui_physical_input.h"
#include "ui/models/ui_ride_data_model.h"
#include "ui/models/ui_weather_model.h"
#include "services/ui_command_service.h"
#include "services/ui_navigation_service.h"
#include "services/ui_nav_session.h"
#include "services/ui_nav_map_provider.h"
#include "services/ui_ride_history_repository.h"
#include "services/ui_state_adapter.h"
#include "services/ui_preferences.h"

#include "services/ui_nav_search.h"
#include "services/ui_nav_store.h"
#include "services/ui_device_info.h"
#include "ui/core/ui_runtime.h"
#include <stddef.h>
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
    void (*cancel_command)(uint32_t command_id, void *context);
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
/* Register task-owned async navigation/map ports before ui_app_init(). */
bool ui_stm32_services_register_navigation(const ui_nav_backend_t *navigation,
    const ui_nav_map_backend_t *maps);
bool ui_stm32_services_complete_navigation(const ui_nav_result_t *result);
bool ui_stm32_services_submit_nav_session(const ui_nav_session_t *session);
bool ui_stm32_services_submit_nav_catalog(const ui_nav_catalog_t *catalog, uint32_t revision);
bool ui_stm32_services_submit_map_frame(const ui_nav_map_frame_t *frame);
/* UiTask only, after receiving a recognized InputService queue record.
 * Returns false for invalid records/wrong context; POWER/lifecycle events
 * are accepted without a UI action. No GPIO sampling or gesture timing. */
bool ui_stm32_services_handle_input_event(const ui_physical_input_event_t *event);
/* UiTask applies a PowerService notification, without resending a command. */
bool ui_stm32_services_apply_display_state(bool enabled);

/* Task-context completion. Optional state is applied before result delivery
 * only for a matching successful command; late acknowledgements are ignored. */
bool ui_stm32_services_complete_command(
    uint32_t command_id,
    ui_command_status_t status,
    int32_t error_code,
    const ui_state_adapter_snapshot_t *state,
    ui_state_adapter_mask_t fields);


/* UiTask registration, before ui_app_init; same existing backend contracts. */
bool ui_stm32_services_register_preferences(const ui_preferences_port_t *port);
bool ui_stm32_services_register_search(const ui_nav_search_backend_t *backend);
bool ui_stm32_services_complete_search(const ui_place_results_t *result);
bool ui_stm32_services_submit_nav_capabilities(uint32_t capabilities);
bool ui_stm32_services_submit_device_info(const ui_device_info_t *info);
ui_ride_history_result_t ui_stm32_services_commit_ride_worker(void);
ui_ride_history_result_t ui_stm32_services_history_page_worker(void);
bool ui_stm32_services_preferences_worker(void);

/* InputTask provides calibrated coordinates; no LVGL calls on this path. */
bool ui_stm32_services_submit_touch(int32_t x, int32_t y, bool pressed, bool cancelled);
bool ui_stm32_services_get_touch(int32_t *x, int32_t *y, bool *pressed);
typedef struct {
    uint8_t kind; /* Existing ui_device_kind_t values 0..4. */
    char status[64], feedback[96];
    uint32_t color;
} ui_stm32_device_status_t;
bool ui_stm32_services_submit_device_status(const ui_stm32_device_status_t *status);
bool ui_stm32_services_take_device_status(ui_stm32_device_status_t *status);
#endif /* UI_STM32_SERVICES_H */
