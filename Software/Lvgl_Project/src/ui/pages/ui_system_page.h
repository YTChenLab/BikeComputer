#ifndef UI_SYSTEM_PAGE_H
#define UI_SYSTEM_PAGE_H

#include "ui_page_action.h"
#include "../core/ui_input.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    const char *device_model;
    const char *ui_version;
    const char *lvgl_version;
    const char *storage_status;
    const char *hardware_target;
    const char *system_health;
} ui_system_info_t;

lv_obj_t *ui_system_page_create(ui_page_action_cb_t callback,
                                void *user_data);
void ui_system_page_set_info(const ui_system_info_t *info);
bool ui_system_page_handle_input(ui_input_event_t input);
bool ui_system_page_contains_scroll_point(int32_t x, int32_t y);

#endif /* UI_SYSTEM_PAGE_H */
