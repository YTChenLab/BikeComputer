#ifndef UI_STM32_INPUT_H
#define UI_STM32_INPUT_H
#include "lvgl.h"
/* Called once by UiTask. Read callback reads the calibrated touch cache only. */
lv_indev_t *ui_stm32_input_create_pointer(void);
#endif
