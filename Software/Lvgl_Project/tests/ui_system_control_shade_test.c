#include "ui/core/ui_system_control_shade.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    ui_system_control_shade_init();
    assert(ui_system_control_shade_state() ==
           UI_SYSTEM_CONTROL_SHADE_CLOSED);
    assert(!ui_system_control_shade_expand());
    assert(!ui_system_control_shade_set_compact());

    assert(ui_system_control_shade_open_compact());
    assert(ui_system_control_shade_state() ==
           UI_SYSTEM_CONTROL_SHADE_COMPACT);
    assert(!ui_system_control_shade_open_compact());

    assert(ui_system_control_shade_expand());
    assert(ui_system_control_shade_state() ==
           UI_SYSTEM_CONTROL_SHADE_EXPANDED);
    assert(!ui_system_control_shade_expand());

    assert(ui_system_control_shade_set_compact());
    assert(ui_system_control_shade_state() ==
           UI_SYSTEM_CONTROL_SHADE_COMPACT);
    assert(!ui_system_control_shade_set_compact());

    assert(ui_system_control_shade_close());
    assert(ui_system_control_shade_state() ==
           UI_SYSTEM_CONTROL_SHADE_CLOSED);
    assert(ui_system_control_shade_open_compact());
    assert(ui_system_control_shade_close());
    assert(ui_system_control_shade_state() ==
           UI_SYSTEM_CONTROL_SHADE_CLOSED);
    assert(!ui_system_control_shade_close());

    puts("ui_system_control_shade_test: PASS");
    return 0;
}
