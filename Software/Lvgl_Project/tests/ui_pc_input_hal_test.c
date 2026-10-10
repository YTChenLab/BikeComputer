#include "hal/hal.h"
#include "platform/pc/ui_pc_services.h"
#include "ui/ui.h"
#include "ui/core/ui_input_queue.h"
#include "ui/core/ui_router.h"
#include "ui/core/ui_state_machine.h"
#include "ui/core/ui_system_control_shade.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>

static uint32_t now;
static uint32_t clock_ms(void) { return now; }
static void advance(uint32_t elapsed)
{
    for(uint32_t i=0;i<elapsed;i+=10) { now+=10; (void)lv_timer_handler(); }
}
static void key(SDL_Keycode code,uint32_t type,uint8_t repeat)
{
    SDL_Event event={.type=type};
    event.key.keysym.sym=code; event.key.repeat=repeat; event.key.timestamp=now;
    assert(SDL_PushEvent(&event)==1);
    advance(250);
}
int main(int argc,char **argv)
{
    (void)argc; (void)argv;
    lv_init();
    assert(sdl_hal_init(480,854));
    lv_tick_set_cb(clock_ms);
    ui_pc_services_register();
    ui_init();
    advance(7000);
    assert(ui_router_current()==UI_ROUTE_HOME);
    unsigned pointers=0;
    for(lv_indev_t *indev=lv_indev_get_next(NULL);indev;indev=lv_indev_get_next(indev)) {
        assert(lv_indev_get_type(indev)==LV_INDEV_TYPE_POINTER);
        ++pointers;
    }
    assert(pointers==1); /* No keypad/encoder indev can deliver the event twice. */
    key(SDLK_RIGHT,SDL_KEYDOWN,0); assert(ui_router_current()==UI_ROUTE_NAVIGATION);
    key(SDLK_LEFT,SDL_KEYDOWN,0); assert(ui_router_current()==UI_ROUTE_HOME);
    key(SDLK_m,SDL_KEYDOWN,0); advance(500);
    assert(ui_system_control_shade_state()==UI_SYSTEM_CONTROL_SHADE_COMPACT);
    key(SDLK_ESCAPE,SDL_KEYDOWN,0); advance(500);
    assert(ui_system_control_shade_state()==UI_SYSTEM_CONTROL_SHADE_CLOSED);
    key(SDLK_F7,SDL_KEYDOWN,0); key(SDLK_F7,SDL_KEYUP,0); advance(1500);
    assert(ui_state_machine_state()==UI_RIDE_STATE_GPS_READY);
    key(SDLK_F7,SDL_KEYDOWN,0); key(SDLK_F7,SDL_KEYUP,0); advance(300);
    assert(ui_state_machine_state()==UI_RIDE_STATE_RIDING);
    key(SDLK_F5,SDL_KEYDOWN,0); key(SDLK_F5,SDL_KEYUP,0); advance(300);
    assert(ui_state_machine_state()==UI_RIDE_STATE_PAUSED);
    key(SDLK_F5,SDL_KEYDOWN,1); assert(ui_state_machine_state()==UI_RIDE_STATE_PAUSED);
    key(SDLK_F5,SDL_KEYDOWN,0); advance(300);
    assert(ui_state_machine_state()==UI_RIDE_STATE_RIDING);
    key(SDLK_F6,SDL_KEYDOWN,0); assert(ui_state_machine_state()==UI_RIDE_STATE_FINISHING);
    key(SDLK_F6,SDL_KEYDOWN,1); key(SDLK_F6,SDL_KEYUP,0);
    assert(ui_state_machine_state()==UI_RIDE_STATE_FINISHING); /* No release CLICK. */
    key(SDLK_ESCAPE,SDL_KEYDOWN,0); assert(ui_state_machine_state()==UI_RIDE_STATE_RIDING);
    assert(ui_input_queue_pending()==0 && ui_input_queue_dropped()==0);
    puts("ui_pc_input_hal_test: PASS (real SDL event watch, one pointer, pages/control shade, ride/double/long exclusivity)");
    return 0;
}
