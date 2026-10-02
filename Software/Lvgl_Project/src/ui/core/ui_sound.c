#include "ui_sound.h"

typedef struct
{
    ui_sound_backend_cb_t backend;
    void *backend_user_data;
    uint8_t volume;
    bool enabled;
} ui_sound_context_t;

static ui_sound_context_t s_sound;

void ui_sound_init(void)
{
    s_sound.volume = 62;
    s_sound.enabled = true;
}

void ui_sound_set_backend(ui_sound_backend_cb_t callback, void *user_data)
{
    s_sound.backend = callback;
    s_sound.backend_user_data = user_data;
}

void ui_sound_set_enabled(bool enabled)
{
    s_sound.enabled = enabled;
}

bool ui_sound_is_enabled(void)
{
    return s_sound.enabled;
}

void ui_sound_set_volume(uint8_t volume)
{
    s_sound.volume = volume > 100 ? 100 : volume;
}

uint8_t ui_sound_get_volume(void)
{
    return s_sound.volume;
}

void ui_sound_play(ui_sound_event_t event)
{
    if(!s_sound.enabled || event >= UI_SFX_COUNT || s_sound.backend == NULL) return;
    s_sound.backend(event, s_sound.volume, s_sound.backend_user_data);
}
