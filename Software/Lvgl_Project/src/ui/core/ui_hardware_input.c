#include "ui_hardware_input.h"
#include <string.h>

static void emit(ui_hardware_input_t *input, ui_input_event_t type)
{
    if(type == UI_INPUT_COUNT) return;
    ui_input_message_t event = { .type = type };
    if(input->emit == NULL || !input->emit(&event, input->context)) ++input->dropped;
}

void ui_hardware_input_init(ui_hardware_input_t *input,
    ui_hardware_input_emit_t callback, void *context)
{
    if(input == NULL) return;
    memset(input, 0, sizeof(*input));
    input->emit = callback;
    input->context = context;
    const ui_input_event_t shorts[UI_HW_KEY_COUNT] = {UI_INPUT_POWER_TOGGLE,
        UI_INPUT_BACK, UI_INPUT_PAGE_PREV, UI_INPUT_PAGE_NEXT,
        UI_INPUT_RIDE_TOGGLE, UI_INPUT_ENCODER_PRESS};
    const ui_input_event_t longs[UI_HW_KEY_COUNT] = {UI_INPUT_OPEN_POWER,
        UI_INPUT_BACK_LONG, UI_INPUT_COUNT, UI_INPUT_COUNT,
        UI_INPUT_BACK_LONG, UI_INPUT_TOGGLE_CONTROL_CENTER};
    memcpy(input->short_event, shorts, sizeof(shorts));
    memcpy(input->long_event, longs, sizeof(longs));
}

void ui_hardware_input_sample(ui_hardware_input_t *input,
    uint32_t pressed_mask, int32_t encoder_delta, uint32_t now_ms)
{
    if(input == NULL) return;
    pressed_mask &= (UI_HW_KEY_BIT(UI_HW_KEY_COUNT) - 1);
    uint32_t changed = pressed_mask ^ input->raw_mask;
    for(unsigned i = 0; i < UI_HW_KEY_COUNT; ++i)
        if(changed & UI_HW_KEY_BIT(i)) input->changed_at[i] = now_ms;
    input->raw_mask = pressed_mask;
    for(unsigned i = 0; i < UI_HW_KEY_COUNT; ++i) {
        uint32_t bit = UI_HW_KEY_BIT(i);
        bool raw = (pressed_mask & bit) != 0, stable = (input->stable_mask & bit) != 0;
        if(raw != stable && (uint32_t)(now_ms - input->changed_at[i]) >= 25) {
            if(raw) {
                input->stable_mask |= bit;
                input->down_at[i] = now_ms;
                input->long_emitted_mask &= ~bit;
            }
            else {
                input->stable_mask &= ~bit;
                if(!(input->long_emitted_mask & bit)) emit(input, input->short_event[i]);
            }
        }
        if((input->stable_mask & bit) && !(input->long_emitted_mask & bit) &&
           input->long_event[i] != UI_INPUT_COUNT &&
           (uint32_t)(now_ms - input->down_at[i]) >= 1200) {
            input->long_emitted_mask |= bit;
            emit(input, input->long_event[i]);
        }
    }
    if(encoder_delta > 64) encoder_delta = 64;
    if(encoder_delta < -64) encoder_delta = -64;
    input->encoder_pending += encoder_delta;
    if(input->encoder_pending > 64) input->encoder_pending = 64;
    if(input->encoder_pending < -64) input->encoder_pending = -64;
    for(unsigned i = 0; i < 4 && input->encoder_pending != 0; ++i) {
        bool clockwise = input->encoder_pending > 0;
        emit(input, clockwise ? UI_INPUT_ENCODER_CW : UI_INPUT_ENCODER_CCW);
        input->encoder_pending += clockwise ? -1 : 1;
    }
}
