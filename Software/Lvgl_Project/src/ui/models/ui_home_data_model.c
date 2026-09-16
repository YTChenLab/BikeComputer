#include "ui_home_data_model.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    int32_t id;
    ui_home_data_observer_cb_t callback;
    void *user_data;
} ui_home_observer_t;

static ui_home_data_t s_home_data;
static ui_home_observer_t s_observers[UI_HOME_OBSERVER_MAX];
static int32_t s_next_observer_id;

static void copy_text(char *destination, size_t capacity,
                      const char *source)
{
    if(destination == NULL || capacity == 0U) return;
    snprintf(destination, capacity, "%s", source != NULL ? source : "");
}

void ui_home_data_model_init(void)
{
    memset(&s_home_data, 0, sizeof(s_home_data));
    memset(s_observers, 0, sizeof(s_observers));
    s_next_observer_id = 1;

    copy_text(s_home_data.user_name, sizeof(s_home_data.user_name),
              "Alex Chen");
    copy_text(s_home_data.tagline, sizeof(s_home_data.tagline),
              "Good Roads, Better Days.");
    s_home_data.total_distance_km = 2864U;
    s_home_data.total_time_hours = 124U;
    s_home_data.total_time_minutes = 36U;
    s_home_data.total_ascent_m = 28417U;
    copy_text(s_home_data.last_ride_date,
              sizeof(s_home_data.last_ride_date), "2026.09.16  08:17");
    s_home_data.last_distance_tenths_km = 867U;
    s_home_data.last_duration_minutes = 201U;
    s_home_data.last_ascent_m = 1268U;
    copy_text(s_home_data.last_ride_caption,
              sizeof(s_home_data.last_ride_caption),
              "山间向阳 · 风景正好");
    s_home_data.version = 1U;
}

bool ui_home_data_model_get(ui_home_data_t *data)
{
    if(data == NULL) return false;
    *data = s_home_data;
    return true;
}

bool ui_home_data_model_update(const ui_home_data_t *data)
{
    if(data == NULL) return false;
    ui_home_data_t next = *data;
    next.user_name[UI_HOME_USER_NAME_MAX - 1U] = '\0';
    next.tagline[UI_HOME_TAGLINE_MAX - 1U] = '\0';
    next.last_ride_date[UI_HOME_DATE_MAX - 1U] = '\0';
    next.last_ride_caption[UI_HOME_CAPTION_MAX - 1U] = '\0';
    if(next.total_time_minutes > 59U) next.total_time_minutes = 59U;
    next.version = s_home_data.version + 1U;
    s_home_data = next;

    for(uint8_t index = 0; index < UI_HOME_OBSERVER_MAX; index++) {
        if(s_observers[index].callback != NULL)
            s_observers[index].callback(&s_home_data,
                                        s_observers[index].user_data);
    }
    return true;
}

int32_t ui_home_data_model_subscribe(ui_home_data_observer_cb_t callback,
                                     void *user_data)
{
    if(callback == NULL) return -1;
    for(uint8_t index = 0; index < UI_HOME_OBSERVER_MAX; index++) {
        if(s_observers[index].callback != NULL) continue;
        int32_t id = s_next_observer_id++;
        if(id <= 0) {
            s_next_observer_id = 2;
            id = 1;
        }
        s_observers[index] = (ui_home_observer_t){
            .id = id,
            .callback = callback,
            .user_data = user_data
        };
        return id;
    }
    return -1;
}

bool ui_home_data_model_unsubscribe(int32_t subscription_id)
{
    if(subscription_id <= 0) return false;
    for(uint8_t index = 0; index < UI_HOME_OBSERVER_MAX; index++) {
        if(s_observers[index].id != subscription_id) continue;
        memset(&s_observers[index], 0, sizeof(s_observers[index]));
        return true;
    }
    return false;
}
