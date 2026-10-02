#include "ui_device_info.h"
#include "ui/core/ui_concurrency.h"
#include <stddef.h>
static ui_device_info_t s_pending;
static bool s_pending_valid;
bool ui_device_info_submit(const ui_device_info_t *info)
{
    if(!info) return false;
    ui_concurrency_lock(); s_pending=*info;
    s_pending.device_model[47]=s_pending.hardware_target[47]=s_pending.health[47]=0;
    s_pending.firmware_version[31]=0; s_pending_valid=true;
    ui_concurrency_unlock(); return true;
}
bool ui_device_info_process(ui_device_info_t *info)
{
    if(!info) return false;
    ui_concurrency_lock(); bool valid=s_pending_valid;
    if(valid) { *info=s_pending; s_pending_valid=false; }
    ui_concurrency_unlock(); return valid;
}
