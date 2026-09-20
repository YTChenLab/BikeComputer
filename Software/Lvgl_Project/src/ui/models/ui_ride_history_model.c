#include "ui_ride_history_model.h"

#include <math.h>
#include <string.h>

typedef struct
{
    ui_ride_record_t records[UI_RIDE_HISTORY_MAX_RECORDS];
    size_t count;
    uint32_t version;
} ui_ride_history_context_t;

static ui_ride_history_context_t s_history;

static bool valid_record(const ui_ride_record_t *record)
{
    return record != NULL && record->id != 0U &&
           record->finished_at_utc >= record->started_at_utc &&
           isfinite(record->distance_km) && record->distance_km >= 0.0f &&
           isfinite(record->average_speed_kmh) &&
           record->average_speed_kmh >= 0.0f &&
           isfinite(record->maximum_speed_kmh) &&
           record->maximum_speed_kmh >= 0.0f &&
           record->trace.point_count <= UI_RIDE_REPORT_POINT_MAX;
}

void ui_ride_history_model_init(void)
{
    memset(&s_history, 0, sizeof(s_history));
    s_history.version = 1U;
}

bool ui_ride_history_model_replace(const ui_ride_record_t *records,
                                   size_t count)
{
    if(count > UI_RIDE_HISTORY_MAX_RECORDS ||
       (count > 0U && records == NULL)) return false;
    for(size_t index = 0U; index < count; index++) {
        if(!valid_record(&records[index])) return false;
        for(size_t previous = 0U; previous < index; previous++) {
            if(records[previous].id == records[index].id) return false;
        }
    }
    memset(s_history.records, 0, sizeof(s_history.records));
    if(count > 0U)
        memcpy(s_history.records, records, count * sizeof(records[0]));
    s_history.count = count;
    s_history.version++;
    return true;
}

bool ui_ride_history_model_contains(uint64_t id)
{
    if(id == 0U) return false;
    for(size_t index = 0U; index < s_history.count; index++) {
        if(s_history.records[index].id == id) return true;
    }
    return false;
}

bool ui_ride_history_model_append(const ui_ride_record_t *record)
{
    if(!valid_record(record)) return false;
    if(ui_ride_history_model_contains(record->id)) return true;
    if(s_history.count == UI_RIDE_HISTORY_MAX_RECORDS) {
        memmove(&s_history.records[0], &s_history.records[1],
                (UI_RIDE_HISTORY_MAX_RECORDS - 1U) *
                sizeof(s_history.records[0]));
        s_history.count--;
    }
    s_history.records[s_history.count++] = *record;
    s_history.version++;
    return true;
}

size_t ui_ride_history_model_count(void)
{
    return s_history.count;
}

bool ui_ride_history_model_get_newest(size_t index,
                                      ui_ride_record_t *record)
{
    if(record == NULL || index >= s_history.count) return false;
    *record = s_history.records[s_history.count - 1U - index];
    return true;
}

bool ui_ride_history_model_copy_oldest_first(ui_ride_record_t *records,
                                             size_t capacity,
                                             size_t *count)
{
    if(count == NULL || capacity < s_history.count ||
       (s_history.count > 0U && records == NULL)) return false;
    if(s_history.count > 0U)
        memcpy(records, s_history.records,
               s_history.count * sizeof(records[0]));
    *count = s_history.count;
    return true;
}

void ui_ride_history_model_totals(ui_ride_history_totals_t *totals)
{
    if(totals == NULL) return;
    memset(totals, 0, sizeof(*totals));
    totals->ride_count = (uint32_t)s_history.count;
    for(size_t index = 0U; index < s_history.count; index++) {
        totals->total_distance_km += s_history.records[index].distance_km;
        totals->total_duration_seconds +=
            s_history.records[index].duration_seconds;
        totals->total_ascent_m += s_history.records[index].ascent_m;
    }
}

uint32_t ui_ride_history_model_version(void)
{
    return s_history.version;
}
