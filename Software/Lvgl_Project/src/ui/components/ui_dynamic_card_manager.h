#ifndef UI_DYNAMIC_CARD_MANAGER_H
#define UI_DYNAMIC_CARD_MANAGER_H

#include "lvgl.h"
#include "../core/ui_input.h"
#include "../models/ui_ride_data_model.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UI_DYNAMIC_CARD_EXTENDED_DATA = 0,
    UI_DYNAMIC_CARD_NAVIGATION,
    UI_DYNAMIC_CARD_MUSIC,
    UI_DYNAMIC_CARD_WEATHER,
    UI_DYNAMIC_CARD_COUNT
} ui_dynamic_card_id_t;

typedef enum
{
    UI_DYNAMIC_CARD_BROWSER = 0,
    UI_DYNAMIC_CARD_CONTROL
} ui_dynamic_card_mode_t;

void ui_dynamic_card_manager_create(lv_obj_t *parent);
bool ui_dynamic_card_manager_contains_point(int32_t screen_x, int32_t screen_y);
bool ui_dynamic_card_manager_handle_input(ui_input_event_t input);
bool ui_dynamic_card_manager_back(void);
void ui_dynamic_card_manager_refresh_data(const ui_ride_data_t *data);

ui_dynamic_card_id_t ui_dynamic_card_manager_get_active_card(void);
ui_dynamic_card_mode_t ui_dynamic_card_manager_get_mode(void);
int8_t ui_dynamic_card_manager_get_control_focus(void);
unsigned ui_dynamic_card_manager_get_extended_page(void);

#endif /* UI_DYNAMIC_CARD_MANAGER_H */
