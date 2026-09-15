#include "perf_benchmark.h"

#include "ui/screens/ui_dashboard.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>


#define APP_PERF_SAMPLE_PERIOD_MS  1000U
#define APP_PERF_SAMPLE_COUNT      10U
#define APP_PERF_DATA_PERIOD_MS    100U


static void app_perf_refresh_cb(lv_event_t *event)
{
    app_perf_benchmark_t *benchmark = lv_event_get_user_data(event);
    if(benchmark != NULL)
    {
        benchmark->refresh_count++;
    }
}


static void app_perf_data_cb(lv_timer_t *timer)
{
    static uint32_t tick = 0;
    ui_dashboard_data_t data = {
        .speed = 24.0f + (float)(tick % 120U) / 10.0f,
        .heart_rate = (uint16_t)(135U + (tick % 25U)),
        .power = (uint16_t)(180U + (tick % 180U)),
        .cadence = (uint16_t)(75U + (tick % 20U)),
        .grade = 1.0f + (float)(tick % 70U) / 10.0f,
        .distance = 50.25f + (float)tick / 100.0f,
        .ride_seconds = 5425U + tick / 10U,
        .clock_hour = 10U,
        .clock_minute = (uint8_t)(24U + (tick / 600U)),
        .satellites = (uint8_t)(8U + (tick % 7U)),
        .battery = (uint8_t)(82U - (tick / 100U)),
        .wifi = true,
        .bluetooth = true,
    };

    LV_UNUSED(timer);
    tick++;
    ui_dashboard_set_data(&data);
}


app_perf_benchmark_mode_t app_perf_benchmark_parse_arg(const char *argument)
{
    if(argument == NULL)
    {
        return APP_PERF_BENCHMARK_NONE;
    }

    if(strcmp(argument, "--perf-benchmark=static") == 0)
    {
        return APP_PERF_BENCHMARK_STATIC;
    }

    if(strcmp(argument, "--perf-benchmark=data") == 0)
    {
        return APP_PERF_BENCHMARK_DATA;
    }

    return APP_PERF_BENCHMARK_NONE;
}


void app_perf_benchmark_start(app_perf_benchmark_t *benchmark, lv_display_t *display,
                              app_perf_benchmark_mode_t mode)
{
    if(benchmark == NULL || display == NULL || mode == APP_PERF_BENCHMARK_NONE)
    {
        return;
    }

    lv_memzero(benchmark, sizeof(*benchmark));
    benchmark->mode = mode;
    benchmark->display = display;
    benchmark->started_at = lv_tick_get();
    benchmark->sample_started_at = benchmark->started_at;
    benchmark->fps_min = UINT_MAX;
    lv_display_add_event_cb(display, app_perf_refresh_cb, LV_EVENT_REFR_READY, benchmark);

    if(mode == APP_PERF_BENCHMARK_DATA)
    {
        benchmark->data_timer = lv_timer_create(app_perf_data_cb, APP_PERF_DATA_PERIOD_MS, NULL);
    }
}


bool app_perf_benchmark_sample(app_perf_benchmark_t *benchmark)
{
    if(benchmark == NULL || benchmark->mode == APP_PERF_BENCHMARK_NONE)
    {
        return false;
    }

    uint32_t elapsed = lv_tick_elaps(benchmark->sample_started_at);
    if(elapsed < APP_PERF_SAMPLE_PERIOD_MS)
    {
        return false;
    }

    uint32_t fps = benchmark->refresh_count * 1000U / elapsed;
    uint32_t cpu = 100U - lv_timer_get_idle();
    benchmark->fps_sum += fps;
    benchmark->cpu_sum += cpu;
    if(fps < benchmark->fps_min) benchmark->fps_min = fps;
    if(cpu > benchmark->cpu_max) benchmark->cpu_max = cpu;
    benchmark->sample_count++;
    benchmark->refresh_count = 0;
    benchmark->sample_started_at = lv_tick_get();

    if(benchmark->sample_count < APP_PERF_SAMPLE_COUNT)
    {
        return false;
    }

    printf("PERF_RESULT mode=%s duration_ms=%lu fps_avg=%lu fps_min=%lu cpu_avg=%lu cpu_max=%lu\n",
           benchmark->mode == APP_PERF_BENCHMARK_STATIC ? "static" : "data",
           (unsigned long)lv_tick_elaps(benchmark->started_at),
           (unsigned long)(benchmark->fps_sum / benchmark->sample_count),
           (unsigned long)benchmark->fps_min,
           (unsigned long)(benchmark->cpu_sum / benchmark->sample_count),
           (unsigned long)benchmark->cpu_max);

    if(benchmark->data_timer != NULL)
    {
        lv_timer_delete(benchmark->data_timer);
        benchmark->data_timer = NULL;
    }

    return true;
}
