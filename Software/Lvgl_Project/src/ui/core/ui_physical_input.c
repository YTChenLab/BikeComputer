#include "ui_input.h"
#include <stddef.h>

_Static_assert(sizeof(ui_physical_input_event_t) == 12, "Input queue ABI must be 12 bytes");
_Static_assert(offsetof(ui_physical_input_event_t, encoder_delta) == 4, "Input delta ABI");
_Static_assert(offsetof(ui_physical_input_event_t, timestamp_ms) == 8, "Input time ABI");

bool ui_input_map_physical_event(const ui_physical_input_event_t *event,
                                 ui_input_mapping_t *mapping)
{
    if(!event || !mapping || event->reserved != 0 ||
       event->key <= UI_PHYSICAL_KEY_NONE || event->key >= UI_PHYSICAL_KEY_COUNT ||
       event->action <= UI_PHYSICAL_ACTION_NONE || event->action >= UI_PHYSICAL_ACTION_COUNT)
        return false;

    mapping->action = UI_INPUT_COUNT;
    mapping->count = 0;
    if(event->action == UI_PHYSICAL_ENCODER_DELTA) {
        if(event->key != UI_PHYSICAL_KEY_ENCODER ||
           event->encoder_delta > UI_PHYSICAL_ENCODER_MAX_DELTA ||
           event->encoder_delta < -UI_PHYSICAL_ENCODER_MAX_DELTA) return false;
        mapping->action = event->encoder_delta < 0 ? UI_INPUT_ENCODER_CCW : UI_INPUT_ENCODER_CW;
        mapping->count = (uint32_t)(event->encoder_delta < 0 ? -event->encoder_delta : event->encoder_delta);
        return true;
    }
    if(event->key == UI_PHYSICAL_KEY_ENCODER || event->encoder_delta != 0) return false;
    if(event->key == UI_PHYSICAL_KEY_POWER) return true;

    if(event->action == UI_PHYSICAL_KEY_CLICK) {
        switch(event->key) {
            case UI_PHYSICAL_KEY_BACK: mapping->action = UI_INPUT_BACK; break;
            case UI_PHYSICAL_KEY_PREV: mapping->action = UI_INPUT_PAGE_PREV; break;
            case UI_PHYSICAL_KEY_NEXT: mapping->action = UI_INPUT_PAGE_NEXT; break;
            case UI_PHYSICAL_KEY_MENU: mapping->action = UI_INPUT_TOGGLE_CONTROL_CENTER; break;
            case UI_PHYSICAL_KEY_ENCODER_BUTTON: mapping->action = UI_INPUT_ENCODER_PRESS; break;
            default: break;
        }
    }
    else if(event->action == UI_PHYSICAL_KEY_DOUBLE_CLICK && event->key == UI_PHYSICAL_KEY_BACK)
        mapping->action = UI_INPUT_BACK_DOUBLE;
    else if(event->action == UI_PHYSICAL_KEY_LONG_START) {
        if(event->key == UI_PHYSICAL_KEY_BACK) mapping->action = UI_INPUT_BACK_LONG;
        else if(event->key == UI_PHYSICAL_KEY_MENU) mapping->action = UI_INPUT_RIDE_TOGGLE;
        else if(event->key == UI_PHYSICAL_KEY_ENCODER_BUTTON) mapping->action = UI_INPUT_TOGGLE_CONTROL_CENTER;
    }
    if(mapping->action != UI_INPUT_COUNT) mapping->count = 1;
    return true;
}
