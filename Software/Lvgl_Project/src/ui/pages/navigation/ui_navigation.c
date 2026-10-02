#include "ui_navigation.h"

#include "../../components/navigation/ui_nav_command_dock.h"
#include "../../components/navigation/ui_nav_data_strip.h"
#include "../../components/navigation/ui_nav_layout.h"
#include "../../components/navigation/ui_nav_map.h"
#include "../../core/ui_colors.h"
#include "../../core/ui_event_bus.h"
#include "../../../services/ui_navigation_service.h"

#include <stdio.h>
#include <string.h>

#define NAV_EVENT_SOURCE   0x4E415600UL

typedef struct
{
    lv_obj_t *screen;
    ui_nav_map_t map;
    ui_nav_data_strip_t data_strip;
    ui_nav_command_dock_t command_dock;
    nav_model_t model;
    int32_t service_subscription;
    uint8_t demo_index;
    bool active;
    bool created;
} ui_navigation_context_t;

static ui_navigation_context_t s_navigation;

void ui_navigation_cancel_pointer(void)
{
    if(s_navigation.created && s_navigation.map.root)
        lv_obj_send_event(s_navigation.map.root, LV_EVENT_PRESS_LOST, NULL);
}

static const char *direction_name(nav_direction_t direction)
{
    switch(direction) {
        case NAV_MANEUVER_LEFT: return "左转";
        case NAV_MANEUVER_RIGHT: return "右转";
        case NAV_MANEUVER_SLIGHT_LEFT: return "轻微左转";
        case NAV_MANEUVER_SLIGHT_RIGHT: return "轻微右转";
        case NAV_MANEUVER_SHARP_LEFT: return "急左转";
        case NAV_MANEUVER_SHARP_RIGHT: return "急右转";
        case NAV_MANEUVER_U_TURN: return "掉头";
        case NAV_MANEUVER_ROUNDABOUT: return "进入环岛";
        case NAV_MANEUVER_ARRIVE: return "到达目的地";
        default: return "直行";
    }
}

static ui_navigation_turn_t event_direction(nav_direction_t direction)
{
    if(direction == NAV_DIRECTION_LEFT) return UI_NAVIGATION_TURN_LEFT;
    if(direction == NAV_DIRECTION_RIGHT) return UI_NAVIGATION_TURN_RIGHT;
    if(direction == NAV_DIRECTION_U_TURN) return UI_NAVIGATION_TURN_U_TURN;
    return UI_NAVIGATION_TURN_STRAIGHT;
}

static void publish_turn_event(const nav_model_t *model)
{
    if(model == NULL) return;
    ui_event_t event = {0};
    event.type = UI_EVENT_NAVIGATION_TURN;
    event.priority = UI_EVENT_PRIORITY_P1_RIDING_SAFETY;
    event.source_id = NAV_EVENT_SOURCE;
    event.dedupe_key = NAV_EVENT_SOURCE;
    event.timeout_ms = 5000U;
    event.value = (int32_t)event_direction(model->direction);
    snprintf(event.title, sizeof(event.title), "前方转向");
    snprintf(event.message, sizeof(event.message), "前方 %lu m %s",
             (unsigned long)model->direction_distance,
             direction_name(model->direction));
    snprintf(event.primary_action, sizeof(event.primary_action), "查看");
    (void)ui_event_bus_publish(&event);
}

static void apply_model(const nav_model_t *model)
{
    bool visual_changed = model->direction != s_navigation.model.direction ||
                          model->ui_state != s_navigation.model.ui_state;
    s_navigation.model = *model;
    ui_nav_data_strip_update(&s_navigation.data_strip, model);
    ui_nav_command_dock_update(&s_navigation.command_dock, model,
                               visual_changed);
    ui_nav_map_apply_model(&s_navigation.map, model);
}

static void navigation_model_changed(const nav_model_t *model,
                                     uint32_t version, void *user_data)
{
    (void)version;
    (void)user_data;
    if(model == NULL) return;
    if(s_navigation.active) apply_model(model);
    else {
        bool direction_changed = model->direction != s_navigation.model.direction;
        s_navigation.model = *model;
        if(direction_changed) publish_turn_event(model);
    }
}

void ui_navigation_create(lv_obj_t *screen)
{
    if(screen == NULL || s_navigation.created) return;
    memset(&s_navigation, 0, sizeof(s_navigation));
    s_navigation.screen = screen;
    s_navigation.service_subscription =
        ui_navigation_service_subscribe(navigation_model_changed, NULL);
    ui_nav_map_create(&s_navigation.map, screen, UI_NAV_PAGE_MARGIN,
                      UI_NAV_MAP_TOP);
    ui_nav_data_strip_create(&s_navigation.data_strip, screen);
    ui_nav_command_dock_create(&s_navigation.command_dock, screen);

    s_navigation.model = (nav_model_t){
        .demo = true,
        .speed = 28.6f,
        .distance = 12.6f,
        .remaining_ascent_m = 156U,
        .eta_hour = 11,
        .eta_min = 5,
        .latitude = 34.3416f,
        .longitude = 108.9398f,
        .heading = 36.0f,
        .zoom = 14,
        .direction = NAV_DIRECTION_RIGHT,
        .direction_distance = 420U,
        .ui_state = NAV_UI_CRUISE,
        .road_name = "科技二路"
    };
    if(!ui_navigation_service_demo_enabled()) {
        s_navigation.model = (nav_model_t){ .zoom = 14, .ui_state = NAV_UI_GPS_WEAK };
        snprintf(s_navigation.model.road_name, sizeof(s_navigation.model.road_name), "等待定位");
    }
    apply_model(&s_navigation.model);
    s_navigation.created = true;
}

void ui_navigation_set_active(bool active)
{
    if(!s_navigation.created) return;
    s_navigation.active = active;
    if(active) {
        nav_model_t latest;
        uint32_t version;
        if(ui_navigation_service_get(&latest, &version) && version > 0U)
            apply_model(&latest);
    }
}

bool ui_navigation_contains_point(int32_t x, int32_t y)
{
    return s_navigation.created && ui_nav_map_contains_point(&s_navigation.map, x, y);
}

bool ui_navigation_handle_input(ui_input_event_t input)
{
    if(!s_navigation.created) return false;
    if(input == UI_INPUT_ENCODER_CW) {
        ui_nav_map_handle_input(&s_navigation.map, 1, false);
        return true;
    }
    if(input == UI_INPUT_ENCODER_CCW) {
        ui_nav_map_handle_input(&s_navigation.map, -1, false);
        return true;
    }
    if(input == UI_INPUT_ENCODER_PRESS) {
        ui_nav_map_handle_input(&s_navigation.map, 0, true);
        return true;
    }
    return false;
}

void ui_navigation_demo_next_state(void)
{
    if(!s_navigation.created || !ui_navigation_service_demo_enabled()) return;
    nav_model_t model = s_navigation.model;
    static const struct
    {
        nav_ui_state_t state;
        nav_maneuver_t maneuver;
        uint32_t distance_m;
        const char *road;
    } demos[] = {
        {NAV_UI_CRUISE, NAV_MANEUVER_LEFT, 420U, "环城南路"},
        {NAV_UI_CRUISE, NAV_MANEUVER_RIGHT, 420U, "科技二路"},
        {NAV_UI_CRUISE, NAV_MANEUVER_STRAIGHT, 1200U, "环城南路"},
        {NAV_UI_CRUISE, NAV_MANEUVER_U_TURN, 180U, ""},
        {NAV_UI_APPROACH, NAV_MANEUVER_LEFT, 120U, "环城南路"},
        {NAV_UI_IMMINENT, NAV_MANEUVER_LEFT, 35U, "环城南路"},
        {NAV_UI_NOW, NAV_MANEUVER_LEFT, 0U, "环城南路"},
        {NAV_UI_REROUTING, NAV_MANEUVER_STRAIGHT, 0U, ""},
        {NAV_UI_GPS_WEAK, NAV_MANEUVER_STRAIGHT, 0U, ""},
        {NAV_UI_ARRIVED, NAV_MANEUVER_ARRIVE, 0U, ""}
    };
    const uint8_t count = (uint8_t)(sizeof(demos) / sizeof(demos[0]));
    uint8_t index = s_navigation.demo_index % count;
    model.ui_state = demos[index].state;
    model.direction = demos[index].maneuver;
    model.direction_distance = demos[index].distance_m;
    snprintf(model.road_name, sizeof(model.road_name), "%s", demos[index].road);
    s_navigation.demo_index = (uint8_t)((index + 1U) % count);
    ui_navigation_service_submit(&model);
    (void)ui_navigation_service_process();
}
