#include "ui_ride_report_model.h"

#include <math.h>
#include <string.h>

static void copy_summary(ui_ride_report_model_t *report,
                         const ui_ride_summary_t *summary)
{
    report->started_at_utc = summary->started_at_utc;
    report->finished_at_utc = summary->finished_at_utc;
    report->duration_seconds = summary->duration_seconds;
    report->moving_time_seconds = summary->moving_time_seconds;
    report->metric_flags = summary->metric_flags;
    report->distance_km = summary->distance_km;
    report->average_speed_kmh = summary->average_speed_kmh;
    report->maximum_speed_kmh = summary->maximum_speed_kmh;
    report->total_ascent_m = summary->ascent_m;
    report->total_descent_m = summary->descent_m;
    report->maximum_elevation_m = summary->maximum_elevation_m;
    report->calories_kcal = summary->calories_kcal;
    report->has_heart_rate = summary->has_heart_rate;
    report->average_heart_rate = summary->average_heart_rate;
    report->maximum_heart_rate = summary->maximum_heart_rate;
    report->minimum_heart_rate = summary->minimum_heart_rate;
    report->has_power = summary->has_power;
    report->average_power = summary->average_power;
    report->maximum_power = summary->maximum_power;
    report->has_cadence = summary->has_cadence;
    report->average_cadence = summary->average_cadence;
    report->maximum_cadence = summary->maximum_cadence;
    report->has_temperature = summary->has_temperature;
    report->average_temperature_tenths_c =
        summary->average_temperature_tenths_c;
    report->saved = summary->saved;
    report->trace = summary->trace;
}

bool ui_ride_report_model_from_summary(ui_ride_report_model_t *report,
                                       const ui_ride_summary_t *summary)
{
    if(report == NULL || summary == NULL) return false;
    memset(report, 0, sizeof(*report));
    copy_summary(report, summary);
    return ui_ride_report_model_valid(report);
}

bool ui_ride_report_model_from_record(ui_ride_report_model_t *report,
                                      const ui_ride_record_t *record)
{
    if(report == NULL || record == NULL) return false;
    memset(report, 0, sizeof(*report));
    report->started_at_utc = record->started_at_utc;
    report->finished_at_utc = record->finished_at_utc;
    report->duration_seconds = record->duration_seconds;
    report->moving_time_seconds = record->moving_time_seconds;
    report->metric_flags = record->metric_flags;
    report->distance_km = record->distance_km;
    report->average_speed_kmh = record->average_speed_kmh;
    report->maximum_speed_kmh = record->maximum_speed_kmh;
    report->total_ascent_m = record->ascent_m;
    report->total_descent_m = record->descent_m;
    report->maximum_elevation_m = record->maximum_elevation_m;
    report->calories_kcal = record->calories_kcal;
    report->has_heart_rate = record->has_heart_rate;
    report->average_heart_rate = record->average_heart_rate;
    report->maximum_heart_rate = record->maximum_heart_rate;
    report->minimum_heart_rate = record->minimum_heart_rate;
    report->has_power = record->has_power;
    report->average_power = record->average_power;
    report->maximum_power = record->maximum_power;
    report->has_cadence = record->has_cadence;
    report->average_cadence = record->average_cadence;
    report->maximum_cadence = record->maximum_cadence;
    report->has_temperature = record->has_temperature;
    report->average_temperature_tenths_c =
        record->average_temperature_tenths_c;
    report->saved = true;
    report->trace = record->trace;
    return ui_ride_report_model_valid(report);
}

bool ui_ride_report_model_valid(const ui_ride_report_model_t *report)
{
    if(report == NULL || !isfinite(report->distance_km) ||
       !isfinite(report->average_speed_kmh) ||
       !isfinite(report->maximum_speed_kmh) || report->distance_km < 0.0f ||
       report->average_speed_kmh < 0.0f ||
       report->maximum_speed_kmh < 0.0f ||
       report->trace.point_count > UI_RIDE_REPORT_POINT_MAX)
        return false;
    return true;
}

size_t ui_ride_report_downsample_u16(const uint16_t *input,
                                     size_t input_count,
                                     uint16_t *output,
                                     size_t output_capacity)
{
    if(input == NULL || output == NULL || input_count == 0U ||
       output_capacity == 0U) return 0U;
    size_t count = input_count < output_capacity ? input_count :
                                                    output_capacity;
    if(count == 1U) {
        output[0] = input[0];
        return 1U;
    }
    for(size_t index = 0U; index < count; index++) {
        size_t source = index * (input_count - 1U) / (count - 1U);
        output[index] = input[source];
    }
    return count;
}

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
