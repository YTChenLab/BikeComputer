#include "ui/core/ui_dock.h"
#include "ui/core/ui_dock_controller.h"
#include "ui/core/ui_concurrency.h"
#include "ui/core/ui_event_bridge.h"
#include "ui/core/ui_event_bus.h"
#include "ui/core/ui_ride_controller.h"
#include "ui/core/ui_state_machine.h"
#include "ui/models/ui_ride_data_model.h"
#include "platform/pc/ui_pc_services.h"
#include "platform/stm32/ui_stm32_services.h"
#include "services/ui_system_services.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t s_action_card_id;
static ui_dock_interaction_t s_action_interaction;
static int32_t s_action_value;
static ui_event_type_t s_event_order[32];
static int32_t s_event_values[32];
static uint8_t s_event_count;
static ui_ride_data_t s_last_ride_data;
static uint32_t s_last_ride_version;
static uint8_t s_ride_observer_calls;

static void action_callback(uint32_t card_id,
                            ui_dock_interaction_t interaction,
                            int32_t value, void *context)
{
    (void)context;
    s_action_card_id = card_id;
    s_action_interaction = interaction;
    s_action_value = value;
}

static void event_callback(const ui_event_t *event, void *context)
{
    (void)context;
    if(s_event_count >= 32) return;
    s_event_order[s_event_count] = event->type;
    s_event_values[s_event_count] = event->value;
    s_event_count++;
}

static void ride_data_callback(const ui_ride_data_t *data, uint32_t version,
                               void *context)
{
    (void)context;
    s_last_ride_data = *data;
    s_last_ride_version = version;
    s_ride_observer_calls++;
}

static ui_event_t event_value(ui_event_type_t type,
                              ui_event_priority_t priority,
                              uint32_t key, int32_t value)
{
    ui_event_t event = {0};
    event.type = type;
    event.priority = priority;
    event.dedupe_key = key;
    event.value = value;
    return event;
}

static ui_dock_card_t card(uint32_t id, ui_dock_type_t type,
                           ui_dock_priority_t priority, const char *title)
{
    ui_dock_card_t value = {0};
    value.id = id;
    value.type = type;
    value.priority = priority;
    value.dismissible = true;
    snprintf(value.title, sizeof(value.title), "%s", title);
    return value;
}

static void test_state_machine(void)
{
    ui_state_machine_init();
    assert(ui_state_machine_state() == UI_RIDE_STATE_IDLE);
    assert(ui_state_machine_dispatch(UI_RIDE_EVENT_BOOT_COMPLETE));
    assert(ui_state_machine_dispatch(UI_RIDE_EVENT_START_REQUEST));
    assert(ui_state_machine_dispatch(UI_RIDE_EVENT_GPS_READY));
    assert(ui_state_machine_dispatch(UI_RIDE_EVENT_GO));
    assert(ui_state_machine_is_riding());
    assert(ui_state_machine_dispatch(UI_RIDE_EVENT_PAUSE));
    assert(ui_state_machine_is_paused());
    assert(ui_state_machine_dispatch(UI_RIDE_EVENT_FINISH_REQUEST));
    assert(ui_state_machine_dispatch(UI_RIDE_EVENT_FINISH_CONFIRMED));
    assert(ui_state_machine_dispatch(UI_RIDE_EVENT_SAVE_SUCCESS));
    assert(ui_state_machine_state() == UI_RIDE_STATE_SUMMARY);
    assert(ui_state_machine_dispatch(UI_RIDE_EVENT_SUMMARY_DISMISSED));
    assert(ui_state_machine_state() == UI_RIDE_STATE_HOME);
    assert(!ui_state_machine_dispatch(UI_RIDE_EVENT_PAUSE));
}

static void test_dock_priority_and_restore(void)
{
    ui_dock_card_t normal = card(10, UI_DOCK_TYPE_INFO,
                                 UI_DOCK_PRIORITY_P4_NORMAL, "normal");
    ui_dock_card_t critical = card(20, UI_DOCK_TYPE_CONFIRM,
                                   UI_DOCK_PRIORITY_P0_CRITICAL, "critical");
    ui_dock_card_t action = card(30, UI_DOCK_TYPE_ACTION,
                                 UI_DOCK_PRIORITY_P3_USER_ACTION, "action");

    ui_dock_init();
    assert(ui_dock_show(&normal));
    assert(ui_dock_show(&critical));
    assert(ui_dock_restore_depth() == 1);
    assert(ui_dock_show(&action));
    assert(ui_dock_pending_count() == 1);
    assert(ui_dock_current()->id == critical.id);

    assert(ui_dock_hide(critical.id));
    assert(ui_dock_current()->id == action.id);
    assert(ui_dock_restore_depth() == 1);
    assert(ui_dock_hide(action.id));
    assert(ui_dock_current()->id == normal.id);
    assert(ui_dock_hide(normal.id));
    assert(!ui_dock_is_visible());
}

static void test_dock_update_and_replace(void)
{
    ui_dock_card_t media = card(40, UI_DOCK_TYPE_MEDIA,
                                UI_DOCK_PRIORITY_P4_NORMAL, "track A");
    ui_dock_init();
    assert(ui_dock_show(&media));
    snprintf(media.title, sizeof(media.title), "%s", "track B");
    assert(ui_dock_update(&media));
    assert(strcmp(ui_dock_current()->title, "track B") == 0);

    ui_dock_card_t navigation = card(50, UI_DOCK_TYPE_NAVIGATION,
                                     UI_DOCK_PRIORITY_P1_RIDING_SAFETY,
                                     "turn right");
    assert(ui_dock_replace(&navigation));
    assert(ui_dock_current()->id == navigation.id);
    assert(ui_dock_restore_depth() == 0);
}

static void test_dock_interactions(void)
{
    ui_dock_card_t control = card(60, UI_DOCK_TYPE_CONTROL,
                                  UI_DOCK_PRIORITY_P3_USER_ACTION,
                                  "volume");
    control.minimum = 0;
    control.maximum = 100;
    control.value = 50;
    control.action_cb = action_callback;
    ui_dock_init();
    assert(ui_dock_show(&control));
    assert(ui_dock_invoke(control.id, UI_DOCK_INTERACTION_VALUE_CHANGED, 140));
    assert(ui_dock_current()->value == 100);
    assert(s_action_card_id == control.id);
    assert(s_action_interaction == UI_DOCK_INTERACTION_VALUE_CHANGED);
    assert(s_action_value == 100);
    assert(ui_dock_invoke(control.id, UI_DOCK_INTERACTION_PRIMARY, 0));
    assert(s_action_interaction == UI_DOCK_INTERACTION_PRIMARY);
    assert(ui_dock_invoke(control.id, UI_DOCK_INTERACTION_DISMISS, 0));
    assert(!ui_dock_is_visible());

    control.dismissible = false;
    assert(ui_dock_show(&control));
    assert(!ui_dock_invoke(control.id, UI_DOCK_INTERACTION_DISMISS, 0));
    assert(ui_dock_is_visible());
}

static void test_dock_capacity_degradation(void)
{
    ui_dock_init();
    ui_dock_card_t active = card(100, UI_DOCK_TYPE_CONFIRM,
                                 UI_DOCK_PRIORITY_P0_CRITICAL, "critical");
    assert(ui_dock_show(&active));
    for(uint8_t i = 0; i < UI_DOCK_PENDING_MAX; i++) {
        ui_dock_card_t pending = card(200U + i, UI_DOCK_TYPE_INFO,
                                      UI_DOCK_PRIORITY_P4_NORMAL, "normal");
        assert(ui_dock_show(&pending));
    }
    assert(ui_dock_pending_count() == UI_DOCK_PENDING_MAX);
    ui_dock_card_t safety = card(300, UI_DOCK_TYPE_ACTION,
                                 UI_DOCK_PRIORITY_P1_RIDING_SAFETY, "safety");
    assert(ui_dock_show(&safety));
    assert(ui_dock_pending_count() == UI_DOCK_PENDING_MAX);
    assert(ui_dock_dropped_count() == 1);

    ui_dock_card_t safety_update = safety;
    snprintf(safety_update.title, sizeof(safety_update.title), "%s", "updated");
    assert(ui_dock_show(&safety_update));
    assert(ui_dock_pending_count() == UI_DOCK_PENDING_MAX);
    assert(ui_dock_dropped_count() == 1);
}

static void test_event_bus(void)
{
    ui_event_bus_init();
    s_event_count = 0;
    assert(ui_event_bus_subscribe(UI_EVENT_MASK_ALL, event_callback, NULL) > 0);
    ui_event_t normal = event_value(UI_EVENT_MEDIA_CHANGED,
                                    UI_EVENT_PRIORITY_P4_NORMAL, 0, 1);
    ui_event_t critical = event_value(UI_EVENT_STORAGE_ERROR,
                                      UI_EVENT_PRIORITY_P0_CRITICAL, 0, 2);
    assert(ui_event_bus_publish(&normal));
    assert(ui_event_bus_publish(&critical));
    assert(ui_event_bus_process(8) == 2);
    assert(s_event_order[0] == UI_EVENT_STORAGE_ERROR);
    assert(s_event_order[1] == UI_EVENT_MEDIA_CHANGED);

    s_event_count = 0;
    normal.dedupe_key = 77;
    normal.value = 3;
    assert(ui_event_bus_publish(&normal));
    normal.value = 4;
    assert(ui_event_bus_publish(&normal));
    assert(ui_event_bus_pending_count() == 1);
    assert(ui_event_bus_process(8) == 1);
    assert(s_event_values[0] == 4);

    ui_event_bus_clear();
    s_event_count = 0;
    normal.dedupe_key = 0;
    for(uint8_t i = 0; i < UI_EVENT_BUS_QUEUE_MAX; i++) {
        normal.value = i;
        assert(ui_event_bus_publish(&normal));
    }
    assert(ui_event_bus_publish(&critical));
    assert(ui_event_bus_dropped_count() == 1);
    assert(ui_event_bus_process(1) == 1);
    assert(s_event_order[0] == UI_EVENT_STORAGE_ERROR);
}

static void test_event_bridge(void)
{
    ui_event_bus_init();
    ui_event_bridge_init();
    s_event_count = 0;
    assert(ui_event_bus_subscribe(UI_EVENT_MASK_ALL, event_callback, NULL) > 0);
    ui_event_t event = event_value(UI_EVENT_MEDIA_CHANGED,
                                   UI_EVENT_PRIORITY_P4_NORMAL, 0, 42);
    assert(ui_event_bridge_post(&event));
    assert(ui_event_bridge_pending_count() == 1);
    assert(s_event_count == 0);
    assert(ui_event_bridge_drain(1) == 1);
    assert(ui_event_bridge_pending_count() == 0);
    assert(ui_event_bus_process(1) == 1);
    assert(s_event_count == 1);
    assert(s_event_values[0] == 42);

    for(uint8_t i = 0; i < UI_EVENT_BRIDGE_QUEUE_MAX; i++)
        assert(ui_event_bridge_post(&event));
    ui_event_t critical = event_value(UI_EVENT_STORAGE_ERROR,
                                      UI_EVENT_PRIORITY_P0_CRITICAL, 0, 99);
    assert(ui_event_bridge_post(&critical));
    assert(ui_event_bridge_dropped_count() == 1);
    assert(ui_event_bridge_drain(UI_EVENT_BRIDGE_QUEUE_MAX) ==
           UI_EVENT_BRIDGE_QUEUE_MAX);
    s_event_count = 0;
    assert(ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX) ==
           UI_EVENT_BUS_QUEUE_MAX);
    assert(s_event_order[0] == UI_EVENT_STORAGE_ERROR);

    ui_event_bus_init();
    ui_event_bridge_init();
    for(uint8_t i = 0; i < UI_EVENT_BRIDGE_QUEUE_MAX; i++)
        assert(ui_event_bridge_post(&critical));
    assert(!ui_event_bridge_post(&event));
    assert(ui_event_bridge_dropped_count() == 1);

    ui_event_bus_init();
    ui_event_bridge_init();
    event.dedupe_key = 77;
    event.value = 1;
    assert(ui_event_bridge_post(&event));
    event.value = 2;
    assert(ui_event_bridge_post(&event));
    assert(ui_event_bridge_pending_count() == 1);
    assert(ui_event_bridge_drain(1) == 1);
    s_event_count = 0;
    assert(ui_event_bus_subscribe(UI_EVENT_MASK_ALL, event_callback, NULL) > 0);
    assert(ui_event_bus_process(1) == 1);
    assert(s_event_values[0] == 2);
}

static void test_ride_data_model(void)
{
    ui_ride_data_model_init();
    s_ride_observer_calls = 0;
    assert(ui_ride_data_model_subscribe(ride_data_callback, NULL) > 0);
    ui_ride_data_t first = {.speed = 10.0f, .heart_rate = 120};
    ui_ride_data_t latest = {.speed = 32.8f, .heart_rate = 156};
    ui_ride_data_model_submit(&first);
    ui_ride_data_model_submit(&latest);
    assert(s_ride_observer_calls == 0);
    assert(ui_ride_data_model_process());
    assert(s_ride_observer_calls == 1);
    assert(s_last_ride_version == 1);
    assert(s_last_ride_data.speed == 32.8f);
    assert(s_last_ride_data.heart_rate == 156);
    assert(!ui_ride_data_model_process());

    ui_ride_data_model_set_connectivity(true, false);
    assert(ui_ride_data_model_process());
    assert(s_ride_observer_calls == 2);
    assert(s_last_ride_version == 2);
    assert(s_last_ride_data.wifi);
    assert(!s_last_ride_data.bluetooth);
}

typedef struct
{
    uint32_t lock_calls;
    uint32_t unlock_calls;
    uint32_t gnss_calls;
    uint32_t save_calls;
    uint32_t tick_ms;
} fake_stm32_context_t;

static void fake_stm32_lock(void *context)
{
    ((fake_stm32_context_t *)context)->lock_calls++;
}

static void fake_stm32_unlock(void *context)
{
    ((fake_stm32_context_t *)context)->unlock_calls++;
}

static bool fake_stm32_gnss(void *context)
{
    ((fake_stm32_context_t *)context)->gnss_calls++;
    return true;
}

static bool fake_stm32_save(void *context)
{
    ((fake_stm32_context_t *)context)->save_calls++;
    return true;
}

static void fake_stm32_tick(uint32_t elapsed_ms, void *context)
{
    ((fake_stm32_context_t *)context)->tick_ms += elapsed_ms;
}

static void test_stm32_service_contract(void)
{
    fake_stm32_context_t context = {0};
    ui_event_bus_init();
    ui_event_bridge_init();
    ui_ride_data_model_init();
    const ui_stm32_service_port_t port = {
        .request_gnss_fix = fake_stm32_gnss,
        .save_ride = fake_stm32_save,
        .retry_save = NULL,
        .tick = fake_stm32_tick,
        .lock = fake_stm32_lock,
        .unlock = fake_stm32_unlock,
        .context = &context
    };
    assert(ui_stm32_services_register(&port));
    assert(ui_system_services_request_gnss_fix());
    assert(ui_system_services_save_ride());
    assert(ui_system_services_retry_save());
    ui_system_services_tick(25);
    assert(context.gnss_calls == 1);
    assert(context.save_calls == 2);
    assert(context.tick_ms == 25);

    s_event_count = 0;
    assert(ui_event_bus_subscribe(UI_EVENT_MASK_ALL, event_callback, NULL) > 0);
    ui_event_t ready = event_value(UI_EVENT_GPS_READY,
                                   UI_EVENT_PRIORITY_P2_RIDE_STATE, 0, 7);
    assert(ui_stm32_services_publish_event(&ready));
    assert(s_event_count == 0);
    assert(ui_event_bridge_drain(1) == 1);
    assert(ui_event_bus_process(1) == 1);
    assert(s_event_order[0] == UI_EVENT_GPS_READY);

    s_ride_observer_calls = 0;
    assert(ui_ride_data_model_subscribe(ride_data_callback, NULL) > 0);
    ui_ride_data_t data = {.speed = 27.5f, .satellites = 10};
    ui_stm32_services_submit_ride_data(&data);
    assert(s_ride_observer_calls == 0);
    assert(ui_ride_data_model_process());
    assert(s_last_ride_data.speed == 27.5f);
    assert(s_last_ride_data.satellites == 10);
    assert(context.lock_calls == context.unlock_calls);

    ui_system_services_reset();
    ui_concurrency_reset_port();
}

static uint8_t process_platform_events(void)
{
    (void)ui_event_bridge_drain(UI_EVENT_BRIDGE_QUEUE_MAX);
    return ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
}

static void test_async_pc_services(void)
{
    ui_event_bus_init();
    ui_event_bridge_init();
    ui_ride_data_model_init();
    ui_pc_services_register();
    s_event_count = 0;
    assert(ui_event_bus_subscribe(UI_EVENT_MASK_ALL, event_callback, NULL) > 0);

    assert(ui_system_services_request_gnss_fix());
    assert(ui_pc_services_gnss_busy());
    assert(!ui_system_services_request_gnss_fix());
    ui_system_services_tick(UI_PC_GNSS_DELAY_MS - 1U);
    assert(process_platform_events() == 0);
    ui_system_services_tick(1U);
    assert(process_platform_events() == 1);
    assert(s_event_order[0] == UI_EVENT_GPS_READY);
    assert(!ui_pc_services_gnss_busy());

    s_event_count = 0;
    ui_pc_services_set_next_gnss_result(false);
    assert(ui_system_services_request_gnss_fix());
    ui_system_services_tick(UI_PC_GNSS_DELAY_MS);
    assert(process_platform_events() == 1);
    assert(s_event_order[0] == UI_EVENT_GPS_FAILED);

    s_event_count = 0;
    assert(ui_system_services_save_ride());
    assert(ui_pc_services_storage_busy());
    assert(!ui_system_services_save_ride());
    assert(process_platform_events() == 1);
    assert(s_event_order[0] == UI_EVENT_RIDE_SAVE_STARTED);
    ui_system_services_tick(UI_PC_STORAGE_DELAY_MS);
    assert(process_platform_events() == 1);
    assert(s_event_order[1] == UI_EVENT_RIDE_SAVE_SUCCESS);

    s_event_count = 0;
    ui_pc_services_set_next_storage_result(false);
    assert(ui_system_services_save_ride());
    (void)process_platform_events();
    ui_system_services_tick(UI_PC_STORAGE_DELAY_MS);
    assert(process_platform_events() == 1);
    assert(s_event_order[1] == UI_EVENT_RIDE_SAVE_FAILED);
    ui_pc_services_set_next_storage_result(true);
    assert(ui_system_services_retry_save());
    (void)process_platform_events();
    ui_system_services_tick(UI_PC_STORAGE_DELAY_MS);
    assert(process_platform_events() == 1);
    assert(s_event_order[3] == UI_EVENT_RIDE_SAVE_SUCCESS);

    ui_system_services_reset();
}

static void publish_and_process(ui_event_type_t type)
{
    ui_event_t event = event_value(type, UI_EVENT_PRIORITY_P2_RIDE_STATE,
                                   0, 0);
    assert(ui_event_bus_publish(&event));
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
}

static void test_ride_event_workflow(void)
{
    ui_event_bus_init();
    ui_dock_init();
    ui_state_machine_init();
    ui_dock_controller_init();
    ui_ride_controller_init();

    publish_and_process(UI_EVENT_BOOT_COMPLETE);
    assert(ui_state_machine_state() == UI_RIDE_STATE_HOME);
    publish_and_process(UI_EVENT_RIDE_START_REQUESTED);
    assert(ui_state_machine_state() == UI_RIDE_STATE_PRE_RIDE);
    assert(ui_dock_current() != NULL);
    assert(ui_dock_current()->type == UI_DOCK_TYPE_INFO);

    publish_and_process(UI_EVENT_GPS_FAILED);
    assert(ui_state_machine_state() == UI_RIDE_STATE_PRE_RIDE);
    assert(ui_dock_current()->type == UI_DOCK_TYPE_ACTION);
    assert(ui_dock_invoke(ui_dock_current()->id,
                          UI_DOCK_INTERACTION_PRIMARY, 0));
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
    assert(ui_dock_current() != NULL);
    assert(ui_dock_current()->type == UI_DOCK_TYPE_INFO);

    publish_and_process(UI_EVENT_GPS_READY);
    assert(ui_state_machine_state() == UI_RIDE_STATE_GPS_READY);
    assert(ui_dock_current()->type == UI_DOCK_TYPE_ACTION);
    assert(ui_dock_invoke(ui_dock_current()->id,
                          UI_DOCK_INTERACTION_PRIMARY, 0));
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
    assert(ui_state_machine_state() == UI_RIDE_STATE_RIDING);
    assert(ui_dock_current() != NULL);
    assert(ui_dock_current()->type == UI_DOCK_TYPE_INFO);
    ui_dock_controller_tick(1800);
    assert(!ui_dock_is_visible());

    publish_and_process(UI_EVENT_RIDE_PAUSE_REQUESTED);
    assert(ui_state_machine_state() == UI_RIDE_STATE_PAUSED);
    publish_and_process(UI_EVENT_RIDE_FINISH_REQUESTED);
    assert(ui_state_machine_state() == UI_RIDE_STATE_FINISHING);
    assert(ui_dock_current()->type == UI_DOCK_TYPE_CONFIRM);
    ui_dock_controller_tick(60000);
    assert(ui_dock_current() != NULL);
    assert(ui_dock_current()->type == UI_DOCK_TYPE_CONFIRM);
    assert(ui_dock_invoke(ui_dock_current()->id,
                          UI_DOCK_INTERACTION_SECONDARY, 0));
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
    assert(ui_state_machine_state() == UI_RIDE_STATE_PAUSED);

    publish_and_process(UI_EVENT_RIDE_RESUME_REQUESTED);
    assert(ui_state_machine_state() == UI_RIDE_STATE_RIDING);
    publish_and_process(UI_EVENT_RIDE_FINISH_REQUESTED);
    assert(ui_state_machine_state() == UI_RIDE_STATE_FINISHING);
    assert(ui_dock_invoke(ui_dock_current()->id,
                          UI_DOCK_INTERACTION_PRIMARY, 0));
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
    assert(ui_state_machine_state() == UI_RIDE_STATE_FINISHING);
    publish_and_process(UI_EVENT_RIDE_SAVE_STARTED);
    assert(ui_dock_current() != NULL);
    assert(ui_dock_current()->type == UI_DOCK_TYPE_INFO);
    publish_and_process(UI_EVENT_RIDE_SAVE_SUCCESS);
    assert(ui_state_machine_state() == UI_RIDE_STATE_SUMMARY);

    ui_ride_controller_deinit();
    ui_dock_controller_deinit();
}

int main(void)
{
    test_state_machine();
    test_dock_priority_and_restore();
    test_dock_update_and_replace();
    test_dock_interactions();
    test_dock_capacity_degradation();
    test_event_bus();
    test_event_bridge();
    test_ride_data_model();
    test_stm32_service_contract();
    test_async_pc_services();
    test_ride_event_workflow();
    puts("ui_core_logic_test: PASS");
    return 0;
}
