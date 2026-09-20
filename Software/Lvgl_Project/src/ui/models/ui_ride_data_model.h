#ifndef UI_RIDE_DATA_MODEL_H
#define UI_RIDE_DATA_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define UI_RIDE_DATA_OBSERVER_MAX 4

typedef struct
{
    float speed;
    uint16_t heart_rate;
    uint16_t power;
    uint16_t cadence;
    float grade;
    float distance;
    float average_speed;
    float maximum_speed;
    int32_t altitude_m;
    uint32_t ascent_m;
    uint32_t calories_kcal;
    int8_t temperature_c;
    uint8_t heart_rate_zone;
    uint8_t current_lap;
    uint8_t total_laps;
    uint32_t ride_seconds;
    uint8_t clock_hour;
    uint8_t clock_minute;
    uint8_t satellites;
    uint8_t battery;
} ui_ride_data_t;

typedef void (*ui_ride_data_observer_cb_t)(const ui_ride_data_t *data,
                                           uint32_t version,
                                           void *user_data);

void ui_ride_data_model_init(void);
int32_t ui_ride_data_model_subscribe(ui_ride_data_observer_cb_t callback,
                                     void *user_data);
bool ui_ride_data_model_unsubscribe(int32_t subscription_id);

/* Producer-safe latest-value mailbox; it does not call UI observers. */
void ui_ride_data_model_submit(const ui_ride_data_t *data);

/* UI-thread drain. Multiple producer updates collapse to the latest snapshot. */
bool ui_ride_data_model_process(void);
bool ui_ride_data_model_get(ui_ride_data_t *data, uint32_t *version);

#endif /* UI_RIDE_DATA_MODEL_H */
