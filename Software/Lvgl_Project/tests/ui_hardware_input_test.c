#include "ui/core/ui_hardware_input.h"
#include <assert.h>
#include <stdio.h>
static ui_input_event_t events[32];
static unsigned count;
static bool capture(const ui_input_message_t *event, void *context)
{
    (void)context;
    events[count++] = event->type;
    return true;
}
int main(void)
{
    ui_hardware_input_t input;
    ui_hardware_input_init(&input, capture, NULL);
    uint32_t ride = UI_HW_KEY_BIT(UI_HW_KEY_RIDE);
    ui_hardware_input_sample(&input, ride, 0, 10);
    ui_hardware_input_sample(&input, 0, 0, 20);
    ui_hardware_input_sample(&input, 0, 0, 50);
    assert(count == 0);
    ui_hardware_input_sample(&input, ride, 0, 100);
    ui_hardware_input_sample(&input, ride, 0, 130);
    ui_hardware_input_sample(&input, 0, 0, 160);
    ui_hardware_input_sample(&input, 0, 0, 190);
    assert(count == 1 && events[0] == UI_INPUT_RIDE_TOGGLE);
    uint32_t power = UI_HW_KEY_BIT(UI_HW_KEY_POWER);
    ui_hardware_input_sample(&input, power, 0, 200);
    ui_hardware_input_sample(&input, power, 0, 230);
    ui_hardware_input_sample(&input, power, 0, 1430);
    ui_hardware_input_sample(&input, power, 0, 1500);
    ui_hardware_input_sample(&input, 0, 0, 1600);
    ui_hardware_input_sample(&input, 0, 0, 1630);
    assert(count == 2 && events[1] == UI_INPUT_OPEN_POWER);
    ui_hardware_input_sample(&input, 0, -7, 1700);
    assert(count == 6 && input.encoder_pending == -3);
    ui_hardware_input_sample(&input, 0, 0, 1710);
    assert(count == 9 && input.encoder_pending == 0);
    for(unsigned i = 2; i < count; ++i) assert(events[i] == UI_INPUT_ENCODER_CCW);
    puts("ui_hardware_input_test: PASS");
}
