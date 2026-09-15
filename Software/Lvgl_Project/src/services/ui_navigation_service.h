#ifndef UI_NAVIGATION_SERVICE_H
#define UI_NAVIGATION_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#define UI_NAVIGATION_OBSERVER_MAX 4

typedef enum
{
    NAV_DIRECTION_STRAIGHT = 0,
    NAV_DIRECTION_LEFT,
    NAV_DIRECTION_RIGHT,
    NAV_DIRECTION_U_TURN
} nav_direction_t;

typedef enum
{
    NAV_FOLLOW = 0,
    NAV_EXPLORE,
    NAV_RECENTER
} nav_state_t;

typedef struct
{
    float speed;
    float distance;
    uint8_t eta_hour;
    uint8_t eta_min;
    float latitude;
    float longitude;
    float heading;
    uint8_t zoom;
    nav_direction_t direction;
    uint32_t direction_distance;
} nav_model_t;

typedef void (*ui_navigation_observer_cb_t)(const nav_model_t *model,
                                             uint32_t version,
                                             void *user_data);

void ui_navigation_service_init(void);
int32_t ui_navigation_service_subscribe(ui_navigation_observer_cb_t callback,
                                        void *user_data);
bool ui_navigation_service_unsubscribe(int32_t subscription_id);

/* Producer/task-safe latest-value mailbox. */
void ui_navigation_service_submit(const nav_model_t *model);

/* UI-thread only. */
bool ui_navigation_service_process(void);
bool ui_navigation_service_get(nav_model_t *model, uint32_t *version);

#endif /* UI_NAVIGATION_SERVICE_H */
