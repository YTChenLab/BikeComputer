#ifndef UI_SETTINGS_LIST_H
#define UI_SETTINGS_LIST_H

#include "../../core/ui_input.h"
#include "lvgl.h"
#include "../ui_liquid_selection.h"

#include <stdbool.h>
#include <stdint.h>

#define UI_SETTINGS_LIST_MAX_ROWS 16U

typedef enum
{
    UI_SETTINGS_ROW_ACTION = 0,
    UI_SETTINGS_ROW_TOGGLE,
    UI_SETTINGS_ROW_SLIDER,
    UI_SETTINGS_ROW_SELECTION,
    UI_SETTINGS_ROW_INFO
} ui_settings_row_type_t;

typedef enum
{
    UI_SETTINGS_ROW_IDLE = 0,
    UI_SETTINGS_ROW_PENDING,
    UI_SETTINGS_ROW_FAILED
} ui_settings_row_feedback_t;

struct ui_settings_list;

typedef struct
{
    struct ui_settings_list *owner;
    lv_obj_t *root;
    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *value;
    lv_obj_t *toggle_track;
    lv_obj_t *toggle_knob;
    lv_obj_t *icon_box;
    lv_obj_t *icon;
    lv_obj_t *arrow;
    uint32_t accent;
    uint16_t id;
    ui_settings_row_type_t type;
    bool enabled;
} ui_settings_row_t;

typedef void (*ui_settings_list_action_cb_t)(uint16_t id, void *user_data);

typedef struct ui_settings_list
{
    lv_obj_t *root;
    lv_obj_t *scroll_hint;
    ui_liquid_selection_t liquid;
    bool touch_scrolling;
    ui_settings_row_t rows[UI_SETTINGS_LIST_MAX_ROWS];
    uint8_t row_count;
    uint8_t selection;
    int32_t row_height;
    int32_t row_gap;
    ui_settings_list_action_cb_t callback;
    void *user_data;
} ui_settings_list_t;

void ui_settings_list_create(ui_settings_list_t *list, lv_obj_t *parent,
                             int32_t x, int32_t y,
                             int32_t width, int32_t height,
                             ui_settings_list_action_cb_t callback,
                             void *user_data);
/* Configure before adding rows; compact subpages retain their default size. */
bool ui_settings_list_set_row_size(ui_settings_list_t *list, int32_t height, int32_t gap);
bool ui_settings_list_add(ui_settings_list_t *list, uint16_t id,
                          ui_settings_row_type_t type,
                          const char *title, const char *subtitle,
                          const char *value);
bool ui_settings_list_set_value(ui_settings_list_t *list, uint16_t id,
                                const char *value);
bool ui_settings_list_set_accent(ui_settings_list_t *list, uint16_t id,
                                 uint32_t color, const char *symbol);
void ui_settings_list_set_scroll_hint(ui_settings_list_t *list, lv_obj_t *label);
bool ui_settings_list_set_toggle(ui_settings_list_t *list, uint16_t id,
                                 bool enabled);
bool ui_settings_list_set_feedback(ui_settings_list_t *list, uint16_t id,
                                   ui_settings_row_feedback_t feedback);
bool ui_settings_list_handle_input(ui_settings_list_t *list,
                                   ui_input_event_t input);
bool ui_settings_list_contains_point(const ui_settings_list_t *list,
                                     int32_t x, int32_t y);

#endif /* UI_SETTINGS_LIST_H */
