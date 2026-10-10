#include "ui_ride_controller.h"

#include "ui_event_bus.h"
#include "ui_state_machine.h"

#include <stdint.h>
#include <stddef.h>

static int32_t s_subscription_id = -1;

static void publish_ride_event(ui_event_type_t type, uint32_t timeout_ms,
                               int32_t value)
{
    ui_event_t event = {0};
    event.type = type;
    event.priority = UI_EVENT_PRIORITY_P2_RIDE_STATE;
    event.dedupe_key = 0x2000U + (uint32_t)type;
    event.timeout_ms = timeout_ms;
    event.value = value;
    (void)ui_event_bus_publish(&event);
}

static void handle_event(const ui_event_t *event, void *user_data)
{
    (void)user_data;
    switch(event->type) {
        case UI_EVENT_BOOT_COMPLETE:
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_BOOT_COMPLETE);
            break;
        case UI_EVENT_RIDE_START_REQUESTED:
            if(ui_state_machine_dispatch(UI_RIDE_EVENT_START_REQUEST))
                publish_ride_event(UI_EVENT_GPS_SEARCHING, 0, 0);
            break;
        case UI_EVENT_GPS_READY:
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_GPS_READY);
            break;
        case UI_EVENT_RIDE_GO:
            if(ui_state_machine_dispatch(UI_RIDE_EVENT_GO))
                publish_ride_event(UI_EVENT_RIDE_STARTED, 1800, 0);
            break;
        case UI_EVENT_RIDE_PAUSE_REQUESTED:
            if(ui_state_machine_dispatch(UI_RIDE_EVENT_PAUSE))
                publish_ride_event(UI_EVENT_RIDE_PAUSED, 1800,
                                   event->value);
            break;
        case UI_EVENT_RIDE_RESUME_REQUESTED:
            if(ui_state_machine_dispatch(UI_RIDE_EVENT_RESUME))
                publish_ride_event(UI_EVENT_RIDE_RESUMED, 1800,
                                   event->value);
            break;
        case UI_EVENT_RIDE_FINISH_REQUESTED:
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_FINISH_REQUEST);
            break;
        case UI_EVENT_RIDE_FINISH_CONFIRMED:
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_FINISH_CONFIRMED);
            break;
        case UI_EVENT_RIDE_FINISH_CANCELLED:
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_FINISH_CANCEL);
            break;
        case UI_EVENT_RIDE_SAVE_SUCCESS:
            if(ui_state_machine_dispatch(UI_RIDE_EVENT_SAVE_SUCCESS))
                publish_ride_event(UI_EVENT_RIDE_SUMMARY_READY, 0, 0);
            break;
        case UI_EVENT_RIDE_SAVE_STARTED:
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_RECORDING_STOPPED);
            break;
        case UI_EVENT_STORAGE_RETRY:
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_SAVE_RETRY);
            break;
        case UI_EVENT_RIDE_SAVE_FAILED:
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_SAVE_FAILED);
            break;
        case UI_EVENT_RIDE_SUMMARY_DISMISSED:
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_SUMMARY_DISMISSED);
            break;
        default:
            break;
    }
}

void ui_ride_controller_init(void)
{
    uint64_t mask =
        UI_EVENT_MASK(UI_EVENT_BOOT_COMPLETE) |
        UI_EVENT_MASK(UI_EVENT_RIDE_START_REQUESTED) |
        UI_EVENT_MASK(UI_EVENT_GPS_READY) |
        UI_EVENT_MASK(UI_EVENT_RIDE_GO) |
        UI_EVENT_MASK(UI_EVENT_RIDE_PAUSE_REQUESTED) |
        UI_EVENT_MASK(UI_EVENT_RIDE_RESUME_REQUESTED) |
        UI_EVENT_MASK(UI_EVENT_RIDE_FINISH_REQUESTED) |
        UI_EVENT_MASK(UI_EVENT_RIDE_FINISH_CONFIRMED) |
        UI_EVENT_MASK(UI_EVENT_RIDE_FINISH_CANCELLED) |
        UI_EVENT_MASK(UI_EVENT_RIDE_SAVE_SUCCESS) |
        UI_EVENT_MASK(UI_EVENT_RIDE_SAVE_STARTED) |
        UI_EVENT_MASK(UI_EVENT_STORAGE_RETRY) |
        UI_EVENT_MASK(UI_EVENT_RIDE_SAVE_FAILED) |
        UI_EVENT_MASK(UI_EVENT_RIDE_SUMMARY_DISMISSED);
    s_subscription_id = ui_event_bus_subscribe(mask, handle_event, NULL);
}

void ui_ride_controller_deinit(void)
{
    if(s_subscription_id > 0)
        (void)ui_event_bus_unsubscribe(s_subscription_id);
    s_subscription_id = -1;
}
