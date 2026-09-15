#ifndef UI_NAV_MODEL_H
#define UI_NAV_MODEL_H

#include <stdint.h>

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

#endif /* UI_NAV_MODEL_H */
