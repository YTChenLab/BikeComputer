#include "ui/models/ui_ride_data_model.h"
#include "ui/models/ui_ride_summary_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static ui_ride_data_t frame(uint32_t time, uint16_t heart, uint16_t power)
{
    ui_ride_data_t data = {.sample_time_ms = time, .timer_ms = time,
        .ride_seconds = time / 1000, .heart_rate = heart, .power = power};
    ui_ride_data_set_quality(&data, UI_RIDE_FIELD_SPEED, true, true,
                            time, 2000, UI_DATA_SOURCE_WHEEL);
    ui_ride_data_set_quality(&data, UI_RIDE_FIELD_DISTANCE, true, true,
                            time, 2000, UI_DATA_SOURCE_WHEEL);
    ui_ride_data_set_quality(&data, UI_RIDE_FIELD_HEART_RATE, true, heart > 0,
                            time, 2000, UI_DATA_SOURCE_BLE);
    ui_ride_data_set_quality(&data, UI_RIDE_FIELD_POWER, true, true,
                            time, 2000, UI_DATA_SOURCE_BLE);
    return data;
}

int main(void)
{
    ui_ride_data_t data = frame(100, 160, 0);
    assert(ui_ride_data_field_valid(&data, UI_RIDE_FIELD_POWER, 100));
    assert(!ui_ride_data_field_valid(&data, UI_RIDE_FIELD_TEMPERATURE, 100));
    assert(!ui_ride_data_field_valid(&data, UI_RIDE_FIELD_SPEED, 2201));
    assert(!ui_ride_data_field_valid(&data, (ui_ride_field_t)-1, 100));
    ui_ride_data_set_quality(&data, UI_RIDE_FIELD_SPEED, true, true,
                            UINT32_MAX - 99, 300, UI_DATA_SOURCE_WHEEL);
    assert(ui_ride_data_field_valid(&data, UI_RIDE_FIELD_SPEED, 100));
    ui_ride_summary_model_init();
    ui_ride_data_model_init();
    data = frame(0, 160, 0);
    ui_ride_summary_model_begin(&data);
    data = frame(1000, 160, 0);
    data.distance = .01f;
    ui_ride_data_model_submit(&data);
    for(unsigned i = 0; i < 10; ++i) ui_ride_data_model_submit(&data);
    data = frame(4000, 0, 200);
    data.distance = .04f;
    ui_ride_data_model_submit(&data);
    /* No UI drain between producer frames: statistics still see all samples. */
    ui_ride_summary_t result;
    assert(ui_ride_summary_model_get(&result));
    assert(result.average_heart_rate == 160);
    assert(result.average_power == 150);
    assert(result.trace.heart_rate_zone_seconds[3] == 1);
    assert(result.trace.point_count == 2);
    assert(result.trace.time_seconds[1] == 4);
    assert(result.moving_time_seconds == 0);
    assert((result.metric_flags & (UI_REPORT_METRICS_KNOWN | UI_REPORT_HAS_DISTANCE | UI_REPORT_HAS_SPEED)) ==
        (UI_REPORT_METRICS_KNOWN | UI_REPORT_HAS_DISTANCE | UI_REPORT_HAS_SPEED));
    assert(!(result.metric_flags & UI_REPORT_HAS_ALTITUDE));
    assert(!result.has_temperature);
    assert(fabsf(result.average_speed_kmh - 36) < .001f);
    data.timer_ms = 5000;
    data.session_id = result.session_id + 1;
    ui_ride_data_model_submit(&data);
    assert(ui_ride_summary_model_get(&result) && result.duration_seconds == 4);
    ui_ride_summary_model_finish(false);
    assert(ui_ride_summary_model_get(&result) && result.moving_time_seconds == 0);
    ui_ride_data_model_init();
    ui_ride_data_t speed = frame(5000, 0, 0);
    speed.speed = 25;
    assert(ui_ride_data_model_submit_patch(&speed, UI_RIDE_FIELD_BIT(UI_RIDE_FIELD_SPEED)));
    ui_ride_data_t heart = frame(5010, 151, 0);
    assert(ui_ride_data_model_submit_patch(&heart, UI_RIDE_FIELD_BIT(UI_RIDE_FIELD_HEART_RATE)));
    assert(ui_ride_data_model_process());
    ui_ride_data_t merged;
    assert(ui_ride_data_model_get(&merged, NULL));
    assert(merged.speed == 25 && merged.heart_rate == 151);
    heart.sample_time_ms = 4990;
    heart.heart_rate = 99;
    assert(!ui_ride_data_model_submit_patch(&heart, UI_RIDE_FIELD_BIT(UI_RIDE_FIELD_HEART_RATE)));
    assert(!ui_ride_data_model_process());
    /* First late altitude fix can be below sea level. Counter baselines
     * become valid when the sensor appears, rather than counting pre-ride data. */
    ui_ride_summary_model_init();
    data = frame(0, 0, 0);
    ui_ride_summary_model_begin(&data);
    data = frame(1000, 0, 0);
    data.altitude_m = -40;
    data.ascent_m = 500;
    data.calories_kcal = 100;
    ui_ride_data_set_quality(&data, UI_RIDE_FIELD_ALTITUDE, true, true, 1000, 2000, UI_DATA_SOURCE_BAROMETER);
    ui_ride_data_set_quality(&data, UI_RIDE_FIELD_ASCENT, true, true, 1000, 0, UI_DATA_SOURCE_FUSED);
    ui_ride_data_set_quality(&data, UI_RIDE_FIELD_CALORIES, true, true, 1000, 0, UI_DATA_SOURCE_FUSED);
    ui_ride_summary_model_update(&data);
    assert(ui_ride_summary_model_get(&result));
    assert(result.maximum_elevation_m == -40 && result.ascent_m == 0 && result.calories_kcal == 0);
    data.timer_ms = 2000;
    data.sample_time_ms = 2000;
    data.altitude_m = -30;
    data.ascent_m = 510;
    data.calories_kcal = 105;
    ui_ride_summary_model_update(&data);
    assert(ui_ride_summary_model_get(&result));
    assert(result.maximum_elevation_m == -30 && result.ascent_m == 10 && result.calories_kcal == 5);
    ui_ride_summary_model_finish(false);
    puts("ui_data_contract_test: PASS");
    return 0;
}
