#include "ui/core/ui_back_key_gesture.h"

#include <assert.h>
#include <stdio.h>

static void test_single_click(void)
{
    ui_back_key_gesture_t gesture;
    ui_back_key_gesture_init(&gesture);
    assert(ui_back_key_gesture_update(&gesture, true, 100U) ==
           UI_BACK_KEY_RESULT_NONE);
    assert(ui_back_key_gesture_update(&gesture, false, 150U) ==
           UI_BACK_KEY_RESULT_NONE);
    assert(ui_back_key_gesture_update(
               &gesture, false, 150U + UI_BACK_KEY_DOUBLE_MS) ==
           UI_BACK_KEY_RESULT_NONE);
    assert(ui_back_key_gesture_update(
               &gesture, false, 151U + UI_BACK_KEY_DOUBLE_MS) ==
           UI_BACK_KEY_RESULT_SINGLE);
}

static void test_double_click(void)
{
    ui_back_key_gesture_t gesture;
    ui_back_key_gesture_init(&gesture);
    assert(ui_back_key_gesture_update(&gesture, true, 1000U) ==
           UI_BACK_KEY_RESULT_NONE);
    assert(ui_back_key_gesture_update(&gesture, false, 1050U) ==
           UI_BACK_KEY_RESULT_NONE);
    assert(ui_back_key_gesture_update(&gesture, true, 1200U) ==
           UI_BACK_KEY_RESULT_NONE);
    assert(ui_back_key_gesture_update(&gesture, false, 1250U) ==
           UI_BACK_KEY_RESULT_DOUBLE);
    assert(ui_back_key_gesture_update(&gesture, false, 1600U) ==
           UI_BACK_KEY_RESULT_NONE);
}

static void test_long_press(void)
{
    ui_back_key_gesture_t gesture;
    ui_back_key_gesture_init(&gesture);
    assert(ui_back_key_gesture_update(&gesture, true, 2000U) ==
           UI_BACK_KEY_RESULT_NONE);
    assert(ui_back_key_gesture_update(
               &gesture, true, 2000U + UI_BACK_KEY_LONG_MS - 1U) ==
           UI_BACK_KEY_RESULT_NONE);
    assert(ui_back_key_gesture_update(
               &gesture, true, 2000U + UI_BACK_KEY_LONG_MS) ==
           UI_BACK_KEY_RESULT_LONG);
    assert(ui_back_key_gesture_update(&gesture, false, 3300U) ==
           UI_BACK_KEY_RESULT_NONE);
}

int main(void)
{
    test_single_click();
    test_double_click();
    test_long_press();
    puts("ui_back_key_gesture_test: PASS");
    return 0;
}
