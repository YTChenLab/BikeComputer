#include "platform/ui_platform_internal.h"
#include "ui/core/ui_runtime.h"
#include "services/ui_navigation_service.h"
#include <stdlib.h>
#include <string.h>

/* A missing mutex is a startup error. There is deliberately no spinlock. */
void ui_platform_default_lock(void)
{ ui_runtime_fault("STM32 mailbox mutex was not registered"); abort(); }
void ui_platform_default_unlock(void) { ui_platform_default_lock(); }
void ui_platform_default_lock_reset(void) {}
void ui_platform_check_lock_context(void)
{ if(ui_runtime_is_isr()) { ui_runtime_fault("Mailbox API called from ISR"); abort(); } }
const void *ui_platform_background_load(void) { return NULL; }
void ui_platform_background_release(const void *source) { (void)source; }
void ui_platform_initial_weather(ui_weather_data_t *weather)
{ memset(weather, 0, sizeof(*weather)); weather->quality_known = true; weather->version = 1; }
void ui_navigation_service_register_demo_step(void (*callback)(void)) { (void)callback; }
bool ui_navigation_service_demo_step(void) { return false; }
void ui_navigation_service_set_demo_enabled(bool enabled) { (void)enabled; }
bool ui_navigation_service_demo_enabled(void) { return false; }

static int s_timezone_minutes = 480; /* Product local time: UTC+08:00. */
bool ui_stm32_datetime_set_offset_minutes(int minutes)
{ if(minutes < -840 || minutes > 840 || !ui_runtime_require_ui()) return false; s_timezone_minutes = minutes; return true; }
bool ui_platform_local_date(int64_t seconds, ui_calendar_date_t *date)
{
    if(!date || seconds <= 0 || seconds > INT64_C(253402250399)) return false;
    seconds += (int64_t)s_timezone_minutes * 60;
    if(seconds < 0) return false;
    int64_t days = seconds / 86400;
    int year = 1970;
    for(;; ++year) {
        int length = 365 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        if(days < length) break;
        days -= length;
    }
    static const int months[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int month = 0;
    for(; month < 11; ++month) {
        int length = months[month] + (month == 1 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        if(days < length) break;
        days -= length;
    }
    *date = (ui_calendar_date_t){year, month + 1, (int)days + 1,
        (int)(seconds / 3600 % 24), (int)(seconds / 60 % 60)};
    return true;
}
