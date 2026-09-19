#ifndef UI_SYSTEM_CONTROL_SHADE_H
#define UI_SYSTEM_CONTROL_SHADE_H

#include <stdbool.h>

typedef enum
{
    UI_SYSTEM_CONTROL_SHADE_CLOSED = 0,
    UI_SYSTEM_CONTROL_SHADE_COMPACT,
    UI_SYSTEM_CONTROL_SHADE_EXPANDED
} ui_system_control_shade_state_t;

void ui_system_control_shade_init(void);
ui_system_control_shade_state_t ui_system_control_shade_state(void);
bool ui_system_control_shade_open_compact(void);
bool ui_system_control_shade_expand(void);
bool ui_system_control_shade_set_compact(void);
bool ui_system_control_shade_close(void);

#endif /* UI_SYSTEM_CONTROL_SHADE_H */
