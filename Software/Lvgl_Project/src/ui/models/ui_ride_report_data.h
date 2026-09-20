#ifndef UI_RIDE_REPORT_DATA_H
#define UI_RIDE_REPORT_DATA_H

#include <stdint.h>

#define UI_RIDE_REPORT_POINT_MAX 120U
#define UI_RIDE_REPORT_ZONE_COUNT 5U

typedef struct
{
    uint16_t point_count;
    uint16_t speed_tenths[UI_RIDE_REPORT_POINT_MAX];
    uint16_t heart_rate[UI_RIDE_REPORT_POINT_MAX];
    uint16_t power[UI_RIDE_REPORT_POINT_MAX];
    uint16_t cadence[UI_RIDE_REPORT_POINT_MAX];
    int16_t elevation_m[UI_RIDE_REPORT_POINT_MAX];
    uint32_t heart_rate_zone_seconds[UI_RIDE_REPORT_ZONE_COUNT];
    uint32_t power_zone_seconds[UI_RIDE_REPORT_ZONE_COUNT];
} ui_ride_report_trace_t;

#endif /* UI_RIDE_REPORT_DATA_H */
