#include "ui/core/ui_input.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

static void check(uint8_t key, uint8_t action, ui_input_event_t expected, uint32_t count)
{
    /* Timestamp zero/wrap/reversal cannot change a recognized outcome. */
    const uint32_t timestamps[] = {0, UINT32_MAX, 25, 1200, 280, 1};
    for(unsigned i = 0; i < sizeof(timestamps) / sizeof(timestamps[0]); ++i) {
        ui_physical_input_event_t event = {.key=key,.action=action,.timestamp_ms=timestamps[i]};
        ui_input_mapping_t mapping;
        assert(ui_input_map_physical_event(&event,&mapping));
        assert(mapping.action==expected && mapping.count==count);
    }
}

int main(void)
{
    check(UI_PHYSICAL_KEY_BACK,UI_PHYSICAL_KEY_CLICK,UI_INPUT_BACK,1);
    check(UI_PHYSICAL_KEY_BACK,UI_PHYSICAL_KEY_DOUBLE_CLICK,UI_INPUT_BACK_DOUBLE,1);
    check(UI_PHYSICAL_KEY_BACK,UI_PHYSICAL_KEY_LONG_START,UI_INPUT_BACK_LONG,1);
    check(UI_PHYSICAL_KEY_MENU,UI_PHYSICAL_KEY_CLICK,UI_INPUT_TOGGLE_CONTROL_CENTER,1);
    check(UI_PHYSICAL_KEY_MENU,UI_PHYSICAL_KEY_LONG_START,UI_INPUT_RIDE_TOGGLE,1);
    check(UI_PHYSICAL_KEY_ENCODER_BUTTON,UI_PHYSICAL_KEY_CLICK,UI_INPUT_ENCODER_PRESS,1);
    check(UI_PHYSICAL_KEY_ENCODER_BUTTON,UI_PHYSICAL_KEY_LONG_START,UI_INPUT_TOGGLE_CONTROL_CENTER,1);
    check(UI_PHYSICAL_KEY_PREV,UI_PHYSICAL_KEY_CLICK,UI_INPUT_PAGE_PREV,1);
    check(UI_PHYSICAL_KEY_NEXT,UI_PHYSICAL_KEY_CLICK,UI_INPUT_PAGE_NEXT,1);
    for(uint8_t key=UI_PHYSICAL_KEY_POWER;key<UI_PHYSICAL_KEY_ENCODER;++key) {
        check(key,UI_PHYSICAL_KEY_DOWN,UI_INPUT_COUNT,0);
        check(key,UI_PHYSICAL_KEY_UP,UI_INPUT_COUNT,0);
        check(key,UI_PHYSICAL_KEY_LONG_REPEAT,UI_INPUT_COUNT,0);
        check(key,UI_PHYSICAL_KEY_LONG_END,UI_INPUT_COUNT,0);
        if(key!=UI_PHYSICAL_KEY_BACK) check(key,UI_PHYSICAL_KEY_DOUBLE_CLICK,UI_INPUT_COUNT,0);
    }
    for(uint8_t action=UI_PHYSICAL_KEY_DOWN;action<=UI_PHYSICAL_KEY_LONG_END;++action)
        check(UI_PHYSICAL_KEY_POWER,action,UI_INPUT_COUNT,0);
    ui_input_mapping_t mapping;
    ui_physical_input_event_t event={.key=UI_PHYSICAL_KEY_ENCODER,.action=UI_PHYSICAL_ENCODER_DELTA};
    for(int32_t delta=-UI_PHYSICAL_ENCODER_MAX_DELTA;delta<=UI_PHYSICAL_ENCODER_MAX_DELTA;++delta) {
        event.encoder_delta=delta;
        assert(ui_input_map_physical_event(&event,&mapping));
        assert(mapping.count==(uint32_t)(delta<0?-delta:delta));
        assert(mapping.action==(delta<0?UI_INPUT_ENCODER_CCW:UI_INPUT_ENCODER_CW));
    }
    const int32_t invalid_delta[]={65,-65,INT_MIN,INT_MAX};
    for(unsigned i=0;i<4;++i) { event.encoder_delta=invalid_delta[i]; assert(!ui_input_map_physical_event(&event,&mapping)); }
    event.encoder_delta=1; event.key=UI_PHYSICAL_KEY_BACK;
    assert(!ui_input_map_physical_event(&event,&mapping));
    event.action=UI_PHYSICAL_KEY_CLICK;
    assert(!ui_input_map_physical_event(&event,&mapping));
    event.encoder_delta=0; event.reserved=1;
    assert(!ui_input_map_physical_event(&event,&mapping));
    event.reserved=0; event.key=UI_PHYSICAL_KEY_COUNT;
    assert(!ui_input_map_physical_event(&event,&mapping));
    event.key=UI_PHYSICAL_KEY_NONE;
    assert(!ui_input_map_physical_event(&event,&mapping));
    event.key=UI_PHYSICAL_KEY_BACK; event.action=UI_PHYSICAL_ACTION_NONE;
    assert(!ui_input_map_physical_event(&event,&mapping));
    event.action=UINT8_MAX;
    assert(!ui_input_map_physical_event(&event,&mapping));
    assert(!ui_input_map_physical_event(NULL,&mapping));
    assert(!ui_input_map_physical_event(&event,NULL));
    puts("ui_physical_input_test: PASS (semantic exclusivity, timestamp independence, all detents, invalid records)");
    return 0;
}
