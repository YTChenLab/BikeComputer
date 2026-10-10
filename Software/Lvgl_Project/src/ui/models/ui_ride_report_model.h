#ifndef UI_RIDE_REPORT_MODEL_H
#define UI_RIDE_REPORT_MODEL_H

#include "ui_ride_history_model.h"
#include "ui_ride_report_data.h"
#include "ui_ride_summary_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    UI_RIDE_REPORT_SUMMARY_MODE = 0,
    UI_RIDE_REPORT_DETAIL_MODE,
    UI_RIDE_REPORT_MODE_COUNT
} ui_ride_report_mode_t;

typedef enum
{
    UI_RIDE_REPORT_CASE_COMPLETE = 0,
    UI_RIDE_REPORT_CASE_NO_POWER,
    UI_RIDE_REPORT_CASE_NO_HR_CADENCE,
    UI_RIDE_REPORT_CASE_SHORT_RIDE,
    UI_RIDE_REPORT_CASE_LONG_CLIMB
} ui_ride_report_mock_case_t;

typedef struct
{
    int64_t started_at_utc;
    int64_t finished_at_utc;
    uint32_t duration_seconds;
    uint32_t moving_time_seconds;
    uint8_t metric_flags; /* UI_REPORT_HAS_* and UI_REPORT_METRICS_KNOWN. */
    float distance_km;
    float average_speed_kmh;
    float maximum_speed_kmh;
    uint32_t total_ascent_m;
    uint32_t total_descent_m;
    int32_t maximum_elevation_m;
    uint32_t calories_kcal;
    bool has_heart_rate;
    uint16_t average_heart_rate;
    uint16_t maximum_heart_rate;
    uint16_t minimum_heart_rate;
    bool has_power;
    uint16_t average_power;
    uint16_t maximum_power;
    bool has_cadence;
    uint16_t average_cadence;
    uint16_t maximum_cadence;
    bool has_temperature;
    int16_t average_temperature_tenths_c;
    bool saved;
    ui_ride_report_trace_t trace;
} ui_ride_report_model_t;

bool ui_ride_report_model_from_summary(ui_ride_report_model_t *report,
                                       const ui_ride_summary_t *summary);
bool ui_ride_report_model_from_record(ui_ride_report_model_t *report,
                                      const ui_ride_record_t *record);
bool ui_ride_report_model_valid(const ui_ride_report_model_t *report);
size_t ui_ride_report_downsample_u16(const uint16_t *input,
                                     size_t input_count,
                                     uint16_t *output,
                                     size_t output_capacity);
/* PC/test-only implementation; never part of the STM32 library. */
void ui_ride_report_model_make_mock(ui_ride_report_model_t *report,
                                    ui_ride_report_mock_case_t mock_case);

#endif /* UI_RIDE_REPORT_MODEL_H */
