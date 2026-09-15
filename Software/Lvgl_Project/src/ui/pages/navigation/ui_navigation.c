#include "ui_navigation.h"

#include "../../components/navigation/ui_nav_card.h"
#include "../../components/navigation/ui_nav_map.h"
#include "../../core/ui_colors.h"

#include <stdio.h>
#include <string.h>

#define NAV_MAP_X          10
#define NAV_MAP_Y          62
#define NAV_GRID_X         10
#define NAV_GRID_Y         530
#define NAV_GRID_W         460
#define NAV_GRID_H         314
#define NAV_CARD_W         227
#define NAV_CARD_H         154
#define NAV_CARD_GAP       6
#define NAV_QUEUE_DEPTH    4
#define NAV_UI_PERIOD_MS   50
#define NAV_MOCK_PERIOD_MS 900

typedef struct
{
    nav_model_t values[NAV_QUEUE_DEPTH];
    volatile uint8_t write_index;
    volatile uint8_t read_index;
} nav_model_queue_t;

typedef struct
{
    lv_obj_t *screen;
    lv_obj_t *grid;
    ui_nav_map_t map;
    ui_nav_card_t speed_card;
    ui_nav_card_t distance_card;
    ui_nav_card_t eta_card;
    ui_nav_card_t direction_card;
    nav_model_queue_t queue;
    nav_model_t model;
    lv_timer_t *ui_timer;
    lv_timer_t *mock_timer;
    uint32_t mock_tick;
    bool active;
    bool created;
} ui_navigation_context_t;

static ui_navigation_context_t s_navigation;

static bool queue_push(nav_model_queue_t *queue, const nav_model_t *model)
{
    uint8_t write = __atomic_load_n(&queue->write_index, __ATOMIC_RELAXED);
    uint8_t read = __atomic_load_n(&queue->read_index, __ATOMIC_ACQUIRE);
    uint8_t next = (uint8_t)((write + 1U) % NAV_QUEUE_DEPTH);
    if(next == read) return false;
    queue->values[write] = *model;
    __atomic_store_n(&queue->write_index, next, __ATOMIC_RELEASE);
    return true;
}

static bool queue_pop(nav_model_queue_t *queue, nav_model_t *model)
{
    uint8_t read = __atomic_load_n(&queue->read_index, __ATOMIC_RELAXED);
    uint8_t write = __atomic_load_n(&queue->write_index, __ATOMIC_ACQUIRE);
    if(read == write) return false;
    *model = queue->values[read];
    __atomic_store_n(&queue->read_index,
                     (uint8_t)((read + 1U) % NAV_QUEUE_DEPTH), __ATOMIC_RELEASE);
    return true;
}

static void apply_model(const nav_model_t *model)
{
    char value[24];
    bool direction_changed = model->direction != s_navigation.model.direction;
    s_navigation.model = *model;

    snprintf(value, sizeof(value), "%.1f", (double)model->speed);
    ui_nav_card_set_metric(&s_navigation.speed_card, value);
    snprintf(value, sizeof(value), "%.1f", (double)model->distance);
    ui_nav_card_set_metric(&s_navigation.distance_card, value);
    ui_nav_card_set_eta(&s_navigation.eta_card, model->eta_hour, model->eta_min);
    ui_nav_card_set_direction(&s_navigation.direction_card, model->direction,
                              model->direction_distance, direction_changed);
    ui_nav_map_apply_model(&s_navigation.map, model);
}

static void navigation_ui_timer_cb(lv_timer_t *timer)
{
    nav_model_t newest;
    bool updated = false;
    while(queue_pop(&s_navigation.queue, &newest)) updated = true;
    if(updated) apply_model(&newest);
    (void)timer;
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
    ui_navigation_post_model(&model);
    (void)timer;
}

static void create_grid(lv_obj_t *screen)
{
    static int32_t columns[] = {NAV_CARD_W, NAV_CARD_W, LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {NAV_CARD_H, NAV_CARD_H, LV_GRID_TEMPLATE_LAST};

    s_navigation.grid = lv_obj_create(screen);
    lv_obj_remove_style_all(s_navigation.grid);
    lv_obj_set_pos(s_navigation.grid, NAV_GRID_X, NAV_GRID_Y);
    lv_obj_set_size(s_navigation.grid, NAV_GRID_W, NAV_GRID_H);
    lv_obj_set_layout(s_navigation.grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(s_navigation.grid, columns, rows);
    lv_obj_set_style_pad_all(s_navigation.grid, 0, 0);
    lv_obj_set_style_pad_column(s_navigation.grid, NAV_CARD_GAP, 0);
    lv_obj_set_style_pad_row(s_navigation.grid, NAV_CARD_GAP, 0);
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
    ui_nav_map_create(&s_navigation.map, screen, NAV_MAP_X, NAV_MAP_Y);
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
    s_navigation.ui_timer = lv_timer_create(navigation_ui_timer_cb, NAV_UI_PERIOD_MS, NULL);
    s_navigation.mock_timer = lv_timer_create(navigation_mock_timer_cb, NAV_MOCK_PERIOD_MS, NULL);
    lv_timer_pause(s_navigation.ui_timer);
    lv_timer_pause(s_navigation.mock_timer);
    s_navigation.created = true;
}

void ui_navigation_set_active(bool active)
{
    if(!s_navigation.created) return;
    s_navigation.active = active;
    if(active) {
        lv_timer_resume(s_navigation.ui_timer);
        lv_timer_resume(s_navigation.mock_timer);
    }
    else {
        lv_timer_pause(s_navigation.ui_timer);
        lv_timer_pause(s_navigation.mock_timer);
    }
}

bool ui_navigation_post_model(const nav_model_t *model)
{
    if(model == NULL || !s_navigation.created) return false;
    return queue_push(&s_navigation.queue, model);
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

const nav_model_t *ui_navigation_get_model(void)
{
    return &s_navigation.model;
}
