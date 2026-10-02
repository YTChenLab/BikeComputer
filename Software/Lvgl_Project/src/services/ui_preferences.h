#ifndef UI_PREFERENCES_H
#define UI_PREFERENCES_H
#include <stdbool.h>
#include <stdint.h>
#include "ui/models/ui_dashboard_layout.h"
#define UI_PREFERENCES_BLOB_SIZE 128U
typedef struct {
    char user_name[32], tagline[48];
    uint16_t heart_zone_bounds[4], power_zone_bounds[4];
    uint32_t idle_sleep_ms; /* 0 disables automatic display sleep. */
    uint8_t brightness, sound;
    bool auto_pause, reduced_motion;
    uint8_t dashboard_fields[UI_DASHBOARD_SLOT_COUNT], dashboard_preset;
    uint32_t revision;
} ui_preferences_t;
typedef struct {
    bool (*load)(uint8_t blob[UI_PREFERENCES_BLOB_SIZE], void *context);
    bool (*store)(const uint8_t blob[UI_PREFERENCES_BLOB_SIZE], void *context);
    bool (*request_worker)(void *context); /* Nonblocking UI-thread notification. */
    void *context;
} ui_preferences_port_t;
bool ui_preferences_register(const ui_preferences_port_t *port);
/* Startup only, before producers. Missing/corrupt files use explicit defaults. */
void ui_preferences_init(void);
bool ui_preferences_get(ui_preferences_t *preferences);
/* Task-safe. Names/units/zones are copied; observers run only during process. */
bool ui_preferences_submit(const ui_preferences_t *preferences);
/* UI-thread. Returns true when a new configuration should be applied. */
bool ui_preferences_process(uint32_t now_ms, ui_preferences_t *preferences);
/* One storage task. Saves a snapshot; changes arriving during I/O stay dirty. */
void ui_preferences_worker(void);
bool ui_preferences_dirty(void);
bool ui_preferences_last_save_ok(void);
typedef enum { UI_PREF_STORAGE_UNCONFIGURED, UI_PREF_STORAGE_DEFAULT,
    UI_PREF_STORAGE_READY, UI_PREF_STORAGE_PENDING, UI_PREF_STORAGE_FAILED } ui_preferences_storage_state_t;
ui_preferences_storage_state_t ui_preferences_storage_state(void);
bool ui_preferences_decode(const uint8_t blob[UI_PREFERENCES_BLOB_SIZE], ui_preferences_t *preferences);
#endif
