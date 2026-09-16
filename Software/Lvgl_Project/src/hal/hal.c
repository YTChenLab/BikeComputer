#include "hal.h"
#include "../platform/pc/ui_pc_services.h"
#include "../ui/core/ui_back_key_gesture.h"
#include "../ui/core/ui_input_queue.h"
#include <SDL.h>

static void post_input(ui_input_event_t type)
{
  const ui_input_message_t message = {.type = type, .x = 0, .y = 0};
  (void)ui_input_queue_post(&message);
}

/* PC 专用适配：滚轮只产生语义化编码器事件，UI 不依赖 SDL。 */
static int SDLCALL simulator_event_watch(void *userdata, SDL_Event *event)
{
  (void)userdata;
  if(event->type == SDL_MOUSEWHEEL && event->wheel.y != 0) {
    post_input(event->wheel.y > 0 ?
               UI_INPUT_ENCODER_CCW : UI_INPUT_ENCODER_CW);
  }
  else if(event->type == SDL_KEYDOWN && event->key.repeat == 0) {
    SDL_Keycode key = event->key.keysym.sym;
    if(key == SDLK_DOWN) post_input(UI_INPUT_ENCODER_CW);
    else if(key == SDLK_UP) post_input(UI_INPUT_ENCODER_CCW);
    else if(key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      post_input(UI_INPUT_ENCODER_PRESS);
    }
    else if(key == SDLK_LEFT) post_input(UI_INPUT_PAGE_PREV);
    else if(key == SDLK_RIGHT) post_input(UI_INPUT_PAGE_NEXT);
    else if(key == SDLK_o) post_input(UI_INPUT_OPEN_POWER);
    else if(key == SDLK_m) post_input(UI_INPUT_TOGGLE_CONTROL_CENTER);
    else if(key == SDLK_p) post_input(UI_INPUT_POWER_TOGGLE);
    else if(key == SDLK_LEFTBRACKET) post_input(UI_INPUT_OPEN_PROFILE);
    else if(key == SDLK_RIGHTBRACKET) post_input(UI_INPUT_OPEN_SETTINGS);
    else if(key == SDLK_s) (void)ui_pc_services_toggle_stationary();
  }
  return 1;
}

/* 直接采样 SDL 左键状态，连续发送 PRESSING，完整模拟触屏按下与拖动。 */
static void mouse_button_timer_cb(lv_timer_t *timer)
{
  static bool left_was_down = false;
  static bool back_gesture_initialized = false;
  static ui_back_key_gesture_t back_gesture;
  int mouse_x = 0;
  int mouse_y = 0;
  uint32_t buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
  lv_display_t *display = lv_display_get_default();
  if(display) {
    mouse_x = LV_CLAMP(0, mouse_x, lv_display_get_horizontal_resolution(display) - 1);
    mouse_y = LV_CLAMP(0, mouse_y, lv_display_get_vertical_resolution(display) - 1);
  }
  bool left_is_down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
  const uint8_t *keyboard = SDL_GetKeyboardState(NULL);
  bool right_is_down = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
  bool keyboard_back_is_down = keyboard != NULL &&
    (keyboard[SDL_SCANCODE_ESCAPE] || keyboard[SDL_SCANCODE_BACKSPACE]);
  bool back_is_down = right_is_down || keyboard_back_is_down;

  if(left_is_down && !left_was_down) {
    const ui_input_pointer_event_t input = {
      .type = UI_INPUT_TOUCH_BEGIN, .x = mouse_x, .y = mouse_y
    };
    (void)ui_input_queue_post(&input);
  }
  else if(left_is_down) {
    const ui_input_pointer_event_t input = {
      .type = UI_INPUT_TOUCH_MOVE, .x = mouse_x, .y = mouse_y
    };
    (void)ui_input_queue_post(&input);
  }
  else if(left_was_down) {
    const ui_input_pointer_event_t input = {
      .type = UI_INPUT_TOUCH_END, .x = mouse_x, .y = mouse_y
    };
    (void)ui_input_queue_post(&input);
  }

  if(!back_gesture_initialized) {
    ui_back_key_gesture_init(&back_gesture);
    back_gesture_initialized = true;
  }
  ui_back_key_result_t back_result = ui_back_key_gesture_update(
    &back_gesture, back_is_down, SDL_GetTicks());
  if(back_result == UI_BACK_KEY_RESULT_SINGLE)
    post_input(UI_INPUT_BACK);
  else if(back_result == UI_BACK_KEY_RESULT_DOUBLE)
    post_input(UI_INPUT_BACK_DOUBLE);
  else if(back_result == UI_BACK_KEY_RESULT_LONG)
    post_input(UI_INPUT_BACK_LONG);
  left_was_down = left_is_down;
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
