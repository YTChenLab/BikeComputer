#ifndef UI_SOUND_H
#define UI_SOUND_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UI_SFX_FOCUS = 0,
    UI_SFX_PRESS,
    UI_SFX_CONFIRM,
    UI_SFX_BACK,
    UI_SFX_SUCCESS,
    UI_SFX_WARNING,
    UI_SFX_CRITICAL,
    UI_SFX_NAV,
    UI_SFX_COUNT
} ui_sound_event_t;

typedef void (*ui_sound_backend_cb_t)(ui_sound_event_t event,
                                      uint8_t volume,
                                      void *user_data);

void ui_sound_init(void);
void ui_sound_set_backend(ui_sound_backend_cb_t callback, void *user_data);
void ui_sound_set_enabled(bool enabled);
bool ui_sound_is_enabled(void);
void ui_sound_set_volume(uint8_t volume);
uint8_t ui_sound_get_volume(void);
void ui_sound_play(ui_sound_event_t event);

#endif /* UI_SOUND_H */
