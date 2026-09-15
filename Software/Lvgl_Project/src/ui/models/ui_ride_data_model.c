#include "ui_ride_data_model.h"

#include "../core/ui_concurrency.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    int32_t id;
    ui_ride_data_observer_cb_t callback;
    void *user_data;
} ui_ride_data_observer_t;

typedef struct
{
    ui_ride_data_t current;
    ui_ride_data_t pending;
    ui_ride_data_observer_t observers[UI_RIDE_DATA_OBSERVER_MAX];
    uint8_t observer_count;
    int32_t next_observer_id;
    uint32_t version;
    bool pending_valid;
} ui_ride_data_context_t;

static ui_ride_data_context_t s_model;

void ui_ride_data_model_init(void)
{
    ui_concurrency_lock();
    memset(&s_model, 0, sizeof(s_model));
    s_model.next_observer_id = 1;
    ui_concurrency_unlock();
}

int32_t ui_ride_data_model_subscribe(ui_ride_data_observer_cb_t callback,
                                     void *user_data)
{
    if(callback == NULL) return -1;
    ui_concurrency_lock();
    if(s_model.observer_count >= UI_RIDE_DATA_OBSERVER_MAX) {
        ui_concurrency_unlock();
        return -1;
    }
    ui_ride_data_observer_t *observer =
        &s_model.observers[s_model.observer_count++];
    observer->id = s_model.next_observer_id++;
    observer->callback = callback;
    observer->user_data = user_data;
    int32_t id = observer->id;
    ui_concurrency_unlock();
    return id;
}

bool ui_ride_data_model_unsubscribe(int32_t subscription_id)
{
    ui_concurrency_lock();
    for(uint8_t i = 0; i < s_model.observer_count; i++) {
        if(s_model.observers[i].id != subscription_id) continue;
        for(uint8_t j = i + 1; j < s_model.observer_count; j++)
            s_model.observers[j - 1] = s_model.observers[j];
        s_model.observer_count--;
        ui_concurrency_unlock();
        return true;
    }
    ui_concurrency_unlock();
    return false;
}

void ui_ride_data_model_submit(const ui_ride_data_t *data)
{
    if(data == NULL) return;
    ui_concurrency_lock();
    s_model.pending = *data;
    s_model.pending_valid = true;
    ui_concurrency_unlock();
}

bool ui_ride_data_model_process(void)
{
    ui_ride_data_t snapshot;
    ui_ride_data_observer_t observers[UI_RIDE_DATA_OBSERVER_MAX];
    uint8_t observer_count;
    uint32_t version;

    ui_concurrency_lock();
    if(!s_model.pending_valid) {
        ui_concurrency_unlock();
        return false;
    }
    s_model.current = s_model.pending;
    s_model.pending_valid = false;
    s_model.version++;
    snapshot = s_model.current;
    version = s_model.version;
    observer_count = s_model.observer_count;
    memcpy(observers, s_model.observers,
           observer_count * sizeof(observers[0]));
    ui_concurrency_unlock();

    for(uint8_t i = 0; i < observer_count; i++)
        observers[i].callback(&snapshot, version, observers[i].user_data);
    return true;
}

bool ui_ride_data_model_get(ui_ride_data_t *data, uint32_t *version)
{
    if(data == NULL) return false;
    ui_concurrency_lock();
    *data = s_model.current;
    if(version != NULL) *version = s_model.version;
    ui_concurrency_unlock();
    return true;
}

void ui_ride_data_model_set_connectivity(bool wifi, bool bluetooth)
{
    ui_concurrency_lock();
    ui_ride_data_t data = s_model.pending_valid ? s_model.pending :
                                                    s_model.current;
    data.wifi = wifi;
    data.bluetooth = bluetooth;
    s_model.pending = data;
    s_model.pending_valid = true;
    ui_concurrency_unlock();
}
