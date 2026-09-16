#ifndef UI_BACK_KEY_GESTURE_H
#define UI_BACK_KEY_GESTURE_H

#include <stdbool.h>
#include <stdint.h>

#define UI_BACK_KEY_DOUBLE_MS 280U
#define UI_BACK_KEY_LONG_MS  1200U

typedef enum
{
    UI_BACK_KEY_RESULT_NONE = 0,
    UI_BACK_KEY_RESULT_SINGLE,
    UI_BACK_KEY_RESULT_DOUBLE,
    UI_BACK_KEY_RESULT_LONG
} ui_back_key_result_t;

typedef struct
{
    uint32_t pressed_at_ms;
    uint32_t first_release_ms;
    bool is_down;
    bool long_emitted;
    bool single_pending;
    bool second_press;
} ui_back_key_gesture_t;

void ui_back_key_gesture_init(ui_back_key_gesture_t *gesture);
ui_back_key_result_t ui_back_key_gesture_update(
    ui_back_key_gesture_t *gesture, bool is_down, uint32_t now_ms);

#endif /* UI_BACK_KEY_GESTURE_H */
