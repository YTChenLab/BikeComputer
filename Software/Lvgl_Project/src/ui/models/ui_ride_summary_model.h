#ifndef UI_RIDE_SUMMARY_MODEL_H
#define UI_RIDE_SUMMARY_MODEL_H

#include "ui_ride_data_model.h"
#include "ui_ride_report_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float distance_km;
    uint32_t duration_seconds;
    uint32_t moving_time_seconds;
    float average_speed_kmh;
    float maximum_speed_kmh;
    uint32_t ascent_m;
    uint32_t descent_m;
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
    ui_ride_report_trace_t trace;
    bool saved;
    uint32_t version;
} ui_ride_summary_t;

void ui_ride_summary_model_init(void);
void ui_ride_summary_model_begin(const ui_ride_data_t *data);
void ui_ride_summary_model_update(const ui_ride_data_t *data);
void ui_ride_summary_model_finish(bool saved);
bool ui_ride_summary_model_get(ui_ride_summary_t *summary);
bool ui_ride_summary_model_active(void);

#endif /* UI_RIDE_SUMMARY_MODEL_H */
