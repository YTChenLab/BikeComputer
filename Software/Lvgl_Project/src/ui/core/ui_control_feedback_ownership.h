#ifndef UI_CONTROL_FEEDBACK_OWNERSHIP_H
#define UI_CONTROL_FEEDBACK_OWNERSHIP_H

#include "../../services/ui_command_service.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UI_CONTROL_FEEDBACK_OWNER_NONE = 0,
    UI_CONTROL_FEEDBACK_OWNER_TOP,
    UI_CONTROL_FEEDBACK_OWNER_BOTTOM
} ui_control_feedback_owner_t;

typedef struct
{
    uint32_t command_id;
    ui_command_type_t command_type;
    ui_control_feedback_owner_t owner;
    bool active;
} ui_control_feedback_state_t;

void ui_control_feedback_ownership_init(void);
bool ui_control_feedback_ownership_manages(ui_command_type_t type);
ui_control_feedback_owner_t ui_control_feedback_ownership_begin(
    const ui_command_result_t *result,
    bool top_visible);
ui_control_feedback_owner_t ui_control_feedback_ownership_owner_for(
    const ui_command_result_t *result);
bool ui_control_feedback_ownership_handoff_to_bottom(
    ui_control_feedback_state_t *state);
bool ui_control_feedback_ownership_reclaim_to_top(
    ui_control_feedback_state_t *state);
void ui_control_feedback_ownership_complete(
    const ui_command_result_t *result);
bool ui_control_feedback_ownership_get(
    ui_control_feedback_state_t *state);

#endif /* UI_CONTROL_FEEDBACK_OWNERSHIP_H */
