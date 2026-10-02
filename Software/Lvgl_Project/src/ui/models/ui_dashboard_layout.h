#ifndef UI_DASHBOARD_LAYOUT_H
#define UI_DASHBOARD_LAYOUT_H
#include "ui_ride_data_model.h"
#define UI_DASHBOARD_SLOT_COUNT 6U
#define UI_DASHBOARD_FIELD_COUNT 12U
typedef enum { UI_DASHBOARD_CRUISE, UI_DASHBOARD_TRAINING,
    UI_DASHBOARD_CLIMB, UI_DASHBOARD_CUSTOM } ui_dashboard_preset_t;
typedef struct {
    ui_ride_field_t field;
    const char *title, *unit;
    bool compact_number;
} ui_dashboard_field_definition_t;
const ui_dashboard_field_definition_t *ui_dashboard_field_definition(ui_ride_field_t field);
const ui_dashboard_field_definition_t *ui_dashboard_field_at(unsigned index);
const char *ui_dashboard_preset_name(unsigned preset);
bool ui_dashboard_preset_fields(unsigned preset, uint8_t fields[UI_DASHBOARD_SLOT_COUNT]);
bool ui_dashboard_layout_valid(unsigned preset, const uint8_t fields[UI_DASHBOARD_SLOT_COUNT]);
ui_ride_field_t ui_dashboard_next_field(const uint8_t fields[UI_DASHBOARD_SLOT_COUNT],
                                       unsigned slot, int direction);
bool ui_dashboard_format_field(ui_ride_field_t field, const ui_ride_data_t *data,
                               uint32_t now, char *text, size_t capacity);
uint32_t ui_dashboard_speed_color(float speed);
#endif
