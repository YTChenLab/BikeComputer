#include "ui_auto_pause.h"

#include <string.h>

static uint32_t saturating_add(uint32_t value, uint32_t increment,
                               uint32_t maximum)
{
    if(value >= maximum || increment >= maximum - value) return maximum;
    return value + increment;
}

void ui_auto_pause_init(ui_auto_pause_t *controller)
{
    if(controller == NULL) return;
    memset(controller, 0, sizeof(*controller));
}

void ui_auto_pause_set_speed(ui_auto_pause_t *controller, float speed_kmh)
{
    if(controller == NULL) return;
    controller->speed_kmh = speed_kmh < 0.0f ? 0.0f : speed_kmh;
    controller->speed_valid = true;
}

ui_auto_pause_action_t ui_auto_pause_tick(ui_auto_pause_t *controller,
                                          ui_ride_state_t ride_state,
                                          uint32_t elapsed_ms)
{
    if(controller == NULL || elapsed_ms == 0U) return UI_AUTO_PAUSE_ACTION_NONE;
    if(!controller->speed_valid) {
        controller->stopped_ms = controller->moving_ms = 0U;
        return UI_AUTO_PAUSE_ACTION_NONE;
    }

    if(ride_state == UI_RIDE_STATE_RIDING) {
        controller->moving_ms = 0U;
        if(controller->speed_kmh <= UI_AUTO_PAUSE_STOP_SPEED_KMH) {
            controller->stopped_ms = saturating_add(
                controller->stopped_ms, elapsed_ms,
                UI_AUTO_PAUSE_STOP_DELAY_MS);
            if(controller->stopped_ms >= UI_AUTO_PAUSE_STOP_DELAY_MS &&
               !controller->command_pending) {
                controller->command_pending = true;
                controller->stopped_ms = 0U;
                return UI_AUTO_PAUSE_ACTION_PAUSE;
            }
        }
        else controller->stopped_ms = 0U;
    }
    else if(ride_state == UI_RIDE_STATE_PAUSED &&
            controller->automatically_paused) {
        controller->stopped_ms = 0U;
        if(controller->speed_kmh >= UI_AUTO_PAUSE_RESUME_SPEED_KMH) {
            controller->moving_ms = saturating_add(
                controller->moving_ms, elapsed_ms,
                UI_AUTO_PAUSE_RESUME_DELAY_MS);
            if(controller->moving_ms >= UI_AUTO_PAUSE_RESUME_DELAY_MS &&
               !controller->command_pending) {
                controller->command_pending = true;
                controller->moving_ms = 0U;
                return UI_AUTO_PAUSE_ACTION_RESUME;
            }
        }
        else controller->moving_ms = 0U;
    }
    else {
        controller->stopped_ms = 0U;
        controller->moving_ms = 0U;
        if(ride_state != UI_RIDE_STATE_PAUSED) {
            controller->automatically_paused = false;
            controller->command_pending = false;
        }
    }
    return UI_AUTO_PAUSE_ACTION_NONE;
}

void ui_auto_pause_complete(ui_auto_pause_t *controller,
                            ui_auto_pause_action_t action, bool success)
{
    if(controller == NULL) return;
    controller->command_pending = false;
    controller->stopped_ms = 0U;
    controller->moving_ms = 0U;
    if(!success) return;
    if(action == UI_AUTO_PAUSE_ACTION_PAUSE)
        controller->automatically_paused = true;
    else if(action == UI_AUTO_PAUSE_ACTION_RESUME)
        controller->automatically_paused = false;
}

void ui_auto_pause_manual_override(ui_auto_pause_t *controller)
{
    if(controller == NULL) return;
    controller->automatically_paused = false;
    controller->command_pending = false;
    controller->stopped_ms = 0U;
    controller->moving_ms = 0U;
}

bool ui_auto_pause_is_active(const ui_auto_pause_t *controller)
{
    return controller != NULL && controller->automatically_paused;
}
