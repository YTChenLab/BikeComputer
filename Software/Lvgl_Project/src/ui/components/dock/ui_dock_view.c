#include "ui_dock_view.h"

#include "ui_dock_templates.h"
#include "../../core/ui_dock.h"
#include "../../core/ui_motion.h"
#include "../../core/ui_sizes.h"
#include "../../core/ui_sound.h"

#include <stdint.h>
#include <string.h>

#define UI_DOCK_VIEW_X        12
#define UI_DOCK_VIEW_WIDTH    456
#define UI_DOCK_VIEW_HEIGHT   156
#define UI_DOCK_VIEW_Y        (UI_SCREEN_H - UI_DOCK_VIEW_HEIGHT - 16)

typedef struct
{
    lv_obj_t *layer;
    lv_obj_t *viewport;
    ui_dock_template_t templates[UI_DOCK_TYPE_COUNT];
    ui_dock_type_t active_type;
    bool initialized;
} ui_dock_view_context_t;

static ui_dock_view_context_t s_view;

static void hide_all_except(ui_dock_type_t type)
{
    for(uint8_t i = 0; i < UI_DOCK_TYPE_COUNT; i++) {
        if(i == type) lv_obj_clear_flag(s_view.templates[i].root, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_view.templates[i].root, LV_OBJ_FLAG_HIDDEN);
    }
}

static void exit_completed(lv_anim_t *animation)
{
    lv_obj_t *root = lv_anim_get_user_data(animation);
    if(ui_dock_is_visible()) return;
    if(root != NULL) lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    if(s_view.layer != NULL) lv_obj_add_flag(s_view.layer, LV_OBJ_FLAG_HIDDEN);
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
            ui_dock_template_update(&s_view.templates[UI_DOCK_TYPE_CONTROL], updated);
    }
}

static void dock_observer(ui_dock_event_t event, const ui_dock_card_t *card,
                          void *user_data)
{
    (void)user_data;
    if(!s_view.initialized || card == NULL || card->type >= UI_DOCK_TYPE_COUNT)
        return;

    ui_dock_template_t *template = &s_view.templates[card->type];
    if(event == UI_DOCK_EVENT_QUEUED || event == UI_DOCK_EVENT_OVERRIDDEN ||
       event == UI_DOCK_EVENT_DROPPED) return;
    if(event == UI_DOCK_EVENT_HIDDEN) {
        ui_motion_dock_exit(template->root, 0, exit_completed, template->root);
        return;
    }

    ui_dock_template_update(template, card);
    if(event == UI_DOCK_EVENT_UPDATED) return;

    s_view.active_type = card->type;
    hide_all_except(card->type);
    lv_obj_clear_flag(s_view.layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_view.layer);
    ui_motion_dock_enter(template->root, 0);
}

void ui_dock_view_init(void)
{
    if(s_view.initialized) return;
    memset(&s_view, 0, sizeof(s_view));
    s_view.active_type = UI_DOCK_TYPE_COUNT;

    s_view.layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_view.layer);
    lv_obj_set_pos(s_view.layer, 0, 0);
    lv_obj_set_size(s_view.layer, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_opa(s_view.layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_view.layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_view.layer, LV_OBJ_FLAG_SCROLLABLE);

    s_view.viewport = lv_obj_create(s_view.layer);
    lv_obj_remove_style_all(s_view.viewport);
    lv_obj_set_pos(s_view.viewport, UI_DOCK_VIEW_X, UI_DOCK_VIEW_Y);
    lv_obj_set_size(s_view.viewport, UI_DOCK_VIEW_WIDTH, UI_DOCK_VIEW_HEIGHT);
    lv_obj_set_style_bg_opa(s_view.viewport, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(s_view.viewport, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_view.viewport, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_view.viewport, LV_OBJ_FLAG_SCROLLABLE);

    for(uint8_t type = 0; type < UI_DOCK_TYPE_COUNT; type++) {
        ui_dock_template_create(&s_view.templates[type], s_view.viewport,
                                (ui_dock_type_t)type, control_event_cb);
    }
    lv_obj_add_flag(s_view.layer, LV_OBJ_FLAG_HIDDEN);
    s_view.initialized = true;
    ui_dock_set_observer(dock_observer, NULL);
}

void ui_dock_view_deinit(void)
{
    if(!s_view.initialized) return;
    ui_dock_set_observer(NULL, NULL);
    lv_obj_delete(s_view.layer);
    memset(&s_view, 0, sizeof(s_view));
}

bool ui_dock_view_is_visible(void)
{
    return s_view.initialized && !lv_obj_has_flag(s_view.layer, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *ui_dock_view_root(void)
{
    return s_view.layer;
}
