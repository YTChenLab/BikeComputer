#include "services/ui_command_service.h"

#include <assert.h>
#include <stdio.h>

typedef struct
{
    ui_command_t dispatched[8];
    ui_command_result_t observed[16];
    uint8_t dispatched_count;
    uint8_t observed_count;
    bool accept;
} fake_backend_t;

static bool dispatch(const ui_command_t *command, void *context)
{
    fake_backend_t *fake = context;
    if(!fake->accept) return false;
    fake->dispatched[fake->dispatched_count++] = *command;
    return true;
}

static void observe(const ui_command_result_t *result, void *user_data)
{
    fake_backend_t *fake = user_data;
    fake->observed[fake->observed_count++] = *result;
}

static void test_success_and_fifo(void)
{
    fake_backend_t fake = {.accept = true};
    ui_command_service_init();
    const ui_command_backend_t backend = {
        .dispatch = dispatch,
        .context = &fake
    };
    assert(ui_command_service_register_backend(&backend));
    assert(ui_command_service_subscribe(observe, &fake) > 0);

    uint32_t first = ui_command_service_submit(
        UI_COMMAND_WIFI_SET_ENABLED, 0, 200U);
    uint32_t second = ui_command_service_submit(
        UI_COMMAND_RIDE_START, 0, 200U);
    assert(first != 0U && second != 0U && first != second);
    assert(ui_command_service_pending() == 2U);

    ui_command_service_process(20U);
    assert(fake.dispatched_count == 1U);
    assert(fake.dispatched[0].id == first);
    assert(fake.observed_count == 1U);
    assert(fake.observed[0].status == UI_COMMAND_STATUS_PENDING);
    assert(ui_command_service_complete(first, UI_COMMAND_STATUS_SUCCESS, 0));
    ui_command_service_process(20U);
    assert(fake.observed[1].status == UI_COMMAND_STATUS_SUCCESS);
    assert(fake.dispatched_count == 2U);
    assert(fake.dispatched[1].id == second);
    assert(ui_command_service_complete(second, UI_COMMAND_STATUS_FAILED, 44));
    ui_command_service_process(20U);
    assert(fake.observed[fake.observed_count - 1U].status ==
           UI_COMMAND_STATUS_FAILED);
    assert(fake.observed[fake.observed_count - 1U].error_code == 44);
    assert(ui_command_service_pending() == 0U);
}

static void test_latest_value_coalescing(void)
{
    fake_backend_t fake = {.accept = true};
    ui_command_service_init();
    const ui_command_backend_t backend = {
        .dispatch = dispatch,
        .context = &fake
    };
    assert(ui_command_service_register_backend(&backend));
    uint32_t first = ui_command_service_submit(
        UI_COMMAND_BRIGHTNESS_SET_LEVEL, 20, 200U);
    uint32_t latest = ui_command_service_submit(
        UI_COMMAND_BRIGHTNESS_SET_LEVEL, 78, 400U);
    assert(first == latest);
    assert(ui_command_service_pending() == 1U);
    ui_command_service_process(1U);
    assert(fake.dispatched[0].value == 78);
    assert(fake.dispatched[0].timeout_ms == 400U);

    uint32_t touch_lock = ui_command_service_submit(
        UI_COMMAND_TOUCH_LOCK_SET_ENABLED, 1, 300U);
    uint32_t touch_unlock = ui_command_service_submit(
        UI_COMMAND_TOUCH_LOCK_SET_ENABLED, 0, 450U);
    assert(touch_lock != 0U && touch_lock == touch_unlock);

    uint32_t ride = ui_command_service_submit(
        UI_COMMAND_RIDE_START, 0, 500U);
    assert(ride != 0U);
    assert(ui_command_service_submit(UI_COMMAND_RIDE_START,
                                     0, 500U) == ride);
}

static void test_timeout_and_rejection(void)
{
    fake_backend_t fake = {.accept = true};
    ui_command_service_init();
    const ui_command_backend_t backend = {
        .dispatch = dispatch,
        .context = &fake
    };
    assert(ui_command_service_register_backend(&backend));
    assert(ui_command_service_subscribe(observe, &fake) > 0);
    assert(ui_command_service_submit(UI_COMMAND_GNSS_ACQUIRE_FIX,
                                     0, 30U) != 0U);
    ui_command_service_process(10U);
    ui_command_service_process(29U);
    assert(fake.observed_count == 1U);
    ui_command_service_process(1U);
    assert(fake.observed_count == 2U);
    assert(fake.observed[1].status == UI_COMMAND_STATUS_TIMEOUT);

    fake.accept = false;
    assert(ui_command_service_submit(UI_COMMAND_RIDE_UPLOAD,
                                     0, 100U) != 0U);
    ui_command_service_process(1U);
    assert(fake.observed[fake.observed_count - 1U].status ==
           UI_COMMAND_STATUS_REJECTED);
    assert(ui_command_service_pending() == 0U);
}

static void test_capacity_and_repeated_completion(void)
{
    ui_command_service_init();
    for(uint8_t index = 0; index < UI_COMMAND_QUEUE_MAX; index++) {
        assert(ui_command_service_submit(UI_COMMAND_MEDIA_NEXT,
                                         0, 100U) != 0U);
    }
    assert(ui_command_service_submit(UI_COMMAND_MEDIA_NEXT,
                                     0, 100U) == 0U);
    assert(ui_command_service_dropped() == 1U);

    fake_backend_t fake = {.accept = true};
    ui_command_service_init();
    const ui_command_backend_t backend = {
        .dispatch = dispatch,
        .context = &fake
    };
    assert(ui_command_service_register_backend(&backend));
    assert(ui_command_service_subscribe(observe, &fake) > 0);
    for(uint16_t cycle = 0; cycle < 250U; cycle++) {
        fake.dispatched_count = 0U;
        fake.observed_count = 0U;
        assert(ui_command_service_submit(UI_COMMAND_MEDIA_NEXT,
                                         0, 100U) != 0U);
        ui_command_service_process(1U);
        assert(fake.dispatched_count == 1U);
        assert(ui_command_service_complete(
            fake.dispatched[0].id, UI_COMMAND_STATUS_SUCCESS, 0));
        ui_command_service_process(1U);
        assert(fake.observed_count == 2U);
        assert(fake.observed[1].status == UI_COMMAND_STATUS_SUCCESS);
        assert(ui_command_service_pending() == 0U);
    }
}

int main(void)
{
    test_success_and_fifo();
    test_latest_value_coalescing();
    test_timeout_and_rejection();
    test_capacity_and_repeated_completion();
    puts("ui_command_service_test: PASS");
    return 0;
}
