#ifndef UI_HARDWARE_INPUT_H
#define UI_HARDWARE_INPUT_H
#include "ui_input.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_HW_KEY_POWER, UI_HW_KEY_BACK, UI_HW_KEY_PREVIOUS, UI_HW_KEY_NEXT,
    UI_HW_KEY_RIDE, UI_HW_KEY_ENCODER, UI_HW_KEY_COUNT
} ui_hardware_key_t;
/* Fifth physical button is KEY_MENU; RIDE retains its existing enum value. */
#define UI_HW_KEY_MENU UI_HW_KEY_RIDE
#define UI_HW_KEY_BIT(key) (UINT32_C(1) << (key))
typedef bool (*ui_hardware_input_emit_t)(const ui_input_message_t *, void *);
typedef struct {
    uint32_t raw_mask, stable_mask, changed_at[UI_HW_KEY_COUNT], down_at[UI_HW_KEY_COUNT];
    uint32_t long_emitted_mask, dropped;
    int32_t encoder_pending;
    ui_input_event_t short_event[UI_HW_KEY_COUNT], long_event[UI_HW_KEY_COUNT];
    ui_hardware_input_emit_t emit;
    void *context;
} ui_hardware_input_t;

/* Single input-task owner. Supply normalized pressed bits every 5-10 ms;
 * the library debounces for 25 ms and emits one long event at 1200 ms.
 * UI_INPUT_COUNT disables a mapping. GPIO/encoder decoding stays in board code. */
void ui_hardware_input_init(ui_hardware_input_t *input,
    ui_hardware_input_emit_t emit, void *context);
void ui_hardware_input_sample(ui_hardware_input_t *input,
    uint32_t pressed_mask, int32_t encoder_delta, uint32_t now_ms);
#endif
