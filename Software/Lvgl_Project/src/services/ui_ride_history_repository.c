#include "ui_ride_history_repository.h"

#include <math.h>
#include <string.h>

typedef struct
{
    ui_ride_history_storage_t storage;
    ui_ride_record_t pending;
    ui_ride_history_result_t last_result;
    bool configured;
    bool pending_valid;
} ui_ride_history_repository_context_t;

static ui_ride_history_repository_context_t s_repository;

static uint64_t mix_id(uint64_t value)
{
    value ^= value >> 30U;
    value *= UINT64_C(0xBF58476D1CE4E5B9);
    value ^= value >> 27U;
    value *= UINT64_C(0x94D049BB133111EB);
    value ^= value >> 31U;
    return value == 0U ? 1U : value;
}

void ui_ride_history_repository_init(void)
{
    ui_ride_history_storage_t storage = s_repository.storage;
    bool configured = s_repository.configured;
    memset(&s_repository, 0, sizeof(s_repository));
    s_repository.storage = storage;
    s_repository.configured = configured;
    s_repository.last_result = configured ? UI_RIDE_HISTORY_OK :
                                             UI_RIDE_HISTORY_NOT_CONFIGURED;
    ui_ride_history_model_init();
}

bool ui_ride_history_repository_register_storage(
    const ui_ride_history_storage_t *storage)
{
    if(storage == NULL || storage->load == NULL || storage->store == NULL ||
       storage->now_utc == NULL) return false;
    s_repository.storage = *storage;
    s_repository.configured = true;
    s_repository.last_result = UI_RIDE_HISTORY_OK;
    return true;
}

ui_ride_history_result_t ui_ride_history_repository_load(void)
{
    if(!s_repository.configured) {
        s_repository.last_result = UI_RIDE_HISTORY_NOT_CONFIGURED;
        return s_repository.last_result;
    }
    ui_ride_record_t records[UI_RIDE_HISTORY_MAX_RECORDS];
    size_t count = 0U;
    ui_ride_history_result_t result = s_repository.storage.load(
        records, UI_RIDE_HISTORY_MAX_RECORDS, &count,
        s_repository.storage.context);
    if(result == UI_RIDE_HISTORY_EMPTY) {
        ui_ride_history_model_init();
        result = UI_RIDE_HISTORY_OK;
    }
    else if(result == UI_RIDE_HISTORY_OK &&
            !ui_ride_history_model_replace(records, count)) {
        result = UI_RIDE_HISTORY_INVALID_DATA;
    }
    s_repository.last_result = result;
    return result;
}

ui_ride_history_result_t ui_ride_history_repository_prepare(
    const ui_ride_summary_t *summary)
{
    if(!s_repository.configured) return UI_RIDE_HISTORY_NOT_CONFIGURED;
    if(summary == NULL || !isfinite(summary->distance_km) ||
       summary->distance_km < 0.0f ||
       !isfinite(summary->average_speed_kmh) ||
       !isfinite(summary->maximum_speed_kmh))
        return UI_RIDE_HISTORY_INVALID_DATA;

    int64_t finished = s_repository.storage.now_utc(
        s_repository.storage.context);
    if(finished <= 0) return UI_RIDE_HISTORY_IO_ERROR;
    int64_t started = finished - (int64_t)summary->duration_seconds;
    uint64_t identity = (uint64_t)finished ^
                        ((uint64_t)summary->duration_seconds << 32U) ^
                        ((uint64_t)(summary->distance_km * 1000.0f + 0.5f)
                         << 1U);
    s_repository.pending = (ui_ride_record_t) {
        .id = mix_id(identity),
        .started_at_utc = started,
        .finished_at_utc = finished,
        .distance_km = summary->distance_km,
        .duration_seconds = summary->duration_seconds,
        .moving_time_seconds = summary->moving_time_seconds,
        .average_speed_kmh = summary->average_speed_kmh,
        .maximum_speed_kmh = summary->maximum_speed_kmh,
        .ascent_m = summary->ascent_m,
        .descent_m = summary->descent_m,
        .maximum_elevation_m = summary->maximum_elevation_m,
        .calories_kcal = summary->calories_kcal,
        .has_heart_rate = summary->has_heart_rate,
        .average_heart_rate = summary->average_heart_rate,
        .maximum_heart_rate = summary->maximum_heart_rate,
        .minimum_heart_rate = summary->minimum_heart_rate,
        .has_power = summary->has_power,
        .average_power = summary->average_power,
        .maximum_power = summary->maximum_power,
        .has_cadence = summary->has_cadence,
        .average_cadence = summary->average_cadence,
        .maximum_cadence = summary->maximum_cadence,
        .has_temperature = summary->has_temperature,
        .average_temperature_tenths_c =
            summary->average_temperature_tenths_c,
        .trace = summary->trace
    };
    s_repository.pending_valid = true;
    s_repository.last_result = UI_RIDE_HISTORY_OK;
    return UI_RIDE_HISTORY_OK;
}

ui_ride_history_result_t ui_ride_history_repository_commit_pending(void)
{
    if(!s_repository.configured) return UI_RIDE_HISTORY_NOT_CONFIGURED;
    if(!s_repository.pending_valid) return UI_RIDE_HISTORY_INVALID_DATA;
    if(ui_ride_history_model_contains(s_repository.pending.id)) {
        s_repository.pending_valid = false;
        return UI_RIDE_HISTORY_OK;
    }

    ui_ride_record_t staged[UI_RIDE_HISTORY_MAX_RECORDS];
    size_t count = 0U;
    if(!ui_ride_history_model_copy_oldest_first(
           staged, UI_RIDE_HISTORY_MAX_RECORDS, &count))
        return UI_RIDE_HISTORY_INVALID_DATA;
    if(count == UI_RIDE_HISTORY_MAX_RECORDS) {
        memmove(&staged[0], &staged[1],
                (UI_RIDE_HISTORY_MAX_RECORDS - 1U) * sizeof(staged[0]));
        count--;
    }
    staged[count++] = s_repository.pending;
    ui_ride_history_result_t result = s_repository.storage.store(
        staged, count, s_repository.storage.context);
    if(result == UI_RIDE_HISTORY_OK) {
        if(!ui_ride_history_model_replace(staged, count))
            result = UI_RIDE_HISTORY_INVALID_DATA;
        else
            s_repository.pending_valid = false;
    }
    s_repository.last_result = result;
    return result;
}

bool ui_ride_history_repository_has_pending(void)
{
    return s_repository.pending_valid;
}

void ui_ride_history_repository_clear_pending(void)
{
    memset(&s_repository.pending, 0, sizeof(s_repository.pending));
    s_repository.pending_valid = false;
}

ui_ride_history_result_t ui_ride_history_repository_last_result(void)
{
    return s_repository.last_result;
}
