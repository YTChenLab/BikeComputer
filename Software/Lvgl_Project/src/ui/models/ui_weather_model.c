#include "ui_weather_model.h"
#include "../core/ui_concurrency.h"

#include <string.h>

typedef struct
{
    int32_t id;
    ui_weather_observer_cb_t callback;
    void *user_data;
} weather_observer_t;

static ui_weather_data_t s_weather;
static ui_weather_data_t s_pending;
static bool s_pending_valid;
static weather_observer_t s_observers[UI_WEATHER_OBSERVER_MAX];
static int32_t s_next_id;

static bool weather_equal(const ui_weather_data_t *left,
                          const ui_weather_data_t *right)
{
    return left->temperature_c == right->temperature_c &&
           left->feels_like_c == right->feels_like_c &&
           left->wind_kmh == right->wind_kmh &&
           left->rain_percent == right->rain_percent &&
           left->air_quality_index == right->air_quality_index &&
           left->condition == right->condition && left->quality_known == right->quality_known &&
           left->valid == right->valid && left->sampled_at_ms == right->sampled_at_ms;
}

void ui_weather_model_init(void)
{
    memset(&s_weather, 0, sizeof(s_weather));
    memset(s_observers, 0, sizeof(s_observers));
    s_weather.temperature_c = 22;
    s_weather.feels_like_c = 21;
    s_weather.wind_kmh = 12U;
    s_weather.rain_percent = 10U;
    s_weather.air_quality_index = 42U;
    s_weather.condition = UI_WEATHER_CLOUDY;
    s_weather.version = 1U;
    s_weather.quality_known = true;
    s_weather.valid = false;
    s_pending_valid = false;
    s_next_id = 1;
}

bool ui_weather_model_get(ui_weather_data_t *weather)
{
    if(weather == NULL) return false;
    ui_concurrency_lock();
    *weather = s_weather;
    ui_concurrency_unlock();
    return true;
}

bool ui_weather_model_submit(const ui_weather_data_t *weather)
{
    if(weather == NULL) return false;
    ui_concurrency_lock();
    const ui_weather_data_t *latest = s_pending_valid ? &s_pending : &s_weather;
    if(weather->quality_known && latest->quality_known &&
       (int32_t)(weather->sampled_at_ms - latest->sampled_at_ms) < 0) {
        ui_concurrency_unlock();
        return false;
    }
    s_pending = *weather;
    s_pending_valid = true;
    ui_concurrency_unlock();
    return true;
}

bool ui_weather_model_process(uint32_t now_ms)
{
    ui_concurrency_lock();
    ui_weather_data_t next = s_pending_valid ? s_pending : s_weather;
    s_pending_valid = false;
    if(next.quality_known && next.valid && next.stale_after_ms &&
       (uint32_t)(now_ms - next.sampled_at_ms) > next.stale_after_ms) next.valid = false;
    if(next.rain_percent > 100U) next.rain_percent = 100U;
    if(next.wind_kmh > 200U) next.wind_kmh = 200U;
    if(next.air_quality_index > 500U) next.air_quality_index = 500U;
    if(next.condition >= UI_WEATHER_CONDITION_COUNT)
        next.condition = UI_WEATHER_CLOUDY;
    uint32_t previous_version = s_weather.version;
    if(weather_equal(&next, &s_weather)) { ui_concurrency_unlock(); return false; }
    next.version = previous_version + 1U;
    s_weather = next;
    ui_concurrency_unlock();
    for(uint8_t index = 0U; index < UI_WEATHER_OBSERVER_MAX; index++) {
        if(s_observers[index].callback != NULL)
            s_observers[index].callback(&next,
                                        s_observers[index].user_data);
    }
    return true;
}

int32_t ui_weather_model_subscribe(ui_weather_observer_cb_t callback,
                                   void *user_data)
{
    if(callback == NULL) return -1;
    for(uint8_t index = 0U; index < UI_WEATHER_OBSERVER_MAX; index++) {
        if(s_observers[index].callback != NULL) continue;
        int32_t id = s_next_id++;
        if(id <= 0) {
            s_next_id = 2;
            id = 1;
        }
        s_observers[index] = (weather_observer_t){
            .id = id, .callback = callback, .user_data = user_data
        };
        return id;
    }
    return -1;
}

bool ui_weather_model_unsubscribe(int32_t subscription_id)
{
    if(subscription_id <= 0) return false;
    for(uint8_t index = 0U; index < UI_WEATHER_OBSERVER_MAX; index++) {
        if(s_observers[index].id != subscription_id) continue;
        memset(&s_observers[index], 0, sizeof(s_observers[index]));
        return true;
    }
    return false;
}
