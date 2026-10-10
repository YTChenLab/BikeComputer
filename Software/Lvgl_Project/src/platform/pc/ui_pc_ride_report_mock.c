#include "ui/models/ui_ride_report_model.h"
#include <string.h>

static uint16_t mock_wave(uint16_t index, uint16_t base, uint16_t amplitude,
                          uint16_t period)
{
    uint16_t phase = index % period;
    uint16_t half = period / 2U;
    uint16_t triangle = phase <= half ? phase : period - phase;
    return (uint16_t)(base + (uint32_t)amplitude * triangle / half);
}

void ui_ride_report_model_make_mock(ui_ride_report_model_t *report,
                                    ui_ride_report_mock_case_t mock_case)
{
    if(report == NULL) return;
    memset(report, 0, sizeof(*report));
    report->duration_seconds = 3U * 3600U + 21U * 60U + 18U;
    report->moving_time_seconds = report->duration_seconds;
    report->distance_km = 86.7f;
    report->average_speed_kmh = 25.8f;
    report->maximum_speed_kmh = 47.0f;
    report->total_ascent_m = 1268U;
    report->total_descent_m = 1245U;
    report->maximum_elevation_m = 1826;
    report->calories_kcal = 1536U;
    report->has_heart_rate = true;
    report->average_heart_rate = 147U;
    report->maximum_heart_rate = 182U;
    report->minimum_heart_rate = 102U;
    report->has_power = true;
    report->average_power = 249U;
    report->maximum_power = 782U;
    report->has_cadence = true;
    report->average_cadence = 84U;
    report->maximum_cadence = 112U;
    report->has_temperature = true;
    report->average_temperature_tenths_c = 214;
    report->saved = true;
    report->metric_flags = UI_REPORT_METRICS_KNOWN | 31U;
    report->trace.point_count = UI_RIDE_REPORT_POINT_MAX;
    report->trace.time_is_estimated = true;
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        report->trace.time_seconds[index] = (uint64_t)report->duration_seconds * index /
                                            (UI_RIDE_REPORT_POINT_MAX - 1);
        report->trace.valid_fields[index] = (UINT32_C(1) << 0) | (UINT32_C(1) << 1) |
            (UINT32_C(1) << 2) | (UINT32_C(1) << 3) | (UINT32_C(1) << 8);
        report->trace.speed_tenths[index] =
            mock_wave(index, 110U, 250U, 28U);
        report->trace.heart_rate[index] =
            mock_wave(index, 98U, 62U, 44U);
        report->trace.power[index] = mock_wave(index, 170U, 210U, 17U);
        report->trace.cadence[index] = mock_wave(index, 72U, 28U, 19U);
        report->trace.elevation_m[index] =
            (int16_t)(420 + mock_wave(index, 0U, 1350U, 118U));
    }
    static const uint8_t heart_percent[UI_RIDE_REPORT_ZONE_COUNT] =
        {8U, 21U, 39U, 26U, 6U};
    static const uint8_t power_percent[UI_RIDE_REPORT_ZONE_COUNT] =
        {11U, 26U, 34U, 18U, 11U};
    for(uint8_t index = 0U; index < UI_RIDE_REPORT_ZONE_COUNT; index++) {
        report->trace.heart_rate_zone_seconds[index] =
            report->duration_seconds * heart_percent[index] / 100U;
        report->trace.power_zone_seconds[index] =
            report->duration_seconds * power_percent[index] / 100U;
    }
    if(mock_case == UI_RIDE_REPORT_CASE_NO_POWER) {
        report->has_power = false;
        memset(report->trace.power, 0, sizeof(report->trace.power));
        memset(report->trace.power_zone_seconds, 0,
               sizeof(report->trace.power_zone_seconds));
    }
    else if(mock_case == UI_RIDE_REPORT_CASE_NO_HR_CADENCE) {
        report->has_heart_rate = false;
        report->has_cadence = false;
        memset(report->trace.heart_rate, 0, sizeof(report->trace.heart_rate));
        memset(report->trace.cadence, 0, sizeof(report->trace.cadence));
        memset(report->trace.heart_rate_zone_seconds, 0,
               sizeof(report->trace.heart_rate_zone_seconds));
    }
    else if(mock_case == UI_RIDE_REPORT_CASE_SHORT_RIDE) {
        report->duration_seconds = 18U * 60U;
        report->moving_time_seconds = report->duration_seconds;
        report->distance_km = 6.4f;
        report->total_ascent_m = 54U;
        report->total_descent_m = 48U;
    }
    else if(mock_case == UI_RIDE_REPORT_CASE_LONG_CLIMB) {
        report->distance_km = 142.6f;
        report->duration_seconds = 6U * 3600U + 42U * 60U;
        report->moving_time_seconds = report->duration_seconds;
        report->total_ascent_m = 2860U;
        report->total_descent_m = 2812U;
        report->maximum_elevation_m = 2360;
    }
}
