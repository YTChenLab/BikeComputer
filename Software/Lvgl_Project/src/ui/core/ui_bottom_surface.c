#include "ui_bottom_surface.h"

#include "../design/ui_component_style.h"
#include "../design/ui_design_tokens.h"

#include <string.h>

typedef struct
{
    ui_bottom_surface_mode_t mode;
    ui_bottom_surface_size_t size;
    ui_bottom_surface_priority_t priority;
    uint32_t owner_id;
} ui_bottom_surface_entry_t;

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *slot;
    lv_obj_t *dynamic_host;
    lv_obj_t *interaction_host;
    ui_bottom_surface_entry_t current;
    ui_bottom_surface_entry_t restore[UI_BOTTOM_SURFACE_STACK_MAX];
    ui_bottom_surface_observer_cb_t observer;
    void *observer_user_data;
    uint32_t version;
    uint8_t restore_depth;
    bool mounted;
    bool dynamic_content_visible;
    bool dynamic_focused;
    bool created;
} ui_bottom_surface_context_t;

static ui_bottom_surface_context_t s_surface;

static bool should_be_visible(void);

static int32_t surface_height(ui_bottom_surface_size_t size)
{
    switch(size) {
        case UI_BOTTOM_SURFACE_SIZE_MINI:
            return UI_BOTTOM_SURFACE_MINI_HEIGHT;
        case UI_BOTTOM_SURFACE_SIZE_EXPANDED:
            return UI_BOTTOM_SURFACE_EXPANDED_HEIGHT;
        case UI_BOTTOM_SURFACE_SIZE_NORMAL:
        default:
            return UI_BOTTOM_SURFACE_NORMAL_HEIGHT;
    }
}

static void height_animation(void *object, int32_t value)
{
    lv_obj_set_height((lv_obj_t *)object, value);
}

static void animate_height(lv_obj_t *object, int32_t target)
{
    lv_anim_delete(object, height_animation);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, object);
    lv_anim_set_exec_cb(&animation, height_animation);
    lv_anim_set_values(&animation, lv_obj_get_height(object), target);
    lv_anim_set_duration(&animation, UI_TOKEN_MOTION_SURFACE_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

static void y_animation(void *object, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)object, value);
}

static void animate_y(lv_obj_t *object, int32_t target)
{
    lv_anim_delete(object, y_animation);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, object);
    lv_anim_set_exec_cb(&animation, y_animation);
    lv_anim_set_values(&animation, lv_obj_get_y(object), target);
    lv_anim_set_duration(&animation, UI_TOKEN_MOTION_SURFACE_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

static void opacity_animation(void *object, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)object, (lv_opa_t)value, 0);
}

static lv_obj_t *desired_host(void)
{
    return s_surface.current.mode == UI_BOTTOM_SURFACE_DYNAMIC_CARD_MODE ?
           s_surface.dynamic_host : s_surface.interaction_host;
}

static void host_fade_out_completed(lv_anim_t *animation)
{
    lv_obj_t *host = lv_anim_get_user_data(animation);
    if(host != NULL && host != desired_host())
        lv_obj_add_flag(host, LV_OBJ_FLAG_HIDDEN);
}

static void animate_host_opacity(lv_obj_t *host, lv_opa_t from, lv_opa_t to,
                                 lv_anim_completed_cb_t completed)
{
    lv_anim_delete(host, opacity_animation);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, host);
    lv_anim_set_exec_cb(&animation, opacity_animation);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_duration(&animation, UI_TOKEN_MOTION_SURFACE_FADE_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_set_user_data(&animation, host);
    if(completed != NULL) lv_anim_set_completed_cb(&animation, completed);
    lv_anim_start(&animation);
}

static void root_opacity_animation(void *object, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)object, (lv_opa_t)value, 0);
}

static void surface_scale_animation(void *object, int32_t value)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)object, value, 0);
}

static void root_hide_completed(lv_anim_t *animation)
{
    (void)animation;
    if(s_surface.created && !should_be_visible())
        lv_obj_add_flag(s_surface.root, LV_OBJ_FLAG_HIDDEN);
}

static void animate_root_opacity(lv_opa_t from, lv_opa_t to,
                                 lv_anim_completed_cb_t completed)
{
    lv_anim_delete(s_surface.root, root_opacity_animation);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, s_surface.root);
    lv_anim_set_exec_cb(&animation, root_opacity_animation);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_duration(&animation, UI_TOKEN_MOTION_SURFACE_FADE_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    if(completed != NULL) lv_anim_set_completed_cb(&animation, completed);
    lv_anim_start(&animation);
}

static void animate_surface_scale(int32_t from)
{
    lv_anim_delete(s_surface.root, surface_scale_animation);
    lv_obj_set_style_transform_scale(s_surface.root, from, 0);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, s_surface.root);
    lv_anim_set_exec_cb(&animation, surface_scale_animation);
    lv_anim_set_values(&animation, from, 256);
    lv_anim_set_duration(&animation, UI_TOKEN_MOTION_SURFACE_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

static bool should_be_visible(void)
{
    return s_surface.mounted &&
           (s_surface.current.mode == UI_BOTTOM_SURFACE_INTERACTION_MODE ||
            s_surface.dynamic_content_visible);
}

static void apply_hosts(bool animated)
{
    if(!s_surface.created) return;
    lv_obj_t *next = desired_host();
    lv_obj_t *previous = next == s_surface.dynamic_host ?
                         s_surface.interaction_host : s_surface.dynamic_host;
    bool next_hidden = lv_obj_has_flag(next, LV_OBJ_FLAG_HIDDEN);

    lv_anim_delete(next, opacity_animation);
    lv_anim_delete(previous, opacity_animation);
    if(!animated || !next_hidden) {
        lv_obj_set_style_opa(next, UI_TOKEN_OPA_PRIMARY, 0);
        lv_obj_clear_flag(next, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(previous, UI_TOKEN_OPA_PRIMARY, 0);
        lv_obj_add_flag(previous, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_set_style_opa(next, UI_TOKEN_OPA_HIDDEN, 0);
    lv_obj_clear_flag(next, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(previous, LV_OBJ_FLAG_HIDDEN);
    animate_host_opacity(next, UI_TOKEN_OPA_HIDDEN, UI_TOKEN_OPA_PRIMARY,
                         NULL);
    animate_host_opacity(previous,
                         lv_obj_get_style_opa(previous, 0),
                         UI_TOKEN_OPA_HIDDEN, host_fade_out_completed);
}

static void apply_dynamic_focus(void)
{
    if(!s_surface.created) return;
    bool focused = s_surface.dynamic_focused &&
                   s_surface.current.mode ==
                       UI_BOTTOM_SURFACE_DYNAMIC_CARD_MODE;
    bool interaction = s_surface.current.mode ==
                       UI_BOTTOM_SURFACE_INTERACTION_MODE;
    lv_color_t border = UI_TOKEN_COLOR_SURFACE_BORDER;
    int32_t width = UI_TOKEN_BORDER_THIN;
    if(interaction &&
       s_surface.current.priority == UI_BOTTOM_SURFACE_PRIORITY_P0_CRITICAL) {
        border = UI_TOKEN_COLOR_CRITICAL;
        width = UI_TOKEN_BORDER_FOCUS;
    }
    else if(interaction &&
            s_surface.current.priority <=
                UI_BOTTOM_SURFACE_PRIORITY_P2_RIDE_STATE) {
        border = UI_TOKEN_COLOR_ACCENT;
        width = UI_TOKEN_BORDER_FOCUS;
    }
    else if(focused) {
        border = UI_TOKEN_COLOR_ACCENT;
        width = UI_TOKEN_BORDER_FOCUS;
    }
    lv_obj_set_style_border_color(
        s_surface.root, border, 0);
    lv_obj_set_style_border_width(s_surface.root, width, 0);
    lv_obj_set_style_shadow_color(s_surface.root, border, 0);
    lv_obj_set_style_shadow_width(s_surface.root,
                                  focused || interaction ? 8 : 0, 0);
    lv_obj_set_style_shadow_opa(
        s_surface.root,
        focused || interaction ? UI_TOKEN_OPA_DISABLED : UI_TOKEN_OPA_HIDDEN,
        0);
}

static void apply_geometry(bool animated)
{
    if(!s_surface.created) return;
    ui_bottom_surface_size_t visual_size = s_surface.current.size;
    if(s_surface.mounted && visual_size == UI_BOTTOM_SURFACE_SIZE_MINI)
        visual_size = UI_BOTTOM_SURFACE_SIZE_NORMAL;
    int32_t height = surface_height(visual_size);
    int32_t y = UI_BOTTOM_SURFACE_NORMAL_HEIGHT - height;
    bool visible = should_be_visible();
    if(!visible) {
        if(animated &&
           !lv_obj_has_flag(s_surface.root, LV_OBJ_FLAG_HIDDEN)) {
            animate_y(s_surface.root, y + UI_TOKEN_SPACE_12);
            animate_root_opacity(
                lv_obj_get_style_opa(s_surface.root, 0),
                UI_TOKEN_OPA_HIDDEN, root_hide_completed);
            animate_surface_scale(252);
        }
        else {
            lv_anim_delete(s_surface.root, root_opacity_animation);
            lv_anim_delete(s_surface.root, surface_scale_animation);
            lv_obj_add_flag(s_surface.root, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_opa(s_surface.root, UI_TOKEN_OPA_PRIMARY, 0);
            lv_obj_set_style_transform_scale(s_surface.root, 256, 0);
        }
        return;
    }

    bool was_hidden = lv_obj_has_flag(s_surface.root, LV_OBJ_FLAG_HIDDEN);
    lv_anim_delete(s_surface.root, root_opacity_animation);
    lv_obj_clear_flag(s_surface.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_surface.root);
    if(animated && was_hidden) {
        lv_obj_set_pos(s_surface.root, 0, y + UI_TOKEN_SPACE_12);
        lv_obj_set_size(s_surface.root, UI_BOTTOM_SURFACE_WIDTH,
                        LV_MAX(height - UI_TOKEN_SPACE_8, 1));
        lv_obj_set_style_opa(s_surface.root, UI_TOKEN_OPA_HIDDEN, 0);
        animate_y(s_surface.root, y);
        animate_height(s_surface.root, height);
        animate_root_opacity(UI_TOKEN_OPA_HIDDEN,
                             UI_TOKEN_OPA_PRIMARY, NULL);
        animate_surface_scale(246);
    }
    else if(animated) {
        animate_y(s_surface.root, y);
        animate_height(s_surface.root, height);
        lv_obj_set_style_opa(s_surface.root, UI_TOKEN_OPA_PRIMARY, 0);
        animate_surface_scale(251);
    }
    else {
        lv_anim_delete(s_surface.root, y_animation);
        lv_anim_delete(s_surface.root, height_animation);
        lv_anim_delete(s_surface.root, surface_scale_animation);
        lv_obj_set_pos(s_surface.root, 0, y);
        lv_obj_set_size(s_surface.root, UI_BOTTOM_SURFACE_WIDTH, height);
        lv_obj_set_style_opa(s_surface.root, UI_TOKEN_OPA_PRIMARY, 0);
        lv_obj_set_style_transform_scale(s_surface.root, 256, 0);
        lv_obj_update_layout(s_surface.root);
    }
}

static void emit_state(void)
{
    s_surface.version++;
    if(s_surface.observer == NULL) return;
    ui_bottom_surface_state_t state;
    (void)ui_bottom_surface_get_state(&state);
    s_surface.observer(&state, s_surface.observer_user_data);
}

static void apply_state(bool animated)
{
    apply_hosts(animated);
    apply_dynamic_focus();
    apply_geometry(animated);
    emit_state();
}

void ui_bottom_surface_init(void)
{
    memset(&s_surface, 0, sizeof(s_surface));
    s_surface.current.mode = UI_BOTTOM_SURFACE_DYNAMIC_CARD_MODE;
    s_surface.current.size = UI_BOTTOM_SURFACE_SIZE_NORMAL;
    s_surface.current.priority = UI_BOTTOM_SURFACE_PRIORITY_P4_NORMAL;
}

bool ui_bottom_surface_create(lv_obj_t *parent)
{
    if(parent == NULL) return false;
    if(s_surface.created) return true;

    s_surface.slot = parent;
    s_surface.root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_surface.root);
    ui_component_style_apply_bottom_surface(s_surface.root);
    lv_obj_set_pos(s_surface.root, 0, 0);
    lv_obj_set_size(s_surface.root, UI_BOTTOM_SURFACE_WIDTH,
                    UI_BOTTOM_SURFACE_NORMAL_HEIGHT);
    lv_obj_set_style_transform_pivot_x(
        s_surface.root, UI_BOTTOM_SURFACE_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(
        s_surface.root, UI_BOTTOM_SURFACE_NORMAL_HEIGHT / 2, 0);
    lv_obj_set_style_transform_scale(s_surface.root, 256, 0);
    lv_obj_clear_flag(s_surface.root,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_surface.dynamic_host = lv_obj_create(s_surface.root);
    lv_obj_remove_style_all(s_surface.dynamic_host);
    ui_component_style_apply_surface_host(s_surface.dynamic_host);
    lv_obj_set_pos(s_surface.dynamic_host, 0, 0);
    lv_obj_set_size(s_surface.dynamic_host, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_opa(s_surface.dynamic_host, UI_TOKEN_OPA_PRIMARY, 0);
    lv_obj_clear_flag(s_surface.dynamic_host,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_surface.interaction_host = lv_obj_create(s_surface.root);
    lv_obj_remove_style_all(s_surface.interaction_host);
    ui_component_style_apply_surface_host(s_surface.interaction_host);
    lv_obj_set_pos(s_surface.interaction_host, 0, 0);
    lv_obj_set_size(s_surface.interaction_host, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_opa(s_surface.interaction_host, UI_TOKEN_OPA_PRIMARY, 0);
    lv_obj_clear_flag(s_surface.interaction_host,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_surface.interaction_host, LV_OBJ_FLAG_HIDDEN);

    s_surface.created = true;
    lv_obj_add_flag(s_surface.root, LV_OBJ_FLAG_HIDDEN);
    return true;
}

bool ui_bottom_surface_is_created(void)
{
    return s_surface.created;
}

lv_obj_t *ui_bottom_surface_root(void)
{
    return s_surface.root;
}

lv_obj_t *ui_bottom_surface_dynamic_host(void)
{
    return s_surface.dynamic_host;
}

lv_obj_t *ui_bottom_surface_interaction_host(void)
{
    return s_surface.interaction_host;
}

bool ui_bottom_surface_mount(lv_obj_t *slot,
                             bool dynamic_content_visible)
{
    if(!s_surface.created || slot == NULL || !lv_obj_is_valid(slot))
        return false;
    s_surface.slot = slot;
    s_surface.mounted = true;
    s_surface.dynamic_content_visible = dynamic_content_visible;
    if(lv_obj_get_parent(s_surface.root) != slot)
        lv_obj_set_parent(s_surface.root, slot);
    apply_state(false);
    return true;
}

void ui_bottom_surface_unmount(void)
{
    if(!s_surface.created || !s_surface.mounted) return;
    s_surface.mounted = false;
    s_surface.dynamic_content_visible = false;
    lv_anim_delete(s_surface.root, y_animation);
    lv_anim_delete(s_surface.root, height_animation);
    lv_anim_delete(s_surface.root, root_opacity_animation);
    lv_anim_delete(s_surface.root, surface_scale_animation);
    lv_obj_add_flag(s_surface.root, LV_OBJ_FLAG_HIDDEN);
    emit_state();
}

void ui_bottom_surface_set_dynamic_focus(bool focused)
{
    if(s_surface.dynamic_focused == focused) return;
    s_surface.dynamic_focused = focused;
    apply_dynamic_focus();
}

bool ui_bottom_surface_present_interaction(
    uint32_t owner_id,
    ui_bottom_surface_size_t size,
    ui_bottom_surface_priority_t priority,
    bool animated)
{
    if(owner_id == UI_BOTTOM_SURFACE_NO_OWNER ||
       size >= UI_BOTTOM_SURFACE_SIZE_COUNT ||
       priority > UI_BOTTOM_SURFACE_PRIORITY_P4_NORMAL) return false;

    if(s_surface.current.mode == UI_BOTTOM_SURFACE_INTERACTION_MODE) {
        if(s_surface.current.owner_id == owner_id) {
            s_surface.current.size = size;
            s_surface.current.priority = priority;
            apply_state(animated);
            return true;
        }
        if(priority > s_surface.current.priority ||
           s_surface.restore_depth >= UI_BOTTOM_SURFACE_STACK_MAX)
            return false;
        s_surface.restore[s_surface.restore_depth++] = s_surface.current;
    }

    s_surface.current.mode = UI_BOTTOM_SURFACE_INTERACTION_MODE;
    s_surface.current.size = size;
    s_surface.current.priority = priority;
    s_surface.current.owner_id = owner_id;
    apply_state(animated);
    return true;
}

bool ui_bottom_surface_update_interaction(uint32_t owner_id,
                                          ui_bottom_surface_size_t size,
                                          bool animated)
{
    if(s_surface.current.mode != UI_BOTTOM_SURFACE_INTERACTION_MODE ||
       s_surface.current.owner_id != owner_id ||
       size >= UI_BOTTOM_SURFACE_SIZE_COUNT) return false;
    s_surface.current.size = size;
    apply_state(animated);
    return true;
}

bool ui_bottom_surface_replace_interaction(
    uint32_t owner_id,
    ui_bottom_surface_size_t size,
    ui_bottom_surface_priority_t priority,
    bool animated)
{
    if(owner_id == UI_BOTTOM_SURFACE_NO_OWNER ||
       size >= UI_BOTTOM_SURFACE_SIZE_COUNT ||
       priority > UI_BOTTOM_SURFACE_PRIORITY_P4_NORMAL) return false;
    if(s_surface.current.mode != UI_BOTTOM_SURFACE_INTERACTION_MODE)
        return ui_bottom_surface_present_interaction(owner_id, size, priority,
                                                     animated);
    s_surface.current.owner_id = owner_id;
    s_surface.current.size = size;
    s_surface.current.priority = priority;
    apply_state(animated);
    return true;
}

bool ui_bottom_surface_dismiss_interaction(uint32_t owner_id,
                                           bool animated)
{
    if(s_surface.current.mode != UI_BOTTOM_SURFACE_INTERACTION_MODE ||
       s_surface.current.owner_id != owner_id) return false;

    if(s_surface.restore_depth > 0U)
        s_surface.current = s_surface.restore[--s_surface.restore_depth];
    else {
        s_surface.current.mode = UI_BOTTOM_SURFACE_DYNAMIC_CARD_MODE;
        s_surface.current.size = UI_BOTTOM_SURFACE_SIZE_NORMAL;
        s_surface.current.priority = UI_BOTTOM_SURFACE_PRIORITY_P4_NORMAL;
        s_surface.current.owner_id = UI_BOTTOM_SURFACE_NO_OWNER;
    }
    apply_state(animated);
    return true;
}

void ui_bottom_surface_reset_to_dynamic(bool animated)
{
    if(!s_surface.created) return;
    s_surface.restore_depth = 0U;
    memset(s_surface.restore, 0, sizeof(s_surface.restore));
    s_surface.current.mode = UI_BOTTOM_SURFACE_DYNAMIC_CARD_MODE;
    s_surface.current.size = UI_BOTTOM_SURFACE_SIZE_NORMAL;
    s_surface.current.priority = UI_BOTTOM_SURFACE_PRIORITY_P4_NORMAL;
    s_surface.current.owner_id = UI_BOTTOM_SURFACE_NO_OWNER;
    apply_state(animated);
}

bool ui_bottom_surface_get_state(ui_bottom_surface_state_t *state)
{
    if(state == NULL) return false;
    *state = (ui_bottom_surface_state_t) {
        .mode = s_surface.current.mode,
        .size = s_surface.current.size,
        .priority = s_surface.current.priority,
        .owner_id = s_surface.current.owner_id,
        .version = s_surface.version,
        .restore_depth = s_surface.restore_depth,
        .mounted = s_surface.mounted,
        .dynamic_content_visible = s_surface.dynamic_content_visible
    };
    return true;
}

void ui_bottom_surface_set_observer(ui_bottom_surface_observer_cb_t observer,
                                    void *user_data)
{
    s_surface.observer = observer;
    s_surface.observer_user_data = user_data;
}
