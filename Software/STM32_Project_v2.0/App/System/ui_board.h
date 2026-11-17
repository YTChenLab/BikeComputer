
#ifndef UI_BOARD_H
#define UI_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "ui/core/ui_physical_input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 在 FreeRTOS UI Task 中调用一次，启动完整 UI */
bool UI_BoardStart(void);

/* 由 UI Task 周期调用，执行 LVGL 刷新 */
uint32_t UI_BoardPoll(void);

/* 将现有 12 字节物理输入事件交给 UI */
bool UI_BoardInput(const ui_physical_input_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* UI_BOARD_H */
