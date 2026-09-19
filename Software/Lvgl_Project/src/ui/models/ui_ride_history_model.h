#ifndef UI_RIDE_HISTORY_MODEL_H
#define UI_RIDE_HISTORY_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_RIDE_HISTORY_MAX_RECORDS 100U
#define UI_RIDE_RECORD_FORMAT_VERSION 1U

typedef struct
{
    uint64_t id;
    int64_t started_at_utc;
    int64_t finished_at_utc;
    float distance_km;
    uint32_t duration_seconds;
    float average_speed_kmh;
    float maximum_speed_kmh;
    uint32_t ascent_m;
    uint16_t average_heart_rate;
    uint16_t average_power;
    uint16_t average_cadence;
} ui_ride_record_t;

typedef struct
{
    double total_distance_km;
    uint64_t total_duration_seconds;
    uint64_t total_ascent_m;
    uint32_t ride_count;
} ui_ride_history_totals_t;

void ui_ride_history_model_init(void);
bool ui_ride_history_model_replace(const ui_ride_record_t *records,
                                   size_t count);
bool ui_ride_history_model_append(const ui_ride_record_t *record);
bool ui_ride_history_model_contains(uint64_t id);
size_t ui_ride_history_model_count(void);
bool ui_ride_history_model_get_newest(size_t index,
                                      ui_ride_record_t *record);
bool ui_ride_history_model_copy_oldest_first(ui_ride_record_t *records,
                                             size_t capacity,
                                             size_t *count);
void ui_ride_history_model_totals(ui_ride_history_totals_t *totals);
uint32_t ui_ride_history_model_version(void);

#endif /* UI_RIDE_HISTORY_MODEL_H */
