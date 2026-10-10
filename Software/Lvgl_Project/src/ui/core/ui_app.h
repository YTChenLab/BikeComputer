#ifndef UI_APP_H
#define UI_APP_H

#include <stdint.h>
#include "ui_input.h"

/* 页面动作仅由 UiTask 调用；实体键从标准事件入口映射到相同行为。 */
void ui_app_init(void);
bool ui_app_handle_input_event(const ui_physical_input_event_t *event);
/* PowerService 状态反馈，仅更新显示；不会再次提交电源命令。 */
bool ui_app_apply_display_state(bool enabled);
void ui_app_handle_input(ui_input_event_t input);
void ui_app_handle_pointer(const ui_input_pointer_event_t *event);
void ui_app_handle_back(void);

#endif /* UI_APP_H */
