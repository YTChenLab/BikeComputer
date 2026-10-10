#include "platform/ui_platform_internal.h"
#include <time.h>
bool ui_platform_local_date(int64_t seconds, ui_calendar_date_t *date)
{
    time_t value = (time_t)seconds;
    struct tm local_time;
    if(!date || seconds <= 0) return false;
#if defined(_WIN32)
    if(localtime_s(&local_time, &value) != 0) return false;
#else
    if(localtime_r(&value, &local_time) == NULL) return false;
#endif
    *date = (ui_calendar_date_t){local_time.tm_year + 1900, local_time.tm_mon + 1,
        local_time.tm_mday, local_time.tm_hour, local_time.tm_min};
    return true;
}
