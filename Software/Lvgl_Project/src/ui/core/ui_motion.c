#include "ui_motion.h"

#include "../design/ui_design_tokens.h"
#include <math.h>
#include <string.h>

#define SPRING_CAPACITY 24U
typedef struct {
    lv_obj_t *object;
    ui_motion_value_cb_t callback;
    lv_anim_completed_cb_t completed;
    void *user_data;
    float position, velocity, goal, frequency, damping;
    uint32_t last_tick, started_tick;
} spring_t;
static spring_t s_springs[SPRING_CAPACITY];
static lv_timer_t *s_spring_timer;
static bool s_reduced, s_riding;

static void spring_object_deleted(lv_event_t *event)
{
    spring_t *spring = lv_event_get_user_data(event);
    memset(spring, 0, sizeof(*spring));
}

static void spring_release(spring_t *spring, bool complete)
{
    lv_obj_t *object = spring->object;
    lv_anim_completed_cb_t completed = spring->completed;
    void *user_data = spring->user_data;
    if(object != NULL)
        lv_obj_remove_event_cb_with_user_data(object, spring_object_deleted, spring);
    memset(spring, 0, sizeof(*spring));
    if(complete && completed != NULL) {
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, object);
        lv_anim_set_user_data(&animation, user_data);
        completed(&animation);
    }
}

uint32_t ui_motion_springs_running(void)
{
    uint32_t count = 0;
    for(unsigned i = 0; i < SPRING_CAPACITY; ++i) count += s_springs[i].object != NULL;
    return count;
}

static void spring_tick(lv_timer_t *timer)
{
    uint32_t now = lv_tick_get();
    for(unsigned i = 0; i < SPRING_CAPACITY; ++i) {
        spring_t *spring = &s_springs[i];
        if(spring->object == NULL) continue;
        float dt = (uint32_t)(now - spring->last_tick) * 0.001f;
        spring->last_tick = now;
        /* Analytic damped oscillator: stable even after a delayed frame. */
        float w = spring->frequency, a = spring->damping * w;
        float wd = w * sqrtf(1.0f - spring->damping * spring->damping);
        float x = spring->position - spring->goal;
        float b = (spring->velocity + a * x) / wd;
        float e = expf(-a * dt), c = cosf(wd * dt), sn = sinf(wd * dt);
        float next = e * (x * c + b * sn);
        spring->velocity = e * ((-a * x + b * wd) * c + (-a * b - x * wd) * sn);
        spring->position = spring->goal + next;
        bool settled = s_reduced || ((fabsf(next) < 0.35f && fabsf(spring->velocity) < 5.0f) ||
                                     (uint32_t)(now - spring->started_tick) >= 1200U);
        if(settled) spring->position = spring->goal;
        spring->callback(spring->object, (int32_t)lroundf(spring->position));
        if(spring->object != NULL && settled) spring_release(spring, true);
    }
    if(ui_motion_springs_running() == 0) lv_timer_pause(timer);
}

void ui_motion_set_reduced(bool reduced) { s_reduced = reduced; }
bool ui_motion_is_reduced(void) { return s_reduced; }
void ui_motion_set_riding(bool riding) { s_riding = riding; }
lv_anim_enable_t ui_motion_scroll_mode(void) { return s_reduced ? LV_ANIM_OFF : LV_ANIM_ON; }

void ui_motion_spring(lv_obj_t *object, ui_motion_value_cb_t callback,
                      int32_t from, int32_t to, bool expressive,
                      lv_anim_completed_cb_t completed, void *user_data)
{
    if(object == NULL || callback == NULL) return;
    lv_anim_delete(object, callback);
    spring_t *spring = NULL;
    for(unsigned i = 0; i < SPRING_CAPACITY; ++i)
        if(s_springs[i].object == object && s_springs[i].callback == callback) {
            spring = &s_springs[i];
            break;
        }
    if(spring == NULL) {
        for(unsigned i = 0; i < SPRING_CAPACITY; ++i)
            if(s_springs[i].object == NULL) { spring = &s_springs[i]; break; }
        if(spring == NULL) {
            ui_motion_value(object, callback, from, to, 1, NULL, completed, user_data);
            return;
        }
        spring->object = object;
        spring->callback = callback;
        spring->position = from;
        spring->velocity = 0;
        lv_obj_add_event_cb(object, spring_object_deleted, LV_EVENT_DELETE, spring);
    }
    spring->goal = to;
    spring->frequency = expressive ? 23.0f : 42.0f;
    spring->damping = s_riding ? 0.96f : expressive ? 0.72f : 0.80f;
    spring->completed = completed;
    spring->user_data = user_data;
    spring->last_tick = spring->started_tick = lv_tick_get();
    if(s_reduced) {
        callback(object, to);
        if(spring->object != NULL) spring_release(spring, true);
        return;
    }
    if(s_spring_timer == NULL) s_spring_timer = lv_timer_create(spring_tick, 8, NULL);
    lv_timer_resume(s_spring_timer);
}

static void set_scale(void *target, int32_t value)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)target, value, 0);
}

static void set_y(void *target, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)target, value);
}

static void set_x(void *target, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)target, value);
}

static void set_width(void *target, int32_t value)
{
    lv_obj_set_width((lv_obj_t *)target, value);
}

static void set_opacity(void *target, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)target, (lv_opa_t)value, 0);
}

void ui_motion_init(void)
{
    for(unsigned i = 0; i < SPRING_CAPACITY; ++i)
        if(s_springs[i].object != NULL) spring_release(&s_springs[i], false);
    if(s_spring_timer != NULL) lv_timer_delete(s_spring_timer);
    s_spring_timer = NULL;
    s_reduced = s_riding = false;
}

void ui_motion_value(void *target, ui_motion_value_cb_t callback,
                     int32_t from, int32_t to, uint32_t duration,
                     lv_anim_path_cb_t path,
                     lv_anim_completed_cb_t completed, void *user_data)
{
    if(target == NULL || callback == NULL) return;
    ui_motion_cancel(target, callback);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, target);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_time(&animation, s_reduced ? 1U : duration);
    lv_anim_set_path_cb(&animation, path != NULL ? path : lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, callback);
    lv_anim_set_user_data(&animation, user_data);
    if(completed != NULL) lv_anim_set_completed_cb(&animation, completed);
    lv_anim_start(&animation);
}

void ui_motion_cancel(void *target, ui_motion_value_cb_t callback)
{
    if(target != NULL && callback != NULL) lv_anim_delete(target, callback);
    for(unsigned i = 0; i < SPRING_CAPACITY; ++i)
        if(s_springs[i].object == target && s_springs[i].callback == callback)
            spring_release(&s_springs[i], false);
}

void ui_motion_button_press(lv_obj_t *button)
{
    if(button == NULL) return;
    lv_obj_set_style_transform_pivot_x(button, lv_obj_get_width(button) / 2, 0);
    lv_obj_set_style_transform_pivot_y(button, lv_obj_get_height(button) / 2, 0);
    ui_motion_value(button, set_scale,
                    lv_obj_get_style_transform_scale_x(button, 0),
                    UI_MOTION_PRESS_SCALE_ACTIVE, UI_MOTION_DURATION_PRESS,
                    lv_anim_path_ease_out, NULL, NULL);
}

void ui_motion_button_release(lv_obj_t *button)
{
    if(button == NULL) return;
    ui_motion_value(button, set_scale,
                    lv_obj_get_style_transform_scale_x(button, 0),
                    UI_MOTION_PRESS_SCALE_NORMAL, UI_MOTION_DURATION_PRESS,
                    lv_anim_path_ease_out, NULL, NULL);
}

void ui_motion_focus_capsule(lv_obj_t *capsule, int32_t target_x,
                             int32_t resting_width)
{
    if(capsule == NULL) return;
    ui_motion_spring(capsule, set_x, lv_obj_get_x(capsule), target_x,
                     false, NULL, NULL);
    ui_motion_spring(capsule, set_width, lv_obj_get_width(capsule), resting_width,
                     false, NULL, NULL);
    if(!s_reduced && !s_riding)
        for(unsigned i = 0; i < SPRING_CAPACITY; ++i)
            if(s_springs[i].object == capsule && s_springs[i].callback == set_width)
                s_springs[i].velocity = fminf(s_springs[i].velocity + 580.0f, 800.0f);
}

void ui_motion_toggle_press(lv_obj_t *object)
{
    if(object == NULL) return;
    ui_motion_value(object, set_scale,
                    lv_obj_get_style_transform_scale_x(object, 0), 246,
                    UI_TOKEN_MOTION_CAPSULE_PRESS_MS / 2U,
                    lv_anim_path_ease_out, NULL, NULL);
}

void ui_motion_toggle_release(lv_obj_t *object)
{
    if(object == NULL) return;
    ui_motion_value(object, set_scale,
                    lv_obj_get_style_transform_scale_x(object, 0),
                    UI_MOTION_PRESS_SCALE_NORMAL,
                    UI_TOKEN_MOTION_CAPSULE_PRESS_MS / 2U,
                    lv_anim_path_ease_out, NULL, NULL);
}

void ui_motion_pulse(lv_obj_t *object, int32_t active_scale,
                     uint32_t duration)
{
    if(object == NULL || duration == 0U) return;
    lv_anim_delete(object, set_scale);
    lv_obj_set_style_transform_scale(object,
                                     UI_MOTION_PRESS_SCALE_NORMAL, 0);
    if(s_reduced) return;
    lv_anim_t pulse;
    lv_anim_init(&pulse);
    lv_anim_set_var(&pulse, object);
    lv_anim_set_exec_cb(&pulse, set_scale);
    lv_anim_set_values(&pulse, UI_MOTION_PRESS_SCALE_NORMAL, active_scale);
    lv_anim_set_time(&pulse, duration / 2U);
    lv_anim_set_playback_time(&pulse, duration / 2U);
    lv_anim_set_path_cb(&pulse, lv_anim_path_ease_in_out);
    lv_anim_start(&pulse);
}

void ui_motion_move_y(lv_obj_t *object, int32_t target_y, uint32_t duration,
                      lv_anim_path_cb_t path,
                      lv_anim_completed_cb_t completed, void *user_data)
{
    if(object == NULL) return;
    ui_motion_value(object, set_y, lv_obj_get_y(object), target_y, duration,
                    path, completed, user_data);
}

void ui_motion_cancel_y(lv_obj_t *object)
{
    ui_motion_cancel(object, set_y);
}

void ui_motion_translate_fade(lv_obj_t *object,
                              int32_t from_y, int32_t to_y,
                              lv_opa_t from_opa, lv_opa_t to_opa,
                              uint32_t duration,
                              lv_anim_completed_cb_t completed,
                              void *user_data)
{
    if(object == NULL) return;
    ui_motion_cancel(object, set_y);
    ui_motion_cancel(object, set_opacity);
    lv_obj_set_y(object, from_y);
    lv_obj_set_style_opa(object, from_opa, 0);
    ui_motion_value(object, set_y, from_y, to_y, duration,
                    lv_anim_path_ease_in_out, NULL, NULL);
    ui_motion_value(object, set_opacity, from_opa, to_opa, duration,
                    lv_anim_path_ease_in_out, completed, user_data);
}

static void delayed_value(void *target, ui_motion_value_cb_t callback,
                          int32_t from, int32_t to, uint32_t duration,
                          uint32_t delay,
                          lv_anim_completed_cb_t completed, void *user_data)
{
    if(target == NULL || callback == NULL) return;
    lv_anim_delete(target, callback);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, target);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_time(&animation, s_reduced ? 1U : duration);
    lv_anim_set_delay(&animation, s_reduced ? 0U : delay);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, callback);
    lv_anim_set_user_data(&animation, user_data);
    if(completed != NULL) lv_anim_set_completed_cb(&animation, completed);
    lv_anim_start(&animation);
}

void ui_motion_translate_fade_delayed(lv_obj_t *object,
                                      int32_t from_y, int32_t to_y,
                                      lv_opa_t from_opa, lv_opa_t to_opa,
                                      uint32_t duration, uint32_t delay,
                                      lv_anim_completed_cb_t completed,
                                      void *user_data)
{
    if(object == NULL) return;
    ui_motion_cancel(object, set_y);
    ui_motion_cancel(object, set_opacity);
    lv_obj_set_y(object, from_y);
    lv_obj_set_style_opa(object, from_opa, 0);
    delayed_value(object, set_y, from_y, to_y, duration, delay, NULL, NULL);
    delayed_value(object, set_opacity, from_opa, to_opa, duration, delay,
                  completed, user_data);
}

void ui_motion_dock_enter(lv_obj_t *object, int32_t resting_y)
{
    if(object == NULL) return;
    ui_motion_cancel(object, set_y);
    ui_motion_cancel(object, set_opacity);
    ui_motion_cancel(object, set_scale);
    lv_obj_set_y(object, resting_y + 50);
    lv_obj_set_style_opa(object, LV_OPA_TRANSP, 0);
    lv_obj_set_style_transform_scale(object, 248, 0);
    ui_motion_value(object, set_y, resting_y + 50, resting_y,
                    UI_MOTION_DURATION_DOCK, lv_anim_path_ease_out, NULL, NULL);
    ui_motion_value(object, set_opacity, LV_OPA_TRANSP, LV_OPA_COVER,
                    UI_MOTION_DURATION_DOCK, lv_anim_path_ease_out, NULL, NULL);
    ui_motion_value(object, set_scale, 248, UI_MOTION_PRESS_SCALE_NORMAL,
                    UI_MOTION_DURATION_DOCK, lv_anim_path_ease_out, NULL, NULL);
}

void ui_motion_dock_exit(lv_obj_t *object, int32_t resting_y,
                         lv_anim_completed_cb_t completed, void *user_data)
{
    if(object == NULL) return;
    ui_motion_cancel(object, set_y);
    ui_motion_cancel(object, set_opacity);
    ui_motion_cancel(object, set_scale);
    ui_motion_value(object, set_y, lv_obj_get_y(object), resting_y + 50,
                    UI_MOTION_DURATION_DOCK, lv_anim_path_ease_in, NULL, NULL);
    ui_motion_value(object, set_scale,
                    lv_obj_get_style_transform_scale_x(object, 0), 248,
                    UI_MOTION_DURATION_DOCK, lv_anim_path_ease_in, NULL, NULL);
    ui_motion_value(object, set_opacity, lv_obj_get_style_opa(object, 0),
                    LV_OPA_TRANSP, UI_MOTION_DURATION_DOCK,
                    lv_anim_path_ease_in, completed, user_data);
}

void ui_motion_page_load(lv_obj_t *screen, ui_motion_page_direction_t direction)
{
    if(screen == NULL) return;
    if(direction == UI_MOTION_PAGE_NONE || s_reduced) {
        lv_screen_load(screen);
    }
    else {
        lv_screen_load_anim(screen,
                            direction == UI_MOTION_PAGE_FORWARD ?
                            LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                            UI_MOTION_DURATION_PAGE, 0, false);
    }
    lv_obj_invalidate(screen);
}
