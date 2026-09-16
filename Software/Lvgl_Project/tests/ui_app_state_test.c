#include "ui/core/ui_app_state.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t s_calls;
static ui_app_state_change_mask_t s_last_change;
static ui_app_state_t s_last_state;

static void state_changed(const ui_app_state_t *state,
                          ui_app_state_change_mask_t changed,
                          void *user_data)
{
    (void)user_data;
    s_calls++;
    s_last_change = changed;
    s_last_state = *state;
}

int main(void)
{
    ui_app_state_init();
    ui_app_state_t state;
    assert(ui_app_state_get(&state));
    assert(state.current_route == UI_APP_ROUTE_UNKNOWN);
    assert(state.ride_state == UI_RIDE_STATE_IDLE);
    assert(state.clock_hour == 10 && state.clock_minute == 24);
    assert(state.battery_percent == 82 && state.satellites == 12);
    assert(state.wifi_enabled && state.wifi_connected);
    assert(state.bluetooth_enabled && state.bluetooth_connected);
    assert(state.gps_enabled);
    assert(state.sound_level == 62 && state.brightness_level == 75);

    int32_t status_subscription = ui_app_state_subscribe(
        UI_APP_STATE_CHANGE_STATUS | UI_APP_STATE_CHANGE_CONNECTIVITY,
        state_changed, NULL);
    assert(status_subscription > 0);

    ui_app_state_set_status(25, 75, 140, true, true, 9);
    assert(s_calls == 1);
    assert((s_last_change & UI_APP_STATE_CHANGE_STATUS) != 0U);
    assert(s_last_state.clock_hour == 1 && s_last_state.clock_minute == 15);
    assert(s_last_state.battery_percent == 100);
    assert(s_last_state.battery_charging && s_last_state.satellites == 9);

    uint32_t version = s_last_state.version;
    ui_app_state_set_status(1, 15, 100, true, true, 9);
    assert(s_calls == 1);
    assert(ui_app_state_get(&state) && state.version == version);

    ui_app_state_set_connectivity(false, false, "",
                                  true, true, "Riding Sensors");
    assert(s_calls == 2);
    assert(!s_last_state.wifi_enabled && !s_last_state.wifi_connected);
    assert(strcmp(s_last_state.bluetooth_name, "Riding Sensors") == 0);

    int32_t all_subscription = ui_app_state_subscribe(
        UI_APP_STATE_CHANGE_ALL, state_changed, NULL);
    assert(all_subscription > 0);
    ui_app_state_set_controls(110, 101, true);
    assert(s_calls == 3);
    assert(ui_app_state_get(&state));
    assert(state.sound_level == 100 && state.brightness_level == 100);
    assert(state.power_save_enabled);

    ui_app_state_set_touch_locked(true);
    assert(s_calls == 4);
    assert(ui_app_state_get(&state) && state.touch_locked);

    ui_app_state_set_media(true, 2, UI_MEDIA_MODE_SHUFFLE);
    assert(s_calls == 5);
    assert(s_last_state.media_playing && s_last_state.media_track == 2);
    assert(s_last_state.media_mode == UI_MEDIA_MODE_SHUFFLE);

    ui_app_state_set_route(3);
    ui_app_state_set_ride_state(UI_RIDE_STATE_RIDING);
    ui_app_state_set_navigation_active(true);
    assert(s_calls == 8);
    assert(s_last_state.navigation_active);

    assert(ui_app_state_unsubscribe(status_subscription));
    assert(ui_app_state_unsubscribe(all_subscription));
    ui_app_state_set_clock(8, 30);
    assert(s_calls == 8);

    puts("ui_app_state_test: PASS");
    return 0;
}
