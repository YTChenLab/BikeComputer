#include "ui_pc_services.h"
#include "ui_pc_ride_history_storage.h"
#include "ui_pc_preferences.h"

#include "services/ui_command_service.h"
#include "services/ui_navigation_service.h"
#include "services/ui_system_services.h"
#include "services/ui_state_adapter.h"
#include "ui/core/ui_event_bridge.h"
#include "ui/models/ui_ride_data_model.h"
#include "ui/models/ui_weather_model.h"
#include "services/ui_ride_history_repository.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct
{
    uint32_t gnss_remaining_ms;
    uint32_t storage_remaining_ms;
    uint32_t ride_data_remaining_ms;
    uint32_t ride_clock_remainder_ms;
    uint32_t monotonic_ms;
    uint64_t recording_timer_ms;
    uint32_t telemetry_elapsed_ms;
    ui_ride_data_t ride_data;
    uint32_t command_remaining_ms;
    uint32_t weather_remaining_ms;
    uint32_t navigation_remaining_ms;
    uint32_t navigation_tick;
    uint32_t ride_rng_state;
    nav_model_t navigation;
    ui_command_t command;
    ui_state_adapter_snapshot_t device_state;
    bool gnss_busy;
    bool storage_busy;
    bool command_busy;
    bool storage_worker_started;
    bool next_gnss_success;
    bool next_storage_success;
    bool next_command_success;
    bool ride_data_initialized;
    bool ride_recording;
    bool simulate_stationary;
} ui_pc_services_context_t;

static ui_pc_services_context_t s_pc;
static ui_pc_ride_history_storage_context_t s_history_storage;

#ifdef _WIN32
static DWORD WINAPI storage_worker(void *argument)
#else
static void *storage_worker(void *argument)
#endif
{
    if(argument) (void)ui_ride_history_repository_page_worker();
    else (void)ui_ride_history_repository_commit_worker();
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static bool start_storage_worker(bool page)
{
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, storage_worker, (void *)(uintptr_t)page, 0, NULL);
    if(thread == NULL) return false;
    CloseHandle(thread);
#else
    pthread_t thread;
    if(pthread_create(&thread, NULL, storage_worker, (void *)(uintptr_t)page) != 0) return false;
    pthread_detach(thread);
#endif
    return true;
}

static bool request_page_worker(void *context)
{
    (void)context; return start_storage_worker(true);
}

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
    ui_ride_data_t data = pc->ride_data;
    data.speed = pc->simulate_stationary ? 0.0f :
        (float)(120U + random_value(pc, 361U)) / 10.0f;
    if(!pc->ride_data_initialized) {
        data.distance = (float)(1200U + random_value(pc, 8800U)) / 100.0f;
        data.ride_seconds = 0;
        data.clock_hour = (uint8_t)random_value(pc, 24U);
        data.clock_minute = (uint8_t)random_value(pc, 60U);
        data.maximum_speed = data.speed;
        data.altitude_m = (int32_t)(260U + random_value(pc, 180U));
        data.ascent_m = 420U + random_value(pc, 420U);
        data.current_lap = 2U;
        data.total_laps = 5U;
        pc->ride_data_initialized = true;
    }
    else if(pc->ride_recording) {
        /* Keep simulator distance/time physically consistent with the
         * 800 ms telemetry cadence so Summary averages remain meaningful. */
        data.distance += data.speed * ((float)pc->telemetry_elapsed_ms / 3600000.0f);
    }
    data.heart_rate = (uint16_t)(105U + random_value(pc, 76U));
    data.power = pc->simulate_stationary ? 0U :
        (uint16_t)(90U + random_value(pc, 331U));
    data.cadence = pc->simulate_stationary ? 0U :
        (uint16_t)(60U + random_value(pc, 56U));
    data.grade = (float)((int32_t)random_value(pc, 151U) - 50) / 10.0f;
    if(data.speed > data.maximum_speed) data.maximum_speed = data.speed;
    data.ride_seconds = (uint32_t)(pc->recording_timer_ms / 1000U);
    data.average_speed = data.ride_seconds > 0U ?
        data.distance * 3600.0f / (float)data.ride_seconds : data.speed;
    data.altitude_m += (int32_t)random_value(pc, 5U) - 2;
    if(pc->ride_recording && pc->telemetry_elapsed_ms > 0 && data.grade > 1.0f)
        data.ascent_m += (uint32_t)(data.grade / 2.0f);
    data.calories_kcal = (uint32_t)(data.ride_seconds *
                                     (data.power > 0U ? data.power : 120U) /
                                     2400U);
    data.temperature_c = (int8_t)(20 + random_value(pc, 9U));
    data.heart_rate_zone = data.heart_rate >= 170U ? 5U :
                           data.heart_rate >= 155U ? 4U :
                           data.heart_rate >= 140U ? 3U :
                           data.heart_rate >= 120U ? 2U : 1U;
    data.satellites = (uint8_t)(5U + random_value(pc, 12U));
    data.battery = (uint8_t)(68U + random_value(pc, 32U));
    data.sample_time_ms = pc->monotonic_ms;
    data.timer_ms = pc->recording_timer_ms;
    data.ride_seconds = (uint32_t)(pc->recording_timer_ms / 1000U);
    data.timestamp_utc = (int64_t)time(NULL);
    for(unsigned field = 0; field < UI_RIDE_FIELD_COUNT; ++field)
        ui_ride_data_set_quality(&data, (ui_ride_field_t)field, true, true,
            pc->monotonic_ms, 3000U, UI_DATA_SOURCE_DEMO);
    ui_ride_data_set_quality(&data, UI_RIDE_FIELD_POSITION, false, false,
                            pc->monotonic_ms, 3000U, UI_DATA_SOURCE_DEMO);
    pc->ride_data = data;
    pc->telemetry_elapsed_ms = 0;
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
    (void)ui_state_adapter_submit(&pc->device_state,
        UI_STATE_ADAPTER_GNSS | UI_STATE_ADAPTER_CONNECTIVITY);
}

static void publish_weather(ui_pc_services_context_t *pc)
{
    ui_weather_data_t weather;
    if(!ui_weather_model_get(&weather)) return;
    weather.quality_known = true;
    weather.valid = true;
    weather.sampled_at_ms = pc->monotonic_ms;
    weather.stale_after_ms = 900000;
    weather.temperature_c = (int8_t)(18 + random_value(pc, 11U));
    weather.feels_like_c = (int8_t)(weather.temperature_c - 1);
    weather.wind_kmh = (uint8_t)(5U + random_value(pc, 21U));
    weather.rain_percent = (uint8_t)random_value(pc, 61U);
    weather.air_quality_index = (uint16_t)(25U + random_value(pc, 76U));
    weather.condition = weather.rain_percent >= 45U ? UI_WEATHER_RAIN :
                        weather.rain_percent >= 20U ? UI_WEATHER_CLOUDY :
                                                     UI_WEATHER_CLEAR;
    (void)ui_weather_model_submit(&weather);
}

static void publish_navigation(ui_pc_services_context_t *pc)
{
    nav_model_t *model = &pc->navigation;
    pc->navigation_tick++;
    model->speed = 25.0f +
                   (float)(pc->navigation_tick % 47U) / 10.0f;
    if(model->distance > 0.02f) model->distance -= 0.02f;
    model->heading += 7.0f;
    if(model->heading >= 360.0f) model->heading -= 360.0f;
    model->latitude += 0.000015f;
    model->longitude += 0.000010f;
    if(model->ui_state < NAV_UI_REROUTING &&
       model->direction_distance > 10U) {
        model->direction_distance -= 5U;
    }
    ui_navigation_service_submit(model);
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
            pc->recording_timer_ms = 0;
            pc->telemetry_elapsed_ms = 0;
            pc->ride_data.distance = 0;
            pc->ride_data.ride_seconds = 0;
            pc->ride_data.ascent_m = pc->ride_data.calories_kcal = 0;
            pc->ride_data.maximum_speed = pc->ride_data.average_speed = 0;
            pc->ride_data.session_id = (uint32_t)pc->command.value;
            pc->ride_recording = true;
            publish_ride_data(pc);
            break;
        case UI_COMMAND_RIDE_RESUME:
            pc->ride_recording = true;
            break;
        case UI_COMMAND_RIDE_PAUSE:
        case UI_COMMAND_RIDE_FINISH:
            publish_ride_data(pc);
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
    if(pc->command.type == UI_COMMAND_RIDE_UPLOAD) success = false;
    if(pc->command.type == UI_COMMAND_GNSS_ACQUIRE_FIX) {
        success = pc->next_gnss_success;
        pc->next_gnss_success = true;
    }
    else if(pc->command.type == UI_COMMAND_RIDE_SAVE) {
        if(pc->storage_worker_started) {
            ui_ride_history_result_t result;
            if(!ui_ride_history_repository_process(&result)) return;
            pc->storage_worker_started = false;
            success = result == UI_RIDE_HISTORY_OK;
        }
        else {
        success = pc->next_storage_success;
        pc->next_storage_success = true;
        if(success) {
            success = ui_ride_history_repository_begin_commit();
            if(success && start_storage_worker(false)) {
                pc->storage_worker_started = true;
                pc->command_remaining_ms = 0;
                return;
            }
            if(success) {
                (void)ui_ride_history_repository_abort_commit();
                success = false;
            }
        }
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
    pc->monotonic_ms += elapsed_ms;
    if(pc->ride_recording) {
        pc->recording_timer_ms += elapsed_ms;
        pc->telemetry_elapsed_ms += elapsed_ms;
    }

    if(elapsed_ms >= pc->ride_data_remaining_ms) {
        publish_ride_data(pc);
        pc->ride_data_remaining_ms = 800U;
    }
    else pc->ride_data_remaining_ms -= elapsed_ms;

    if(elapsed_ms >= pc->weather_remaining_ms) {
        publish_weather(pc);
        pc->weather_remaining_ms = 30000U;
    }
    else pc->weather_remaining_ms -= elapsed_ms;

    if(elapsed_ms >= pc->navigation_remaining_ms) {
        publish_navigation(pc);
        pc->navigation_remaining_ms = 900U;
    }
    else pc->navigation_remaining_ms -= elapsed_ms;

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
    ui_pc_preferences_register();
    (void)ui_ride_history_repository_register_page_worker(request_page_worker,NULL);
    ui_navigation_service_set_demo_enabled(true);
    memset(&s_pc, 0, sizeof(s_pc));
    s_pc.next_gnss_success = true;
    s_pc.next_storage_success = true;
    s_pc.next_command_success = true;
    s_pc.ride_data_remaining_ms = 1U;
    s_pc.weather_remaining_ms = 1U;
    s_pc.navigation_remaining_ms = 1U;
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
    s_pc.navigation = (nav_model_t){
        .demo = true,
        .speed = 28.6f,
        .distance = 12.6f,
        .remaining_ascent_m = 156U,
        .eta_hour = 11U,
        .eta_min = 5U,
        .latitude = 34.3416f,
        .longitude = 108.9398f,
        .heading = 36.0f,
        .zoom = 14U,
        .direction = NAV_DIRECTION_RIGHT,
        .direction_distance = 420U,
        .ui_state = NAV_UI_CRUISE,
        .road_name = "科技二路"
    };
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

void ui_pc_services_set_demo_seed(uint32_t seed)
{
    s_pc.ride_rng_state = seed;
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
