#include "ui_control_feedback_ownership.h"

#include <string.h>

static ui_control_feedback_state_t s_feedback;

void ui_control_feedback_ownership_init(void)
{
    memset(&s_feedback, 0, sizeof(s_feedback));
}

bool ui_control_feedback_ownership_manages(ui_command_type_t type)
{
    return type == UI_COMMAND_WIFI_SET_ENABLED ||
           type == UI_COMMAND_BLUETOOTH_SET_ENABLED ||
           type == UI_COMMAND_GNSS_SET_ENABLED ||
           type == UI_COMMAND_POWER_SAVE_SET_ENABLED ||
           type == UI_COMMAND_TOUCH_LOCK_SET_ENABLED ||
           type == UI_COMMAND_SOUND_SET_LEVEL ||
           type == UI_COMMAND_BRIGHTNESS_SET_LEVEL;
}

ui_control_feedback_owner_t ui_control_feedback_ownership_begin(
    const ui_command_result_t *result,
    bool top_visible)
{
    if(result == NULL || result->status != UI_COMMAND_STATUS_PENDING ||
       !ui_control_feedback_ownership_manages(result->type))
        return UI_CONTROL_FEEDBACK_OWNER_NONE;
    s_feedback.command_id = result->id;
    s_feedback.command_type = result->type;
    s_feedback.owner = top_visible ? UI_CONTROL_FEEDBACK_OWNER_TOP :
                                     UI_CONTROL_FEEDBACK_OWNER_BOTTOM;
    s_feedback.active = true;
    return s_feedback.owner;
}

ui_control_feedback_owner_t ui_control_feedback_ownership_owner_for(
    const ui_command_result_t *result)
{
    if(result == NULL || !s_feedback.active ||
       result->id != s_feedback.command_id ||
       result->type != s_feedback.command_type)
        return UI_CONTROL_FEEDBACK_OWNER_NONE;
    return s_feedback.owner;
}

bool ui_control_feedback_ownership_handoff_to_bottom(
    ui_control_feedback_state_t *state)
{
    if(!s_feedback.active ||
       s_feedback.owner != UI_CONTROL_FEEDBACK_OWNER_TOP)
        return false;
    s_feedback.owner = UI_CONTROL_FEEDBACK_OWNER_BOTTOM;
    if(state != NULL) *state = s_feedback;
    return true;
}

bool ui_control_feedback_ownership_reclaim_to_top(
    ui_control_feedback_state_t *state)
{
    if(!s_feedback.active ||
       s_feedback.owner != UI_CONTROL_FEEDBACK_OWNER_BOTTOM)
        return false;
    s_feedback.owner = UI_CONTROL_FEEDBACK_OWNER_TOP;
    if(state != NULL) *state = s_feedback;
    return true;
}

void ui_control_feedback_ownership_complete(
    const ui_command_result_t *result)
{
    if(ui_control_feedback_ownership_owner_for(result) ==
       UI_CONTROL_FEEDBACK_OWNER_NONE)
        return;
    memset(&s_feedback, 0, sizeof(s_feedback));
}

bool ui_control_feedback_ownership_get(
    ui_control_feedback_state_t *state)
{
    if(state == NULL || !s_feedback.active) return false;
    *state = s_feedback;
    return true;
}
