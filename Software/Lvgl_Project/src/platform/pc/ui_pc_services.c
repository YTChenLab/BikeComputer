#include "ui_pc_services.h"
#include "ui_pc_ride_history_storage.h"

#include "services/ui_command_service.h"
#include "services/ui_system_services.h"
#include "services/ui_state_adapter.h"
#include "ui/core/ui_event_bridge.h"
#include "ui/models/ui_ride_data_model.h"
#include "services/ui_ride_history_repository.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct
{
    uint32_t gnss_remaining_ms;
    uint32_t storage_remaining_ms;
    uint32_t ride_data_remaining_ms;
    uint32_t ride_clock_remainder_ms;
    uint32_t command_remaining_ms;
    uint32_t ride_rng_state;
    ui_command_t command;
    ui_state_adapter_snapshot_t device_state;
    bool gnss_busy;
    bool storage_busy;
    bool command_busy;
    bool next_gnss_success;
    bool next_storage_success;
    bool next_command_success;
    bool ride_data_initialized;
    bool ride_recording;
    bool simulate_stationary;
} ui_pc_services_context_t;

static ui_pc_services_context_t s_pc;
static ui_pc_ride_history_storage_context_t s_history_storage;

static void publish(ui_event_type_t type, ui_event_priority_t priority,
                    uint32_t dedupe_key)
{
    ui_event_t event = {0};
    event.type = type;
    event.priority = priority;
    event.source_id = 0x5043U; /* "PC" */
    event.dedupe_key = dedupe_key;
    (void)ui_event_bridge_post(&event);
}

static uint32_t random_value(ui_pc_services_context_t *pc, uint32_t limit)
{
    pc->ride_rng_state = pc->ride_rng_state * 1664525U + 1013904223U;
    return limit > 0U ? pc->ride_rng_state % limit : 0U;
}

static void publish_ride_data(ui_pc_services_context_t *pc)
{
    ui_ride_data_t data = {0};
    (void)ui_ride_data_model_get(&data, NULL);
    data.speed = pc->simulate_stationary ? 0.0f :
        (float)(120U + random_value(pc, 361U)) / 10.0f;
    if(!pc->ride_data_initialized) {
        data.distance = (float)(1200U + random_value(pc, 8800U)) / 100.0f;
        data.ride_seconds = 1800U + random_value(pc, 7200U);
        data.clock_hour = (uint8_t)random_value(pc, 24U);
        data.clock_minute = (uint8_t)random_value(pc, 60U);
        pc->ride_data_initialized = true;
    }
    else if(pc->ride_recording) {
        /* Keep simulator distance/time physically consistent with the
         * 800 ms telemetry cadence so Summary averages remain meaningful. */
        data.distance += data.speed * (0.8f / 3600.0f);
        pc->ride_clock_remainder_ms += 800U;
        data.ride_seconds += pc->ride_clock_remainder_ms / 1000U;
        pc->ride_clock_remainder_ms %= 1000U;
    }
    data.heart_rate = (uint16_t)(105U + random_value(pc, 76U));
    data.power = pc->simulate_stationary ? 0U :
        (uint16_t)(90U + random_value(pc, 331U));
    data.cadence = pc->simulate_stationary ? 0U :
        (uint16_t)(60U + random_value(pc, 56U));
    data.grade = (float)((int32_t)random_value(pc, 151U) - 50) / 10.0f;
    data.satellites = (uint8_t)(5U + random_value(pc, 12U));
    data.battery = (uint8_t)(68U + random_value(pc, 32U));
    ui_ride_data_model_submit(&data);

    pc->device_state.clock_hour = data.clock_hour;
    pc->device_state.clock_minute = data.clock_minute;
    pc->device_state.battery_percent = data.battery;
    if(pc->device_state.gps_enabled)
        pc->device_state.satellites = data.satellites;

    const ui_state_adapter_snapshot_t platform_state = {
        .clock_hour = data.clock_hour,
        .clock_minute = data.clock_minute,
        .battery_percent = data.battery,
        .battery_charging = false,
        .satellites = pc->device_state.gps_enabled ? data.satellites : 0U
    };
    (void)ui_state_adapter_submit(
        &platform_state,
        UI_STATE_ADAPTER_STATUS | UI_STATE_ADAPTER_SATELLITES);
}

static bool request_gnss_fix(void *context)
{
    ui_pc_services_context_t *pc = context;
    if(pc == NULL || pc->gnss_busy) return false;
    pc->gnss_busy = true;
    pc->gnss_remaining_ms = UI_PC_GNSS_DELAY_MS;
    return true;
}

static bool save_ride(void *context)
{
    ui_pc_services_context_t *pc = context;
    if(pc == NULL || pc->storage_busy) return false;
    pc->storage_busy = true;
    pc->storage_remaining_ms = UI_PC_STORAGE_DELAY_MS;
    publish(UI_EVENT_RIDE_SAVE_STARTED, UI_EVENT_PRIORITY_P2_RIDE_STATE,
            0x53564EU);
    return true;
}

static bool retry_save(void *context)
{
    return save_ride(context);
}

static bool dispatch_command(const ui_command_t *command, void *context)
{
    ui_pc_services_context_t *pc = context;
    if(pc == NULL || command == NULL || pc->command_busy) return false;
    pc->command = *command;
    pc->command_busy = true;
    if(command->type == UI_COMMAND_GNSS_ACQUIRE_FIX)
        pc->command_remaining_ms = UI_PC_GNSS_DELAY_MS;
    else if(command->type == UI_COMMAND_RIDE_SAVE)
        pc->command_remaining_ms = UI_PC_STORAGE_DELAY_MS;
    else
        pc->command_remaining_ms = UI_PC_COMMAND_DELAY_MS;
    return true;
}

static void apply_command_state(ui_pc_services_context_t *pc)
{
    ui_state_adapter_mask_t fields = 0U;
    bool enabled = pc->command.value != 0;
    switch(pc->command.type) {
        case UI_COMMAND_WIFI_SET_ENABLED:
            pc->device_state.wifi_enabled = enabled;
            pc->device_state.wifi_connected = enabled;
            fields = UI_STATE_ADAPTER_CONNECTIVITY;
            break;
        case UI_COMMAND_BLUETOOTH_SET_ENABLED:
            pc->device_state.bluetooth_enabled = enabled;
            pc->device_state.bluetooth_connected = enabled;
            fields = UI_STATE_ADAPTER_CONNECTIVITY;
            break;
        case UI_COMMAND_GNSS_SET_ENABLED:
            pc->device_state.gps_enabled = enabled;
            if(!enabled) pc->device_state.satellites = 0U;
            fields = UI_STATE_ADAPTER_GNSS;
            break;
        case UI_COMMAND_POWER_SAVE_SET_ENABLED:
            pc->device_state.power_save_enabled = enabled;
            fields = UI_STATE_ADAPTER_CONTROL;
            break;
        case UI_COMMAND_TOUCH_LOCK_SET_ENABLED:
            pc->device_state.touch_locked = enabled;
            fields = UI_STATE_ADAPTER_TOUCH_LOCK;
            break;
        case UI_COMMAND_SOUND_SET_LEVEL:
            pc->device_state.sound_level =
                pc->command.value < 0 ? 0U :
                (pc->command.value > 100 ? 100U :
                 (uint8_t)pc->command.value);
            fields = UI_STATE_ADAPTER_CONTROL;
            break;
        case UI_COMMAND_BRIGHTNESS_SET_LEVEL:
            pc->device_state.brightness_level =
                pc->command.value < 0 ? 0U :
                (pc->command.value > 100 ? 100U :
                 (uint8_t)pc->command.value);
            fields = UI_STATE_ADAPTER_CONTROL;
            break;
        case UI_COMMAND_MEDIA_SET_PLAYING:
            pc->device_state.media_playing = enabled;
            fields = UI_STATE_ADAPTER_MEDIA;
            break;
        case UI_COMMAND_MEDIA_SET_TRACK:
            pc->device_state.media_track =
                pc->command.value < 0 ? 0U : (uint8_t)pc->command.value;
            fields = UI_STATE_ADAPTER_MEDIA;
            break;
        case UI_COMMAND_MEDIA_SET_MODE:
            pc->device_state.media_mode =
                pc->command.value < 0 ? UI_MEDIA_MODE_SEQUENCE :
                (ui_media_mode_t)(pc->command.value % UI_MEDIA_MODE_COUNT);
            fields = UI_STATE_ADAPTER_MEDIA;
            break;
        case UI_COMMAND_MEDIA_PREVIOUS:
            pc->device_state.media_track =
                pc->device_state.media_track == 0U ? 2U :
                (uint8_t)(pc->device_state.media_track - 1U);
            fields = UI_STATE_ADAPTER_MEDIA;
            break;
        case UI_COMMAND_MEDIA_TOGGLE_PLAYBACK:
            pc->device_state.media_playing =
                !pc->device_state.media_playing;
            fields = UI_STATE_ADAPTER_MEDIA;
            break;
        case UI_COMMAND_MEDIA_NEXT:
            pc->device_state.media_track =
                (uint8_t)((pc->device_state.media_track + 1U) % 3U);
            fields = UI_STATE_ADAPTER_MEDIA;
            break;
        case UI_COMMAND_MEDIA_CYCLE_MODE:
            pc->device_state.media_mode = (ui_media_mode_t)(
                ((uint8_t)pc->device_state.media_mode + 1U) %
                UI_MEDIA_MODE_COUNT);
            fields = UI_STATE_ADAPTER_MEDIA;
            break;
        case UI_COMMAND_RIDE_START:
        case UI_COMMAND_RIDE_RESUME:
            pc->ride_recording = true;
            break;
        case UI_COMMAND_RIDE_PAUSE:
        case UI_COMMAND_RIDE_FINISH:
            pc->ride_recording = false;
            break;
        default:
            break;
    }
    if(fields != 0U)
        (void)ui_state_adapter_submit(&pc->device_state, fields);
}

static void command_tick(uint32_t elapsed_ms, void *context)
{
    ui_pc_services_context_t *pc = context;
    if(pc == NULL || !pc->command_busy || elapsed_ms == 0U) return;
    if(elapsed_ms < pc->command_remaining_ms) {
        pc->command_remaining_ms -= elapsed_ms;
        return;
    }

    bool success = pc->next_command_success;
    if(pc->command.type == UI_COMMAND_GNSS_ACQUIRE_FIX) {
        success = pc->next_gnss_success;
        pc->next_gnss_success = true;
    }
    else if(pc->command.type == UI_COMMAND_RIDE_SAVE) {
        success = pc->next_storage_success;
        pc->next_storage_success = true;
        if(success) {
            success = ui_ride_history_repository_commit_pending() ==
                      UI_RIDE_HISTORY_OK;
        }
    }
    else {
        pc->next_command_success = true;
    }

    if(success) apply_command_state(pc);
    (void)ui_command_service_complete(
        pc->command.id,
        success ? UI_COMMAND_STATUS_SUCCESS : UI_COMMAND_STATUS_FAILED,
        success ? 0 : UI_PC_COMMAND_ERROR_SIMULATED);
    pc->command_busy = false;
    pc->command_remaining_ms = 0U;
}

static void tick_operation(uint32_t elapsed_ms, uint32_t *remaining,
                           bool *busy, bool success,
                           ui_event_type_t success_event,
                           ui_event_type_t failure_event,
                           uint32_t dedupe_key)
{
    if(!*busy) return;
    if(elapsed_ms < *remaining) {
        *remaining -= elapsed_ms;
        return;
    }
    *remaining = 0;
    *busy = false;
    publish(success ? success_event : failure_event,
            success ? UI_EVENT_PRIORITY_P2_RIDE_STATE :
                      UI_EVENT_PRIORITY_P0_CRITICAL,
            dedupe_key);
}

static void tick(uint32_t elapsed_ms, void *context)
{
    ui_pc_services_context_t *pc = context;
    if(pc == NULL || elapsed_ms == 0) return;

    if(elapsed_ms >= pc->ride_data_remaining_ms) {
        publish_ride_data(pc);
        pc->ride_data_remaining_ms = 800U;
    }
    else pc->ride_data_remaining_ms -= elapsed_ms;

    bool gnss_result = pc->next_gnss_success;
    bool storage_result = pc->next_storage_success;
    bool gnss_was_busy = pc->gnss_busy;
    bool storage_was_busy = pc->storage_busy;

    tick_operation(elapsed_ms, &pc->gnss_remaining_ms, &pc->gnss_busy,
                   gnss_result, UI_EVENT_GPS_READY, UI_EVENT_GPS_FAILED,
                   0x474E53U);
    tick_operation(elapsed_ms, &pc->storage_remaining_ms, &pc->storage_busy,
                   storage_result, UI_EVENT_RIDE_SAVE_SUCCESS,
                   UI_EVENT_RIDE_SAVE_FAILED, 0x53564EU);

    /* Failure/success overrides are one-shot; defaults remain successful. */
    if(gnss_was_busy && !pc->gnss_busy) pc->next_gnss_success = true;
    if(storage_was_busy && !pc->storage_busy)
        pc->next_storage_success = true;
}

void ui_pc_services_register(void)
{
    memset(&s_pc, 0, sizeof(s_pc));
    s_pc.next_gnss_success = true;
    s_pc.next_storage_success = true;
    s_pc.next_command_success = true;
    s_pc.ride_data_remaining_ms = 1U;
    s_pc.ride_rng_state = (uint32_t)time(NULL) ^
                          (uint32_t)(uintptr_t)&s_pc;
    s_pc.device_state.gps_enabled = true;
    s_pc.device_state.satellites = 12U;
    s_pc.device_state.wifi_enabled = true;
    s_pc.device_state.wifi_connected = true;
    s_pc.device_state.bluetooth_enabled = true;
    s_pc.device_state.bluetooth_connected = true;
    s_pc.device_state.sound_level = 62U;
    s_pc.device_state.brightness_level = 75U;
    snprintf(s_pc.device_state.wifi_name,
             sizeof(s_pc.device_state.wifi_name), "%s", "Bike-SpeedView");
    snprintf(s_pc.device_state.bluetooth_name,
             sizeof(s_pc.device_state.bluetooth_name), "%s",
             "Riding Sensors");
    if(ui_pc_ride_history_storage_init(&s_history_storage,
                                       "runtime/ride_history.dat")) {
        ui_ride_history_storage_t history_backend =
            ui_pc_ride_history_storage_backend(&s_history_storage);
        (void)ui_ride_history_repository_register_storage(&history_backend);
    }
    const ui_system_services_backend_t backend = {
        .request_gnss_fix = request_gnss_fix,
        .save_ride = save_ride,
        .retry_save = retry_save,
        .tick = tick,
        .context = &s_pc
    };
    ui_system_services_register(&backend);

    ui_command_service_init();
    const ui_command_backend_t command_backend = {
        .dispatch = dispatch_command,
        .tick = command_tick,
        .context = &s_pc
    };
    (void)ui_command_service_register_backend(&command_backend);
}

void ui_pc_services_set_next_gnss_result(bool success)
{
    s_pc.next_gnss_success = success;
}

void ui_pc_services_set_next_storage_result(bool success)
{
    s_pc.next_storage_success = success;
}

void ui_pc_services_set_next_command_result(bool success)
{
    s_pc.next_command_success = success;
}

bool ui_pc_services_gnss_busy(void)
{
    return s_pc.gnss_busy;
}

bool ui_pc_services_storage_busy(void)
{
    return s_pc.storage_busy;
}

bool ui_pc_services_toggle_stationary(void)
{
    s_pc.simulate_stationary = !s_pc.simulate_stationary;
    return s_pc.simulate_stationary;
}

bool ui_pc_services_is_stationary(void)
{
    return s_pc.simulate_stationary;
}
