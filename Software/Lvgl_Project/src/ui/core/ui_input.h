#ifndef UI_INPUT_H
#define UI_INPUT_H

#include <stdint.h>

/*
 * 平台无关的 UI 语义输入。
 * SDL、STM32 GPIO、实体编码器等平台层只负责产生这些事件。
 */
typedef enum
{
    UI_INPUT_ENCODER_CW,
    UI_INPUT_ENCODER_CCW,
    UI_INPUT_ENCODER_PRESS,
    UI_INPUT_BACK,
    UI_INPUT_BACK_DOUBLE,
    UI_INPUT_BACK_LONG,
    UI_INPUT_PAGE_PREV,
    UI_INPUT_PAGE_NEXT,
    UI_INPUT_OPEN_POWER,
    UI_INPUT_TOGGLE_CONTROL_CENTER,
    UI_INPUT_POWER_TOGGLE,
    UI_INPUT_OPEN_PROFILE,
    UI_INPUT_OPEN_SETTINGS,
    UI_INPUT_TOUCH_BEGIN,
    UI_INPUT_TOUCH_MOVE,
    UI_INPUT_TOUCH_END,
    UI_INPUT_TOUCH_CANCEL,
    UI_INPUT_COUNT
} ui_input_event_t;

typedef struct
{
    ui_input_event_t type;
    int32_t x;
    int32_t y;
} ui_input_message_t;

typedef ui_input_message_t ui_input_pointer_event_t;

#endif /* UI_INPUT_H */
