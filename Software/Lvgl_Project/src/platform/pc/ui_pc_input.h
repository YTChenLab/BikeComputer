#ifndef UI_PC_INPUT_H
#define UI_PC_INPUT_H

#include "ui/core/ui_physical_input.h"
#include <stdbool.h>
#include <SDL.h>

/* Copies a recognized record to the existing UI queue; no LVGL on this path. */
bool ui_pc_input_post(const ui_physical_input_event_t *event);
/* True means the SDL event is an input shortcut (including ignored repeats).
 * Queue overflow is observable through ui_input_queue_dropped(). */
bool ui_pc_input_handle_sdl_event(const SDL_Event *event);

#endif
