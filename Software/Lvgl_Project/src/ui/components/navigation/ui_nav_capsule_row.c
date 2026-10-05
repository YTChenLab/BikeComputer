#include "ui_nav_capsule_row.h"
#include "ui_nav_visual.h"
#include "../../core/ui_motion.h"
#include <string.h>

static void left_edge(void *target, int32_t value)
{
    lv_obj_t *o = target;
    int right = lv_obj_get_style_x(o, 0) + lv_obj_get_style_width(o, 0);
    int limit = lv_obj_get_width(lv_obj_get_parent(o));
    int left = LV_CLAMP(0, value, limit - 32);
    lv_obj_set_x(o, left);
    lv_obj_set_width(o, LV_CLAMP(32, right - left, limit - left));
}

static void right_edge(void *target, int32_t value)
{
    lv_obj_t *o = target;
    int left = lv_obj_get_style_x(o, 0);
    int limit = lv_obj_get_width(lv_obj_get_parent(o));
    lv_obj_set_width(o, LV_CLAMP(32, value - left, limit - left));
}

bool ui_nav_capsule_row_contains(const ui_nav_capsule_row_t *row, lv_obj_t *button)
{
    if(!row || !row->root) return false;
    for(unsigned i = 0; i < row->count; ++i)
        if(row->buttons[i] == button) return true;
    return false;
}

void ui_nav_capsule_row_focus(ui_nav_capsule_row_t *row, lv_obj_t *button,
                             bool active, bool animate)
{
    if(!row || !row->root) return;
    unsigned selected = row->selected;
    for(unsigned i = 0; i < row->count; ++i)
        if(row->buttons[i] == button) selected = i;
    active = active && !lv_obj_has_state(row->buttons[selected], LV_STATE_DISABLED);
    if(animate && active == row->active && selected == row->selected) return;
    row->selected = selected;
    row->active = active;
    if(!active) {
        ui_motion_cancel(row->capsule, left_edge);
        ui_motion_cancel(row->capsule, right_edge);
        lv_obj_add_flag(row->capsule, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(row->capsule, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(row->root);
    lv_obj_t *b = row->buttons[selected];
    int x = lv_obj_get_x(b), w = lv_obj_get_width(b);
    int left = lv_obj_get_style_x(row->capsule, 0);
    int right = left + lv_obj_get_style_width(row->capsule, 0);
    if(animate) {
        ui_motion_spring(row->capsule, left_edge, left, x, x >= left, NULL, NULL);
        ui_motion_spring(row->capsule, right_edge, right, x + w, x < left, NULL, NULL);
    } else {
        ui_motion_cancel(row->capsule, left_edge);
        ui_motion_cancel(row->capsule, right_edge);
        lv_obj_set_pos(row->capsule, x, 0);
        lv_obj_set_size(row->capsule, w, lv_obj_get_height(row->root));
    }
}

void ui_nav_capsule_row_create(ui_nav_capsule_row_t *row, lv_obj_t *parent,
                              int x, int y, int width, int height,
                              lv_obj_t **buttons, unsigned count)
{
    if(!row || !parent || !buttons || !count || count > 4) return;
    memset(row, 0, sizeof(*row));
    row->count = count;
    row->root = ui_nav_surface(parent, x, y, width, height, false);
    lv_obj_set_style_bg_color(row->root, lv_color_hex(0xB8B8B8), 0);
    lv_obj_set_style_bg_opa(row->root, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row->root, height / 2, 0);
    lv_obj_set_style_clip_corner(row->root, true, 0);
    row->capsule = ui_nav_surface(row->root, 0, 0, width / count, height, false);
    lv_obj_set_style_bg_color(row->capsule, lv_color_hex(0xFFD900), 0);
    lv_obj_set_style_bg_opa(row->capsule, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row->capsule, height / 2, 0);
    for(unsigned i = 0; i < count; ++i) {
        row->buttons[i] = buttons[i];
        lv_obj_set_parent(buttons[i], row->root);
        int begin = (int)i * width / (int)count;
        int end = (int)(i + 1) * width / (int)count;
        lv_obj_set_pos(buttons[i], begin, 0);
        lv_obj_set_size(buttons[i], end - begin, height);
        lv_obj_set_style_bg_opa(buttons[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(buttons[i], 0, 0);
        lv_obj_set_style_radius(buttons[i], height / 2, 0);
        lv_obj_set_style_opa(buttons[i], LV_OPA_50, LV_STATE_DISABLED);
        lv_obj_t *text = lv_obj_get_child(buttons[i], 0);
        lv_obj_set_width(text, end - begin - 8);
        lv_obj_set_style_text_color(text, lv_color_hex(0x101010), 0);
        lv_obj_align(text, LV_ALIGN_CENTER, 0, 0);
    }
    ui_nav_capsule_row_focus(row, buttons[0], true, false);
}
