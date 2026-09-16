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
    lv_obj_t *normal_parent;
    lv_obj_t *fallback_parent;
    lv_obj_t *dynamic_host;
    lv_obj_t *interaction_host;
    ui_bottom_surface_entry_t current;
    ui_bottom_surface_entry_t restore[UI_BOTTOM_SURFACE_STACK_MAX];
    ui_bottom_surface_observer_cb_t observer;
    void *observer_user_data;
    uint32_t version;
    uint8_t restore_depth;
    bool normal_page_active;
    bool dynamic_focused;
    bool created;
} ui_bottom_surface_context_t;

static ui_bottom_surface_context_t s_surface;

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

static int32_t surface_y(ui_bottom_surface_size_t size)
{
    return UI_TOKEN_SCREEN_HEIGHT - UI_BOTTOM_SURFACE_BOTTOM_MARGIN -
           surface_height(size);
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
    lv_anim_set_duration(&animation, UI_TOKEN_MOTION_FAST_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_set_user_data(&animation, host);
    if(completed != NULL) lv_anim_set_completed_cb(&animation, completed);
    lv_anim_start(&animation);
}

static bool should_be_visible(void)
{
    return s_surface.current.mode == UI_BOTTOM_SURFACE_INTERACTION_MODE ||
           s_surface.normal_page_active;
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

static void apply_mount(void)
{
    if(!s_surface.created) return;
    lv_obj_t *parent = s_surface.normal_page_active ?
                       s_surface.normal_parent : s_surface.fallback_parent;
    if(parent != NULL && lv_obj_get_parent(s_surface.root) != parent)
        lv_obj_set_parent(s_surface.root, parent);
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
    if(s_surface.normal_page_active &&
       visual_size == UI_BOTTOM_SURFACE_SIZE_MINI)
        visual_size = UI_BOTTOM_SURFACE_SIZE_NORMAL;
    int32_t height = surface_height(visual_size);
    int32_t x = s_surface.normal_page_active ? 0 : UI_BOTTOM_SURFACE_X;
    int32_t y = s_surface.normal_page_active ?
                UI_BOTTOM_SURFACE_NORMAL_HEIGHT - height :
                surface_y(visual_size);
    bool visible = should_be_visible();
    if(!visible) {
        lv_obj_add_flag(s_surface.root, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    bool was_hidden = lv_obj_has_flag(s_surface.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_surface.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_surface.root);
    if(animated && !was_hidden) {
        animate_y(s_surface.root, y);
        animate_height(s_surface.root, height);
    }
    else {
        lv_anim_delete(s_surface.root, y_animation);
        lv_anim_delete(s_surface.root, height_animation);
        lv_obj_set_pos(s_surface.root, x, y);
        lv_obj_set_size(s_surface.root, UI_BOTTOM_SURFACE_WIDTH, height);
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

    s_surface.normal_parent = parent;
    s_surface.fallback_parent = lv_layer_top();
    s_surface.root = lv_obj_create(s_surface.fallback_parent);
    lv_obj_remove_style_all(s_surface.root);
    ui_component_style_apply_bottom_surface(s_surface.root);
    lv_obj_set_pos(s_surface.root, UI_BOTTOM_SURFACE_X,
                   UI_BOTTOM_SURFACE_NORMAL_Y);
    lv_obj_set_size(s_surface.root, UI_BOTTOM_SURFACE_WIDTH,
                    UI_BOTTOM_SURFACE_NORMAL_HEIGHT);
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

void ui_bottom_surface_set_normal_page_active(bool active)
{
    if(s_surface.normal_page_active == active) return;
    s_surface.normal_page_active = active;
    apply_mount();
    apply_geometry(false);
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
        .normal_page_active = s_surface.normal_page_active
    };
    return true;
}

void ui_bottom_surface_set_observer(ui_bottom_surface_observer_cb_t observer,
                                    void *user_data)
{
    s_surface.observer = observer;
    s_surface.observer_user_data = user_data;
}
