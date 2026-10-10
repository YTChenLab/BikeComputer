#include "ui_pc_input.h"
#include "ui/core/ui_input_queue.h"

bool ui_pc_input_post(const ui_physical_input_event_t *event)
{
    ui_input_mapping_t mapping;
    if(!ui_input_map_physical_event(event, &mapping)) return false;
    const ui_input_message_t message = {
        .type = UI_INPUT_PHYSICAL_EVENT, .physical = *event
    };
    return ui_input_queue_post(&message);
}

static bool post(uint8_t key, uint8_t action, int32_t delta, uint32_t timestamp)
{
    const ui_physical_input_event_t event = {
        .key = key, .action = action, .encoder_delta = delta, .timestamp_ms = timestamp
    };
    return ui_pc_input_post(&event);
}

static void post_wheel(const SDL_MouseWheelEvent *wheel)
{
    int64_t delta = -(int64_t)wheel->y;
    if(wheel->direction == SDL_MOUSEWHEEL_FLIPPED) delta = -delta;
    while(delta != 0) {
        int32_t batch = delta > UI_PHYSICAL_ENCODER_MAX_DELTA ? UI_PHYSICAL_ENCODER_MAX_DELTA :
            delta < -UI_PHYSICAL_ENCODER_MAX_DELTA ? -UI_PHYSICAL_ENCODER_MAX_DELTA : (int32_t)delta;
        if(!post(UI_PHYSICAL_KEY_ENCODER, UI_PHYSICAL_ENCODER_DELTA, batch, wheel->timestamp)) break;
        delta -= batch;
    }
}

bool ui_pc_input_handle_sdl_event(const SDL_Event *event)
{
    if(!event) return false;
    if(event->type == SDL_MOUSEWHEEL) {
        post_wheel(&event->wheel);
        return true;
    }
    if((event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) &&
       event->button.button == SDL_BUTTON_RIGHT) {
        /* Right click is one recognized CLICK. Use F5 for DOUBLE_CLICK;
         * SDL's click count would first deliver an unwanted single click. */
        if(event->type == SDL_MOUSEBUTTONUP)
            (void)post(UI_PHYSICAL_KEY_BACK, UI_PHYSICAL_KEY_CLICK, 0, event->button.timestamp);
        return true;
    }
    if(event->type != SDL_KEYDOWN && event->type != SDL_KEYUP) return false;
    const SDL_KeyboardEvent *keyboard = &event->key;
    bool down = event->type == SDL_KEYDOWN;
    uint8_t key;
    switch(keyboard->keysym.sym) {
        case SDLK_UP: case SDLK_DOWN:
            if(down) (void)post(UI_PHYSICAL_KEY_ENCODER, UI_PHYSICAL_ENCODER_DELTA,
                keyboard->keysym.sym == SDLK_DOWN ? 1 : -1, keyboard->timestamp);
            return true;
        case SDLK_ESCAPE: case SDLK_BACKSPACE: key = UI_PHYSICAL_KEY_BACK; break;
        case SDLK_LEFT: key = UI_PHYSICAL_KEY_PREV; break;
        case SDLK_RIGHT: key = UI_PHYSICAL_KEY_NEXT; break;
        case SDLK_m: key = UI_PHYSICAL_KEY_MENU; break;
        case SDLK_RETURN: case SDLK_KP_ENTER: key = UI_PHYSICAL_KEY_ENCODER_BUTTON; break;
        case SDLK_F5:
            if(down && !keyboard->repeat)
                (void)post(UI_PHYSICAL_KEY_BACK, UI_PHYSICAL_KEY_DOUBLE_CLICK, 0, keyboard->timestamp);
            return true;
        case SDLK_F6: case SDLK_F7: case SDLK_F8:
            key = keyboard->keysym.sym == SDLK_F6 ? UI_PHYSICAL_KEY_BACK :
                  keyboard->keysym.sym == SDLK_F7 ? UI_PHYSICAL_KEY_MENU : UI_PHYSICAL_KEY_ENCODER_BUTTON;
            (void)post(key, !down ? UI_PHYSICAL_KEY_LONG_END :
                keyboard->repeat ? UI_PHYSICAL_KEY_LONG_REPEAT : UI_PHYSICAL_KEY_LONG_START,
                0, keyboard->timestamp);
            return true;
        default: return false;
    }
    if(!down || !keyboard->repeat)
        (void)post(key, down ? UI_PHYSICAL_KEY_CLICK : UI_PHYSICAL_KEY_UP, 0, keyboard->timestamp);
    return true;
}
