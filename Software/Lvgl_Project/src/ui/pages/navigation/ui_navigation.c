#include "ui_navigation.h"

#include "../../components/navigation/ui_nav_card.h"
#include "../../components/navigation/ui_nav_layout.h"
#include "../../components/navigation/ui_nav_map.h"
#include "../../core/ui_colors.h"
#include "../../core/ui_event_bus.h"
#include "../../../services/ui_navigation_service.h"

#include <stdio.h>
#include <string.h>

#define NAV_MOCK_PERIOD_MS 900
#define NAV_EVENT_SOURCE   0x4E415600UL

typedef struct
{
    lv_obj_t *screen;
    lv_obj_t *grid;
    ui_nav_map_t map;
    ui_nav_card_t speed_card;
    ui_nav_card_t distance_card;
    ui_nav_card_t eta_card;
    ui_nav_card_t direction_card;
    nav_model_t model;
    lv_timer_t *mock_timer;
    int32_t service_subscription;
    uint32_t mock_tick;
    bool active;
    bool created;
} ui_navigation_context_t;

static ui_navigation_context_t s_navigation;

static const char *direction_name(nav_direction_t direction)
{
    if(direction == NAV_DIRECTION_LEFT) return "左转";
    if(direction == NAV_DIRECTION_RIGHT) return "右转";
    if(direction == NAV_DIRECTION_U_TURN) return "掉头";
    return "直行";
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
    snprintf(event.title, sizeof(event.title), "前方转向");
    snprintf(event.message, sizeof(event.message), "前方 %lu m %s",
             (unsigned long)model->direction_distance,
             direction_name(model->direction));
    snprintf(event.primary_action, sizeof(event.primary_action), "查看");
    (void)ui_event_bus_publish(&event);
}

static void apply_model(const nav_model_t *model)
{
    char value[24];
    bool direction_changed = model->direction != s_navigation.model.direction;
    s_navigation.model = *model;

    if(direction_changed) publish_turn_event(model);

    snprintf(value, sizeof(value), "%.1f", (double)model->speed);
    ui_nav_card_set_metric(&s_navigation.speed_card, value);
    snprintf(value, sizeof(value), "%.1f", (double)model->distance);
    ui_nav_card_set_metric(&s_navigation.distance_card, value);
    ui_nav_card_set_eta(&s_navigation.eta_card, model->eta_hour, model->eta_min);
    ui_nav_card_set_direction(&s_navigation.direction_card, model->direction,
                              model->direction_distance, direction_changed);
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
        bool direction_changed =
            model->direction != s_navigation.model.direction;
        s_navigation.model = *model;
        if(direction_changed) publish_turn_event(model);
    }
}

static void navigation_mock_timer_cb(lv_timer_t *timer)
{
    nav_model_t model = s_navigation.model;
    s_navigation.mock_tick++;
    model.speed = 25.0f + (float)(s_navigation.mock_tick % 47U) / 10.0f;
    model.distance += 0.1f;
    model.heading += 7.0f;
    if(model.heading >= 360.0f) model.heading -= 360.0f;
    model.latitude += 0.000015f;
    model.longitude += 0.000010f;
    if((s_navigation.mock_tick % 8U) == 0U) {
        model.direction = (nav_direction_t)((model.direction + 1) % 4);
        model.direction_distance = 420U;
    } else if(model.direction_distance > 35U) {
        model.direction_distance -= 15U;
    }
    ui_navigation_service_submit(&model);
    (void)timer;
}

static void create_grid(lv_obj_t *screen)
{
    static int32_t columns[] = {UI_NAV_CARD_WIDTH, UI_NAV_CARD_WIDTH,
                                LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {UI_NAV_CARD_HEIGHT, UI_NAV_CARD_HEIGHT,
                             LV_GRID_TEMPLATE_LAST};

    s_navigation.grid = lv_obj_create(screen);
    lv_obj_remove_style_all(s_navigation.grid);
    lv_obj_set_pos(s_navigation.grid, UI_NAV_PAGE_MARGIN, UI_NAV_GRID_TOP);
    lv_obj_set_size(s_navigation.grid, UI_NAV_GRID_WIDTH, UI_NAV_GRID_HEIGHT);
    lv_obj_set_layout(s_navigation.grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(s_navigation.grid, columns, rows);
    lv_obj_set_style_pad_all(s_navigation.grid, 0, 0);
    lv_obj_set_style_pad_column(s_navigation.grid, UI_NAV_CARD_GAP, 0);
    lv_obj_set_style_pad_row(s_navigation.grid, UI_NAV_CARD_GAP, 0);
    lv_obj_clear_flag(s_navigation.grid, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    ui_nav_card_create_metric(&s_navigation.speed_card, s_navigation.grid, 0, 0,
                              "实时速度", "km/h");
    ui_nav_card_create_metric(&s_navigation.distance_card, s_navigation.grid, 1, 0,
                              "累计里程", "km");
    ui_nav_card_create_eta(&s_navigation.eta_card, s_navigation.grid, 0, 1);
    ui_nav_card_create_direction(&s_navigation.direction_card, s_navigation.grid, 1, 1);
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
    create_grid(screen);

    s_navigation.model = (nav_model_t){
        .speed = 28.6f,
        .distance = 46.8f,
        .eta_hour = 11,
        .eta_min = 5,
        .latitude = 34.3416f,
        .longitude = 108.9398f,
        .heading = 36.0f,
        .zoom = 14,
        .direction = NAV_DIRECTION_RIGHT,
        .direction_distance = 250U
    };
    apply_model(&s_navigation.model);
    s_navigation.mock_timer = lv_timer_create(navigation_mock_timer_cb, NAV_MOCK_PERIOD_MS, NULL);
    lv_timer_pause(s_navigation.mock_timer);
    ui_navigation_service_submit(&s_navigation.model);
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
        lv_timer_resume(s_navigation.mock_timer);
    }
    else {
        lv_timer_pause(s_navigation.mock_timer);
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
