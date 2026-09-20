#include "ui/models/ui_ride_report_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_downsample(void)
{
    uint16_t input[240];
    uint16_t output[120];
    for(uint16_t index = 0U; index < 240U; index++) input[index] = index;
    size_t count = ui_ride_report_downsample_u16(
        input, 240U, output, 120U);
    assert(count == 120U);
    assert(output[0] == 0U);
    assert(output[119] == 239U);
}

static void test_optional_cases(void)
{
    ui_ride_report_model_t report;
    ui_ride_report_model_make_mock(
        &report, UI_RIDE_REPORT_CASE_COMPLETE);
    assert(ui_ride_report_model_valid(&report));
    assert(report.has_heart_rate && report.has_power && report.has_cadence);
    assert(report.trace.point_count == UI_RIDE_REPORT_POINT_MAX);

    ui_ride_report_model_make_mock(
        &report, UI_RIDE_REPORT_CASE_NO_POWER);
    assert(!report.has_power);
    for(uint8_t index = 0U; index < UI_RIDE_REPORT_ZONE_COUNT; index++)
        assert(report.trace.power_zone_seconds[index] == 0U);

    ui_ride_report_model_make_mock(
        &report, UI_RIDE_REPORT_CASE_NO_HR_CADENCE);
    assert(!report.has_heart_rate && !report.has_cadence);

    ui_ride_report_model_make_mock(
        &report, UI_RIDE_REPORT_CASE_SHORT_RIDE);
    assert(report.distance_km < 10.0f);

    ui_ride_report_model_make_mock(
        &report, UI_RIDE_REPORT_CASE_LONG_CLIMB);
    assert(report.distance_km > 100.0f && report.total_ascent_m > 2000U);
}

static void test_sources(void)
{
    ui_ride_summary_t summary = {
        .distance_km = 42.5f,
        .duration_seconds = 5400U,
        .moving_time_seconds = 5100U,
        .average_speed_kmh = 28.3f,
        .maximum_speed_kmh = 49.1f,
        .ascent_m = 620U,
        .descent_m = 590U,
        .maximum_elevation_m = 888,
        .calories_kcal = 760U,
        .has_heart_rate = true,
        .average_heart_rate = 146U,
        .maximum_heart_rate = 181U,
        .minimum_heart_rate = 98U,
        .saved = true
    };
    summary.trace.point_count = 2U;
    summary.trace.speed_tenths[0] = 200U;
    summary.trace.speed_tenths[1] = 320U;
    ui_ride_report_model_t report;
    assert(ui_ride_report_model_from_summary(&report, &summary));
    assert(report.distance_km == summary.distance_km);
    assert(report.has_heart_rate);
    assert(report.trace.speed_tenths[1] == 320U);

    ui_ride_record_t record = {
        .id = 1U,
        .started_at_utc = 1750000000,
        .finished_at_utc = 1750005400,
        .distance_km = 42.5f,
        .duration_seconds = 5400U,
        .moving_time_seconds = 5100U,
        .average_speed_kmh = 28.3f,
        .maximum_speed_kmh = 49.1f,
        .has_power = true,
        .average_power = 220U,
        .maximum_power = 640U
    };
    record.trace.point_count = 2U;
    assert(ui_ride_report_model_from_record(&report, &record));
    assert(report.finished_at_utc == record.finished_at_utc);
    assert(report.has_power && report.maximum_power == 640U);
}

int main(void)
{
    test_downsample();
    test_optional_cases();
    test_sources();
    puts("ui_ride_report_model_test: PASS");
    return 0;
}
