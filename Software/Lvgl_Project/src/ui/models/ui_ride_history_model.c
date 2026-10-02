#include "ui_ride_history_model.h"
#include "../core/ui_memory.h"

#include <math.h>
#include <string.h>

typedef struct
{
    ui_ride_record_t records[UI_RIDE_HISTORY_MAX_RECORDS];
    size_t count;
    uint32_t version;
    ui_ride_history_totals_t totals;
} ui_ride_history_context_t;

static UI_LARGE_RAM ui_ride_history_context_t s_history;

bool ui_ride_record_is_valid(const ui_ride_record_t *record)
{
    bool valid = record != NULL && record->id != 0U &&
           record->finished_at_utc >= record->started_at_utc &&
           isfinite(record->distance_km) && record->distance_km >= 0.0f &&
           isfinite(record->average_speed_kmh) &&
           record->average_speed_kmh >= 0.0f &&
           isfinite(record->maximum_speed_kmh) &&
           record->maximum_speed_kmh >= 0.0f &&
           record->trace.point_count <= UI_RIDE_REPORT_POINT_MAX &&
           (record->metric_flags & ~UINT8_C(0x9F)) == 0U &&
           record->moving_time_seconds <= record->duration_seconds;
    if(!valid) return false;
    for(unsigned i = 0; i < record->trace.point_count; ++i) {
        if(record->trace.valid_fields[i] & ~UINT32_C(0x1FFFF)) return false;
        if(record->trace.time_seconds[i] > record->duration_seconds ||
           (i && record->trace.time_seconds[i] < record->trace.time_seconds[i-1])) return false;
        if(record->trace.valid_fields[i] & (UINT32_C(1) << 16)) {
            if(record->trace.latitude_microdegrees[i] < -90000000 || record->trace.latitude_microdegrees[i] > 90000000 ||
               record->trace.longitude_microdegrees[i] < -180000000 || record->trace.longitude_microdegrees[i] > 180000000) return false;
        }
    }
    return true;
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
        if(!ui_ride_record_is_valid(&records[index])) return false;
        for(size_t previous = 0U; previous < index; previous++) {
            if(records[previous].id == records[index].id) return false;
        }
    }
    memset(s_history.records, 0, sizeof(s_history.records));
    if(count > 0U)
        memcpy(s_history.records, records, count * sizeof(records[0]));
    s_history.count = count;
    memset(&s_history.totals, 0, sizeof(s_history.totals));
    for(size_t i = 0; i < count; ++i) {
        s_history.totals.total_distance_km += records[i].distance_km;
        s_history.totals.total_duration_seconds += records[i].duration_seconds;
        s_history.totals.total_ascent_m += records[i].ascent_m;
        ++s_history.totals.ride_count;
    }
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
    if(!ui_ride_record_is_valid(record)) return false;
    if(ui_ride_history_model_contains(record->id)) return true;
    if(s_history.count == UI_RIDE_HISTORY_MAX_RECORDS) {
        memmove(&s_history.records[0], &s_history.records[1],
                (UI_RIDE_HISTORY_MAX_RECORDS - 1U) *
                sizeof(s_history.records[0]));
        s_history.count--;
    }
    s_history.records[s_history.count++] = *record;
    s_history.totals.total_distance_km += record->distance_km;
    s_history.totals.total_duration_seconds += record->duration_seconds;
    s_history.totals.total_ascent_m += record->ascent_m;
    ++s_history.totals.ride_count;
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
    *totals = s_history.totals;
}

bool ui_ride_history_model_set_totals(const ui_ride_history_totals_t *totals)
{
    if(totals == NULL || !isfinite(totals->total_distance_km) ||
       totals->total_distance_km < 0 || totals->ride_count < s_history.count) return false;
    double distance = 0;
    uint64_t duration = 0, ascent = 0;
    for(size_t i = 0; i < s_history.count; ++i) {
        distance += s_history.records[i].distance_km;
        duration += s_history.records[i].duration_seconds;
        ascent += s_history.records[i].ascent_m;
    }
    if(totals->total_distance_km + 0.001 < distance ||
       totals->total_duration_seconds < duration || totals->total_ascent_m < ascent) return false;
    s_history.totals = *totals;
    ++s_history.version;
    return true;
}

uint32_t ui_ride_history_model_version(void)
{
    return s_history.version;
}
