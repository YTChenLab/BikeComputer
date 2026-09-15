#include "hal.h"
#include "../ui/ui.h"
#include <SDL.h>

/* PC 专用适配：滚轮只产生语义化编码器事件，UI 不依赖 SDL。 */
static int SDLCALL simulator_event_watch(void *userdata, SDL_Event *event)
{
  (void)userdata;
  if(event->type == SDL_MOUSEWHEEL && event->wheel.y != 0) {
    ui_handle_input(event->wheel.y > 0 ?
                    UI_INPUT_ENCODER_CCW : UI_INPUT_ENCODER_CW);
  }
  else if(event->type == SDL_KEYDOWN && event->key.repeat == 0) {
    SDL_Keycode key = event->key.keysym.sym;
    if(key == SDLK_DOWN) ui_handle_input(UI_INPUT_ENCODER_CW);
    else if(key == SDLK_UP) ui_handle_input(UI_INPUT_ENCODER_CCW);
    else if(key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      ui_handle_input(UI_INPUT_ENCODER_PRESS);
    }
    else if(key == SDLK_ESCAPE || key == SDLK_BACKSPACE) {
      ui_handle_input(UI_INPUT_BACK);
    }
    else if(key == SDLK_LEFT) ui_handle_key(LV_KEY_LEFT);
    else if(key == SDLK_RIGHT) ui_handle_key(LV_KEY_RIGHT);
    else if(key >= 32 && key <= 126) ui_handle_key((uint32_t)key);
  }
  return 1;
}

/* 直接采样 SDL 左键状态，连续发送 PRESSING，完整模拟触屏按下与拖动。 */
static void mouse_button_timer_cb(lv_timer_t *timer)
{
  static bool left_was_down = false;
  static bool right_was_down = false;
  int mouse_x = 0;
  int mouse_y = 0;
  uint32_t buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
  lv_display_t *display = lv_display_get_default();
  if(display) {
    mouse_x = LV_CLAMP(0, mouse_x, lv_display_get_horizontal_resolution(display) - 1);
    mouse_y = LV_CLAMP(0, mouse_y, lv_display_get_vertical_resolution(display) - 1);
  }
  bool left_is_down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
  bool right_is_down = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;

  if(left_is_down && !left_was_down) {
    ui_handle_pointer(LV_EVENT_PRESSED, mouse_x, mouse_y);
  }
  else if(left_is_down) {
    ui_handle_pointer(LV_EVENT_PRESSING, mouse_x, mouse_y);
  }
  else if(left_was_down) {
    ui_handle_pointer(LV_EVENT_RELEASED, mouse_x, mouse_y);
  }

  if(right_is_down && !right_was_down) {
    ui_handle_input(UI_INPUT_BACK);
  }
  left_was_down = left_is_down;
  right_was_down = right_is_down;
  (void)timer;
}


lv_display_t * sdl_hal_init(int32_t w, int32_t h)
{

  lv_group_set_default(lv_group_create());

  lv_display_t * disp = lv_sdl_window_create(w, h);

  lv_indev_t * mouse = lv_sdl_mouse_create();
  lv_indev_set_group(mouse, lv_group_get_default());
  lv_indev_set_display(mouse, disp);
  lv_timer_create(mouse_button_timer_cb, 8, NULL);
  lv_display_set_default(disp);

  /* 不再创建 LVGL 软件光标。Windows/SDL 原生指针负责显示，避免局部刷新产生拖影。 */

  SDL_AddEventWatch(simulator_event_watch, NULL);

  return disp;
}
