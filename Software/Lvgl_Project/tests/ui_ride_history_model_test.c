#include "ui/models/ui_ride_history_model.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static ui_ride_record_t record(uint64_t id, float distance,
                               uint32_t duration, uint32_t ascent)
{
    return (ui_ride_record_t) {
        .id = id,
        .started_at_utc = 1700000000 + (int64_t)id * 1000,
        .finished_at_utc = 1700000000 + (int64_t)id * 1000 + duration,
        .distance_km = distance,
        .duration_seconds = duration,
        .average_speed_kmh = 28.5f,
        .maximum_speed_kmh = 46.2f,
        .ascent_m = ascent,
        .average_heart_rate = 145U,
        .average_power = 210U,
        .average_cadence = 88U
    };
}

int main(void)
{
    ui_ride_history_model_init();
    assert(ui_ride_history_model_count() == 0U);

    ui_ride_record_t first = record(1U, 20.25f, 3600U, 240U);
    ui_ride_record_t second = record(2U, 35.75f, 5400U, 410U);
    assert(ui_ride_history_model_append(&first));
    assert(ui_ride_history_model_append(&second));
    assert(ui_ride_history_model_append(&second));
    assert(ui_ride_history_model_count() == 2U);

    ui_ride_record_t value;
    assert(ui_ride_history_model_get_newest(0U, &value));
    assert(value.id == 2U);
    assert(ui_ride_history_model_get_newest(1U, &value));
    assert(value.id == 1U);

    ui_ride_history_totals_t totals;
    ui_ride_history_model_totals(&totals);
    assert(totals.ride_count == 2U);
    assert(fabs(totals.total_distance_km - 56.0) < 0.001);
    assert(totals.total_duration_seconds == 9000U);
    assert(totals.total_ascent_m == 650U);

    ui_ride_record_t invalid = record(3U, 1.0f, 60U, 0U);
    invalid.distance_km = NAN;
    assert(!ui_ride_history_model_append(&invalid));
    invalid = record(3U, 1.0f, 60U, 0U);
    invalid.finished_at_utc = invalid.started_at_utc - 1;
    assert(!ui_ride_history_model_append(&invalid));

    ui_ride_history_model_init();
    for(uint64_t id = 1U; id <= UI_RIDE_HISTORY_MAX_RECORDS + 2U; id++) {
        ui_ride_record_t item = record(id, 1.0f, 60U, 1U);
        assert(ui_ride_history_model_append(&item));
    }
    assert(ui_ride_history_model_count() == UI_RIDE_HISTORY_MAX_RECORDS);
    assert(!ui_ride_history_model_contains(1U));
    assert(!ui_ride_history_model_contains(2U));
    assert(ui_ride_history_model_contains(3U));
    assert(ui_ride_history_model_get_newest(0U, &value));
    assert(value.id == UI_RIDE_HISTORY_MAX_RECORDS + 2U);

    puts("ui_ride_history_model_test: PASS");
    return 0;
}
