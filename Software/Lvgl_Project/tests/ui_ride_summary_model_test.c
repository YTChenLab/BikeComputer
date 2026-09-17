#include "ui/models/ui_ride_summary_model.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    ui_ride_summary_model_init();
    ui_ride_data_t data = {
        .speed = 20.0f,
        .heart_rate = 120U,
        .power = 180U,
        .cadence = 80U,
        .grade = 2.0f,
        .distance = 10.0f,
        .ride_seconds = 1000U
    };
    ui_ride_summary_model_begin(&data);
    assert(ui_ride_summary_model_active());

    data.speed = 24.0f;
    data.heart_rate = 130U;
    data.power = 200U;
    data.cadence = 84U;
    data.grade = 4.0f;
    data.distance = 10.5f;
    data.ride_seconds = 1060U;
    ui_ride_summary_model_update(&data);
    data.speed = 30.0f;
    data.heart_rate = 150U;
    data.power = 260U;
    data.cadence = 92U;
    data.grade = -2.0f;
    data.distance = 11.0f;
    data.ride_seconds = 1120U;
    ui_ride_summary_model_update(&data);
    ui_ride_summary_model_finish(true);

    ui_ride_summary_t summary;
    assert(ui_ride_summary_model_get(&summary));
    assert(!ui_ride_summary_model_active());
    assert(fabsf(summary.distance_km - 1.0f) < 0.001f);
    assert(summary.duration_seconds == 120U);
    assert(fabsf(summary.average_speed_kmh - 27.0f) < 0.001f);
    assert(fabsf(summary.maximum_speed_kmh - 30.0f) < 0.001f);
    assert(summary.ascent_m == 20U);
    assert(summary.average_heart_rate == 140U);
    assert(summary.average_power == 230U);
    assert(summary.average_cadence == 88U);
    assert(summary.saved);
    puts("ui_ride_summary_model_test: PASS");
    return 0;
}
