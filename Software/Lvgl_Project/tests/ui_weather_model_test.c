#include "ui/models/ui_weather_model.h"

#include <assert.h>
#include <stdio.h>

static uint32_t s_notifications;

static void changed(const ui_weather_data_t *weather, void *user_data)
{
    (void)user_data;
    assert(weather != NULL);
    s_notifications++;
}

int main(void)
{
    ui_weather_model_init();
    ui_weather_data_t weather;
    assert(ui_weather_model_get(&weather));
    assert(weather.temperature_c == 22);
    int32_t subscription = ui_weather_model_subscribe(changed, NULL);
    assert(subscription > 0);
    weather.temperature_c = 27;
    weather.rain_percent = 140U;
    weather.air_quality_index = 620U;
    weather.condition = UI_WEATHER_CLEAR;
    assert(ui_weather_model_submit(&weather));
    assert(s_notifications == 1U);
    assert(ui_weather_model_get(&weather));
    assert(weather.temperature_c == 27);
    assert(weather.rain_percent == 100U);
    assert(weather.air_quality_index == 500U);
    assert(ui_weather_model_submit(&weather));
    assert(s_notifications == 1U);
    weather.version += 100U;
    assert(ui_weather_model_submit(&weather));
    assert(s_notifications == 1U);
    assert(ui_weather_model_unsubscribe(subscription));
    puts("ui_weather_model_test: PASS");
    return 0;
}
