#include "ui_navigation_service.h"

#include "ui/core/ui_concurrency.h"

#include <stddef.h>
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
    bool pending_valid;
} ui_navigation_service_context_t;

static ui_navigation_service_context_t s_navigation_service;
static bool s_demo_enabled = true;
void ui_navigation_service_set_demo_enabled(bool enabled) { s_demo_enabled = enabled; }
bool ui_navigation_service_demo_enabled(void) { return s_demo_enabled; }

void ui_navigation_service_init(void)
{
    ui_concurrency_lock();
    memset(&s_navigation_service, 0, sizeof(s_navigation_service));
    s_navigation_service.next_observer_id = 1;
    ui_concurrency_unlock();
}

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
{
    if(model == NULL) return;
    if(model->route_point_count > 16 || (model->demo && !s_demo_enabled)) return;
    ui_concurrency_lock();
    s_navigation_service.pending = *model;
    s_navigation_service.pending.road_name[sizeof(model->road_name) - 1] = '\0';
    s_navigation_service.pending_valid = true;
    ui_concurrency_unlock();
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
    bool expired = !model.demo && model.position_valid && model.stale_after_ms &&
        (uint32_t)(now_ms - model.sampled_at_ms) > model.stale_after_ms;
    if(expired && !s_navigation_service.pending_valid) {
        model.position_valid = false;
        model.ui_state = NAV_UI_GPS_WEAK;
        s_navigation_service.pending = model;
        s_navigation_service.pending_valid = true;
    }
    ui_concurrency_unlock();
    return expired && ui_navigation_service_process();
}
