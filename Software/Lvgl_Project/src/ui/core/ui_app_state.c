#include "ui_app_state.h"

#include "ui_concurrency.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    int32_t id;
    ui_app_state_change_mask_t mask;
    ui_app_state_observer_cb_t callback;
    void *user_data;
} ui_app_state_observer_t;

typedef struct
{
    ui_app_state_t state;
    ui_app_state_observer_t observers[UI_APP_STATE_OBSERVER_MAX];
    uint8_t observer_count;
    int32_t next_observer_id;
} ui_app_state_context_t;

static ui_app_state_context_t s_app_state;

static uint8_t clamp_percent(uint8_t value)
{
    return value > 100U ? 100U : value;
}

static void copy_name(char destination[UI_APP_STATE_NAME_MAX],
                      const char *source)
{
    if(source == NULL) source = "";
    strncpy(destination, source, UI_APP_STATE_NAME_MAX - 1U);
    destination[UI_APP_STATE_NAME_MAX - 1U] = '\0';
}

static void notify(ui_app_state_change_mask_t changed)
{
    if(changed == UI_APP_STATE_CHANGE_NONE) return;

    ui_app_state_t snapshot;
    ui_app_state_observer_t observers[UI_APP_STATE_OBSERVER_MAX];
    uint8_t observer_count;

    ui_concurrency_lock();
    s_app_state.state.version++;
    snapshot = s_app_state.state;
    observer_count = s_app_state.observer_count;
    memcpy(observers, s_app_state.observers,
           observer_count * sizeof(observers[0]));
    ui_concurrency_unlock();

    for(uint8_t index = 0; index < observer_count; index++) {
        if((observers[index].mask & changed) != 0U) {
            observers[index].callback(&snapshot, changed,
                                      observers[index].user_data);
        }
    }
}

void ui_app_state_init(void)
{
    ui_concurrency_lock();
    memset(&s_app_state, 0, sizeof(s_app_state));
    s_app_state.next_observer_id = 1;
    s_app_state.state.current_route = UI_APP_ROUTE_UNKNOWN;
    s_app_state.state.ride_state = UI_RIDE_STATE_IDLE;
    s_app_state.state.clock_hour = 10;
    s_app_state.state.clock_minute = 24;
    s_app_state.state.battery_percent = 82;
    s_app_state.state.satellites = 12;
    s_app_state.state.wifi_enabled = true;
    s_app_state.state.wifi_connected = true;
    s_app_state.state.bluetooth_enabled = true;
    s_app_state.state.bluetooth_connected = true;
    s_app_state.state.gps_enabled = true;
    s_app_state.state.sound_level = 62;
    s_app_state.state.brightness_level = 75;
    copy_name(s_app_state.state.wifi_name, "Bike-SpeedView");
    copy_name(s_app_state.state.bluetooth_name, "Riding Sensors");
    ui_concurrency_unlock();
}

bool ui_app_state_get(ui_app_state_t *state)
{
    if(state == NULL) return false;
    ui_concurrency_lock();
    *state = s_app_state.state;
    ui_concurrency_unlock();
    return true;
}

int32_t ui_app_state_subscribe(ui_app_state_change_mask_t mask,
                               ui_app_state_observer_cb_t callback,
                               void *user_data)
{
    if(mask == UI_APP_STATE_CHANGE_NONE || callback == NULL) return -1;
    ui_concurrency_lock();
    if(s_app_state.observer_count >= UI_APP_STATE_OBSERVER_MAX) {
        ui_concurrency_unlock();
        return -1;
    }
    ui_app_state_observer_t *observer =
        &s_app_state.observers[s_app_state.observer_count++];
    observer->id = s_app_state.next_observer_id++;
    observer->mask = mask;
    observer->callback = callback;
    observer->user_data = user_data;
    int32_t id = observer->id;
    ui_concurrency_unlock();
    return id;
}

bool ui_app_state_unsubscribe(int32_t subscription_id)
{
    ui_concurrency_lock();
    for(uint8_t index = 0; index < s_app_state.observer_count; index++) {
        if(s_app_state.observers[index].id != subscription_id) continue;
        for(uint8_t next = index + 1U;
            next < s_app_state.observer_count; next++) {
            s_app_state.observers[next - 1U] = s_app_state.observers[next];
        }
        s_app_state.observer_count--;
        ui_concurrency_unlock();
        return true;
    }
    ui_concurrency_unlock();
    return false;
}

void ui_app_state_set_route(uint8_t route)
{
    ui_concurrency_lock();
    bool changed = s_app_state.state.current_route != route;
    s_app_state.state.current_route = route;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_ROUTE);
}

void ui_app_state_set_ride_state(ui_ride_state_t ride_state)
{
    ui_concurrency_lock();
    bool changed = s_app_state.state.ride_state != ride_state;
    s_app_state.state.ride_state = ride_state;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_RIDE);
}

void ui_app_state_set_status(uint8_t hour, uint8_t minute,
                             uint8_t battery_percent, bool charging,
                             bool gps_enabled, uint8_t satellites)
{
    hour = (uint8_t)(hour % 24U);
    minute = (uint8_t)(minute % 60U);
    battery_percent = clamp_percent(battery_percent);
    ui_concurrency_lock();
    bool changed = s_app_state.state.clock_hour != hour ||
                   s_app_state.state.clock_minute != minute ||
                   s_app_state.state.battery_percent != battery_percent ||
                   s_app_state.state.battery_charging != charging ||
                   s_app_state.state.gps_enabled != gps_enabled ||
                   s_app_state.state.satellites != satellites;
    s_app_state.state.clock_hour = hour;
    s_app_state.state.clock_minute = minute;
    s_app_state.state.battery_percent = battery_percent;
    s_app_state.state.battery_charging = charging;
    s_app_state.state.gps_enabled = gps_enabled;
    s_app_state.state.satellites = satellites;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_STATUS |
                       UI_APP_STATE_CHANGE_CONNECTIVITY);
}

void ui_app_state_set_telemetry(uint8_t hour, uint8_t minute,
                                uint8_t battery_percent, bool charging,
                                uint8_t satellites)
{
    hour = (uint8_t)(hour % 24U);
    minute = (uint8_t)(minute % 60U);
    battery_percent = clamp_percent(battery_percent);
    ui_concurrency_lock();
    bool changed = s_app_state.state.clock_hour != hour ||
                   s_app_state.state.clock_minute != minute ||
                   s_app_state.state.battery_percent != battery_percent ||
                   s_app_state.state.battery_charging != charging ||
                   s_app_state.state.satellites != satellites;
    s_app_state.state.clock_hour = hour;
    s_app_state.state.clock_minute = minute;
    s_app_state.state.battery_percent = battery_percent;
    s_app_state.state.battery_charging = charging;
    s_app_state.state.satellites = satellites;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_STATUS);
}

void ui_app_state_set_clock(uint8_t hour, uint8_t minute)
{
    hour = (uint8_t)(hour % 24U);
    minute = (uint8_t)(minute % 60U);
    ui_concurrency_lock();
    bool changed = s_app_state.state.clock_hour != hour ||
                   s_app_state.state.clock_minute != minute;
    s_app_state.state.clock_hour = hour;
    s_app_state.state.clock_minute = minute;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_STATUS);
}

void ui_app_state_set_battery(uint8_t percent, bool charging)
{
    percent = clamp_percent(percent);
    ui_concurrency_lock();
    bool changed = s_app_state.state.battery_percent != percent ||
                   s_app_state.state.battery_charging != charging;
    s_app_state.state.battery_percent = percent;
    s_app_state.state.battery_charging = charging;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_STATUS);
}

void ui_app_state_set_gnss(bool enabled, uint8_t satellites)
{
    ui_concurrency_lock();
    bool changed = s_app_state.state.gps_enabled != enabled ||
                   s_app_state.state.satellites != satellites;
    s_app_state.state.gps_enabled = enabled;
    s_app_state.state.satellites = satellites;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_STATUS |
                       UI_APP_STATE_CHANGE_CONNECTIVITY);
}

void ui_app_state_set_satellites(uint8_t satellites)
{
    ui_concurrency_lock();
    bool changed = s_app_state.state.satellites != satellites;
    s_app_state.state.satellites = satellites;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_STATUS);
}

void ui_app_state_set_connectivity(bool wifi_enabled, bool wifi_connected,
                                   const char *wifi_name,
                                   bool bluetooth_enabled,
                                   bool bluetooth_connected,
                                   const char *bluetooth_name)
{
    char next_wifi_name[UI_APP_STATE_NAME_MAX];
    char next_bluetooth_name[UI_APP_STATE_NAME_MAX];
    copy_name(next_wifi_name, wifi_name);
    copy_name(next_bluetooth_name, bluetooth_name);

    ui_concurrency_lock();
    bool changed = s_app_state.state.wifi_enabled != wifi_enabled ||
                   s_app_state.state.wifi_connected != wifi_connected ||
                   s_app_state.state.bluetooth_enabled != bluetooth_enabled ||
                   s_app_state.state.bluetooth_connected != bluetooth_connected ||
                   strcmp(s_app_state.state.wifi_name, next_wifi_name) != 0 ||
                   strcmp(s_app_state.state.bluetooth_name,
                          next_bluetooth_name) != 0;
    s_app_state.state.wifi_enabled = wifi_enabled;
    s_app_state.state.wifi_connected = wifi_connected;
    s_app_state.state.bluetooth_enabled = bluetooth_enabled;
    s_app_state.state.bluetooth_connected = bluetooth_connected;
    copy_name(s_app_state.state.wifi_name, next_wifi_name);
    copy_name(s_app_state.state.bluetooth_name, next_bluetooth_name);
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_CONNECTIVITY);
}

void ui_app_state_set_controls(uint8_t sound_level,
                               uint8_t brightness_level,
                               bool power_save_enabled)
{
    sound_level = clamp_percent(sound_level);
    brightness_level = clamp_percent(brightness_level);
    ui_concurrency_lock();
    bool changed = s_app_state.state.sound_level != sound_level ||
                   s_app_state.state.brightness_level != brightness_level ||
                   s_app_state.state.power_save_enabled != power_save_enabled;
    s_app_state.state.sound_level = sound_level;
    s_app_state.state.brightness_level = brightness_level;
    s_app_state.state.power_save_enabled = power_save_enabled;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_CONTROL);
}

void ui_app_state_set_touch_locked(bool locked)
{
    ui_concurrency_lock();
    bool changed = s_app_state.state.touch_locked != locked;
    s_app_state.state.touch_locked = locked;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_CONTROL);
}

void ui_app_state_set_media(bool playing, uint8_t track,
                            ui_media_mode_t mode)
{
    if(mode >= UI_MEDIA_MODE_COUNT) mode = UI_MEDIA_MODE_SEQUENCE;
    ui_concurrency_lock();
    bool changed = s_app_state.state.media_playing != playing ||
                   s_app_state.state.media_track != track ||
                   s_app_state.state.media_mode != mode;
    s_app_state.state.media_playing = playing;
    s_app_state.state.media_track = track;
    s_app_state.state.media_mode = mode;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_MEDIA);
}

void ui_app_state_set_navigation_active(bool active)
{
    ui_concurrency_lock();
    bool changed = s_app_state.state.navigation_active != active;
    s_app_state.state.navigation_active = active;
    ui_concurrency_unlock();
    if(changed) notify(UI_APP_STATE_CHANGE_NAVIGATION);
}
