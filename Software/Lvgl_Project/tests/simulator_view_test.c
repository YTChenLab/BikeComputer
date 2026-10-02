#include "hal/simulator_view.h"
#include <assert.h>
#include <stdio.h>

static void pump(lv_display_t *display)
{
    for(int i = 0; i < 8; ++i) {
        SDL_Delay(6);
        lv_timer_handler();
    }
    assert(lv_display_get_horizontal_resolution(display) == 480);
    assert(lv_display_get_vertical_resolution(display) == 854);
    lv_refr_now(display);
}

static void assert_window_size(SDL_Window *window, int width, int height)
{
    int actual_width, actual_height;
    SDL_GetWindowSize(window, &actual_width, &actual_height);
    assert(actual_width == width && actual_height == height);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    /* Independent working directory: never touch the rider's preferences. */
    (void)remove("runtime/simulator_view.ini");
    SDL_setenv("UI_SIMULATOR_DPI", "96", 1);
    SDL_setenv("UI_SIMULATOR_MODE", "compact", 1);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    lv_init();
    simulator_view_prepare();
    lv_display_t *display = lv_sdl_window_create(480, 854);
    assert(display != NULL);
    SDL_Window *window = SDL_RenderGetWindow(lv_sdl_window_get_renderer(display));
    assert(window != NULL);
    simulator_view_init(display);
    pump(display);
    assert_window_size(window, 240, 427);

    /* A real SDL resize must not resize/reallocate the virtual LCD. */
    SDL_SetWindowSize(window, 192, 342);
    pump(display);
    assert_window_size(window, 192, 342);
    assert(simulator_view_handle_key(SDLK_F1));
    simulator_view_tick();
    pump(display);
    assert_window_size(window, 141, 251);
    SDL_WarpMouseInWindow(window, 70, 125);
    SDL_PumpEvents();
    int x, y;
    (void)simulator_view_mouse_state(&x, &y);
    assert(x >= 235 && x <= 241);
    assert(y >= 421 && y <= 427);
    lv_indev_data_t pointer = {0};
    simulator_view_pointer_read(NULL, &pointer);
    assert(pointer.point.x == x && pointer.point.y == y);
    assert(simulator_view_handle_key(SDLK_PLUS));
    simulator_view_tick();
    pump(display);
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    assert(width > 141 && height > 251);
    FILE *saved = fopen("runtime/simulator_view.ini", "r");
    assert(saved != NULL);
    fclose(saved);
    assert(simulator_view_handle_key(SDLK_F2));
    simulator_view_tick();
    pump(display);
    assert_window_size(window, 240, 427);
    SDL_WarpMouseInWindow(window, 239, 426);
    SDL_PumpEvents();
    (void)simulator_view_mouse_state(&x, &y);
    assert(x == 478 && y == 852);
    assert(simulator_view_handle_key(SDLK_F3));
    simulator_view_tick();
    pump(display);
    SDL_GetWindowSize(window, &width, &height);
    assert(width > 240 && width <= 480);
    assert(height > 427 && height <= 854);
    assert(!simulator_view_handle_key(SDLK_RETURN));
    (void)remove("runtime/simulator_view.ini");
    lv_deinit();
    puts("Simulator resize, fixed LCD resolution, calibration and pointer mapping passed.");
    return 0;
}
