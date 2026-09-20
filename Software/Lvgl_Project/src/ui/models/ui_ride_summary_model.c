#include "ui_ride_summary_model.h"

#include <limits.h>
#include <math.h>
#include <string.h>

typedef struct
{
    ui_ride_summary_t summary;
    float start_distance_km;
    float previous_distance_km;
    uint32_t start_seconds;
    uint64_t heart_rate_sum;
    uint64_t power_sum;
    uint64_t cadence_sum;
    int64_t temperature_tenths_sum;
    double speed_sum;
    uint32_t sample_count;
    uint32_t start_ascent_m;
    uint32_t start_calories_kcal;
    uint32_t previous_seconds;
    int32_t previous_altitude_m;
    bool active;
} ui_ride_summary_context_t;

static ui_ride_summary_context_t s_summary;

void ui_ride_summary_model_init(void)
{
    memset(&s_summary, 0, sizeof(s_summary));
}

void ui_ride_summary_model_begin(const ui_ride_data_t *data)
{
    if(data == NULL) return;
    uint32_t next_version = s_summary.summary.version + 1U;
    memset(&s_summary, 0, sizeof(s_summary));
    s_summary.summary.version = next_version;
    s_summary.start_distance_km = data->distance;
    s_summary.previous_distance_km = data->distance;
    s_summary.start_seconds = data->ride_seconds;
    s_summary.previous_seconds = data->ride_seconds;
    s_summary.start_ascent_m = data->ascent_m;
    s_summary.start_calories_kcal = data->calories_kcal;
    s_summary.previous_altitude_m = data->altitude_m;
    s_summary.summary.maximum_elevation_m = data->altitude_m;
    s_summary.summary.minimum_heart_rate = UINT16_MAX;
    s_summary.active = true;
}

static uint16_t clamp_u16(float value, float scale)
{
    float scaled = value * scale;
    if(scaled <= 0.0f) return 0U;
    if(scaled >= 65535.0f) return UINT16_MAX;
    return (uint16_t)lroundf(scaled);
}

static int16_t clamp_i16(int32_t value)
{
    if(value < INT16_MIN) return INT16_MIN;
    if(value > INT16_MAX) return INT16_MAX;
    return (int16_t)value;
}

static void compact_trace(ui_ride_report_trace_t *trace)
{
    uint16_t next = 0U;
    for(uint16_t source = 0U; source < trace->point_count; source += 2U) {
        trace->speed_tenths[next] = trace->speed_tenths[source];
        trace->heart_rate[next] = trace->heart_rate[source];
        trace->power[next] = trace->power[source];
        trace->cadence[next] = trace->cadence[source];
        trace->elevation_m[next] = trace->elevation_m[source];
        next++;
    }
    trace->point_count = next;
}

static void append_trace(const ui_ride_data_t *data)
{
    ui_ride_report_trace_t *trace = &s_summary.summary.trace;
    if(trace->point_count >= UI_RIDE_REPORT_POINT_MAX) compact_trace(trace);
    uint16_t index = trace->point_count++;
    trace->speed_tenths[index] = clamp_u16(data->speed, 10.0f);
    trace->heart_rate[index] = data->heart_rate;
    trace->power[index] = data->power;
    trace->cadence[index] = data->cadence;
    trace->elevation_m[index] = clamp_i16(data->altitude_m);
}

static uint8_t heart_zone(uint16_t value)
{
    if(value < 120U) return 0U;
    if(value < 140U) return 1U;
    if(value < 155U) return 2U;
    if(value < 170U) return 3U;
    return 4U;
}

static uint8_t power_zone(uint16_t value)
{
    if(value < 150U) return 0U;
    if(value < 220U) return 1U;
    if(value < 300U) return 2U;
    if(value < 400U) return 3U;
    return 4U;
}

void ui_ride_summary_model_update(const ui_ride_data_t *data)
{
    if(data == NULL || !s_summary.active) return;

    float distance = data->distance - s_summary.start_distance_km;
    if(distance < 0.0f) distance = 0.0f;
    s_summary.summary.distance_km = distance;
    s_summary.summary.duration_seconds =
        data->ride_seconds >= s_summary.start_seconds ?
        data->ride_seconds - s_summary.start_seconds : 0U;
    uint32_t elapsed = data->ride_seconds >= s_summary.previous_seconds ?
        data->ride_seconds - s_summary.previous_seconds : 0U;
    if(data->speed > 0.5f) {
        s_summary.summary.moving_time_seconds += elapsed;
    }
    s_summary.previous_seconds = data->ride_seconds;

    s_summary.summary.ascent_m = data->ascent_m >= s_summary.start_ascent_m ?
        data->ascent_m - s_summary.start_ascent_m : 0U;
    if(data->altitude_m < s_summary.previous_altitude_m)
        s_summary.summary.descent_m +=
            (uint32_t)(s_summary.previous_altitude_m - data->altitude_m);
    s_summary.previous_altitude_m = data->altitude_m;
    if(data->altitude_m > s_summary.summary.maximum_elevation_m)
        s_summary.summary.maximum_elevation_m = data->altitude_m;
    s_summary.summary.calories_kcal =
        data->calories_kcal >= s_summary.start_calories_kcal ?
        data->calories_kcal - s_summary.start_calories_kcal : 0U;
    s_summary.previous_distance_km = data->distance;

    if(data->speed > s_summary.summary.maximum_speed_kmh)
        s_summary.summary.maximum_speed_kmh = data->speed;
    s_summary.speed_sum += data->speed;
    s_summary.heart_rate_sum += data->heart_rate;
    s_summary.power_sum += data->power;
    s_summary.cadence_sum += data->cadence;
    s_summary.temperature_tenths_sum += (int32_t)data->temperature_c * 10;
    s_summary.sample_count++;
    s_summary.summary.average_speed_kmh =
        (float)(s_summary.speed_sum / s_summary.sample_count);
    s_summary.summary.average_heart_rate =
        (uint16_t)(s_summary.heart_rate_sum / s_summary.sample_count);
    s_summary.summary.average_power =
        (uint16_t)(s_summary.power_sum / s_summary.sample_count);
    s_summary.summary.average_cadence =
        (uint16_t)(s_summary.cadence_sum / s_summary.sample_count);
    s_summary.summary.average_temperature_tenths_c =
        (int16_t)(s_summary.temperature_tenths_sum /
                  (int64_t)s_summary.sample_count);
    if(data->heart_rate > 0U) {
        s_summary.summary.has_heart_rate = true;
        if(data->heart_rate > s_summary.summary.maximum_heart_rate)
            s_summary.summary.maximum_heart_rate = data->heart_rate;
        if(data->heart_rate < s_summary.summary.minimum_heart_rate)
            s_summary.summary.minimum_heart_rate = data->heart_rate;
        s_summary.summary.trace.heart_rate_zone_seconds[
            heart_zone(data->heart_rate)] += elapsed > 0U ? elapsed : 1U;
    }
    if(data->power > 0U) {
        s_summary.summary.has_power = true;
        if(data->power > s_summary.summary.maximum_power)
            s_summary.summary.maximum_power = data->power;
        s_summary.summary.trace.power_zone_seconds[
            power_zone(data->power)] += elapsed > 0U ? elapsed : 1U;
    }
    if(data->cadence > 0U) {
        s_summary.summary.has_cadence = true;
        if(data->cadence > s_summary.summary.maximum_cadence)
            s_summary.summary.maximum_cadence = data->cadence;
    }
    s_summary.summary.has_temperature = true;
    append_trace(data);
    s_summary.summary.version++;
}

void ui_ride_summary_model_finish(bool saved)
{
    s_summary.active = false;
    if(s_summary.summary.minimum_heart_rate == UINT16_MAX)
        s_summary.summary.minimum_heart_rate = 0U;
    if(s_summary.summary.moving_time_seconds == 0U)
        s_summary.summary.moving_time_seconds =
            s_summary.summary.duration_seconds;
    s_summary.summary.saved = saved;
    s_summary.summary.version++;
}

bool ui_ride_summary_model_get(ui_ride_summary_t *summary)
{
    if(summary == NULL) return false;
    *summary = s_summary.summary;
    return true;
}

bool ui_ride_summary_model_active(void)
{
    return s_summary.active;
}
