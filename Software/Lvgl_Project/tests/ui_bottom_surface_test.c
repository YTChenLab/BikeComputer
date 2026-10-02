#include "ui/core/ui_bottom_surface.h"
#include "ui/design/ui_design_tokens.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>

static uint8_t s_draw_buffer[UI_TOKEN_SCREEN_WIDTH * 20 * 2];
static uint32_t s_observer_calls;
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area; (void)pixels;
    lv_display_flush_ready(display);
}

static void state_changed(const ui_bottom_surface_state_t *state,
                          void *user_data)
{
    (void)user_data;
    assert(state != NULL);
    s_observer_calls++;
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(UI_TOKEN_SCREEN_WIDTH,
                                               UI_TOKEN_SCREEN_HEIGHT);
    assert(display != NULL);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, s_draw_buffer, NULL,
                           sizeof(s_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);

    lv_obj_t *slot = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(slot);
    lv_obj_set_pos(slot, UI_BOTTOM_SURFACE_X, UI_BOTTOM_SURFACE_NORMAL_Y);
    lv_obj_set_size(slot, UI_BOTTOM_SURFACE_WIDTH,
                    UI_BOTTOM_SURFACE_NORMAL_HEIGHT);
    lv_obj_t *other_screen = lv_obj_create(NULL);
    lv_obj_t *other_slot = lv_obj_create(other_screen);
    lv_obj_remove_style_all(other_slot);
    lv_obj_set_pos(other_slot, UI_BOTTOM_SURFACE_X,
                   UI_BOTTOM_SURFACE_NORMAL_Y);
    lv_obj_set_size(other_slot, UI_BOTTOM_SURFACE_WIDTH,
                    UI_BOTTOM_SURFACE_NORMAL_HEIGHT);

    ui_bottom_surface_init();
    ui_bottom_surface_set_observer(state_changed, NULL);
    assert(ui_bottom_surface_create(slot));
    assert(ui_bottom_surface_is_created());

    lv_obj_t *root = ui_bottom_surface_root();
    lv_obj_t *normal = ui_bottom_surface_dynamic_host();
    lv_obj_t *interaction = ui_bottom_surface_interaction_host();
    assert(root != NULL && normal != NULL && interaction != NULL);
    assert(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_child_count(root) == 2U);

    assert(ui_bottom_surface_mount(slot, true));
    lv_obj_update_layout(root);
    assert(!lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_parent(root) == slot);
    assert(lv_obj_get_x(root) == 0);
    assert(lv_obj_get_y(root) == 0);
    assert(lv_obj_get_width(root) == UI_BOTTOM_SURFACE_WIDTH);
    assert(lv_obj_get_height(root) == UI_BOTTOM_SURFACE_NORMAL_HEIGHT);
    assert(!lv_obj_has_flag(normal, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(interaction, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_style_border_width(root, 0) == UI_TOKEN_BORDER_THIN);
    ui_bottom_surface_state_t state;
    assert(ui_bottom_surface_get_state(&state));
    assert(state.mounted && state.dynamic_content_visible);

    ui_bottom_surface_set_dynamic_focus(true);
    assert(lv_obj_get_style_border_width(root, 0) == UI_TOKEN_BORDER_FOCUS);
    assert(lv_color_eq(lv_obj_get_style_border_color(root, 0),
                       UI_TOKEN_COLOR_ACCENT));
    ui_bottom_surface_set_dynamic_focus(false);
    assert(lv_obj_get_style_border_width(root, 0) == UI_TOKEN_BORDER_THIN);

    assert(ui_bottom_surface_present_interaction(
        100U, UI_BOTTOM_SURFACE_SIZE_NORMAL,
        UI_BOTTOM_SURFACE_PRIORITY_P3_USER_ACTION, true));
    assert(lv_obj_get_parent(root) == slot);
    assert(lv_obj_has_flag(normal, LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(interaction, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_style_transform_scale_x(root, 0) == 256);
    assert(lv_obj_get_style_transform_scale_y(root, 0) == 256);
    assert(lv_obj_get_style_opa(root, 0) == LV_OPA_COVER);
    assert(lv_obj_get_style_opa(normal, 0) == LV_OPA_COVER);
    assert(lv_obj_get_style_opa(interaction, 0) == LV_OPA_COVER);
    lv_tick_inc(UI_TOKEN_MOTION_SURFACE_MS);
    lv_anim_refr_now();
    assert(lv_obj_get_x(root) == 0 && lv_obj_get_y(root) == 0);
    assert(lv_obj_get_width(root) == UI_BOTTOM_SURFACE_WIDTH);
    assert(lv_obj_has_flag(normal, LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(interaction, LV_OBJ_FLAG_HIDDEN));
    assert(ui_bottom_surface_present_interaction(
        150U, UI_BOTTOM_SURFACE_SIZE_MINI,
        UI_BOTTOM_SURFACE_PRIORITY_P3_USER_ACTION, false));
    assert(ui_bottom_surface_get_state(&state));
    assert(state.owner_id == 150U && state.restore_depth == 1U);
    assert(ui_bottom_surface_dismiss_interaction(150U, false));
    assert(ui_bottom_surface_get_state(&state));
    assert(state.owner_id == 100U && state.restore_depth == 0U);
    assert(!ui_bottom_surface_present_interaction(
        200U, UI_BOTTOM_SURFACE_SIZE_MINI,
        UI_BOTTOM_SURFACE_PRIORITY_P4_NORMAL, false));
    assert(ui_bottom_surface_present_interaction(
        200U, UI_BOTTOM_SURFACE_SIZE_MINI,
        UI_BOTTOM_SURFACE_PRIORITY_P1_RIDING_SAFETY, false));

    assert(ui_bottom_surface_get_state(&state));
    assert(state.owner_id == 200U && state.restore_depth == 1U);
    assert(lv_obj_get_height(root) == UI_BOTTOM_SURFACE_NORMAL_HEIGHT);
    assert(lv_obj_get_y(root) == 0);

    assert(ui_bottom_surface_dismiss_interaction(200U, false));
    assert(ui_bottom_surface_get_state(&state));
    assert(state.owner_id == 100U && state.restore_depth == 0U);
    assert(ui_bottom_surface_replace_interaction(
        101U, UI_BOTTOM_SURFACE_SIZE_NORMAL,
        UI_BOTTOM_SURFACE_PRIORITY_P0_CRITICAL, false));
    assert(ui_bottom_surface_get_state(&state));
    assert(state.owner_id == 101U && state.restore_depth == 0U);
    assert(lv_color_eq(lv_obj_get_style_border_color(root, 0),
                       UI_TOKEN_COLOR_CRITICAL));
    assert(ui_bottom_surface_update_interaction(
        101U, UI_BOTTOM_SURFACE_SIZE_EXPANDED, false));
    assert(lv_obj_get_height(root) == UI_BOTTOM_SURFACE_EXPANDED_HEIGHT);
    assert(lv_obj_get_y(root) == UI_BOTTOM_SURFACE_NORMAL_HEIGHT -
                                 UI_BOTTOM_SURFACE_EXPANDED_HEIGHT);

    assert(ui_bottom_surface_dismiss_interaction(101U, false));
    assert(ui_bottom_surface_get_state(&state));
    assert(state.mode == UI_BOTTOM_SURFACE_DYNAMIC_CARD_MODE);
    assert(state.size == UI_BOTTOM_SURFACE_SIZE_NORMAL);
    assert(!lv_obj_has_flag(normal, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(interaction, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_y(root) == 0);

    assert(ui_bottom_surface_present_interaction(
        250U, UI_BOTTOM_SURFACE_SIZE_NORMAL,
        UI_BOTTOM_SURFACE_PRIORITY_P2_RIDE_STATE, false));
    assert(ui_bottom_surface_present_interaction(
        251U, UI_BOTTOM_SURFACE_SIZE_NORMAL,
        UI_BOTTOM_SURFACE_PRIORITY_P1_RIDING_SAFETY, false));
    assert(ui_bottom_surface_get_state(&state));
    assert(state.mode == UI_BOTTOM_SURFACE_INTERACTION_MODE);
    assert(state.restore_depth == 1U);
    ui_bottom_surface_reset_to_dynamic(false);
    assert(ui_bottom_surface_get_state(&state));
    assert(state.mode == UI_BOTTOM_SURFACE_DYNAMIC_CARD_MODE);
    assert(state.owner_id == UI_BOTTOM_SURFACE_NO_OWNER);
    assert(state.restore_depth == 0U);

    assert(ui_bottom_surface_mount(other_slot, false));
    assert(lv_obj_get_parent(root) == other_slot);
    assert(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
    assert(ui_bottom_surface_get_state(&state));
    assert(state.mounted && !state.dynamic_content_visible);
    assert(ui_bottom_surface_present_interaction(
        300U, UI_BOTTOM_SURFACE_SIZE_MINI,
        UI_BOTTOM_SURFACE_PRIORITY_P1_RIDING_SAFETY, false));
    assert(!lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_x(root) == 0);
    assert(lv_obj_get_y(root) == 0);
    assert(lv_obj_get_height(root) == UI_BOTTOM_SURFACE_NORMAL_HEIGHT);
    assert(ui_bottom_surface_dismiss_interaction(300U, true));
    assert(!lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(normal, LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(interaction, LV_OBJ_FLAG_HIDDEN));
    lv_tick_inc(UI_TOKEN_MOTION_SURFACE_MS);
    lv_anim_refr_now();
    (void)lv_timer_handler();
    assert(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(normal, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(interaction, LV_OBJ_FLAG_HIDDEN));

    ui_bottom_surface_unmount();
    assert(ui_bottom_surface_get_state(&state));
    assert(!state.mounted && !state.dynamic_content_visible);
    assert(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
    assert(ui_bottom_surface_present_interaction(
        301U, UI_BOTTOM_SURFACE_SIZE_NORMAL,
        UI_BOTTOM_SURFACE_PRIORITY_P1_RIDING_SAFETY, false));
    assert(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
    assert(ui_bottom_surface_mount(other_slot, false));
    assert(!lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
    assert(ui_bottom_surface_dismiss_interaction(301U, false));
    assert(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));

    for(uint16_t cycle = 0; cycle < 250U; cycle++) {
        assert(ui_bottom_surface_mount(slot, true));
        assert(lv_obj_get_parent(root) == slot);
        assert(!lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
        ui_bottom_surface_unmount();
        assert(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
        assert(ui_bottom_surface_mount(other_slot, false));
        assert(lv_obj_get_parent(root) == other_slot);
        assert(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
        ui_bottom_surface_unmount();
    }
    assert(lv_obj_get_child_count(lv_layer_top()) == 0U);
    assert(s_observer_calls >= 6U);

    lv_deinit();
    puts("ui_bottom_surface_test: PASS");
    return 0;
}
