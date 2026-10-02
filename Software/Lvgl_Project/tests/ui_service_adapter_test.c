#include "services/ui_navigation_service.h"
#include "services/ui_state_adapter.h"
#include "ui/core/ui_app_state.h"
#include "ui/core/ui_input_queue.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static ui_input_message_t s_inputs[UI_INPUT_QUEUE_MAX];
static uint8_t s_input_count;
static nav_model_t s_navigation;
static uint8_t s_navigation_calls;

static void input_received(const ui_input_message_t *message, void *user_data)
{
    (void)user_data;
    s_inputs[s_input_count++] = *message;
}

static void navigation_changed(const nav_model_t *model, uint32_t version,
                               void *user_data)
{
    (void)user_data;
    assert(version > 0U);
    s_navigation = *model;
    s_navigation_calls++;
}

static void test_state_adapter(void)
{
    ui_app_state_init();
    ui_state_adapter_init();

    ui_state_adapter_snapshot_t first = {
        .clock_hour = 7,
        .clock_minute = 18,
        .battery_percent = 91,
        .satellites = 11
    };
    ui_state_adapter_snapshot_t latest = first;
    latest.clock_minute = 19;
    latest.battery_percent = 140;
    latest.satellites = 13;
    assert(ui_state_adapter_submit(
        &first, UI_STATE_ADAPTER_STATUS | UI_STATE_ADAPTER_SATELLITES));
    assert(ui_state_adapter_submit(
        &latest, UI_STATE_ADAPTER_STATUS | UI_STATE_ADAPTER_SATELLITES));

    ui_app_state_t state;
    assert(ui_app_state_get(&state));
    assert(!state.clock_valid && !state.battery_valid);
    assert(ui_state_adapter_pending() ==
           (UI_STATE_ADAPTER_STATUS | UI_STATE_ADAPTER_SATELLITES));
    assert(ui_state_adapter_process() ==
           (UI_STATE_ADAPTER_STATUS | UI_STATE_ADAPTER_SATELLITES));
    assert(ui_app_state_get(&state));
    assert(state.clock_hour == 7 && state.clock_minute == 19);
    assert(state.battery_percent == 100 && state.satellites == 13);
    assert(state.gps_enabled);
    latest.status_quality_known = true;
    latest.clock_valid = false;
    latest.battery_valid = true;
    assert(ui_state_adapter_submit(&latest, UI_STATE_ADAPTER_STATUS));
    assert(ui_state_adapter_process() == UI_STATE_ADAPTER_STATUS);
    assert(ui_app_state_get(&state));
    assert(!state.clock_valid && state.battery_valid);
    latest.clock_valid = true;
    latest.battery_valid = false;
    assert(ui_state_adapter_submit(&latest, UI_STATE_ADAPTER_STATUS));
    (void)ui_state_adapter_process();
    assert(ui_app_state_get(&state));
    assert(state.clock_valid && !state.battery_valid);

    ui_state_adapter_snapshot_t external = {
        .gps_enabled = false,
        .satellites = 0,
        .wifi_enabled = true,
        .wifi_connected = true,
        .bluetooth_enabled = true,
        .bluetooth_connected = false,
        .sound_level = 120,
        .brightness_level = 64,
        .power_save_enabled = true,
        .touch_locked = true,
        .media_playing = true,
        .media_track = 2,
        .media_mode = UI_MEDIA_MODE_REPEAT_ONE,
        .navigation_active = true
    };
    snprintf(external.wifi_name, sizeof(external.wifi_name), "%s",
             "Bike-Lab");
    snprintf(external.bluetooth_name, sizeof(external.bluetooth_name), "%s",
             "Power Meter");
    ui_state_adapter_mask_t fields =
        UI_STATE_ADAPTER_GNSS | UI_STATE_ADAPTER_CONNECTIVITY |
        UI_STATE_ADAPTER_CONTROL | UI_STATE_ADAPTER_TOUCH_LOCK |
        UI_STATE_ADAPTER_MEDIA |
        UI_STATE_ADAPTER_NAVIGATION;
    assert(ui_state_adapter_submit(&external, fields));
    assert(ui_state_adapter_process() == fields);
    assert(ui_app_state_get(&state));
    assert(!state.gps_enabled && state.satellites == 0);
    assert(state.wifi_connected && !state.bluetooth_connected);
    assert(strcmp(state.wifi_name, "Bike-Lab") == 0);
    assert(state.sound_level == 100 && state.brightness_level == 64);
    assert(state.power_save_enabled && state.touch_locked);
    assert(state.media_playing);
    assert(state.media_track == 2);
    assert(state.media_mode == UI_MEDIA_MODE_REPEAT_ONE);
    assert(state.navigation_active);
    assert(ui_state_adapter_pending() == 0U);
}

static void test_input_queue(void)
{
    ui_input_queue_init();
    s_input_count = 0;
    const ui_input_message_t begin = {
        .type = UI_INPUT_TOUCH_BEGIN, .x = 20, .y = 100
    };
    const ui_input_message_t move_a = {
        .type = UI_INPUT_TOUCH_MOVE, .x = 20, .y = 120
    };
    const ui_input_message_t move_b = {
        .type = UI_INPUT_TOUCH_MOVE, .x = 20, .y = 180
    };
    const ui_input_message_t end = {
        .type = UI_INPUT_TOUCH_END, .x = 20, .y = 180
    };
    assert(ui_input_queue_post(&begin));
    assert(ui_input_queue_post(&move_a));
    assert(ui_input_queue_post(&move_b));
    assert(ui_input_queue_post(&end));
    assert(ui_input_queue_pending() == 3U);
    assert(ui_input_queue_drain(UI_INPUT_QUEUE_MAX,
                                input_received, NULL) == 3U);
    assert(s_inputs[0].type == UI_INPUT_TOUCH_BEGIN);
    assert(s_inputs[1].type == UI_INPUT_TOUCH_MOVE && s_inputs[1].y == 180);
    assert(s_inputs[2].type == UI_INPUT_TOUCH_END);

    ui_input_queue_init();
    const ui_input_message_t rotate = {.type = UI_INPUT_ENCODER_CW};
    for(uint8_t index = 0; index < UI_INPUT_QUEUE_MAX; index++)
        assert(ui_input_queue_post(&rotate));
    assert(ui_input_queue_post(&end));
    assert(ui_input_queue_pending() == UI_INPUT_QUEUE_MAX);
    assert(ui_input_queue_dropped() == 1U);
    s_input_count = 0;
    (void)ui_input_queue_drain(UI_INPUT_QUEUE_MAX, input_received, NULL);
    assert(s_inputs[s_input_count - 1U].type == UI_INPUT_TOUCH_END);
}

static void test_navigation_service(void)
{
    ui_navigation_service_init();
    s_navigation_calls = 0;
    assert(ui_navigation_service_subscribe(navigation_changed, NULL) > 0);
    const nav_model_t first = {.speed = 20.0f, .zoom = 12};
    const nav_model_t latest = {
        .speed = 31.5f,
        .distance = 48.2f,
        .zoom = 15,
        .direction = NAV_DIRECTION_RIGHT,
        .direction_distance = 220U
    };
    ui_navigation_service_submit(&first);
    ui_navigation_service_submit(&latest);
    assert(s_navigation_calls == 0U);
    assert(ui_navigation_service_process());
    assert(s_navigation_calls == 1U);
    assert(s_navigation.speed == 31.5f && s_navigation.zoom == 15);
    assert(!ui_navigation_service_process());
}

int main(void)
{
    test_state_adapter();
    test_input_queue();
    test_navigation_service();
    puts("ui_service_adapter_test: PASS");
    return 0;
}
