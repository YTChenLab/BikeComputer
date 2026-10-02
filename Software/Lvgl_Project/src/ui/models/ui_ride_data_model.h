#ifndef UI_RIDE_DATA_MODEL_H
#define UI_RIDE_DATA_MODEL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

#define UI_RIDE_DATA_OBSERVER_MAX 4

typedef enum {
    UI_RIDE_FIELD_SPEED = 0, UI_RIDE_FIELD_HEART_RATE, UI_RIDE_FIELD_POWER,
    UI_RIDE_FIELD_CADENCE, UI_RIDE_FIELD_GRADE, UI_RIDE_FIELD_DISTANCE,
    UI_RIDE_FIELD_AVERAGE_SPEED, UI_RIDE_FIELD_MAXIMUM_SPEED,
    UI_RIDE_FIELD_ALTITUDE, UI_RIDE_FIELD_ASCENT, UI_RIDE_FIELD_CALORIES,
    UI_RIDE_FIELD_TEMPERATURE, UI_RIDE_FIELD_TIME, UI_RIDE_FIELD_GNSS,
    UI_RIDE_FIELD_BATTERY, UI_RIDE_FIELD_LAP, UI_RIDE_FIELD_POSITION,
    UI_RIDE_FIELD_COUNT
} ui_ride_field_t;

typedef enum {
    UI_DATA_SOURCE_UNKNOWN = 0, UI_DATA_SOURCE_GNSS, UI_DATA_SOURCE_WHEEL,
    UI_DATA_SOURCE_BLE, UI_DATA_SOURCE_BAROMETER, UI_DATA_SOURCE_BOARD,
    UI_DATA_SOURCE_FUSED, UI_DATA_SOURCE_DEMO
} ui_data_source_t;

typedef struct {
    uint32_t sampled_at_ms;
    uint32_t stale_after_ms; /* 0: non-expiring configuration/counter. */
    ui_data_source_t source;
} ui_ride_field_quality_t;

#define UI_RIDE_FIELD_BIT(field) (UINT32_C(1) << (uint32_t)(field))

typedef struct
{
    float speed;
    uint16_t heart_rate;
    uint16_t power;
    uint16_t cadence;
    float grade;
    float distance;
    float average_speed;
    float maximum_speed;
    int32_t altitude_m;
    uint32_t ascent_m;
    uint32_t calories_kcal;
    int8_t temperature_c;
    uint8_t heart_rate_zone;
    uint8_t current_lap;
    uint8_t total_laps;
    uint32_t ride_seconds;
    uint8_t clock_hour;
    uint8_t clock_minute;
    uint8_t satellites;
    uint8_t battery;
    /* Canonical units above: km/h, km, bpm, W, rpm, %, m, kcal, degrees C.
     * Monotonic timestamps use the same clock as the UI tick. Producers own
     * the recording timer; it must stop during pauses, not during UI stalls. */
    uint32_t sample_time_ms;
    uint64_t timer_ms;
    int64_t timestamp_utc;
    uint32_t session_id;
    double latitude;
    double longitude;
    bool quality_known;
    uint32_t capability_flags;
    uint32_t valid_flags;
    ui_ride_field_quality_t quality[UI_RIDE_FIELD_COUNT];
} ui_ride_data_t;

static inline void ui_ride_data_set_quality(ui_ride_data_t *data,
    ui_ride_field_t field, bool supported, bool valid, uint32_t sampled_at_ms,
    uint32_t stale_after_ms, ui_data_source_t source)
{
    if(data == NULL || (unsigned)field >= UI_RIDE_FIELD_COUNT) return;
    uint32_t bit = UI_RIDE_FIELD_BIT(field);
    data->quality_known = true;
    data->capability_flags = supported ? data->capability_flags | bit :
                                        data->capability_flags & ~bit;
    data->valid_flags = supported && valid ? data->valid_flags | bit :
                                            data->valid_flags & ~bit;
    data->quality[field] = (ui_ride_field_quality_t){sampled_at_ms,
                                                   stale_after_ms, source};
}

static inline bool ui_ride_data_field_valid(const ui_ride_data_t *data,
    ui_ride_field_t field, uint32_t now_ms)
{
    if(data == NULL || (unsigned)field >= UI_RIDE_FIELD_COUNT) return false;
    if(data->quality_known) {
        uint32_t bit = UI_RIDE_FIELD_BIT(field);
        if((data->capability_flags & data->valid_flags & bit) == 0U) return false;
        uint32_t age = now_ms - data->quality[field].sampled_at_ms;
        if(age > INT32_MAX) return false;
        if(data->quality[field].stale_after_ms != 0U &&
           age > data->quality[field].stale_after_ms) return false;
    }
    switch(field) {
        case UI_RIDE_FIELD_SPEED: return isfinite(data->speed) && data->speed >= 0;
        case UI_RIDE_FIELD_DISTANCE: return isfinite(data->distance) && data->distance >= 0;
        case UI_RIDE_FIELD_GRADE: return isfinite(data->grade);
        case UI_RIDE_FIELD_AVERAGE_SPEED: return isfinite(data->average_speed) && data->average_speed >= 0;
        case UI_RIDE_FIELD_MAXIMUM_SPEED: return isfinite(data->maximum_speed) && data->maximum_speed >= 0;
        case UI_RIDE_FIELD_HEART_RATE: return data->heart_rate > 0U;
        case UI_RIDE_FIELD_POWER: return data->quality_known || data->power > 0U;
        case UI_RIDE_FIELD_CADENCE: return data->quality_known || data->cadence > 0U;
        case UI_RIDE_FIELD_TEMPERATURE: return data->quality_known || data->temperature_c != 0;
        case UI_RIDE_FIELD_POSITION: return data->quality_known &&
            isfinite(data->latitude) && isfinite(data->longitude) &&
            data->latitude >= -90 && data->latitude <= 90 &&
            data->longitude >= -180 && data->longitude <= 180;
        default: return true;
    }
}

typedef void (*ui_ride_data_observer_cb_t)(const ui_ride_data_t *data,
                                           uint32_t version,
                                           void *user_data);

void ui_ride_data_model_init(void);
int32_t ui_ride_data_model_subscribe(ui_ride_data_observer_cb_t callback,
                                     void *user_data);
bool ui_ride_data_model_unsubscribe(int32_t subscription_id);

/* Producer-safe latest-value mailbox; it does not call UI observers. */
void ui_ride_data_model_submit(const ui_ride_data_t *data);
/* Merge independent sensor updates without overwriting other fields. Header
 * sample_time_ms/timer_ms/session_id accompanies every patch. Only explicit
 * quality is accepted; older field timestamps are ignored. */
bool ui_ride_data_model_submit_patch(const ui_ride_data_t *patch, uint32_t fields);

/* UI-thread drain. Multiple producer updates collapse to the latest snapshot. */
bool ui_ride_data_model_process(void);
bool ui_ride_data_model_get(ui_ride_data_t *data, uint32_t *version);

#endif /* UI_RIDE_DATA_MODEL_H */
