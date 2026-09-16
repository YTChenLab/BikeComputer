#ifndef UI_AUTO_PAUSE_H
#define UI_AUTO_PAUSE_H

#include "ui_state_machine.h"

#include <stdbool.h>
#include <stdint.h>

#define UI_AUTO_PAUSE_STOP_SPEED_KMH    1.0f
#define UI_AUTO_PAUSE_RESUME_SPEED_KMH  3.0f
#define UI_AUTO_PAUSE_STOP_DELAY_MS  3000U
#define UI_AUTO_PAUSE_RESUME_DELAY_MS 1000U

typedef enum
{
    UI_AUTO_PAUSE_ACTION_NONE = 0,
    UI_AUTO_PAUSE_ACTION_PAUSE,
    UI_AUTO_PAUSE_ACTION_RESUME
} ui_auto_pause_action_t;

typedef struct
{
    float speed_kmh;
    uint32_t stopped_ms;
    uint32_t moving_ms;
    bool speed_valid;
    bool automatically_paused;
    bool command_pending;
} ui_auto_pause_t;

void ui_auto_pause_init(ui_auto_pause_t *controller);
void ui_auto_pause_set_speed(ui_auto_pause_t *controller, float speed_kmh);
ui_auto_pause_action_t ui_auto_pause_tick(ui_auto_pause_t *controller,
                                          ui_ride_state_t ride_state,
                                          uint32_t elapsed_ms);
void ui_auto_pause_complete(ui_auto_pause_t *controller,
                            ui_auto_pause_action_t action, bool success);
void ui_auto_pause_manual_override(ui_auto_pause_t *controller);
bool ui_auto_pause_is_active(const ui_auto_pause_t *controller);

#endif /* UI_AUTO_PAUSE_H */
