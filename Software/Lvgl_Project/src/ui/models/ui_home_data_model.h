#ifndef UI_HOME_DATA_MODEL_H
#define UI_HOME_DATA_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define UI_HOME_USER_NAME_MAX     32
#define UI_HOME_TAGLINE_MAX       48
#define UI_HOME_DATE_MAX          24
#define UI_HOME_CAPTION_MAX       48
#define UI_HOME_OBSERVER_MAX       4

typedef struct
{
    char user_name[UI_HOME_USER_NAME_MAX];
    char tagline[UI_HOME_TAGLINE_MAX];

    uint32_t total_distance_km;
    uint32_t total_time_hours;
    uint8_t total_time_minutes;
    uint32_t total_ascent_m;

    char last_ride_date[UI_HOME_DATE_MAX];
    uint32_t last_distance_tenths_km;
    uint32_t last_duration_minutes;
    uint32_t last_ascent_m;
    char last_ride_caption[UI_HOME_CAPTION_MAX];

    uint32_t version;
} ui_home_data_t;

typedef void (*ui_home_data_observer_cb_t)(const ui_home_data_t *data,
                                            void *user_data);

void ui_home_data_model_init(void);
bool ui_home_data_model_get(ui_home_data_t *data);
bool ui_home_data_model_update(const ui_home_data_t *data);
int32_t ui_home_data_model_subscribe(ui_home_data_observer_cb_t callback,
                                     void *user_data);
bool ui_home_data_model_unsubscribe(int32_t subscription_id);

#endif /* UI_HOME_DATA_MODEL_H */
