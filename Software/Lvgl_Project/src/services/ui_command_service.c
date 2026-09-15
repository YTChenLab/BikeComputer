#include "ui_command_service.h"

#include "ui/core/ui_concurrency.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    int32_t id;
    ui_command_observer_cb_t callback;
    void *user_data;
} ui_command_observer_t;

typedef struct
{
    ui_command_t queued[UI_COMMAND_QUEUE_MAX];
    ui_command_result_t results[UI_COMMAND_RESULT_MAX];
    ui_command_observer_t observers[UI_COMMAND_OBSERVER_MAX];
    ui_command_backend_t backend;
    ui_command_t active;
    uint32_t active_remaining_ms;
    uint32_t next_command_id;
    uint32_t dropped;
    uint32_t unexpected_results;
    int32_t next_observer_id;
    uint8_t queued_count;
    uint8_t result_count;
    uint8_t observer_count;
    bool active_valid;
} ui_command_service_context_t;

static ui_command_service_context_t s_commands;

static bool final_status(ui_command_status_t status)
{
    return status == UI_COMMAND_STATUS_SUCCESS ||
           status == UI_COMMAND_STATUS_FAILED;
}

static bool coalescible(ui_command_type_t type)
{
    return type == UI_COMMAND_WIFI_SET_ENABLED ||
           type == UI_COMMAND_BLUETOOTH_SET_ENABLED ||
           type == UI_COMMAND_GNSS_SET_ENABLED ||
           type == UI_COMMAND_POWER_SAVE_SET_ENABLED ||
           type == UI_COMMAND_SOUND_SET_LEVEL ||
           type == UI_COMMAND_BRIGHTNESS_SET_LEVEL ||
           type == UI_COMMAND_MEDIA_SET_PLAYING ||
           type == UI_COMMAND_MEDIA_SET_TRACK ||
           type == UI_COMMAND_MEDIA_SET_MODE;
}

static bool deduplicated(ui_command_type_t type)
{
    return type == UI_COMMAND_GNSS_ACQUIRE_FIX ||
           type == UI_COMMAND_RIDE_START ||
           type == UI_COMMAND_RIDE_PAUSE ||
           type == UI_COMMAND_RIDE_RESUME ||
           type == UI_COMMAND_RIDE_FINISH ||
           type == UI_COMMAND_RIDE_SAVE ||
           type == UI_COMMAND_RIDE_UPLOAD;
}

static void remove_command(uint8_t index)
{
    for(uint8_t next = index + 1U; next < s_commands.queued_count; next++)
        s_commands.queued[next - 1U] = s_commands.queued[next];
    s_commands.queued_count--;
}

static void remove_result(uint8_t index)
{
    for(uint8_t next = index + 1U; next < s_commands.result_count; next++)
        s_commands.results[next - 1U] = s_commands.results[next];
    s_commands.result_count--;
}

static void notify_observers(const ui_command_result_t *result)
{
    ui_command_observer_t observers[UI_COMMAND_OBSERVER_MAX];
    uint8_t observer_count;
    ui_concurrency_lock();
    observer_count = s_commands.observer_count;
    memcpy(observers, s_commands.observers,
           observer_count * sizeof(observers[0]));
    ui_concurrency_unlock();

    for(uint8_t index = 0; index < observer_count; index++)
        observers[index].callback(result, observers[index].user_data);
}

void ui_command_service_init(void)
{
    ui_concurrency_lock();
    memset(&s_commands, 0, sizeof(s_commands));
    s_commands.next_command_id = 1U;
    s_commands.next_observer_id = 1;
    ui_concurrency_unlock();
}

bool ui_command_service_register_backend(const ui_command_backend_t *backend)
{
    if(backend == NULL || backend->dispatch == NULL) return false;
    ui_concurrency_lock();
    s_commands.backend = *backend;
    ui_concurrency_unlock();
    return true;
}

void ui_command_service_reset_backend(void)
{
    ui_concurrency_lock();
    memset(&s_commands.backend, 0, sizeof(s_commands.backend));
    ui_concurrency_unlock();
}

uint32_t ui_command_service_submit(ui_command_type_t type, int32_t value,
                                   uint32_t timeout_ms)
{
    if(type >= UI_COMMAND_COUNT) return 0U;
    if(timeout_ms == 0U) timeout_ms = UI_COMMAND_TIMEOUT_DEFAULT;

    ui_concurrency_lock();
    if(deduplicated(type) && s_commands.active_valid &&
       s_commands.active.type == type) {
        uint32_t id = s_commands.active.id;
        ui_concurrency_unlock();
        return id;
    }
    if(coalescible(type)) {
        for(uint8_t index = 0; index < s_commands.queued_count; index++) {
            if(s_commands.queued[index].type != type) continue;
            s_commands.queued[index].value = value;
            s_commands.queued[index].timeout_ms = timeout_ms;
            uint32_t id = s_commands.queued[index].id;
            ui_concurrency_unlock();
            return id;
        }
    }
    if(deduplicated(type)) {
        for(uint8_t index = 0; index < s_commands.queued_count; index++) {
            if(s_commands.queued[index].type != type) continue;
            uint32_t id = s_commands.queued[index].id;
            ui_concurrency_unlock();
            return id;
        }
    }
    if(s_commands.queued_count >= UI_COMMAND_QUEUE_MAX) {
        s_commands.dropped++;
        ui_concurrency_unlock();
        return 0U;
    }

    uint32_t id = s_commands.next_command_id++;
    if(s_commands.next_command_id == 0U) s_commands.next_command_id = 1U;
    s_commands.queued[s_commands.queued_count++] = (ui_command_t) {
        .id = id,
        .type = type,
        .value = value,
        .timeout_ms = timeout_ms
    };
    ui_concurrency_unlock();
    return id;
}

bool ui_command_service_complete(uint32_t command_id,
                                 ui_command_status_t status,
                                 int32_t error_code)
{
    if(command_id == 0U || !final_status(status)) return false;
    ui_concurrency_lock();
    if(s_commands.result_count >= UI_COMMAND_RESULT_MAX) {
        s_commands.dropped++;
        ui_concurrency_unlock();
        return false;
    }
    s_commands.results[s_commands.result_count++] = (ui_command_result_t) {
        .id = command_id,
        .status = status,
        .error_code = error_code
    };
    ui_concurrency_unlock();
    return true;
}

static bool finish_active_from_result(ui_command_result_t *result)
{
    ui_concurrency_lock();
    if(!s_commands.active_valid) {
        s_commands.unexpected_results += s_commands.result_count;
        s_commands.result_count = 0U;
        ui_concurrency_unlock();
        return false;
    }
    for(uint8_t index = 0; index < s_commands.result_count; index++) {
        if(s_commands.results[index].id != s_commands.active.id) continue;
        *result = s_commands.results[index];
        result->type = s_commands.active.type;
        remove_result(index);
        s_commands.active_valid = false;
        ui_concurrency_unlock();
        return true;
    }
    for(uint8_t index = 0; index < s_commands.result_count;) {
        if(s_commands.results[index].id == s_commands.active.id) {
            index++;
            continue;
        }
        remove_result(index);
        s_commands.unexpected_results++;
    }
    ui_concurrency_unlock();
    return false;
}

static bool timeout_active(uint32_t elapsed_ms,
                           ui_command_result_t *result)
{
    ui_concurrency_lock();
    if(!s_commands.active_valid) {
        ui_concurrency_unlock();
        return false;
    }
    if(elapsed_ms < s_commands.active_remaining_ms) {
        s_commands.active_remaining_ms -= elapsed_ms;
        ui_concurrency_unlock();
        return false;
    }
    *result = (ui_command_result_t) {
        .id = s_commands.active.id,
        .type = s_commands.active.type,
        .status = UI_COMMAND_STATUS_TIMEOUT,
        .error_code = 0
    };
    s_commands.active_valid = false;
    ui_concurrency_unlock();
    return true;
}

static bool start_next(ui_command_t *command,
                       ui_command_backend_t *backend)
{
    ui_concurrency_lock();
    if(s_commands.active_valid || s_commands.queued_count == 0U) {
        ui_concurrency_unlock();
        return false;
    }
    *command = s_commands.queued[0];
    remove_command(0U);
    s_commands.active = *command;
    s_commands.active_remaining_ms = command->timeout_ms;
    s_commands.active_valid = true;
    *backend = s_commands.backend;
    ui_concurrency_unlock();
    return true;
}

void ui_command_service_process(uint32_t elapsed_ms)
{
    ui_command_backend_t backend;
    ui_concurrency_lock();
    backend = s_commands.backend;
    ui_concurrency_unlock();
    if(elapsed_ms > 0U && backend.tick != NULL)
        backend.tick(elapsed_ms, backend.context);

    ui_command_result_t result;
    if(finish_active_from_result(&result)) notify_observers(&result);
    else if(elapsed_ms > 0U && timeout_active(elapsed_ms, &result))
        notify_observers(&result);

    ui_command_t command;
    if(!start_next(&command, &backend)) return;
    if(backend.dispatch == NULL ||
       !backend.dispatch(&command, backend.context)) {
        ui_concurrency_lock();
        if(s_commands.active_valid && s_commands.active.id == command.id)
            s_commands.active_valid = false;
        ui_concurrency_unlock();
        result = (ui_command_result_t) {
            .id = command.id,
            .type = command.type,
            .status = UI_COMMAND_STATUS_REJECTED,
            .error_code = 0
        };
    }
    else {
        result = (ui_command_result_t) {
            .id = command.id,
            .type = command.type,
            .status = UI_COMMAND_STATUS_PENDING,
            .error_code = 0
        };
    }
    notify_observers(&result);
}

int32_t ui_command_service_subscribe(ui_command_observer_cb_t callback,
                                     void *user_data)
{
    if(callback == NULL) return -1;
    ui_concurrency_lock();
    if(s_commands.observer_count >= UI_COMMAND_OBSERVER_MAX) {
        ui_concurrency_unlock();
        return -1;
    }
    ui_command_observer_t *observer =
        &s_commands.observers[s_commands.observer_count++];
    observer->id = s_commands.next_observer_id++;
    observer->callback = callback;
    observer->user_data = user_data;
    int32_t id = observer->id;
    ui_concurrency_unlock();
    return id;
}

bool ui_command_service_unsubscribe(int32_t subscription_id)
{
    ui_concurrency_lock();
    for(uint8_t index = 0; index < s_commands.observer_count; index++) {
        if(s_commands.observers[index].id != subscription_id) continue;
        for(uint8_t next = index + 1U;
            next < s_commands.observer_count; next++)
            s_commands.observers[next - 1U] = s_commands.observers[next];
        s_commands.observer_count--;
        ui_concurrency_unlock();
        return true;
    }
    ui_concurrency_unlock();
    return false;
}

uint8_t ui_command_service_pending(void)
{
    ui_concurrency_lock();
    uint8_t count = s_commands.queued_count +
                    (s_commands.active_valid ? 1U : 0U);
    ui_concurrency_unlock();
    return count;
}

bool ui_command_service_active(ui_command_t *command)
{
    if(command == NULL) return false;
    ui_concurrency_lock();
    bool active = s_commands.active_valid;
    if(active) *command = s_commands.active;
    ui_concurrency_unlock();
    return active;
}

uint32_t ui_command_service_dropped(void)
{
    ui_concurrency_lock();
    uint32_t dropped = s_commands.dropped;
    ui_concurrency_unlock();
    return dropped;
}

uint32_t ui_command_service_unexpected_results(void)
{
    ui_concurrency_lock();
    uint32_t unexpected = s_commands.unexpected_results;
    ui_concurrency_unlock();
    return unexpected;
}
