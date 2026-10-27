#include "platform/pc/ui_pc_input.h"
#include "ui/core/ui_input_queue.h"
#include <assert.h>
#include <stdio.h>

static ui_physical_input_event_t last;
static unsigned records, actions;
static int rotation;
static void collect(const ui_input_message_t *message,void *context)
{
    (void)context;
    assert(message->type==UI_INPUT_PHYSICAL_EVENT);
    last=message->physical;
    ui_input_mapping_t map;
    assert(ui_input_map_physical_event(&last,&map));
    records++; actions+=map.count;
    if(map.action==UI_INPUT_ENCODER_CW) rotation+=(int)map.count;
    if(map.action==UI_INPUT_ENCODER_CCW) rotation-=(int)map.count;
}
static void drain(void) { (void)ui_input_queue_drain(UI_INPUT_QUEUE_MAX,collect,NULL); }
static void key(SDL_Keycode code,uint32_t type,uint8_t repeat)
{
    SDL_Event event={.type=type};
    event.key.keysym.sym=code; event.key.repeat=repeat; event.key.timestamp=1234;
    assert(ui_pc_input_handle_sdl_event(&event));
}
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    ui_input_queue_init();
    key(SDLK_ESCAPE,SDL_KEYDOWN,0); drain();
    assert(records==1 && actions==1 && last.action==UI_PHYSICAL_KEY_CLICK && last.timestamp_ms==1234);
    key(SDLK_ESCAPE,SDL_KEYDOWN,1); key(SDLK_ESCAPE,SDL_KEYUP,0); drain();
    assert(records==2 && actions==1);
    key(SDLK_F5,SDL_KEYDOWN,0); key(SDLK_F5,SDL_KEYDOWN,1); key(SDLK_F5,SDL_KEYUP,0); drain();
    assert(records==3 && actions==2 && last.key==UI_PHYSICAL_KEY_MENU &&
           last.action==UI_PHYSICAL_KEY_DOUBLE_CLICK);
    key(SDLK_F6,SDL_KEYUP,0); drain(); assert(records==3 && actions==2);
    key(SDLK_F6,SDL_KEYDOWN,0); drain(); assert(records==4 && actions==3 && last.action==UI_PHYSICAL_KEY_LONG_START);
    key(SDLK_F6,SDL_KEYDOWN,1); key(SDLK_F6,SDL_KEYUP,0); drain();
    assert(records==5 && actions==3 && last.action==UI_PHYSICAL_KEY_LONG_END);
    key(SDLK_F6,SDL_KEYDOWN,0);
    SDL_Event focus_lost={.type=SDL_WINDOWEVENT};
    focus_lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    focus_lost.window.timestamp=1235;
    assert(!ui_pc_input_handle_sdl_event(&focus_lost));
    key(SDLK_F6,SDL_KEYUP,0); drain();
    assert(records==7 && actions==4 && last.action==UI_PHYSICAL_KEY_LONG_END);
    SDL_Event obsolete_f7={.type=SDL_KEYDOWN}; obsolete_f7.key.keysym.sym=SDLK_F7;
    SDL_Event obsolete_f8={.type=SDL_KEYDOWN}; obsolete_f8.key.keysym.sym=SDLK_F8;
    assert(!ui_pc_input_handle_sdl_event(&obsolete_f7));
    assert(!ui_pc_input_handle_sdl_event(&obsolete_f8));
    key(SDLK_m,SDL_KEYDOWN,0); key(SDLK_RETURN,SDL_KEYDOWN,0); key(SDLK_LEFT,SDL_KEYDOWN,0); key(SDLK_RIGHT,SDL_KEYDOWN,0); drain();
    assert(actions==8 && last.key==UI_PHYSICAL_KEY_NEXT);
    key(SDLK_DOWN,SDL_KEYDOWN,0); key(SDLK_DOWN,SDL_KEYDOWN,1); key(SDLK_UP,SDL_KEYDOWN,0); drain(); assert(rotation==1);
    SDL_Event wheel={.type=SDL_MOUSEWHEEL};
    wheel.wheel.y=-9; assert(ui_pc_input_handle_sdl_event(&wheel)); drain(); assert(rotation==10);
    wheel.wheel.y=4; assert(ui_pc_input_handle_sdl_event(&wheel)); drain(); assert(rotation==6);
    wheel.wheel.y=130; assert(ui_pc_input_handle_sdl_event(&wheel)); drain(); assert(rotation==-124);
    wheel.wheel.y=2; wheel.wheel.direction=SDL_MOUSEWHEEL_FLIPPED;
    assert(ui_pc_input_handle_sdl_event(&wheel)); drain(); assert(rotation==-122);
    for(unsigned i=0;i<20;++i) { key(SDLK_DOWN,SDL_KEYDOWN,0); key(SDLK_UP,SDL_KEYDOWN,0); drain(); }
    assert(rotation==-122);
    unsigned before=actions;
    SDL_Event mouse={.type=SDL_MOUSEBUTTONDOWN}; mouse.button.button=SDL_BUTTON_RIGHT;
    assert(ui_pc_input_handle_sdl_event(&mouse)); mouse.type=SDL_MOUSEBUTTONUP;
    assert(ui_pc_input_handle_sdl_event(&mouse)); drain(); assert(actions==before+1);
    mouse.button.button=SDL_BUTTON_LEFT; assert(!ui_pc_input_handle_sdl_event(&mouse));
    SDL_Event other={.type=SDL_KEYDOWN}; other.key.keysym.sym=SDLK_F1;
    assert(!ui_pc_input_handle_sdl_event(&other));
    ui_physical_input_event_t event={.key=UI_PHYSICAL_KEY_ENCODER,.action=UI_PHYSICAL_ENCODER_DELTA,.encoder_delta=1};
    for(unsigned i=0;i<UI_INPUT_QUEUE_MAX;++i) assert(ui_pc_input_post(&event));
    assert(!ui_pc_input_post(&event) && ui_input_queue_dropped()==1);
    event.key=UI_PHYSICAL_KEY_BACK; event.action=UI_PHYSICAL_KEY_DOUBLE_CLICK; event.encoder_delta=0;
    assert(!ui_pc_input_post(&event) && ui_input_queue_dropped()==2); drain();
    puts("ui_pc_input_test: PASS (SDL shortcuts, no duplicate click, repeat/end, signed batches, overflow, touch separation)");
    return 0;
}
