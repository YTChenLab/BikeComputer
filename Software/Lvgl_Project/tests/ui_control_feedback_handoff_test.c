#include "services/ui_command_service.h"
#include "ui/core/ui_control_feedback_ownership.h"
#include "ui/core/ui_dock.h"
#include "ui/core/ui_dock_controller.h"
#include "ui/core/ui_event_bus.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void publish_command_event(ui_event_type_t type,
                                  const ui_command_result_t *result)
{
    ui_event_t event = {0};
    event.type = type;
    event.priority = UI_EVENT_PRIORITY_P3_USER_ACTION;
    event.source_id = result->id;
    event.dedupe_key = 0xC001U;
    event.value = (int32_t)result->type;
    event.timeout_ms = type == UI_EVENT_COMMAND_SUCCESS ? 1200U : 0U;
    snprintf(event.title, sizeof(event.title), "%s",
             type == UI_EVENT_COMMAND_PENDING ? "PROCESSING" : "DONE");
    snprintf(event.message, sizeof(event.message), "DEVICE REQUEST");
    assert(ui_event_bus_publish(&event));
    assert(ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX) == 1U);
}

int main(void)
{
    ui_event_bus_init();
    ui_dock_init();
    ui_dock_controller_init();
    ui_control_feedback_ownership_init();

    assert(ui_control_feedback_ownership_manages(
        UI_COMMAND_WIFI_SET_ENABLED));
    assert(ui_control_feedback_ownership_manages(
        UI_COMMAND_BLUETOOTH_SET_ENABLED));
    assert(ui_control_feedback_ownership_manages(
        UI_COMMAND_SOUND_SET_LEVEL));

    ui_command_result_t wifi = {
        .id = 10U,
        .type = UI_COMMAND_WIFI_SET_ENABLED,
        .status = UI_COMMAND_STATUS_PENDING
    };
    assert(ui_control_feedback_ownership_begin(&wifi, true) ==
           UI_CONTROL_FEEDBACK_OWNER_TOP);
    ui_control_feedback_state_t state;
    assert(ui_control_feedback_ownership_handoff_to_bottom(&state));
    assert(state.command_id == wifi.id);
    assert(state.owner == UI_CONTROL_FEEDBACK_OWNER_BOTTOM);
    publish_command_event(UI_EVENT_COMMAND_PENDING, &wifi);
    assert(ui_dock_current() != NULL);
    assert(strcmp(ui_dock_current()->title, "PROCESSING") == 0);

    assert(ui_control_feedback_ownership_reclaim_to_top(&state));
    assert(ui_dock_controller_hide_command_feedback());
    assert(!ui_dock_is_visible());

    ui_command_result_t bluetooth = {
        .id = 11U,
        .type = UI_COMMAND_BLUETOOTH_SET_ENABLED,
        .status = UI_COMMAND_STATUS_PENDING
    };
    assert(ui_control_feedback_ownership_begin(&bluetooth, false) ==
           UI_CONTROL_FEEDBACK_OWNER_BOTTOM);
    publish_command_event(UI_EVENT_COMMAND_PENDING, &bluetooth);
    assert(ui_dock_is_visible());

    ui_event_t safety = {0};
    safety.type = UI_EVENT_SENSOR_DISCONNECTED;
    safety.priority = UI_EVENT_PRIORITY_P1_RIDING_SAFETY;
    assert(ui_event_bus_publish(&safety));
    assert(ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX) == 1U);
    assert(ui_dock_current() != NULL);
    assert(ui_dock_current()->priority == UI_DOCK_PRIORITY_P1_RIDING_SAFETY);
    assert(ui_dock_restore_depth() == 1U);

    assert(ui_dock_controller_hide_command_feedback());
    assert(ui_dock_restore_depth() == 0U);
    assert(ui_dock_hide(ui_dock_current()->id));
    assert(!ui_dock_is_visible());
    ui_control_feedback_ownership_complete(&bluetooth);

    wifi.id = 12U;
    assert(ui_control_feedback_ownership_begin(&wifi, false) ==
           UI_CONTROL_FEEDBACK_OWNER_BOTTOM);
    publish_command_event(UI_EVENT_COMMAND_PENDING, &wifi);
    wifi.status = UI_COMMAND_STATUS_SUCCESS;
    assert(ui_control_feedback_ownership_owner_for(&wifi) ==
           UI_CONTROL_FEEDBACK_OWNER_BOTTOM);
    publish_command_event(UI_EVENT_COMMAND_SUCCESS, &wifi);
    ui_control_feedback_ownership_complete(&wifi);
    assert(strcmp(ui_dock_current()->title, "DONE") == 0);
    ui_dock_controller_tick(1200U);
    assert(!ui_dock_is_visible());

    ui_dock_controller_deinit();
    puts("ui_control_feedback_handoff_test: PASS");
    return 0;
}
