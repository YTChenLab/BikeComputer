#ifndef UI_PHYSICAL_INPUT_H
#define UI_PHYSICAL_INPUT_H

#include <stdint.h>

/* Stable codes for InputService -> queue -> UiTask. No HAL/RTOS/LVGL types. */
typedef enum {
    UI_PHYSICAL_KEY_NONE = 0,
    UI_PHYSICAL_KEY_POWER = 1,
    UI_PHYSICAL_KEY_BACK = 2,
    UI_PHYSICAL_KEY_PREV = 3,
    UI_PHYSICAL_KEY_NEXT = 4,
    UI_PHYSICAL_KEY_MENU = 5,
    UI_PHYSICAL_KEY_ENCODER_BUTTON = 6,
    UI_PHYSICAL_KEY_ENCODER = 7,
    UI_PHYSICAL_KEY_COUNT
} ui_physical_key_t;

typedef enum {
    UI_PHYSICAL_ACTION_NONE = 0,
    UI_PHYSICAL_KEY_DOWN = 1,
    UI_PHYSICAL_KEY_UP = 2,
    UI_PHYSICAL_KEY_CLICK = 3,
    UI_PHYSICAL_KEY_DOUBLE_CLICK = 4,
    UI_PHYSICAL_KEY_LONG_START = 5,
    UI_PHYSICAL_KEY_LONG_REPEAT = 6,
    UI_PHYSICAL_KEY_LONG_END = 7,
    UI_PHYSICAL_ENCODER_DELTA = 8,
    UI_PHYSICAL_ACTION_COUNT
} ui_physical_action_t;

/* A bounded UiTask call consumes every detent; it never clips or accumulates.
 * InputService splits larger batches into multiple queue records. */
#define UI_PHYSICAL_ENCODER_MAX_DELTA 64

typedef struct {
    uint8_t key;             /* ui_physical_key_t code, independent of enum ABI. */
    uint8_t action;          /* ui_physical_action_t code. */
    uint16_t reserved;       /* Must be zero; initialize the whole record. */
    int32_t encoder_delta;  /* Positive=CW; negative=CCW; non-encoder events=0. */
    uint32_t timestamp_ms;  /* InputService's monotonic clock; no UI timing. */
} ui_physical_input_event_t;

/* InputService emits exactly one CLICK, DOUBLE_CLICK or LONG_START outcome.
 * A long sequence may include REPEAT and ends with LONG_END/UP, never CLICK.
 * POWER is owned by PowerService; forwarding it to UI has no hardware action. */

#endif /* UI_PHYSICAL_INPUT_H */
