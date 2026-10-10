#ifndef UI_NAVIGATION_SERVICE_H
#define UI_NAVIGATION_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#define UI_NAVIGATION_OBSERVER_MAX 4

typedef enum
{
    NAV_MANEUVER_STRAIGHT = 0,
    NAV_MANEUVER_LEFT,
    NAV_MANEUVER_RIGHT,
    NAV_MANEUVER_SLIGHT_LEFT,
    NAV_MANEUVER_SLIGHT_RIGHT,
    NAV_MANEUVER_SHARP_LEFT,
    NAV_MANEUVER_SHARP_RIGHT,
    NAV_MANEUVER_U_TURN,
    NAV_MANEUVER_ROUNDABOUT,
    NAV_MANEUVER_ARRIVE,
    NAV_MANEUVER_KEEP_LEFT,NAV_MANEUVER_KEEP_RIGHT,NAV_MANEUVER_EXIT_ROUNDABOUT,
    NAV_MANEUVER_FORK_LEFT,NAV_MANEUVER_FORK_RIGHT,
    NAV_MANEUVER_COUNT
} nav_maneuver_t;

/* Source compatibility for the existing map/service adapters. */
typedef nav_maneuver_t nav_direction_t;
#define NAV_DIRECTION_STRAIGHT NAV_MANEUVER_STRAIGHT
#define NAV_DIRECTION_LEFT     NAV_MANEUVER_LEFT
#define NAV_DIRECTION_RIGHT    NAV_MANEUVER_RIGHT
#define NAV_DIRECTION_U_TURN   NAV_MANEUVER_U_TURN

typedef enum
{
    NAV_UI_CRUISE = 0,
    NAV_UI_APPROACH,
    NAV_UI_IMMINENT,
    NAV_UI_NOW,
    NAV_UI_REROUTING,
    NAV_UI_GPS_WEAK,
    NAV_UI_ARRIVED,
    NAV_UI_PASSED,
    NAV_UI_STATE_COUNT
} nav_ui_state_t;

typedef enum
{
    NAV_FOLLOW = 0,
    NAV_EXPLORE,
    NAV_RECENTER
    ,NAV_OVERVIEW, NAV_FOLLOW_USER_ZOOM
} nav_state_t;

typedef struct
{
    float speed;
    float distance;
    uint32_t remaining_ascent_m;
    uint8_t eta_hour;
    uint8_t eta_min;
    double latitude;
    double longitude;
    float heading;
    uint8_t zoom;
    nav_direction_t direction;
    uint32_t direction_distance;
    nav_ui_state_t ui_state;
    char road_name[64];
    bool demo;
    bool position_valid;
    uint32_t sampled_at_ms, stale_after_ms;
    /* Local geographic window supplied by route/GNSS software. No road
     * network or route-planning engine is inferred from position alone. */
    bool map_geometry_valid;
    double map_origin_latitude, map_origin_longitude;
    float meters_per_pixel;
    uint8_t route_point_count;
    double route_latitude[16], route_longitude[16];
    /* New producers set session_bound. Legacy component fixtures remain usable. */
    bool session_bound, guidance_valid, speed_valid, distance_valid, eta_valid, heading_valid;
    uint32_t session_id, route_id, route_revision, step_id;
    uint8_t coordinate_system;
    bool junction_valid, next_valid, exit_valid, off_route, reroute_failed;
    bool exit_position_valid;
    double exit_latitude,exit_longitude;
    double junction_latitude, junction_longitude;
    nav_maneuver_t next_direction;
    uint32_t next_distance_m;
    uint8_t roundabout_exit;
} nav_model_t;

typedef void (*ui_navigation_observer_cb_t)(const nav_model_t *model,
                                             uint32_t version,
                                             void *user_data);

void ui_navigation_service_init(void);
/* Board audio task reads this preference; UI never performs audio I/O. */
void ui_navigation_service_set_prompts_enabled(bool enabled);
bool ui_navigation_service_prompts_enabled(void);
void ui_navigation_service_set_demo_enabled(bool enabled);
bool ui_navigation_service_demo_enabled(void);
void ui_navigation_service_register_demo_step(void (*callback)(void));
bool ui_navigation_service_demo_step(void);
int32_t ui_navigation_service_subscribe(ui_navigation_observer_cb_t callback,
                                        void *user_data);
bool ui_navigation_service_unsubscribe(int32_t subscription_id);

/* Producer/task-safe latest-value mailbox. */
void ui_navigation_service_submit(const nav_model_t *model);

/* UI-thread only. */
bool ui_navigation_service_process(void);
bool ui_navigation_service_expire(uint32_t now_ms);
bool ui_navigation_service_get(nav_model_t *model, uint32_t *version);

/* Preserves legacy void entry; board observes rejected/stale producer frames. */
bool ui_navigation_service_submit_checked(const nav_model_t *model);

#endif /* UI_NAVIGATION_SERVICE_H */
