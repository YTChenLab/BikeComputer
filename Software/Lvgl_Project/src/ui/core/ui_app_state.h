#ifndef UI_APP_STATE_H
#define UI_APP_STATE_H

#include "ui_state_machine.h"

#include <stdbool.h>
#include <stdint.h>

#define UI_APP_STATE_OBSERVER_MAX 12
#define UI_APP_STATE_NAME_MAX     32
#define UI_APP_ROUTE_UNKNOWN      UINT8_MAX

typedef enum
{
    UI_MEDIA_MODE_SEQUENCE = 0,
    UI_MEDIA_MODE_REPEAT_ONE,
    UI_MEDIA_MODE_SHUFFLE,
    UI_MEDIA_MODE_COUNT
} ui_media_mode_t;

typedef enum
{
    UI_APP_STATE_CHANGE_NONE         = 0,
    UI_APP_STATE_CHANGE_ROUTE        = 1U << 0,
    UI_APP_STATE_CHANGE_RIDE         = 1U << 1,
    UI_APP_STATE_CHANGE_STATUS       = 1U << 2,
    UI_APP_STATE_CHANGE_CONNECTIVITY = 1U << 3,
    UI_APP_STATE_CHANGE_CONTROL      = 1U << 4,
    UI_APP_STATE_CHANGE_MEDIA        = 1U << 5,
    UI_APP_STATE_CHANGE_NAVIGATION   = 1U << 6,
    UI_APP_STATE_CHANGE_ALL          = 0x7FU
} ui_app_state_change_t;

typedef uint32_t ui_app_state_change_mask_t;

typedef struct
{
    uint8_t current_route;
    ui_ride_state_t ride_state;

    uint8_t clock_hour;
    uint8_t clock_minute;
    uint8_t battery_percent;
    uint8_t satellites;
    bool battery_charging;
    bool clock_valid, battery_valid;

    bool wifi_enabled;
    bool wifi_connected;
    bool bluetooth_enabled;
    bool bluetooth_connected;
    bool gps_enabled;
    bool power_save_enabled;
    bool touch_locked;
    char wifi_name[UI_APP_STATE_NAME_MAX];
    char bluetooth_name[UI_APP_STATE_NAME_MAX];

    uint8_t sound_level;
    uint8_t brightness_level;

    bool media_playing;
    uint8_t media_track;
    ui_media_mode_t media_mode;

    bool navigation_active;
    uint32_t version;
} ui_app_state_t;

typedef void (*ui_app_state_observer_cb_t)(
    const ui_app_state_t *state,
    ui_app_state_change_mask_t changed,
    void *user_data);

void ui_app_state_init(void);
bool ui_app_state_get(ui_app_state_t *state);

int32_t ui_app_state_subscribe(ui_app_state_change_mask_t mask,
                               ui_app_state_observer_cb_t callback,
                               void *user_data);
bool ui_app_state_unsubscribe(int32_t subscription_id);

void ui_app_state_set_route(uint8_t route);
void ui_app_state_set_ride_state(ui_ride_state_t ride_state);
void ui_app_state_set_status(uint8_t hour, uint8_t minute,
                             uint8_t battery_percent, bool charging,
                             bool gps_enabled, uint8_t satellites);
void ui_app_state_set_telemetry(uint8_t hour, uint8_t minute,
                                uint8_t battery_percent, bool charging,
                                uint8_t satellites);
void ui_app_state_set_clock(uint8_t hour, uint8_t minute);
void ui_app_state_set_status_validity(bool clock_valid, bool battery_valid);
void ui_app_state_set_battery(uint8_t percent, bool charging);
void ui_app_state_set_gnss(bool enabled, uint8_t satellites);
void ui_app_state_set_satellites(uint8_t satellites);
void ui_app_state_set_connectivity(bool wifi_enabled, bool wifi_connected,
                                   const char *wifi_name,
                                   bool bluetooth_enabled,
                                   bool bluetooth_connected,
                                   const char *bluetooth_name);
void ui_app_state_set_controls(uint8_t sound_level,
                               uint8_t brightness_level,
                               bool power_save_enabled);
void ui_app_state_set_touch_locked(bool locked);
void ui_app_state_set_media(bool playing, uint8_t track,
                            ui_media_mode_t mode);
void ui_app_state_set_navigation_active(bool active);

#endif /* UI_APP_STATE_H */
