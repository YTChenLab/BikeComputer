#include "ui_state_machine.h"

#include <stddef.h>

typedef struct
{
    ui_ride_state_t state;
    ui_ride_state_t before_finishing;
    ui_state_changed_cb_t changed;
    void *changed_user_data;
} ui_state_context_t;

static ui_state_context_t s_state;

void ui_state_machine_init(void)
{
    s_state.state = UI_RIDE_STATE_IDLE;
    s_state.before_finishing = UI_RIDE_STATE_RIDING;
    s_state.changed = NULL;
    s_state.changed_user_data = NULL;
}

bool ui_state_machine_dispatch(ui_ride_event_t event)
{
    ui_ride_state_t previous = s_state.state;
    ui_ride_state_t next = previous;

    if(event == UI_RIDE_EVENT_RESET) next = UI_RIDE_STATE_IDLE;
    else switch(previous) {
        case UI_RIDE_STATE_IDLE:
            if(event == UI_RIDE_EVENT_BOOT_COMPLETE) next = UI_RIDE_STATE_HOME;
            else return false;
            break;
        case UI_RIDE_STATE_HOME:
            if(event == UI_RIDE_EVENT_START_REQUEST) next = UI_RIDE_STATE_PRE_RIDE;
            else if(event == UI_RIDE_EVENT_SAVE_RETRY) next = UI_RIDE_STATE_SAVING;
            else return false;
            break;
        case UI_RIDE_STATE_PRE_RIDE:
            if(event == UI_RIDE_EVENT_GPS_READY) next = UI_RIDE_STATE_GPS_READY;
            else if(event == UI_RIDE_EVENT_PREPARE_CANCEL) next = UI_RIDE_STATE_HOME;
            else return false;
            break;
        case UI_RIDE_STATE_GPS_READY:
            if(event == UI_RIDE_EVENT_GO) next = UI_RIDE_STATE_RIDING;
            else if(event == UI_RIDE_EVENT_PREPARE_CANCEL) next = UI_RIDE_STATE_HOME;
            else return false;
            break;
        case UI_RIDE_STATE_RIDING:
            if(event == UI_RIDE_EVENT_PAUSE) next = UI_RIDE_STATE_PAUSED;
            else if(event == UI_RIDE_EVENT_FINISH_REQUEST) {
                s_state.before_finishing = previous;
                next = UI_RIDE_STATE_FINISHING;
            }
            else return false;
            break;
        case UI_RIDE_STATE_PAUSED:
            if(event == UI_RIDE_EVENT_RESUME) next = UI_RIDE_STATE_RIDING;
            else if(event == UI_RIDE_EVENT_FINISH_REQUEST) {
                s_state.before_finishing = previous;
                next = UI_RIDE_STATE_FINISHING;
            }
            else return false;
            break;
        case UI_RIDE_STATE_FINISHING:
            if(event == UI_RIDE_EVENT_FINISH_CANCEL) next = s_state.before_finishing;
            else if(event == UI_RIDE_EVENT_FINISH_CONFIRMED) next = UI_RIDE_STATE_STOPPING;
            else return false;
            break;
        case UI_RIDE_STATE_STOPPING:
            if(event == UI_RIDE_EVENT_FINISH_CANCEL) next = s_state.before_finishing;
            else if(event == UI_RIDE_EVENT_RECORDING_STOPPED) next = UI_RIDE_STATE_SAVING;
            else return false;
            break;
        case UI_RIDE_STATE_SAVING:
            if(event == UI_RIDE_EVENT_SAVE_FAILED) next = UI_RIDE_STATE_UNSAVED;
            else if(event == UI_RIDE_EVENT_SAVE_SUCCESS) next = UI_RIDE_STATE_SUMMARY;
            else return false;
            break;
        case UI_RIDE_STATE_UNSAVED:
            if(event == UI_RIDE_EVENT_SAVE_RETRY) next = UI_RIDE_STATE_SAVING;
            else if(event == UI_RIDE_EVENT_SAVE_SUCCESS) next = UI_RIDE_STATE_SUMMARY;
            else if(event == UI_RIDE_EVENT_SUMMARY_DISMISSED) next = UI_RIDE_STATE_HOME;
            else return false;
            break;
        case UI_RIDE_STATE_SUMMARY:
            if(event == UI_RIDE_EVENT_SUMMARY_DISMISSED) next = UI_RIDE_STATE_HOME;
            else return false;
            break;
        default:
            return false;
    }

    s_state.state = next;
    if(s_state.changed != NULL) {
        s_state.changed(previous, next, event, s_state.changed_user_data);
    }
    return true;
}

ui_ride_state_t ui_state_machine_state(void)
{
    return s_state.state;
}

bool ui_state_machine_is_riding(void)
{
    return s_state.state == UI_RIDE_STATE_RIDING ||
           s_state.state == UI_RIDE_STATE_PAUSED ||
           s_state.state == UI_RIDE_STATE_FINISHING ||
           s_state.state == UI_RIDE_STATE_STOPPING;
}

bool ui_state_machine_is_paused(void)
{
    return s_state.state == UI_RIDE_STATE_PAUSED;
}

void ui_state_machine_set_changed_callback(ui_state_changed_cb_t callback,
                                           void *user_data)
{
    s_state.changed = callback;
    s_state.changed_user_data = user_data;
}

const char *ui_state_machine_state_name(ui_ride_state_t state)
{
    static const char * const names[] = {
        "IDLE", "HOME", "PRE_RIDE", "GPS_READY", "RIDING",
        "PAUSED", "FINISHING", "STOPPING", "SAVING", "UNSAVED", "SUMMARY"
    };
    return state <= UI_RIDE_STATE_SUMMARY ? names[state] : "UNKNOWN";
}
