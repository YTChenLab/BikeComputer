#include "ui_system_control_shade.h"

static ui_system_control_shade_state_t s_state;

void ui_system_control_shade_init(void)
{
    s_state = UI_SYSTEM_CONTROL_SHADE_CLOSED;
}

ui_system_control_shade_state_t ui_system_control_shade_state(void)
{
    return s_state;
}

bool ui_system_control_shade_open_compact(void)
{
    if(s_state != UI_SYSTEM_CONTROL_SHADE_CLOSED) return false;
    s_state = UI_SYSTEM_CONTROL_SHADE_COMPACT;
    return true;
}

bool ui_system_control_shade_expand(void)
{
    if(s_state != UI_SYSTEM_CONTROL_SHADE_COMPACT) return false;
    s_state = UI_SYSTEM_CONTROL_SHADE_EXPANDED;
    return true;
}

bool ui_system_control_shade_set_compact(void)
{
    if(s_state != UI_SYSTEM_CONTROL_SHADE_EXPANDED) return false;
    s_state = UI_SYSTEM_CONTROL_SHADE_COMPACT;
    return true;
}

bool ui_system_control_shade_close(void)
{
    if(s_state == UI_SYSTEM_CONTROL_SHADE_CLOSED) return false;
    s_state = UI_SYSTEM_CONTROL_SHADE_CLOSED;
    return true;
}
