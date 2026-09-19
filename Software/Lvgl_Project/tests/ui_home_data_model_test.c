#include "ui/models/ui_home_data_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t s_calls;
static ui_home_data_t s_last_data;

static void home_changed(const ui_home_data_t *data, void *user_data)
{
    (void)user_data;
    s_calls++;
    s_last_data = *data;
}

int main(void)
{
    ui_home_data_model_init();
    ui_home_data_t data;
    assert(ui_home_data_model_get(&data));
    assert(strcmp(data.user_name, "Alex Chen") == 0);
    assert(data.total_distance_km == 0U);
    assert(data.total_time_hours == 0U);
    assert(data.total_time_minutes == 0U);
    assert(data.total_ascent_m == 0U);
    assert(data.last_distance_tenths_km == 0U);
    assert(strcmp(data.last_ride_date, "NO RIDES YET") == 0);
    assert(strcmp(data.last_ride_caption,
                  "START YOUR FIRST RIDE") == 0);

    int32_t subscription = ui_home_data_model_subscribe(
        home_changed, NULL);
    assert(subscription > 0);
    uint32_t version = data.version;
    data.total_distance_km = 3001U;
    data.total_time_minutes = 88U;
    snprintf(data.user_name, sizeof(data.user_name), "Test Rider");
    assert(ui_home_data_model_update(&data));
    assert(s_calls == 1U);
    assert(s_last_data.version == version + 1U);
    assert(s_last_data.total_distance_km == 3001U);
    assert(s_last_data.total_time_minutes == 59U);
    assert(strcmp(s_last_data.user_name, "Test Rider") == 0);

    assert(ui_home_data_model_unsubscribe(subscription));
    data.total_distance_km = 3002U;
    assert(ui_home_data_model_update(&data));
    assert(s_calls == 1U);

    puts("ui_home_data_model_test: PASS");
    return 0;
}
