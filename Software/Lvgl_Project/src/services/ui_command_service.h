#ifndef UI_COMMAND_SERVICE_H
#define UI_COMMAND_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "ui_state_adapter.h"

#define UI_COMMAND_QUEUE_MAX      16U
#define UI_COMMAND_RESULT_MAX      8U
#define UI_COMMAND_OBSERVER_MAX    8U
#define UI_COMMAND_TIMEOUT_DEFAULT 1500U

typedef enum
{
    UI_COMMAND_WIFI_SET_ENABLED = 0,
    UI_COMMAND_BLUETOOTH_SET_ENABLED,
    UI_COMMAND_GNSS_SET_ENABLED,
    UI_COMMAND_GNSS_ACQUIRE_FIX,
    UI_COMMAND_POWER_SAVE_SET_ENABLED,
    UI_COMMAND_TOUCH_LOCK_SET_ENABLED,
    UI_COMMAND_SOUND_SET_LEVEL,
    UI_COMMAND_BRIGHTNESS_SET_LEVEL,
    UI_COMMAND_MEDIA_SET_PLAYING,
    UI_COMMAND_MEDIA_SET_TRACK,
    UI_COMMAND_MEDIA_SET_MODE,
    UI_COMMAND_MEDIA_PREVIOUS,
    UI_COMMAND_MEDIA_TOGGLE_PLAYBACK,
    UI_COMMAND_MEDIA_NEXT,
    UI_COMMAND_MEDIA_CYCLE_MODE,
    UI_COMMAND_RIDE_START,
    UI_COMMAND_RIDE_PAUSE,
    UI_COMMAND_RIDE_RESUME,
    UI_COMMAND_RIDE_FINISH,
    UI_COMMAND_RIDE_SAVE,
    UI_COMMAND_RIDE_UPLOAD,
    UI_COMMAND_DISPLAY_SET_ENABLED,
    UI_COMMAND_SYSTEM_SHUTDOWN,
    UI_COMMAND_SYSTEM_RESTART,
    UI_COMMAND_COUNT
} ui_command_type_t;

typedef enum
{
    UI_COMMAND_STATUS_PENDING = 0,
    UI_COMMAND_STATUS_SUCCESS,
    UI_COMMAND_STATUS_FAILED,
    UI_COMMAND_STATUS_TIMEOUT,
    UI_COMMAND_STATUS_REJECTED
} ui_command_status_t;

typedef struct
{
    uint32_t id;
    ui_command_type_t type;
    int32_t value;
    uint32_t timeout_ms;
} ui_command_t;

typedef struct
{
    uint32_t id;
    ui_command_type_t type;
    ui_command_status_t status;
    int32_t error_code;
    int32_t value;
} ui_command_result_t;

typedef struct
{
    bool (*dispatch)(const ui_command_t *command, void *context);
    void (*tick)(uint32_t elapsed_ms, void *context);
    void *context;
    /* Optional transport cancellation for an abandoned GNSS acquisition. */
    void (*cancel)(uint32_t command_id, void *context);
} ui_command_backend_t;

typedef void (*ui_command_observer_cb_t)(const ui_command_result_t *result,
                                         void *user_data);

/* Initialize before registering a PC or STM32 backend. */
void ui_command_service_init(void);
bool ui_command_service_register_backend(const ui_command_backend_t *backend);
void ui_command_service_reset_backend(void);

/* Thread-safe producer APIs. A zero id means the queue rejected the command. */
uint32_t ui_command_service_submit(ui_command_type_t type, int32_t value,
                                   uint32_t timeout_ms);
/* Only GNSS acquisition is abortable. Recording commands require an ACK. */
bool ui_command_service_cancel_fix(uint32_t command_id);
bool ui_command_service_complete(uint32_t command_id,
                                 ui_command_status_t status,
                                 int32_t error_code);
/* The state is copied with the result and applied only when its id matches
 * the active successful command. Late, duplicate or unknown results cannot
 * roll back a newer UI state. */
bool ui_command_service_complete_with_state(uint32_t command_id,
    ui_command_status_t status, int32_t error_code,
    const ui_state_adapter_snapshot_t *state, ui_state_adapter_mask_t fields);

/* UI-thread consumer. Backend callbacks and observers run outside the lock. */
void ui_command_service_process(uint32_t elapsed_ms);

int32_t ui_command_service_subscribe(ui_command_observer_cb_t callback,
                                     void *user_data);
bool ui_command_service_unsubscribe(int32_t subscription_id);

uint8_t ui_command_service_pending(void);
bool ui_command_service_active(ui_command_t *command);
uint32_t ui_command_service_dropped(void);
uint32_t ui_command_service_unexpected_results(void);
uint32_t ui_command_service_timeouts(void);

#endif /* UI_COMMAND_SERVICE_H */
