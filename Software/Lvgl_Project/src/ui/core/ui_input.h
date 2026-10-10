#ifndef UI_INPUT_H
#define UI_INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "ui_physical_input.h"

/*
 * 页面动作与触摸语义。实体键使用 ui_physical_input_event_t，
 * 由 UiTask 映射为下列既有动作；这里不识别 GPIO 或按键时序。
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
    UI_INPUT_NAV_DEMO_NEXT,
    UI_INPUT_RIDE_TOGGLE,
    UI_INPUT_TOUCH_BEGIN,
    UI_INPUT_TOUCH_MOVE,
    UI_INPUT_TOUCH_END,
    UI_INPUT_TOUCH_CANCEL,
    UI_INPUT_PHYSICAL_EVENT, /* PC queue envelope; never a page action. */
    UI_INPUT_COUNT
} ui_input_event_t;

typedef struct
{
    ui_input_event_t type;
    int32_t x;
    int32_t y;
    ui_physical_input_event_t physical;
} ui_input_message_t;

typedef struct {
    ui_input_event_t action;
    uint32_t count;
} ui_input_mapping_t;

/* Stateless validation/mapping. No clocks, debounce, pending clicks or LVGL. */
bool ui_input_map_physical_event(const ui_physical_input_event_t *event,
                                 ui_input_mapping_t *mapping);

typedef ui_input_message_t ui_input_pointer_event_t;

#endif /* UI_INPUT_H */
