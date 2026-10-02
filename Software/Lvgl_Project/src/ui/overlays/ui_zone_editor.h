#ifndef UI_ZONE_EDITOR_H
#define UI_ZONE_EDITOR_H
#include "../core/ui_input.h"
#include <stdbool.h>
void ui_zone_editor_open(void);
void ui_zone_editor_close(void);
bool ui_zone_editor_visible(void);
bool ui_zone_editor_handle_input(ui_input_event_t input);
#endif
