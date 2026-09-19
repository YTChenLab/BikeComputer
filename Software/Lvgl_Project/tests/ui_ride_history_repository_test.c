#include "services/ui_ride_history_repository.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    ui_ride_record_t records[UI_RIDE_HISTORY_MAX_RECORDS];
    size_t count;
    int64_t now;
    bool fail_store;
    uint32_t store_calls;
} memory_storage_t;

static ui_ride_history_result_t load_records(
    ui_ride_record_t *records, size_t capacity, size_t *count, void *context)
{
    memory_storage_t *storage = context;
    if(storage->count == 0U) return UI_RIDE_HISTORY_EMPTY;
    if(capacity < storage->count) return UI_RIDE_HISTORY_INVALID_DATA;
    memcpy(records, storage->records,
           storage->count * sizeof(storage->records[0]));
    *count = storage->count;
    return UI_RIDE_HISTORY_OK;
}

static ui_ride_history_result_t store_records(
    const ui_ride_record_t *records, size_t count, void *context)
{
    memory_storage_t *storage = context;
    storage->store_calls++;
    if(storage->fail_store) return UI_RIDE_HISTORY_IO_ERROR;
    memcpy(storage->records, records, count * sizeof(records[0]));
    storage->count = count;
    return UI_RIDE_HISTORY_OK;
}

static int64_t now_utc(void *context)
{
    return ((memory_storage_t *)context)->now;
}

static ui_ride_summary_t summary(void)
{
    return (ui_ride_summary_t) {
        .distance_km = 42.5f,
        .duration_seconds = 5400U,
        .average_speed_kmh = 28.3f,
        .maximum_speed_kmh = 49.1f,
        .ascent_m = 620U,
        .average_heart_rate = 146U,
        .average_power = 218U,
        .average_cadence = 89U
    };
}

int main(void)
{
    memory_storage_t memory = {.now = 1750000000};
    ui_ride_history_storage_t backend = {
        .load = load_records,
        .store = store_records,
        .now_utc = now_utc,
        .context = &memory
    };
    assert(ui_ride_history_repository_register_storage(&backend));
    ui_ride_history_repository_init();
    assert(ui_ride_history_repository_load() == UI_RIDE_HISTORY_OK);
    assert(ui_ride_history_model_count() == 0U);

    ui_ride_summary_t ride = summary();
    assert(ui_ride_history_repository_prepare(&ride) == UI_RIDE_HISTORY_OK);
    assert(ui_ride_history_repository_has_pending());
    memory.fail_store = true;
    assert(ui_ride_history_repository_commit_pending() ==
           UI_RIDE_HISTORY_IO_ERROR);
    assert(ui_ride_history_model_count() == 0U);
    assert(ui_ride_history_repository_has_pending());

    memory.fail_store = false;
    assert(ui_ride_history_repository_commit_pending() == UI_RIDE_HISTORY_OK);
    assert(ui_ride_history_model_count() == 1U);
    assert(memory.count == 1U);
    assert(!ui_ride_history_repository_has_pending());

    uint64_t saved_id = memory.records[0].id;
    uint32_t calls = memory.store_calls;
    assert(ui_ride_history_repository_prepare(&ride) == UI_RIDE_HISTORY_OK);
    assert(ui_ride_history_repository_commit_pending() == UI_RIDE_HISTORY_OK);
    assert(memory.store_calls == calls);
    assert(memory.count == 1U);
    assert(memory.records[0].id == saved_id);

    ui_ride_history_repository_init();
    assert(ui_ride_history_model_count() == 0U);
    assert(ui_ride_history_repository_load() == UI_RIDE_HISTORY_OK);
    assert(ui_ride_history_model_count() == 1U);
    ui_ride_record_t loaded;
    assert(ui_ride_history_model_get_newest(0U, &loaded));
    assert(loaded.id == saved_id);

    puts("ui_ride_history_repository_test: PASS");
    return 0;
}
