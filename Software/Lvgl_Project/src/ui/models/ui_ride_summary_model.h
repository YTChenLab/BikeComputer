#ifndef UI_RIDE_SUMMARY_MODEL_H
#define UI_RIDE_SUMMARY_MODEL_H

#include "ui_ride_data_model.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float distance_km;
    uint32_t duration_seconds;
    float average_speed_kmh;
    float maximum_speed_kmh;
    uint32_t ascent_m;
    uint16_t average_heart_rate;
    uint16_t average_power;
    uint16_t average_cadence;
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
