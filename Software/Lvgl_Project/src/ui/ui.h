#ifndef UI_H
#define UI_H

#include "lvgl.h"
#include "core/ui_input.h"

/*
 * ============================================================
 * UI 系统总入口
 * ============================================================
 */

void ui_init(void);
void ui_handle_key(uint32_t key);
void ui_handle_input(ui_input_event_t input);
void ui_handle_pointer(const ui_input_pointer_event_t *event);
void ui_handle_back(void);

#endif
