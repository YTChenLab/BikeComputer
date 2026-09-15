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
    uint32_t ride_seconds;
    uint8_t clock_hour;
    uint8_t clock_minute;
    uint8_t satellites;
    uint8_t battery;
    bool wifi;
    bool bluetooth;
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
void ui_ride_data_model_set_connectivity(bool wifi, bool bluetooth);

#endif /* UI_RIDE_DATA_MODEL_H */
