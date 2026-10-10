#include "ui/core/ui_memory.h"
#include "ui_navigation_service.h"

#include "ui/core/ui_concurrency.h"

#include <stddef.h>
#include <math.h>
#include <string.h>

typedef struct
{
    int32_t id;
    ui_navigation_observer_cb_t callback;
    void *user_data;
} ui_navigation_observer_t;

typedef struct
{
    nav_model_t current;
    nav_model_t pending;
    ui_navigation_observer_t observers[UI_NAVIGATION_OBSERVER_MAX];
    uint8_t observer_count;
    int32_t next_observer_id;
    uint32_t version;
    bool pending_valid, prompts_enabled;
} ui_navigation_service_context_t;

static UI_LARGE_RAM ui_navigation_service_context_t s_navigation_service;

void ui_navigation_service_init(void)
{
    ui_concurrency_lock();
    memset(&s_navigation_service, 0, sizeof(s_navigation_service));
    s_navigation_service.next_observer_id = 1;
    s_navigation_service.prompts_enabled = true;
    ui_concurrency_unlock();
}

void ui_navigation_service_set_prompts_enabled(bool enabled)
{ ui_concurrency_lock(); s_navigation_service.prompts_enabled=enabled; ui_concurrency_unlock(); }
bool ui_navigation_service_prompts_enabled(void)
{ ui_concurrency_lock(); bool enabled=s_navigation_service.prompts_enabled; ui_concurrency_unlock(); return enabled; }

int32_t ui_navigation_service_subscribe(ui_navigation_observer_cb_t callback,
                                        void *user_data)
{
    if(callback == NULL) return -1;
    ui_concurrency_lock();
    if(s_navigation_service.observer_count >= UI_NAVIGATION_OBSERVER_MAX) {
        ui_concurrency_unlock();
        return -1;
    }
    ui_navigation_observer_t *observer =
        &s_navigation_service.observers[s_navigation_service.observer_count++];
    observer->id = s_navigation_service.next_observer_id++;
    observer->callback = callback;
    observer->user_data = user_data;
    int32_t id = observer->id;
    ui_concurrency_unlock();
    return id;
}

bool ui_navigation_service_unsubscribe(int32_t subscription_id)
{
    ui_concurrency_lock();
    for(uint8_t index = 0; index < s_navigation_service.observer_count;
        index++) {
        if(s_navigation_service.observers[index].id != subscription_id)
            continue;
        for(uint8_t next = index + 1U;
            next < s_navigation_service.observer_count; next++) {
            s_navigation_service.observers[next - 1U] =
                s_navigation_service.observers[next];
        }
        s_navigation_service.observer_count--;
        ui_concurrency_unlock();
        return true;
    }
    ui_concurrency_unlock();
    return false;
}

void ui_navigation_service_submit(const nav_model_t *model)
{ (void)ui_navigation_service_submit_checked(model); }

bool ui_navigation_service_submit_checked(const nav_model_t *model)
{
    if(model == NULL) return false;
    if(model->route_point_count > 16 || (model->demo && !ui_navigation_service_demo_enabled())) return false;
    if(model->session_bound && (!model->session_id || !model->route_id || !model->route_revision ||
        model->coordinate_system>1 || model->ui_state>=NAV_UI_STATE_COUNT ||
        (model->position_valid && (!isfinite(model->latitude)||!isfinite(model->longitude)||fabs(model->latitude)>90||fabs(model->longitude)>180)) ||
        (model->heading_valid && !isfinite(model->heading)) ||
        (model->speed_valid && (!isfinite(model->speed)||model->speed<0)) ||
        (model->distance_valid && (!isfinite(model->distance)||model->distance<0)) ||
        (model->eta_valid && (model->eta_hour>23||model->eta_min>59)) ||
        (model->guidance_valid && (!model->step_id||model->direction>=NAV_MANEUVER_COUNT)) ||
        (model->next_valid && model->next_direction>=NAV_MANEUVER_COUNT) ||
        (model->exit_valid && !model->roundabout_exit) ||
        (model->exit_position_valid && (!isfinite(model->exit_latitude)||!isfinite(model->exit_longitude)||fabs(model->exit_latitude)>90||fabs(model->exit_longitude)>180)) ||
        (model->junction_valid && (!isfinite(model->junction_latitude)||!isfinite(model->junction_longitude)||fabs(model->junction_latitude)>90||fabs(model->junction_longitude)>180)))) return false;
    ui_concurrency_lock();
    const nav_model_t *latest=s_navigation_service.pending_valid?&s_navigation_service.pending:&s_navigation_service.current;
    if(model->session_bound&&latest->session_bound&&model->session_id==latest->session_id&&model->route_id==latest->route_id&&
       (model->route_revision<latest->route_revision ||
        (model->route_revision==latest->route_revision && (model->step_id<latest->step_id ||
         (model->sampled_at_ms&&latest->sampled_at_ms&&(int32_t)(model->sampled_at_ms-latest->sampled_at_ms)<0))))) {
        ui_concurrency_unlock();return false;
    }
    s_navigation_service.pending = *model;
    s_navigation_service.pending.road_name[sizeof(model->road_name) - 1] = '\0';
    s_navigation_service.pending_valid = true;
    ui_concurrency_unlock();
    return true;
}

bool ui_navigation_service_process(void)
{
    nav_model_t snapshot;
    ui_navigation_observer_t observers[UI_NAVIGATION_OBSERVER_MAX];
    uint8_t observer_count;
    uint32_t version;

    ui_concurrency_lock();
    if(!s_navigation_service.pending_valid) {
        ui_concurrency_unlock();
        return false;
    }
    s_navigation_service.current = s_navigation_service.pending;
    s_navigation_service.pending_valid = false;
    s_navigation_service.version++;
    snapshot = s_navigation_service.current;
    version = s_navigation_service.version;
    observer_count = s_navigation_service.observer_count;
    memcpy(observers, s_navigation_service.observers,
           observer_count * sizeof(observers[0]));
    ui_concurrency_unlock();

    for(uint8_t index = 0; index < observer_count; index++) {
        observers[index].callback(&snapshot, version,
                                  observers[index].user_data);
    }
    return true;
}

bool ui_navigation_service_get(nav_model_t *model, uint32_t *version)
{
    if(model == NULL) return false;
    ui_concurrency_lock();
    *model = s_navigation_service.current;
    if(version != NULL) *version = s_navigation_service.version;
    ui_concurrency_unlock();
    return true;
}

bool ui_navigation_service_expire(uint32_t now_ms)
{
    ui_concurrency_lock();
    nav_model_t model = s_navigation_service.current;
    bool expired = model.position_valid && model.stale_after_ms &&
        (uint32_t)(now_ms - model.sampled_at_ms) > model.stale_after_ms;
    if(expired && !s_navigation_service.pending_valid) {
        model.position_valid = false;
        if(model.session_bound) model.guidance_valid=model.heading_valid=model.eta_valid=model.speed_valid=false;
        model.ui_state = NAV_UI_GPS_WEAK;
        s_navigation_service.pending = model;
        s_navigation_service.pending_valid = true;
    }
    ui_concurrency_unlock();
    return expired && ui_navigation_service_process();
}
