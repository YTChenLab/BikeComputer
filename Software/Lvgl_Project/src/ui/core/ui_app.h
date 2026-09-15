#ifndef UI_APP_H
#define UI_APP_H

#include <stdint.h>
#include "lvgl.h"
#include "ui_input.h"

/* 应用级页面与交互控制器。PC 端由键盘事件调用，真机端可直接映射实体按键。 */
void ui_app_init(void);
void ui_app_handle_key(uint32_t key);
void ui_app_handle_input(ui_input_event_t input);
void ui_app_handle_pointer(lv_event_code_t code, int32_t x, int32_t y);
void ui_app_handle_back(void);

#endif /* UI_APP_H */
