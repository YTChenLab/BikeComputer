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
