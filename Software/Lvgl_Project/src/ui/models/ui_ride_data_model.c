#include "ui_ride_data_model.h"
#include "ui_ride_summary_model.h"

#include "../core/ui_concurrency.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    int32_t id;
    ui_ride_data_observer_cb_t callback;
    void *user_data;
} ui_ride_data_observer_t;

typedef struct
{
    ui_ride_data_t current;
    ui_ride_data_t pending;
    ui_ride_data_observer_t observers[UI_RIDE_DATA_OBSERVER_MAX];
    uint8_t observer_count;
    int32_t next_observer_id;
    uint32_t version;
    bool pending_valid;
} ui_ride_data_context_t;

static ui_ride_data_context_t s_model;

void ui_ride_data_model_init(void)
{
    ui_concurrency_lock();
    memset(&s_model, 0, sizeof(s_model));
    s_model.current.quality_known = true;
    s_model.next_observer_id = 1;
    ui_concurrency_unlock();
}

int32_t ui_ride_data_model_subscribe(ui_ride_data_observer_cb_t callback,
                                     void *user_data)
{
    if(callback == NULL) return -1;
    ui_concurrency_lock();
    if(s_model.observer_count >= UI_RIDE_DATA_OBSERVER_MAX) {
        ui_concurrency_unlock();
        return -1;
    }
    ui_ride_data_observer_t *observer =
        &s_model.observers[s_model.observer_count++];
    observer->id = s_model.next_observer_id++;
    observer->callback = callback;
    observer->user_data = user_data;
    int32_t id = observer->id;
    ui_concurrency_unlock();
    return id;
}

bool ui_ride_data_model_unsubscribe(int32_t subscription_id)
{
    ui_concurrency_lock();
    for(uint8_t i = 0; i < s_model.observer_count; i++) {
        if(s_model.observers[i].id != subscription_id) continue;
        for(uint8_t j = i + 1; j < s_model.observer_count; j++)
            s_model.observers[j - 1] = s_model.observers[j];
        s_model.observer_count--;
        ui_concurrency_unlock();
        return true;
    }
    ui_concurrency_unlock();
    return false;
}

void ui_ride_data_model_submit(const ui_ride_data_t *data)
{ (void)ui_ride_data_model_submit_checked(data); }

bool ui_ride_data_model_submit_checked(const ui_ride_data_t *data)
{
    if(data == NULL) return false;
    ui_concurrency_lock();
    const ui_ride_data_t *latest = s_model.pending_valid ? &s_model.pending : &s_model.current;
    if((s_model.pending_valid || s_model.version != 0) && data->quality_known && latest->quality_known &&
       (int32_t)(data->sample_time_ms - latest->sample_time_ms) < 0) {
        ui_concurrency_unlock();
        return false;
    }
    s_model.pending = *data;
    s_model.pending_valid = true;
    ui_concurrency_unlock();
    /* Consume every producer frame before display mailbox coalescing. No LVGL. */
    ui_ride_summary_model_update(data);
    return true;
}

bool ui_ride_data_model_submit_patch(const ui_ride_data_t *patch, uint32_t fields)
{
    const uint32_t all = (UINT32_C(1) << UI_RIDE_FIELD_COUNT) - 1;
    if(patch == NULL || !patch->quality_known || fields == 0 || (fields & ~all)) return false;
    ui_ride_data_t merged;
    ui_concurrency_lock();
    merged = s_model.pending_valid ? s_model.pending : s_model.current;
    if((s_model.pending_valid || s_model.version != 0) && merged.quality_known &&
       (int32_t)(patch->sample_time_ms - merged.sample_time_ms) < 0) {
        ui_concurrency_unlock();
        return false;
    }
    if(patch->session_id != 0 && merged.session_id != 0 && patch->session_id != merged.session_id) {
        /* A full snapshot establishes a new recording session. */
        ui_concurrency_unlock();
        return false;
    }
    merged.quality_known = true;
    merged.sample_time_ms = patch->sample_time_ms;
    merged.timer_ms = patch->timer_ms;
    merged.ride_seconds = patch->ride_seconds;
    merged.session_id = patch->session_id;
    merged.timestamp_utc = patch->timestamp_utc;
    for(unsigned field = 0; field < UI_RIDE_FIELD_COUNT; ++field) {
        uint32_t bit = UI_RIDE_FIELD_BIT(field);
        if(!(fields & bit)) continue;
        if((merged.capability_flags & bit) &&
           (int32_t)(patch->quality[field].sampled_at_ms - merged.quality[field].sampled_at_ms) < 0)
            continue;
        merged.capability_flags = (merged.capability_flags & ~bit) | (patch->capability_flags & bit);
        merged.valid_flags = (merged.valid_flags & ~bit) | (patch->valid_flags & bit);
        merged.quality[field] = patch->quality[field];
        switch(field) {
            case UI_RIDE_FIELD_SPEED: merged.speed = patch->speed; break;
            case UI_RIDE_FIELD_HEART_RATE: merged.heart_rate = patch->heart_rate; merged.heart_rate_zone = patch->heart_rate_zone; break;
            case UI_RIDE_FIELD_POWER: merged.power = patch->power; break;
            case UI_RIDE_FIELD_CADENCE: merged.cadence = patch->cadence; break;
            case UI_RIDE_FIELD_GRADE: merged.grade = patch->grade; break;
            case UI_RIDE_FIELD_DISTANCE: merged.distance = patch->distance; break;
            case UI_RIDE_FIELD_AVERAGE_SPEED: merged.average_speed = patch->average_speed; break;
            case UI_RIDE_FIELD_MAXIMUM_SPEED: merged.maximum_speed = patch->maximum_speed; break;
            case UI_RIDE_FIELD_ALTITUDE: merged.altitude_m = patch->altitude_m; break;
            case UI_RIDE_FIELD_ASCENT: merged.ascent_m = patch->ascent_m; break;
            case UI_RIDE_FIELD_CALORIES: merged.calories_kcal = patch->calories_kcal; break;
            case UI_RIDE_FIELD_TEMPERATURE: merged.temperature_c = patch->temperature_c; break;
            case UI_RIDE_FIELD_TIME: merged.clock_hour = patch->clock_hour; merged.clock_minute = patch->clock_minute; break;
            case UI_RIDE_FIELD_GNSS: merged.satellites = patch->satellites; break;
            case UI_RIDE_FIELD_BATTERY: merged.battery = patch->battery; break;
            case UI_RIDE_FIELD_LAP: merged.current_lap = patch->current_lap; merged.total_laps = patch->total_laps; break;
            case UI_RIDE_FIELD_POSITION: merged.latitude = patch->latitude; merged.longitude = patch->longitude; break;
            default: break;
        }
    }
    s_model.pending = merged;
    s_model.pending_valid = true;
    ui_concurrency_unlock();
    ui_ride_summary_model_update(&merged);
    return true;
}

bool ui_ride_data_model_process(void)
{
    ui_ride_data_t snapshot;
    ui_ride_data_observer_t observers[UI_RIDE_DATA_OBSERVER_MAX];
    uint8_t observer_count;
    uint32_t version;

    ui_concurrency_lock();
    if(!s_model.pending_valid) {
        ui_concurrency_unlock();
        return false;
    }
    s_model.current = s_model.pending;
    s_model.pending_valid = false;
    s_model.version++;
    snapshot = s_model.current;
    version = s_model.version;
    observer_count = s_model.observer_count;
    memcpy(observers, s_model.observers,
           observer_count * sizeof(observers[0]));
    ui_concurrency_unlock();

    for(uint8_t i = 0; i < observer_count; i++)
        observers[i].callback(&snapshot, version, observers[i].user_data);
    return true;
}

bool ui_ride_data_model_get(ui_ride_data_t *data, uint32_t *version)
{
    if(data == NULL) return false;
    ui_concurrency_lock();
    *data = s_model.current;
    if(version != NULL) *version = s_model.version;
    ui_concurrency_unlock();
    return true;
}
