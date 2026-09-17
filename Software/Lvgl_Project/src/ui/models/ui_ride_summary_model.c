#include "ui_ride_summary_model.h"

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
    double speed_sum;
    float ascent_accumulator_m;
    uint32_t sample_count;
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
    s_summary.active = true;
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

    float delta_km = data->distance - s_summary.previous_distance_km;
    if(delta_km > 0.0f && delta_km < 1.0f && data->grade > 0.0f)
        s_summary.ascent_accumulator_m += delta_km * 10.0f * data->grade;
    s_summary.summary.ascent_m =
        (uint32_t)lroundf(s_summary.ascent_accumulator_m);
    s_summary.previous_distance_km = data->distance;

    if(data->speed > s_summary.summary.maximum_speed_kmh)
        s_summary.summary.maximum_speed_kmh = data->speed;
    s_summary.speed_sum += data->speed;
    s_summary.heart_rate_sum += data->heart_rate;
    s_summary.power_sum += data->power;
    s_summary.cadence_sum += data->cadence;
    s_summary.sample_count++;
    s_summary.summary.average_speed_kmh =
        (float)(s_summary.speed_sum / s_summary.sample_count);
    s_summary.summary.average_heart_rate =
        (uint16_t)(s_summary.heart_rate_sum / s_summary.sample_count);
    s_summary.summary.average_power =
        (uint16_t)(s_summary.power_sum / s_summary.sample_count);
    s_summary.summary.average_cadence =
        (uint16_t)(s_summary.cadence_sum / s_summary.sample_count);
    s_summary.summary.version++;
}

void ui_ride_summary_model_finish(bool saved)
{
    s_summary.active = false;
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
