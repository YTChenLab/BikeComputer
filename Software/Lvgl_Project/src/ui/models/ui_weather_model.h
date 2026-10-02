#ifndef UI_WEATHER_MODEL_H
#define UI_WEATHER_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define UI_WEATHER_OBSERVER_MAX 4U

typedef enum
{
    UI_WEATHER_CLEAR = 0,
    UI_WEATHER_CLOUDY,
    UI_WEATHER_RAIN,
    UI_WEATHER_CONDITION_COUNT
} ui_weather_condition_t;

typedef struct
{
    int8_t temperature_c;
    int8_t feels_like_c;
    uint8_t wind_kmh;
    uint8_t rain_percent;
    uint16_t air_quality_index;
    ui_weather_condition_t condition;
    uint32_t version;
    bool quality_known, valid;
    uint32_t sampled_at_ms, stale_after_ms;
} ui_weather_data_t;

typedef void (*ui_weather_observer_cb_t)(const ui_weather_data_t *weather,
                                         void *user_data);

void ui_weather_model_init(void);
bool ui_weather_model_get(ui_weather_data_t *weather);
bool ui_weather_model_submit(const ui_weather_data_t *weather);
/* UI-thread drain; producer submit never invokes a UI observer. */
bool ui_weather_model_process(uint32_t now_ms);
int32_t ui_weather_model_subscribe(ui_weather_observer_cb_t callback,
                                   void *user_data);
bool ui_weather_model_unsubscribe(int32_t subscription_id);

#endif /* UI_WEATHER_MODEL_H */
