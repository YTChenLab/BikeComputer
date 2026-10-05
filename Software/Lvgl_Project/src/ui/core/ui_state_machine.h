#ifndef UI_STATE_MACHINE_H
#define UI_STATE_MACHINE_H

#include <stdbool.h>

typedef enum
{
    UI_RIDE_STATE_IDLE = 0,
    UI_RIDE_STATE_HOME,
    UI_RIDE_STATE_PRE_RIDE,
    UI_RIDE_STATE_GPS_READY,
    UI_RIDE_STATE_RIDING,
    UI_RIDE_STATE_PAUSED,
    UI_RIDE_STATE_FINISHING,
    UI_RIDE_STATE_STOPPING,
    UI_RIDE_STATE_SAVING,
    UI_RIDE_STATE_UNSAVED,
    UI_RIDE_STATE_SUMMARY
} ui_ride_state_t;

typedef enum
{
    UI_RIDE_EVENT_BOOT_COMPLETE = 0,
    UI_RIDE_EVENT_START_REQUEST,
    UI_RIDE_EVENT_GPS_READY,
    UI_RIDE_EVENT_GO,
    UI_RIDE_EVENT_PAUSE,
    UI_RIDE_EVENT_RESUME,
    UI_RIDE_EVENT_FINISH_REQUEST,
    UI_RIDE_EVENT_FINISH_CANCEL,
    UI_RIDE_EVENT_FINISH_CONFIRMED,
    UI_RIDE_EVENT_RECORDING_STOPPED,
    UI_RIDE_EVENT_SAVE_RETRY,
    UI_RIDE_EVENT_SAVE_SUCCESS,
    UI_RIDE_EVENT_SAVE_FAILED,
    UI_RIDE_EVENT_SUMMARY_DISMISSED,
    UI_RIDE_EVENT_RESET,
    UI_RIDE_EVENT_PREPARE_CANCEL
} ui_ride_event_t;

typedef void (*ui_state_changed_cb_t)(ui_ride_state_t previous,
                                      ui_ride_state_t current,
                                      ui_ride_event_t event,
                                      void *user_data);

void ui_state_machine_init(void);
bool ui_state_machine_dispatch(ui_ride_event_t event);
ui_ride_state_t ui_state_machine_state(void);
bool ui_state_machine_is_riding(void);
bool ui_state_machine_is_paused(void);
void ui_state_machine_set_changed_callback(ui_state_changed_cb_t callback,
                                           void *user_data);
const char *ui_state_machine_state_name(ui_ride_state_t state);

#endif /* UI_STATE_MACHINE_H */
