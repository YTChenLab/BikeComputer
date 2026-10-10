#include "ui_stm32_services.h"
#include "ui/core/ui_app.h"

/* UiTask has already received the FreeRTOS queue record. No second queue,
 * recognizer, LVGL keypad/encoder indev, or hardware operation here. */
bool ui_stm32_services_handle_input_event(const ui_physical_input_event_t *event)
{
    return ui_app_handle_input_event(event);
}

bool ui_stm32_services_apply_display_state(bool enabled)
{
    return ui_app_apply_display_state(enabled);
}
