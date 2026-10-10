#include "ui/core/ui_memory.h"
#include "ui_ride_summary_model.h"
#include "../core/ui_concurrency.h"
#include <limits.h>
#include <math.h>
#include <string.h>

typedef struct {
    ui_ride_summary_t summary;
    float start_distance_km;
    uint64_t start_timer_ms, previous_timer_ms;
    uint64_t heart_rate_sum, power_sum, cadence_sum;
    uint64_t heart_rate_ms, power_ms, cadence_ms, temperature_ms;
    int64_t temperature_sum;
    uint64_t moving_ms;
    uint64_t heart_zone_ms[UI_RIDE_REPORT_ZONE_COUNT];
    uint64_t power_zone_ms[UI_RIDE_REPORT_ZONE_COUNT];
    uint32_t start_ascent_m, start_calories_kcal;
    int32_t previous_altitude_m;
    bool distance_baseline_valid, altitude_valid;
    bool ascent_baseline_valid, calories_baseline_valid;
    bool active, paused;
} summary_context_t;

static UI_LARGE_RAM summary_context_t s_summary;
static uint32_t s_next_session;
static uint16_t s_heart_boundaries[4] = {120, 140, 155, 170};
static uint16_t s_power_boundaries[4] = {150, 220, 300, 400};

uint32_t ui_ride_summary_model_reserve_session(void)
{
    ui_concurrency_lock();
    if(++s_next_session == 0) ++s_next_session;
    uint32_t session = s_next_session;
    ui_concurrency_unlock();
    return session;
}

static uint64_t timer_ms(const ui_ride_data_t *data)
{
    return data->timer_ms != 0U ? data->timer_ms : (uint64_t)data->ride_seconds * 1000U;
}

static bool valid(const ui_ride_data_t *data, ui_ride_field_t field)
{
    return ui_ride_data_field_valid(data, field, data->sample_time_ms);
}

void ui_ride_summary_model_init(void)
{
    ui_concurrency_lock();
    memset(&s_summary, 0, sizeof(s_summary));
    ui_concurrency_unlock();
}

void ui_ride_summary_model_begin(const ui_ride_data_t *data)
{
    if(data == NULL) return;
    ui_concurrency_lock();
    uint32_t version = s_summary.summary.version + 1U;
    memset(&s_summary, 0, sizeof(s_summary));
    s_summary.summary.version = version;
    if(++s_next_session == 0U) ++s_next_session;
    s_summary.summary.session_id = data->session_id != 0U ? data->session_id : s_next_session;
    s_summary.summary.started_at_utc = data->timestamp_utc;
    s_summary.distance_baseline_valid = valid(data, UI_RIDE_FIELD_DISTANCE);
    s_summary.start_distance_km = data->distance;
    s_summary.start_timer_ms = s_summary.previous_timer_ms = timer_ms(data);
    s_summary.start_ascent_m = data->ascent_m;
    s_summary.ascent_baseline_valid = valid(data, UI_RIDE_FIELD_ASCENT);
    s_summary.calories_baseline_valid = valid(data, UI_RIDE_FIELD_CALORIES);
    s_summary.summary.metric_flags = UI_REPORT_METRICS_KNOWN;
    s_summary.start_calories_kcal = data->calories_kcal;
    s_summary.altitude_valid = valid(data, UI_RIDE_FIELD_ALTITUDE);
    s_summary.previous_altitude_m = data->altitude_m;
    if(s_summary.altitude_valid) s_summary.summary.maximum_elevation_m = data->altitude_m;
    s_summary.summary.minimum_heart_rate = UINT16_MAX;
    s_summary.active = true;
    ui_concurrency_unlock();
}

static uint16_t clamp_u16(float value, float scale)
{
    if(!isfinite(value) || value <= 0) return 0U;
    float scaled = value * scale;
    return scaled >= UINT16_MAX ? UINT16_MAX : (uint16_t)lroundf(scaled);
}

static int16_t clamp_i16(int32_t value)
{
    return value < INT16_MIN ? INT16_MIN : value > INT16_MAX ? INT16_MAX : (int16_t)value;
}

static void compact_trace(ui_ride_report_trace_t *trace)
{
    uint16_t next = 0U;
    for(uint16_t source = 0; source < trace->point_count; source += 2U) {
        trace->speed_tenths[next] = trace->speed_tenths[source];
        trace->heart_rate[next] = trace->heart_rate[source];
        trace->power[next] = trace->power[source];
        trace->cadence[next] = trace->cadence[source];
        trace->elevation_m[next] = trace->elevation_m[source];
        trace->time_seconds[next] = trace->time_seconds[source];
        trace->valid_fields[next] = trace->valid_fields[source];
        trace->latitude_microdegrees[next] = trace->latitude_microdegrees[source];
        trace->longitude_microdegrees[next] = trace->longitude_microdegrees[source];
        ++next;
    }
    trace->point_count = next;
}

static uint8_t zone(uint16_t value, const uint16_t boundaries[4])
{
    uint8_t result = 0;
    while(result < 4 && value >= boundaries[result]) ++result;
    return result;
}

void ui_ride_summary_model_update(const ui_ride_data_t *data)
{
    if(data == NULL) return;
    ui_concurrency_lock();
    if(!s_summary.active || s_summary.paused ||
       (data->session_id != 0U && data->session_id != s_summary.summary.session_id)) {
        ui_concurrency_unlock();
        return;
    }
    uint64_t now = timer_ms(data);
    if(now <= s_summary.previous_timer_ms) {
        ui_concurrency_unlock();
        return;
    }
    uint64_t elapsed = now - s_summary.previous_timer_ms;
    s_summary.previous_timer_ms = now;
    ui_ride_summary_t *summary = &s_summary.summary;
    summary->duration_seconds = (uint32_t)((now - s_summary.start_timer_ms) / 1000U);
    if(valid(data, UI_RIDE_FIELD_DISTANCE)) {
        summary->metric_flags |= UI_REPORT_HAS_DISTANCE;
        if(!s_summary.distance_baseline_valid) {
            s_summary.start_distance_km = data->distance;
            s_summary.distance_baseline_valid = true;
        }
        summary->distance_km = fmaxf(0, data->distance - s_summary.start_distance_km);
    }
    summary->average_speed_kmh = now > s_summary.start_timer_ms ?
        (float)(summary->distance_km * 3600000.0 / (now - s_summary.start_timer_ms)) : 0;
    uint32_t valid_fields = 0;
    for(unsigned field = 0; field < UI_RIDE_FIELD_COUNT; ++field)
        if(valid(data, (ui_ride_field_t)field)) valid_fields |= UI_RIDE_FIELD_BIT(field);
    if(valid(data, UI_RIDE_FIELD_SPEED)) {
        summary->metric_flags |= UI_REPORT_HAS_SPEED;
        if(data->speed > 0.5f) s_summary.moving_ms += elapsed;
        if(data->speed > summary->maximum_speed_kmh) summary->maximum_speed_kmh = data->speed;
    }
    summary->moving_time_seconds = (uint32_t)(s_summary.moving_ms / 1000U);
    if(valid(data, UI_RIDE_FIELD_ASCENT)) {
        summary->metric_flags |= UI_REPORT_HAS_ASCENT;
        if(!s_summary.ascent_baseline_valid) {
            s_summary.start_ascent_m = data->ascent_m;
            s_summary.ascent_baseline_valid = true;
        }
        summary->ascent_m = data->ascent_m >= s_summary.start_ascent_m ? data->ascent_m - s_summary.start_ascent_m : 0;
    }
    if(valid(data, UI_RIDE_FIELD_CALORIES)) {
        summary->metric_flags |= UI_REPORT_HAS_CALORIES;
        if(!s_summary.calories_baseline_valid) {
            s_summary.start_calories_kcal = data->calories_kcal;
            s_summary.calories_baseline_valid = true;
        }
        summary->calories_kcal = data->calories_kcal >= s_summary.start_calories_kcal ? data->calories_kcal - s_summary.start_calories_kcal : 0;
    }
    if(valid(data, UI_RIDE_FIELD_ALTITUDE)) {
        summary->metric_flags |= UI_REPORT_HAS_ALTITUDE;
        if(!s_summary.altitude_valid) summary->maximum_elevation_m = data->altitude_m;
        if(s_summary.altitude_valid && data->altitude_m < s_summary.previous_altitude_m)
            summary->descent_m += (uint32_t)(s_summary.previous_altitude_m - data->altitude_m);
        s_summary.previous_altitude_m = data->altitude_m;
        s_summary.altitude_valid = true;
        if(data->altitude_m > summary->maximum_elevation_m) summary->maximum_elevation_m = data->altitude_m;
    }
    if(valid(data, UI_RIDE_FIELD_HEART_RATE)) {
        summary->has_heart_rate = true;
        s_summary.heart_rate_sum += (uint64_t)data->heart_rate * elapsed;
        s_summary.heart_rate_ms += elapsed;
        summary->average_heart_rate = (uint16_t)(s_summary.heart_rate_sum / s_summary.heart_rate_ms);
        if(data->heart_rate > summary->maximum_heart_rate) summary->maximum_heart_rate = data->heart_rate;
        if(data->heart_rate < summary->minimum_heart_rate) summary->minimum_heart_rate = data->heart_rate;
        s_summary.heart_zone_ms[zone(data->heart_rate, s_heart_boundaries)] += elapsed;
    }
    if(valid(data, UI_RIDE_FIELD_POWER)) {
        summary->has_power = true;
        s_summary.power_sum += (uint64_t)data->power * elapsed;
        s_summary.power_ms += elapsed;
        summary->average_power = (uint16_t)(s_summary.power_sum / s_summary.power_ms);
        if(data->power > summary->maximum_power) summary->maximum_power = data->power;
        s_summary.power_zone_ms[zone(data->power, s_power_boundaries)] += elapsed;
    }
    if(valid(data, UI_RIDE_FIELD_CADENCE)) {
        summary->has_cadence = true;
        s_summary.cadence_sum += (uint64_t)data->cadence * elapsed;
        s_summary.cadence_ms += elapsed;
        summary->average_cadence = (uint16_t)(s_summary.cadence_sum / s_summary.cadence_ms);
        if(data->cadence > summary->maximum_cadence) summary->maximum_cadence = data->cadence;
    }
    if(valid(data, UI_RIDE_FIELD_TEMPERATURE)) {
        summary->has_temperature = true;
        s_summary.temperature_sum += (int64_t)data->temperature_c * 10 * (int64_t)elapsed;
        s_summary.temperature_ms += elapsed;
        summary->average_temperature_tenths_c = (int16_t)(s_summary.temperature_sum / (int64_t)s_summary.temperature_ms);
    }
    ui_ride_report_trace_t *trace = &summary->trace;
    for(unsigned i = 0; i < UI_RIDE_REPORT_ZONE_COUNT; ++i) {
        trace->heart_rate_zone_seconds[i] = (uint32_t)(s_summary.heart_zone_ms[i] / 1000U);
        trace->power_zone_seconds[i] = (uint32_t)(s_summary.power_zone_ms[i] / 1000U);
    }
    if(trace->point_count == 0 || trace->time_seconds[trace->point_count - 1] != summary->duration_seconds) {
        if(trace->point_count >= UI_RIDE_REPORT_POINT_MAX) compact_trace(trace);
        uint16_t i = trace->point_count++;
        trace->speed_tenths[i] = clamp_u16(data->speed, 10);
        trace->heart_rate[i] = data->heart_rate;
        trace->power[i] = data->power;
        trace->cadence[i] = data->cadence;
        trace->elevation_m[i] = clamp_i16(data->altitude_m);
        trace->time_seconds[i] = summary->duration_seconds;
        trace->valid_fields[i] = valid_fields;
        if(valid(data, UI_RIDE_FIELD_POSITION)) {
            trace->latitude_microdegrees[i] = (int32_t)lround(data->latitude * 1000000);
            trace->longitude_microdegrees[i] = (int32_t)lround(data->longitude * 1000000);
        }
    }
    ++summary->version;
    ui_concurrency_unlock();
}

void ui_ride_summary_model_finish(bool saved)
{
    ui_concurrency_lock();
    s_summary.active = false;
    if(s_summary.summary.minimum_heart_rate == UINT16_MAX) s_summary.summary.minimum_heart_rate = 0;
    s_summary.summary.saved = saved;
    ++s_summary.summary.version;
    ui_concurrency_unlock();
}

bool ui_ride_summary_model_restore(const ui_ride_summary_t *summary)
{
    if(summary == NULL || !isfinite(summary->distance_km) || summary->distance_km < 0 ||
       summary->trace.point_count > UI_RIDE_REPORT_POINT_MAX) return false;
    ui_concurrency_lock();
    if(s_summary.active) { ui_concurrency_unlock(); return false; }
    s_summary.summary = *summary;
    ++s_summary.summary.version;
    ui_concurrency_unlock();
    return true;
}

void ui_ride_summary_model_set_paused(bool paused)
{
    ui_concurrency_lock();
    s_summary.paused = paused;
    ui_concurrency_unlock();
}

void ui_ride_summary_model_get_zones(uint16_t heart_rate[4], uint16_t power[4])
{
    ui_concurrency_lock();
    if(heart_rate != NULL) memcpy(heart_rate, s_heart_boundaries, sizeof(s_heart_boundaries));
    if(power != NULL) memcpy(power, s_power_boundaries, sizeof(s_power_boundaries));
    ui_concurrency_unlock();
}

bool ui_ride_summary_model_set_zones(const uint16_t heart_rate[4], const uint16_t power[4])
{
    if(heart_rate == NULL || power == NULL || heart_rate[0] == 0 || power[0] == 0) return false;
    for(unsigned i = 1; i < 4; ++i)
        if(heart_rate[i] <= heart_rate[i - 1] || power[i] <= power[i - 1]) return false;
    ui_concurrency_lock();
    if(s_summary.active) { ui_concurrency_unlock(); return false; }
    memcpy(s_heart_boundaries, heart_rate, sizeof(s_heart_boundaries));
    memcpy(s_power_boundaries, power, sizeof(s_power_boundaries));
    ui_concurrency_unlock();
    return true;
}

bool ui_ride_summary_model_get(ui_ride_summary_t *summary)
{
    if(summary == NULL) return false;
    ui_concurrency_lock();
    *summary = s_summary.summary;
    ui_concurrency_unlock();
    return true;
}

bool ui_ride_summary_model_active(void)
{
    ui_concurrency_lock();
    bool active = s_summary.active;
    ui_concurrency_unlock();
    return active;
}

uint32_t ui_ride_summary_model_session_id(void)
{
    ui_concurrency_lock();
    uint32_t id = s_summary.summary.session_id;
    ui_concurrency_unlock();
    return id;
}
