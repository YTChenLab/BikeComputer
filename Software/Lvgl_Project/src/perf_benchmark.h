#ifndef APP_PERF_BENCHMARK_H
#define APP_PERF_BENCHMARK_H

#include "lvgl.h"

#include <stdbool.h>

typedef enum {
    APP_PERF_BENCHMARK_NONE,
    APP_PERF_BENCHMARK_STATIC,
    APP_PERF_BENCHMARK_DATA,
} app_perf_benchmark_mode_t;

typedef struct {
    app_perf_benchmark_mode_t mode;
    lv_display_t *display;
    lv_timer_t *data_timer;
    uint32_t started_at;
    uint32_t sample_started_at;
    uint32_t refresh_count;
    uint32_t sample_count;
    uint32_t fps_sum;
    uint32_t fps_min;
    uint32_t cpu_sum;
    uint32_t cpu_max;
} app_perf_benchmark_t;

app_perf_benchmark_mode_t app_perf_benchmark_parse_arg(const char *argument);
void app_perf_benchmark_start(app_perf_benchmark_t *benchmark, lv_display_t *display,
                              app_perf_benchmark_mode_t mode);
bool app_perf_benchmark_sample(app_perf_benchmark_t *benchmark);

#endif
