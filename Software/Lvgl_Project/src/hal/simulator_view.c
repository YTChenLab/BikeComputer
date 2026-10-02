#include "simulator_view.h"
#include <math.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

enum { VIEW_PHYSICAL = 1, VIEW_COMPACT, VIEW_NATIVE, VIEW_CUSTOM };
static SDL_Window *s_window;
static lv_display_t *s_display;
static uint32_t s_window_id;
static SDL_atomic_t s_requested_mode;
static SDL_atomic_t s_adjustment;
static SDL_EventFilter s_previous_filter;
static void *s_previous_userdata;
static int s_mode = VIEW_PHYSICAL;
static int s_monitor = -1;
static double s_scale = 1.0;
static double s_calibration = 1.0;
static double s_custom_scale = 0.5;

static bool valid_number(double value, double low, double high)
{
    return isfinite(value) && value >= low && value <= high;
}

static double monitor_dpi(void)
{
    const char *override = SDL_getenv("UI_SIMULATOR_DPI");
    if(override) {
        char *end;
        double value = SDL_strtod(override, &end);
        if(*end == '\0' && valid_number(value, 40.0, 600.0)) return value;
    }
    float dpi = 96.0f;
    if(SDL_GetDisplayDPI(s_monitor, &dpi, NULL, NULL) != 0 ||
       !valid_number(dpi, 40.0, 600.0)) dpi = 96.0f;
    /* SDL's reported DPI is only an estimate. +/- provides ruler calibration. */
    return dpi;
}

static void load_preferences(void)
{
    FILE *file = fopen("runtime/simulator_view.ini", "r");
    if(!file) return;
    int mode;
    double calibration, custom_scale;
    if(fscanf(file, "mode=%d\ncalibration=%lf\nscale=%lf",
              &mode, &calibration, &custom_scale) == 3 &&
       mode >= VIEW_PHYSICAL && mode <= VIEW_CUSTOM &&
       valid_number(calibration, 0.25, 4.0) &&
       valid_number(custom_scale, 0.1, 2.0)) {
        s_mode = mode;
        s_calibration = calibration;
        s_custom_scale = custom_scale;
    }
    fclose(file);
}

static void save_preferences(void)
{
#ifdef _WIN32
    (void)_mkdir("runtime");
#else
    (void)mkdir("runtime", 0755);
#endif
    FILE *file = fopen("runtime/simulator_view.ini", "w");
    if(!file) return;
    fprintf(file, "mode=%d\ncalibration=%.8f\nscale=%.8f\n",
            s_mode, s_calibration, s_custom_scale);
    fclose(file);
}

/* The stock driver treats RESIZED as a new LCD resolution. Keep the event
 * for other observers, but prevent it from reallocating/changing the LCD.
 * No SDL/LVGL operations here: SDL may invoke filters on another thread. */
static int SDLCALL fixed_resolution_filter(void *userdata, SDL_Event *event)
{
    (void)userdata;
    if(s_previous_filter && !s_previous_filter(s_previous_userdata, event)) return 0;
    if(event->type == SDL_WINDOWEVENT && event->window.windowID == s_window_id &&
       event->window.event == SDL_WINDOWEVENT_RESIZED)
        event->window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    return 1;
}

static void apply_size(void)
{
    int width = lv_display_get_horizontal_resolution(s_display);
    int height = lv_display_get_vertical_resolution(s_display);
    double scale = s_mode == VIEW_PHYSICAL ?
        3.0 * monitor_dpi() * s_calibration / hypot(width, height) :
        s_mode == VIEW_COMPACT ? 0.5 :
        s_mode == VIEW_NATIVE ? 1.0 : s_custom_scale;
    SDL_Rect bounds;
    int top = 0, left = 0, bottom = 0, right = 0;
    (void)SDL_GetWindowBordersSize(s_window, &top, &left, &bottom, &right);
    if(SDL_GetDisplayUsableBounds(s_monitor, &bounds) == 0) {
        /* Keep the complete window inside the usable desktop. */
        double maximum = fmin((bounds.w - left - right - 24.0) / width,
                              (bounds.h - top - bottom - 24.0) / height);
        if(maximum > 0.0) scale = fmin(scale, maximum);
    }
    s_scale = fmax(0.1, fmin(2.0, scale));
    SDL_SetWindowSize(s_window, (int)lround(width * s_scale),
                      (int)lround(height * s_scale));
    if(SDL_GetDisplayUsableBounds(s_monitor, &bounds) == 0) {
        int x, y, w, h;
        SDL_GetWindowPosition(s_window, &x, &y);
        SDL_GetWindowSize(s_window, &w, &h);
        x = LV_CLAMP(bounds.x + left, x, bounds.x + bounds.w - w - right);
        y = LV_CLAMP(bounds.y + top, y, bounds.y + bounds.h - h - bottom);
        SDL_SetWindowPosition(s_window, x, y);
    }
    const char *mode = s_mode == VIEW_PHYSICAL ? "3in ~" :
                       s_mode == VIEW_COMPACT ? "Compact" :
                       s_mode == VIEW_NATIVE ? "Native" : "Custom";
    char title[180];
    SDL_snprintf(title, sizeof(title),
                 "BikeComputer | %s %.0f%% | F1:3in F2:50%% F3:100%% +/-",
                 mode, s_scale * 100.0);
    SDL_SetWindowTitle(s_window, title);
    lv_obj_invalidate(lv_screen_active());
}

void simulator_view_prepare(void)
{
#ifdef SDL_HINT_WINDOWS_DPI_AWARENESS
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
}

void simulator_view_init(lv_display_t *display)
{
    s_display = display;
    SDL_Renderer *renderer = lv_sdl_window_get_renderer(display);
    s_window = SDL_RenderGetWindow(renderer);
    if(!s_window) return;
    s_window_id = SDL_GetWindowID(s_window);
    s_monitor = SDL_GetWindowDisplayIndex(s_window);
    SDL_GetEventFilter(&s_previous_filter, &s_previous_userdata);
    SDL_SetEventFilter(fixed_resolution_filter, NULL);
    lv_sdl_window_set_resizeable(display, false);
    load_preferences();
    const char *mode = SDL_getenv("UI_SIMULATOR_MODE");
    if(mode && SDL_strcmp(mode, "physical") == 0) s_mode = VIEW_PHYSICAL;
    else if(mode && SDL_strcmp(mode, "compact") == 0) s_mode = VIEW_COMPACT;
    else if(mode && SDL_strcmp(mode, "native") == 0) s_mode = VIEW_NATIVE;
    apply_size();
}

bool simulator_view_handle_key(SDL_Keycode key)
{
    if(key == SDLK_F1) SDL_AtomicSet(&s_requested_mode, VIEW_PHYSICAL);
    else if(key == SDLK_F2) SDL_AtomicSet(&s_requested_mode, VIEW_COMPACT);
    else if(key == SDLK_F3) SDL_AtomicSet(&s_requested_mode, VIEW_NATIVE);
    else if(key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS)
        SDL_AtomicAdd(&s_adjustment, 1);
    else if(key == SDLK_MINUS || key == SDLK_KP_MINUS)
        SDL_AtomicAdd(&s_adjustment, -1);
    else return false;
    return true;
}

void simulator_view_tick(void)
{
    if(!s_window) return;
    int mode = SDL_AtomicSet(&s_requested_mode, 0);
    int adjustment = SDL_AtomicSet(&s_adjustment, 0);
    int monitor = SDL_GetWindowDisplayIndex(s_window);
    bool changed_monitor = monitor >= 0 && monitor != s_monitor;
    if(changed_monitor) s_monitor = monitor;
    if(mode) s_mode = mode;
    if(adjustment) {
        double factor = pow(1.025, adjustment);
        if(s_mode == VIEW_PHYSICAL)
            s_calibration = fmax(0.25, fmin(4.0, s_calibration * factor));
        else {
            s_mode = VIEW_CUSTOM;
            s_custom_scale = fmax(0.1, fmin(2.0, s_scale * factor));
        }
    }
    if(mode || adjustment || changed_monitor) apply_size();
    if(mode || adjustment) save_preferences();
}

uint32_t simulator_view_mouse_state(int *x, int *y)
{
    int mouse_x, mouse_y, width = 0, height = 0;
    uint32_t buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    if(s_window) SDL_GetWindowSize(s_window, &width, &height);
    int lcd_width = s_display ? lv_display_get_horizontal_resolution(s_display) : 480;
    int lcd_height = s_display ? lv_display_get_vertical_resolution(s_display) : 854;
    *x = width > 0 ? LV_CLAMP(0, (int)((int64_t)mouse_x * lcd_width / width), lcd_width - 1) : 0;
    *y = height > 0 ? LV_CLAMP(0, (int)((int64_t)mouse_y * lcd_height / height), lcd_height - 1) : 0;
    /* Losing focus releases touch/back rather than leaving a stuck press. */
    return SDL_GetMouseFocus() == s_window ? buttons : 0;
}

void simulator_view_pointer_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    int x, y;
    uint32_t buttons = simulator_view_mouse_state(&x, &y);
    data->point.x = x;
    data->point.y = y;
    data->state = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) ?
                  LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    (void)indev;
}
