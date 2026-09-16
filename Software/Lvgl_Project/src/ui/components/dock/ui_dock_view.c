#include "ui_dock_view.h"

#include "ui_dock_templates.h"
#include "../../core/ui_bottom_surface.h"
#include "../../core/ui_dock.h"
#include "../../core/ui_motion.h"
#include "../../core/ui_sound.h"
#include "../../design/ui_design_tokens.h"

#include <stdint.h>
#include <string.h>

typedef struct
{
    lv_obj_t *host;
    ui_dock_template_t templates[UI_DOCK_TYPE_COUNT];
    ui_dock_type_t active_type;
    bool initialized;
} ui_dock_view_context_t;

static ui_dock_view_context_t s_view;

static void content_opacity_animation(void *object, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)object, (lv_opa_t)value, 0);
}

static void content_scale_animation(void *object, int32_t value)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)object, value, 0);
}

static void content_y_animation(void *object, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)object, value);
}

static void animate_content_settle(lv_obj_t *root, bool refresh)
{
    if(root == NULL) return;
    lv_anim_delete(root, content_opacity_animation);
    lv_anim_delete(root, content_scale_animation);
    lv_anim_delete(root, content_y_animation);

    int32_t from_scale = refresh ? 251 : 246;
    int32_t from_y = refresh ? UI_TOKEN_SPACE_4 : UI_TOKEN_SPACE_8;
    lv_opa_t from_opa = refresh ? LV_OPA_60 : UI_TOKEN_OPA_HIDDEN;
    lv_obj_set_style_transform_pivot_x(
        root, UI_BOTTOM_SURFACE_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(
        root, UI_BOTTOM_SURFACE_NORMAL_HEIGHT / 2, 0);
    lv_obj_set_style_transform_scale(root, from_scale, 0);
    lv_obj_set_style_opa(root, from_opa, 0);
    lv_obj_set_y(root, from_y);

    ui_motion_value(root, content_opacity_animation,
                    from_opa, UI_TOKEN_OPA_PRIMARY,
                    UI_TOKEN_MOTION_SURFACE_FADE_MS,
                    lv_anim_path_ease_in_out, NULL, NULL);
    ui_motion_value(root, content_scale_animation,
                    from_scale, 256,
                    UI_TOKEN_MOTION_DOCK_CONTENT_MS,
                    lv_anim_path_ease_out, NULL, NULL);
    ui_motion_value(root, content_y_animation,
                    from_y, 0,
                    UI_TOKEN_MOTION_DOCK_CONTENT_MS,
                    lv_anim_path_ease_out, NULL, NULL);
}

static ui_bottom_surface_size_t surface_size(ui_dock_type_t type)
{
    (void)type;
    /* 首页交互只替换内容，绝不因类型改变 Surface 的位置或尺寸。 */
    return UI_BOTTOM_SURFACE_SIZE_NORMAL;
}

static ui_bottom_surface_priority_t surface_priority(
    ui_dock_priority_t priority)
{
    switch(priority) {
        case UI_DOCK_PRIORITY_P0_CRITICAL:
            return UI_BOTTOM_SURFACE_PRIORITY_P0_CRITICAL;
        case UI_DOCK_PRIORITY_P1_RIDING_SAFETY:
            return UI_BOTTOM_SURFACE_PRIORITY_P1_RIDING_SAFETY;
        case UI_DOCK_PRIORITY_P2_RIDE_STATE:
            return UI_BOTTOM_SURFACE_PRIORITY_P2_RIDE_STATE;
        case UI_DOCK_PRIORITY_P3_USER_ACTION:
            return UI_BOTTOM_SURFACE_PRIORITY_P3_USER_ACTION;
        case UI_DOCK_PRIORITY_P4_NORMAL:
        default:
            return UI_BOTTOM_SURFACE_PRIORITY_P4_NORMAL;
    }
}

static void hide_all_except(ui_dock_type_t type)
{
    for(uint8_t i = 0; i < UI_DOCK_TYPE_COUNT; i++) {
        if(i == type) {
            lv_obj_set_style_opa(s_view.templates[i].root, LV_OPA_COVER, 0);
            lv_obj_set_y(s_view.templates[i].root, 0);
            lv_obj_clear_flag(s_view.templates[i].root, LV_OBJ_FLAG_HIDDEN);
        }
        else lv_obj_add_flag(s_view.templates[i].root, LV_OBJ_FLAG_HIDDEN);
    }
}

static void control_event_cb(lv_event_t *event)
{
    if(!s_view.initialized || !ui_dock_is_visible()) return;
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    ui_dock_interaction_t interaction =
        (ui_dock_interaction_t)(uintptr_t)lv_event_get_user_data(event);

    if(code == LV_EVENT_PRESSED) {
        ui_motion_button_press(target);
        return;
    }
    if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ui_motion_button_release(target);
        return;
    }
    if(code != LV_EVENT_CLICKED && code != LV_EVENT_VALUE_CHANGED) return;

    const ui_dock_card_t *card = ui_dock_current();
    if(card == NULL) return;
    int32_t value = card->value;
    if(code == LV_EVENT_VALUE_CHANGED) value = lv_slider_get_value(target);
    ui_sound_play(UI_SFX_PRESS);
    (void)ui_dock_invoke(card->id, interaction, value);
    if(code == LV_EVENT_VALUE_CHANGED && ui_dock_is_visible()) {
        const ui_dock_card_t *updated = ui_dock_current();
        if(updated != NULL && updated->type == UI_DOCK_TYPE_CONTROL)
            ui_dock_template_update(
                &s_view.templates[UI_DOCK_TYPE_CONTROL], updated);
    }
}

static bool present_on_surface(ui_dock_event_t event,
                               const ui_dock_card_t *card)
{
    ui_bottom_surface_size_t size = surface_size(card->type);
    ui_bottom_surface_priority_t priority =
        surface_priority(card->priority);
    ui_bottom_surface_state_t state;
    (void)ui_bottom_surface_get_state(&state);

    /* ui_dock is the single owner of priority/restore state. The visual
     * Surface must not maintain a second restore stack for the same cards,
     * otherwise removing a non-active Dock card can resurrect a stale frame. */
    if(state.mode == UI_BOTTOM_SURFACE_INTERACTION_MODE &&
       state.owner_id != card->id)
        return ui_bottom_surface_replace_interaction(
            card->id, size, priority, true);
    if((event == UI_DOCK_EVENT_UPDATED ||
        event == UI_DOCK_EVENT_RESTORED) &&
       state.mode == UI_BOTTOM_SURFACE_INTERACTION_MODE &&
       state.owner_id == card->id)
        return ui_bottom_surface_update_interaction(card->id, size, true);
    return ui_bottom_surface_present_interaction(card->id, size, priority,
                                                 true);
}

static void dock_observer(ui_dock_event_t event, const ui_dock_card_t *card,
                          void *user_data)
{
    (void)user_data;
    if(!s_view.initialized || card == NULL || card->type >= UI_DOCK_TYPE_COUNT)
        return;
    if(event == UI_DOCK_EVENT_QUEUED || event == UI_DOCK_EVENT_OVERRIDDEN ||
       event == UI_DOCK_EVENT_DROPPED) return;

    ui_dock_template_t *template = &s_view.templates[card->type];
    if(event == UI_DOCK_EVENT_HIDDEN) {
        lv_obj_add_flag(template->root, LV_OBJ_FLAG_HIDDEN);
        s_view.active_type = UI_DOCK_TYPE_COUNT;
        if(!ui_bottom_surface_dismiss_interaction(card->id, true)) {
            LV_LOG_WARN("Bottom Surface rejected Dock dismiss id=%lu",
                        (unsigned long)card->id);
            /* Recover from an owner/restore-stack mismatch instead of
             * leaving an empty interaction frame mounted on later pages. */
            ui_bottom_surface_reset_to_dynamic(true);
        }
        return;
    }

    ui_dock_template_update(template, card);
    if(event == UI_DOCK_EVENT_UPDATED) {
        if(!present_on_surface(event, card)) {
            LV_LOG_WARN("Bottom Surface rejected Dock update id=%lu",
                        (unsigned long)card->id);
        }
        else {
            animate_content_settle(template->root, true);
        }
        return;
    }

    s_view.active_type = card->type;
    hide_all_except(card->type);
    if(!present_on_surface(event, card)) {
        LV_LOG_WARN("Bottom Surface rejected Dock event=%d id=%lu",
                    (int)event, (unsigned long)card->id);
        return;
    }
    animate_content_settle(template->root,
                           event == UI_DOCK_EVENT_RESTORED);
}

void ui_dock_view_init(void)
{
    if(s_view.initialized) return;
    memset(&s_view, 0, sizeof(s_view));
    s_view.active_type = UI_DOCK_TYPE_COUNT;

    if(!ui_bottom_surface_is_created()) {
        LV_LOG_ERROR("Bottom Surface must exist before Dock View init");
        return;
    }
    s_view.host = ui_bottom_surface_interaction_host();
    if(s_view.host == NULL) return;

    for(uint8_t type = 0; type < UI_DOCK_TYPE_COUNT; type++) {
        ui_dock_template_create(&s_view.templates[type], s_view.host,
                                (ui_dock_type_t)type, control_event_cb);
    }
    s_view.initialized = true;
    ui_dock_set_observer(dock_observer, NULL);
}

void ui_dock_view_deinit(void)
{
    if(!s_view.initialized) return;
    ui_dock_set_observer(NULL, NULL);
    const ui_dock_card_t *card = ui_dock_current();
    if(card != NULL)
        (void)ui_bottom_surface_dismiss_interaction(card->id, false);
    lv_obj_clean(s_view.host);
    memset(&s_view, 0, sizeof(s_view));
}

bool ui_dock_view_is_visible(void)
{
    if(!s_view.initialized || !ui_dock_is_visible()) return false;
    ui_bottom_surface_state_t state;
    return ui_bottom_surface_get_state(&state) &&
           state.mode == UI_BOTTOM_SURFACE_INTERACTION_MODE &&
           !lv_obj_has_flag(s_view.host, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *ui_dock_view_root(void)
{
    return s_view.host;
}
