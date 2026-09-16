#include "ui/core/ui_auto_pause.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    ui_auto_pause_t controller;
    ui_auto_pause_init(&controller);

    ui_auto_pause_set_speed(&controller, 28.0f);
    assert(ui_auto_pause_tick(&controller, UI_RIDE_STATE_RIDING,
                              UI_AUTO_PAUSE_STOP_DELAY_MS) ==
           UI_AUTO_PAUSE_ACTION_NONE);

    ui_auto_pause_set_speed(&controller, 0.0f);
    assert(ui_auto_pause_tick(&controller, UI_RIDE_STATE_RIDING,
                              UI_AUTO_PAUSE_STOP_DELAY_MS - 1U) ==
           UI_AUTO_PAUSE_ACTION_NONE);
    assert(ui_auto_pause_tick(&controller, UI_RIDE_STATE_RIDING, 1U) ==
           UI_AUTO_PAUSE_ACTION_PAUSE);
    ui_auto_pause_complete(&controller, UI_AUTO_PAUSE_ACTION_PAUSE, true);
    assert(ui_auto_pause_is_active(&controller));

    ui_auto_pause_set_speed(&controller, 2.5f);
    assert(ui_auto_pause_tick(&controller, UI_RIDE_STATE_PAUSED,
                              UI_AUTO_PAUSE_RESUME_DELAY_MS) ==
           UI_AUTO_PAUSE_ACTION_NONE);
    ui_auto_pause_set_speed(&controller, 3.0f);
    assert(ui_auto_pause_tick(&controller, UI_RIDE_STATE_PAUSED,
                              UI_AUTO_PAUSE_RESUME_DELAY_MS - 1U) ==
           UI_AUTO_PAUSE_ACTION_NONE);
    assert(ui_auto_pause_tick(&controller, UI_RIDE_STATE_PAUSED, 1U) ==
           UI_AUTO_PAUSE_ACTION_RESUME);
    ui_auto_pause_complete(&controller, UI_AUTO_PAUSE_ACTION_RESUME, true);
    assert(!ui_auto_pause_is_active(&controller));

    ui_auto_pause_manual_override(&controller);
    ui_auto_pause_set_speed(&controller, 8.0f);
    assert(ui_auto_pause_tick(&controller, UI_RIDE_STATE_PAUSED,
                              UI_AUTO_PAUSE_RESUME_DELAY_MS) ==
           UI_AUTO_PAUSE_ACTION_NONE);

    puts("ui_auto_pause_test: PASS");
    return 0;
}
