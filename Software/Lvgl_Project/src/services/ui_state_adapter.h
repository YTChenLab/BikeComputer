#ifndef UI_STATE_ADAPTER_H
#define UI_STATE_ADAPTER_H

#include "ui/core/ui_app_state.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UI_STATE_ADAPTER_STATUS       = 1U << 0,
    UI_STATE_ADAPTER_GNSS         = 1U << 1,
    UI_STATE_ADAPTER_SATELLITES   = 1U << 2,
    UI_STATE_ADAPTER_CONNECTIVITY = 1U << 3,
    UI_STATE_ADAPTER_CONTROL      = 1U << 4,
    UI_STATE_ADAPTER_MEDIA        = 1U << 5,
    UI_STATE_ADAPTER_NAVIGATION   = 1U << 6,
    UI_STATE_ADAPTER_ALL          = 0x7FU
} ui_state_adapter_field_t;

typedef uint32_t ui_state_adapter_mask_t;

typedef struct
{
    uint8_t clock_hour;
    uint8_t clock_minute;
    uint8_t battery_percent;
    bool battery_charging;
    uint8_t satellites;
    bool gps_enabled;

    bool wifi_enabled;
    bool wifi_connected;
    bool bluetooth_enabled;
    bool bluetooth_connected;
    char wifi_name[UI_APP_STATE_NAME_MAX];
    char bluetooth_name[UI_APP_STATE_NAME_MAX];

    uint8_t sound_level;
    uint8_t brightness_level;
    bool power_save_enabled;

    bool media_playing;
    uint8_t media_track;
    ui_media_mode_t media_mode;

    bool navigation_active;
} ui_state_adapter_snapshot_t;

void ui_state_adapter_init(void);

/*
 * Producer/task-safe latest-value mailbox. A field group submitted multiple
 * times before process() is called collapses to its newest value.
 */
bool ui_state_adapter_submit(const ui_state_adapter_snapshot_t *snapshot,
                             ui_state_adapter_mask_t fields);

/* UI-thread only: apply pending platform state to ui_app_state. */
ui_state_adapter_mask_t ui_state_adapter_process(void);
ui_state_adapter_mask_t ui_state_adapter_pending(void);

#endif /* UI_STATE_ADAPTER_H */
