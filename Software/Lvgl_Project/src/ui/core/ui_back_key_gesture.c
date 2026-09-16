#include "ui_back_key_gesture.h"

#include <string.h>

static uint32_t elapsed_ms(uint32_t now, uint32_t then)
{
    return now - then;
}

void ui_back_key_gesture_init(ui_back_key_gesture_t *gesture)
{
    if(gesture == NULL) return;
    memset(gesture, 0, sizeof(*gesture));
}

ui_back_key_result_t ui_back_key_gesture_update(
    ui_back_key_gesture_t *gesture, bool is_down, uint32_t now_ms)
{
    if(gesture == NULL) return UI_BACK_KEY_RESULT_NONE;

    ui_back_key_result_t result = UI_BACK_KEY_RESULT_NONE;

    if(gesture->single_pending && !gesture->is_down &&
       elapsed_ms(now_ms, gesture->first_release_ms) >
       UI_BACK_KEY_DOUBLE_MS) {
        gesture->single_pending = false;
        result = UI_BACK_KEY_RESULT_SINGLE;
    }

    if(is_down && !gesture->is_down) {
        gesture->second_press = gesture->single_pending &&
            elapsed_ms(now_ms, gesture->first_release_ms) <=
            UI_BACK_KEY_DOUBLE_MS;
        if(gesture->second_press) gesture->single_pending = false;
        gesture->is_down = true;
        gesture->long_emitted = false;
        gesture->pressed_at_ms = now_ms;
        return result;
    }

    if(is_down && gesture->is_down && !gesture->long_emitted &&
       elapsed_ms(now_ms, gesture->pressed_at_ms) >=
       UI_BACK_KEY_LONG_MS) {
        gesture->long_emitted = true;
        gesture->single_pending = false;
        gesture->second_press = false;
        return UI_BACK_KEY_RESULT_LONG;
    }

    if(!is_down && gesture->is_down) {
        gesture->is_down = false;
        if(gesture->long_emitted) {
            gesture->long_emitted = false;
            gesture->second_press = false;
            return result;
        }
        if(gesture->second_press) {
            gesture->second_press = false;
            return UI_BACK_KEY_RESULT_DOUBLE;
        }
        gesture->single_pending = true;
        gesture->first_release_ms = now_ms;
    }

    return result;
}
