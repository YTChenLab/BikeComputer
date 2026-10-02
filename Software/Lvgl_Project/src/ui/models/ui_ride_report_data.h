#ifndef UI_RIDE_REPORT_DATA_H
#define UI_RIDE_REPORT_DATA_H

#include <stdint.h>
#include <stdbool.h>

#define UI_RIDE_REPORT_POINT_MAX 120U
#define UI_RIDE_REPORT_ZONE_COUNT 5U

/* Aggregate availability survives trace downsampling. Old records have no
 * KNOWN bit and retain their original aggregate values for compatibility. */
enum {
    UI_REPORT_HAS_DISTANCE = 1U,
    UI_REPORT_HAS_SPEED = 2U,
    UI_REPORT_HAS_ALTITUDE = 4U,
    UI_REPORT_HAS_ASCENT = 8U,
    UI_REPORT_HAS_CALORIES = 16U,
    UI_REPORT_METRICS_KNOWN = 128U
};

typedef struct
{
    uint16_t point_count;
    bool time_is_estimated;
    uint16_t speed_tenths[UI_RIDE_REPORT_POINT_MAX];
    uint16_t heart_rate[UI_RIDE_REPORT_POINT_MAX];
    uint16_t power[UI_RIDE_REPORT_POINT_MAX];
    uint16_t cadence[UI_RIDE_REPORT_POINT_MAX];
    int16_t elevation_m[UI_RIDE_REPORT_POINT_MAX];
    uint32_t time_seconds[UI_RIDE_REPORT_POINT_MAX];
    uint32_t valid_fields[UI_RIDE_REPORT_POINT_MAX];
    int32_t latitude_microdegrees[UI_RIDE_REPORT_POINT_MAX];
    int32_t longitude_microdegrees[UI_RIDE_REPORT_POINT_MAX];
    uint32_t heart_rate_zone_seconds[UI_RIDE_REPORT_ZONE_COUNT];
    uint32_t power_zone_seconds[UI_RIDE_REPORT_ZONE_COUNT];
} ui_ride_report_trace_t;

#endif /* UI_RIDE_REPORT_DATA_H */
