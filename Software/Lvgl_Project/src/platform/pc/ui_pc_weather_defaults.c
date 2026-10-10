#include "platform/ui_platform_internal.h"
#include <string.h>
void ui_platform_initial_weather(ui_weather_data_t *weather)
{
    memset(weather, 0, sizeof(*weather));
    weather->temperature_c = 22; weather->feels_like_c = 21;
    weather->wind_kmh = 12; weather->rain_percent = 10;
    weather->air_quality_index = 42; weather->condition = UI_WEATHER_CLOUDY;
    weather->quality_known = true; weather->version = 1;
}
