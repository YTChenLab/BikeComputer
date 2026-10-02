#ifndef UI_DASHBOARD_EDITOR_H
#define UI_DASHBOARD_EDITOR_H
#include "../core/ui_input.h"
#include <stdbool.h>
void ui_dashboard_editor_open(void);
void ui_dashboard_editor_close(void);
bool ui_dashboard_editor_visible(void);
bool ui_dashboard_editor_handle_input(ui_input_event_t input);
#endif
