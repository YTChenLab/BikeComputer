#include "hal.h"
#include "simulator_view.h"
#include "../platform/pc/ui_pc_services.h"
#include "../platform/pc/ui_pc_input.h"
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
  if(event->type == SDL_KEYDOWN && event->key.repeat == 0 &&
     simulator_view_handle_key(event->key.keysym.sym)) return 1;
  if(ui_pc_input_handle_sdl_event(event)) return 1;
  if(event->type == SDL_KEYDOWN && event->key.repeat == 0) {
    SDL_Keycode key = event->key.keysym.sym;
    /* PC development/system shortcuts; these are not physical POWER events. */
    if(key == SDLK_o) post_input(UI_INPUT_OPEN_POWER);
    else if(key == SDLK_p) post_input(UI_INPUT_POWER_TOGGLE);
    else if(key == SDLK_LEFTBRACKET) post_input(UI_INPUT_OPEN_PROFILE);
    else if(key == SDLK_RIGHTBRACKET) post_input(UI_INPUT_OPEN_SETTINGS);
    else if(key == SDLK_n) post_input(UI_INPUT_NAV_DEMO_NEXT);
    else if(key == SDLK_s) (void)ui_pc_services_toggle_stationary();
  }
  return 1;
}

/* 直接采样 SDL 左键状态，连续发送 PRESSING，完整模拟触屏按下与拖动。 */
static void mouse_button_timer_cb(lv_timer_t *timer)
{
  static bool left_was_down = false;
  int mouse_x = 0;
  int mouse_y = 0;
  simulator_view_tick();
  uint32_t buttons = simulator_view_mouse_state(&mouse_x, &mouse_y);
  bool left_is_down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
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

  left_was_down = left_is_down;
  (void)timer;
}


lv_display_t * sdl_hal_init(int32_t w, int32_t h)
{

  lv_group_set_default(lv_group_create());

  simulator_view_prepare();
  lv_display_t * disp = lv_sdl_window_create(w, h);
  simulator_view_init(disp);

  /* Poll the same scaled coordinates as the semantic gesture queue. The
   * stock SDL pointer driver only supports integer zoom factors. */
  lv_indev_t * mouse = lv_indev_create();
  lv_indev_set_type(mouse, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(mouse, simulator_view_pointer_read);
  lv_timer_set_period(lv_indev_get_read_timer(mouse), 8);
  lv_indev_set_group(mouse, lv_group_get_default());
  lv_indev_set_display(mouse, disp);
  lv_timer_create(mouse_button_timer_cb, 8, NULL);
  lv_display_set_default(disp);
  /* Diagnostics float over the product UI; they never affect its geometry. */
#if LV_USE_SYSMON
  lv_obj_t *system_layer = lv_display_get_layer_sys(disp);
  for(uint32_t i = 0; i < lv_obj_get_child_count(system_layer); ++i) {
    lv_obj_t *object = lv_obj_get_child(system_layer, i);
    if(lv_obj_check_type(object, &lv_label_class))
      lv_obj_set_style_bg_opa(object, LV_OPA_30, 0);
  }
#endif
  /* 模拟器默认显示右下角性能信息；UI_SHOW_PERFORMANCE=0 可显式隐藏。 */
#if LV_USE_SYSMON && LV_USE_PERF_MONITOR
  const char *show_performance = SDL_getenv("UI_SHOW_PERFORMANCE");
  if(show_performance != NULL && SDL_strcmp(show_performance, "0") == 0) {
    lv_sysmon_hide_performance(disp);
  }
#endif

  /* 不再创建 LVGL 软件光标。Windows/SDL 原生指针负责显示，避免局部刷新产生拖影。 */

  SDL_AddEventWatch(simulator_event_watch, NULL);

  return disp;
}
