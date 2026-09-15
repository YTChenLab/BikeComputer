#include "ui_pc_services.h"

#include "services/ui_system_services.h"
#include "ui/core/ui_event_bridge.h"
#include "ui/models/ui_ride_data_model.h"

#include <string.h>
#include <time.h>

typedef struct
{
    uint32_t gnss_remaining_ms;
    uint32_t storage_remaining_ms;
    uint32_t ride_data_remaining_ms;
    uint32_t ride_rng_state;
    bool gnss_busy;
    bool storage_busy;
    bool next_gnss_success;
    bool next_storage_success;
    bool ride_data_initialized;
} ui_pc_services_context_t;

static ui_pc_services_context_t s_pc;

static void publish(ui_event_type_t type, ui_event_priority_t priority,
                    uint32_t dedupe_key)
{
    ui_event_t event = {0};
    event.type = type;
    event.priority = priority;
    event.source_id = 0x5043U; /* "PC" */
    event.dedupe_key = dedupe_key;
    (void)ui_event_bridge_post(&event);
}

static uint32_t random_value(ui_pc_services_context_t *pc, uint32_t limit)
{
    pc->ride_rng_state = pc->ride_rng_state * 1664525U + 1013904223U;
    return limit > 0U ? pc->ride_rng_state % limit : 0U;
}

static void publish_ride_data(ui_pc_services_context_t *pc)
{
    ui_ride_data_t data = {0};
    (void)ui_ride_data_model_get(&data, NULL);
    if(!pc->ride_data_initialized) {
        data.distance = (float)(1200U + random_value(pc, 8800U)) / 100.0f;
        data.ride_seconds = 1800U + random_value(pc, 7200U);
        data.clock_hour = (uint8_t)random_value(pc, 24U);
        data.clock_minute = (uint8_t)random_value(pc, 60U);
        pc->ride_data_initialized = true;
    }
    else {
        data.distance += (float)(1U + random_value(pc, 8U)) / 100.0f;
        data.ride_seconds += 1U;
    }
    data.speed = (float)(120U + random_value(pc, 361U)) / 10.0f;
    data.heart_rate = (uint16_t)(105U + random_value(pc, 76U));
    data.power = (uint16_t)(90U + random_value(pc, 331U));
    data.cadence = (uint16_t)(60U + random_value(pc, 56U));
    data.grade = (float)((int32_t)random_value(pc, 151U) - 50) / 10.0f;
    data.satellites = (uint8_t)(5U + random_value(pc, 12U));
    data.battery = (uint8_t)(68U + random_value(pc, 32U));
    ui_ride_data_model_submit(&data);
}

static bool request_gnss_fix(void *context)
{
    ui_pc_services_context_t *pc = context;
    if(pc == NULL || pc->gnss_busy) return false;
    pc->gnss_busy = true;
    pc->gnss_remaining_ms = UI_PC_GNSS_DELAY_MS;
    return true;
}

static bool save_ride(void *context)
{
    ui_pc_services_context_t *pc = context;
    if(pc == NULL || pc->storage_busy) return false;
    pc->storage_busy = true;
    pc->storage_remaining_ms = UI_PC_STORAGE_DELAY_MS;
    publish(UI_EVENT_RIDE_SAVE_STARTED, UI_EVENT_PRIORITY_P2_RIDE_STATE,
            0x53564EU);
    return true;
}

static bool retry_save(void *context)
{
    return save_ride(context);
}

static void tick_operation(uint32_t elapsed_ms, uint32_t *remaining,
                           bool *busy, bool success,
                           ui_event_type_t success_event,
                           ui_event_type_t failure_event,
                           uint32_t dedupe_key)
{
    if(!*busy) return;
    if(elapsed_ms < *remaining) {
        *remaining -= elapsed_ms;
        return;
    }
    *remaining = 0;
    *busy = false;
    publish(success ? success_event : failure_event,
            success ? UI_EVENT_PRIORITY_P2_RIDE_STATE :
                      UI_EVENT_PRIORITY_P0_CRITICAL,
            dedupe_key);
}

static void tick(uint32_t elapsed_ms, void *context)
{
    ui_pc_services_context_t *pc = context;
    if(pc == NULL || elapsed_ms == 0) return;

    if(elapsed_ms >= pc->ride_data_remaining_ms) {
        publish_ride_data(pc);
        pc->ride_data_remaining_ms = 800U;
    }
    else pc->ride_data_remaining_ms -= elapsed_ms;

    bool gnss_result = pc->next_gnss_success;
    bool storage_result = pc->next_storage_success;
    bool gnss_was_busy = pc->gnss_busy;
    bool storage_was_busy = pc->storage_busy;

    tick_operation(elapsed_ms, &pc->gnss_remaining_ms, &pc->gnss_busy,
                   gnss_result, UI_EVENT_GPS_READY, UI_EVENT_GPS_FAILED,
                   0x474E53U);
    tick_operation(elapsed_ms, &pc->storage_remaining_ms, &pc->storage_busy,
                   storage_result, UI_EVENT_RIDE_SAVE_SUCCESS,
                   UI_EVENT_RIDE_SAVE_FAILED, 0x53564EU);

    /* Failure/success overrides are one-shot; defaults remain successful. */
    if(gnss_was_busy && !pc->gnss_busy) pc->next_gnss_success = true;
    if(storage_was_busy && !pc->storage_busy)
        pc->next_storage_success = true;
}

void ui_pc_services_register(void)
{
    memset(&s_pc, 0, sizeof(s_pc));
    s_pc.next_gnss_success = true;
    s_pc.next_storage_success = true;
    s_pc.ride_data_remaining_ms = 1U;
    s_pc.ride_rng_state = (uint32_t)time(NULL) ^
                          (uint32_t)(uintptr_t)&s_pc;
    const ui_system_services_backend_t backend = {
        .request_gnss_fix = request_gnss_fix,
        .save_ride = save_ride,
        .retry_save = retry_save,
        .tick = tick,
        .context = &s_pc
    };
    ui_system_services_register(&backend);
}

void ui_pc_services_set_next_gnss_result(bool success)
{
    s_pc.next_gnss_success = success;
}

void ui_pc_services_set_next_storage_result(bool success)
{
    s_pc.next_storage_success = success;
}

bool ui_pc_services_gnss_busy(void)
{
    return s_pc.gnss_busy;
}

bool ui_pc_services_storage_busy(void)
{
    return s_pc.storage_busy;
}
