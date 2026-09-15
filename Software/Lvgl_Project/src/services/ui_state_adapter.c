#include "ui_state_adapter.h"

#include "ui/core/ui_concurrency.h"

#include <string.h>

typedef struct
{
    ui_state_adapter_snapshot_t pending;
    ui_state_adapter_mask_t pending_fields;
} ui_state_adapter_context_t;

static ui_state_adapter_context_t s_adapter;

static void copy_name(char destination[UI_APP_STATE_NAME_MAX],
                      const char source[UI_APP_STATE_NAME_MAX])
{
    memcpy(destination, source, UI_APP_STATE_NAME_MAX);
    destination[UI_APP_STATE_NAME_MAX - 1U] = '\0';
}

void ui_state_adapter_init(void)
{
    ui_concurrency_lock();
    memset(&s_adapter, 0, sizeof(s_adapter));
    ui_concurrency_unlock();
}

bool ui_state_adapter_submit(const ui_state_adapter_snapshot_t *snapshot,
                             ui_state_adapter_mask_t fields)
{
    if(snapshot == NULL || fields == 0U ||
       (fields & ~UI_STATE_ADAPTER_ALL) != 0U) return false;

    ui_concurrency_lock();
    if((fields & UI_STATE_ADAPTER_STATUS) != 0U) {
        s_adapter.pending.clock_hour = snapshot->clock_hour;
        s_adapter.pending.clock_minute = snapshot->clock_minute;
        s_adapter.pending.battery_percent = snapshot->battery_percent;
        s_adapter.pending.battery_charging = snapshot->battery_charging;
    }
    if((fields & (UI_STATE_ADAPTER_GNSS |
                  UI_STATE_ADAPTER_SATELLITES)) != 0U) {
        s_adapter.pending.gps_enabled = snapshot->gps_enabled;
        s_adapter.pending.satellites = snapshot->satellites;
    }
    if((fields & UI_STATE_ADAPTER_CONNECTIVITY) != 0U) {
        s_adapter.pending.wifi_enabled = snapshot->wifi_enabled;
        s_adapter.pending.wifi_connected = snapshot->wifi_connected;
        s_adapter.pending.bluetooth_enabled = snapshot->bluetooth_enabled;
        s_adapter.pending.bluetooth_connected = snapshot->bluetooth_connected;
        copy_name(s_adapter.pending.wifi_name, snapshot->wifi_name);
        copy_name(s_adapter.pending.bluetooth_name,
                  snapshot->bluetooth_name);
    }
    if((fields & UI_STATE_ADAPTER_CONTROL) != 0U) {
        s_adapter.pending.sound_level = snapshot->sound_level;
        s_adapter.pending.brightness_level = snapshot->brightness_level;
        s_adapter.pending.power_save_enabled = snapshot->power_save_enabled;
    }
    if((fields & UI_STATE_ADAPTER_MEDIA) != 0U) {
        s_adapter.pending.media_playing = snapshot->media_playing;
        s_adapter.pending.media_track = snapshot->media_track;
        s_adapter.pending.media_mode = snapshot->media_mode;
    }
    if((fields & UI_STATE_ADAPTER_NAVIGATION) != 0U)
        s_adapter.pending.navigation_active = snapshot->navigation_active;
    s_adapter.pending_fields |= fields;
    ui_concurrency_unlock();
    return true;
}

ui_state_adapter_mask_t ui_state_adapter_process(void)
{
    ui_state_adapter_snapshot_t snapshot;
    ui_state_adapter_mask_t fields;

    ui_concurrency_lock();
    fields = s_adapter.pending_fields;
    if(fields == 0U) {
        ui_concurrency_unlock();
        return 0U;
    }
    snapshot = s_adapter.pending;
    s_adapter.pending_fields = 0U;
    ui_concurrency_unlock();

    bool has_status = (fields & UI_STATE_ADAPTER_STATUS) != 0U;
    bool has_gnss = (fields & UI_STATE_ADAPTER_GNSS) != 0U;
    bool has_satellites =
        (fields & UI_STATE_ADAPTER_SATELLITES) != 0U;
    if(has_status && has_gnss) {
        ui_app_state_set_status(snapshot.clock_hour, snapshot.clock_minute,
                                snapshot.battery_percent,
                                snapshot.battery_charging,
                                snapshot.gps_enabled,
                                snapshot.satellites);
    }
    else if(has_status) {
        ui_app_state_t current;
        if(ui_app_state_get(&current)) {
            ui_app_state_set_telemetry(
                snapshot.clock_hour, snapshot.clock_minute,
                snapshot.battery_percent, snapshot.battery_charging,
                has_satellites ? snapshot.satellites : current.satellites);
        }
    }
    else if(has_gnss) {
        ui_app_state_set_gnss(snapshot.gps_enabled, snapshot.satellites);
    }
    else if(has_satellites) {
        ui_app_state_set_satellites(snapshot.satellites);
    }
    if((fields & UI_STATE_ADAPTER_CONNECTIVITY) != 0U) {
        ui_app_state_set_connectivity(
            snapshot.wifi_enabled, snapshot.wifi_connected,
            snapshot.wifi_name,
            snapshot.bluetooth_enabled, snapshot.bluetooth_connected,
            snapshot.bluetooth_name);
    }
    if((fields & UI_STATE_ADAPTER_CONTROL) != 0U) {
        ui_app_state_set_controls(snapshot.sound_level,
                                  snapshot.brightness_level,
                                  snapshot.power_save_enabled);
    }
    if((fields & UI_STATE_ADAPTER_MEDIA) != 0U) {
        ui_app_state_set_media(snapshot.media_playing, snapshot.media_track,
                               snapshot.media_mode);
    }
    if((fields & UI_STATE_ADAPTER_NAVIGATION) != 0U)
        ui_app_state_set_navigation_active(snapshot.navigation_active);
    return fields;
}

ui_state_adapter_mask_t ui_state_adapter_pending(void)
{
    ui_concurrency_lock();
    ui_state_adapter_mask_t fields = s_adapter.pending_fields;
    ui_concurrency_unlock();
    return fields;
}
