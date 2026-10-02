#ifndef SIMULATOR_VIEW_H
#define SIMULATOR_VIEW_H

#include "lvgl/lvgl.h"
#include <SDL.h>

/* PC-only: scale the SDL surface while retaining the hardware resolution. */
void simulator_view_prepare(void);
void simulator_view_init(lv_display_t *display);
void simulator_view_tick(void);
bool simulator_view_handle_key(SDL_Keycode key);
uint32_t simulator_view_mouse_state(int *x, int *y);
void simulator_view_pointer_read(lv_indev_t *indev, lv_indev_data_t *data);

#endif
