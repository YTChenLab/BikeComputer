#ifndef UI_PLATFORM_INTERNAL_H
#define UI_PLATFORM_INTERNAL_H
#include <stdbool.h>
#include <stdint.h>
#include "ui/models/ui_weather_model.h"

/* Selected by the build, never by OS conditionals in page/algorithm code. */
void ui_platform_default_lock(void);
void ui_platform_default_unlock(void);
void ui_platform_default_lock_reset(void);
void ui_platform_check_lock_context(void);
const void *ui_platform_background_load(void);
void ui_platform_background_release(const void *source);
void ui_platform_initial_weather(ui_weather_data_t *weather);
typedef struct { int year, month, day, hour, minute; } ui_calendar_date_t;
bool ui_platform_local_date(int64_t utc_seconds, ui_calendar_date_t *date);
#endif
