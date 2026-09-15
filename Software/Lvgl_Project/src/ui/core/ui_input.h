#ifndef UI_INPUT_H
#define UI_INPUT_H

/*
 * 平台无关的 UI 语义输入。
 * SDL、STM32 GPIO、实体编码器等平台层只负责产生这些事件。
 */
typedef enum
{
    UI_INPUT_ENCODER_CW,
    UI_INPUT_ENCODER_CCW,
    UI_INPUT_ENCODER_PRESS,
    UI_INPUT_BACK
} ui_input_event_t;

#endif /* UI_INPUT_H */
