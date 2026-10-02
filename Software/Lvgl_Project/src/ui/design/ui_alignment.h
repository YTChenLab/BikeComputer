#ifndef UI_ALIGNMENT_H
#define UI_ALIGNMENT_H

#include "lvgl.h"

/* Use layout anchors rather than estimated font heights. These anchors remain
 * valid when a value changes font, a focus border grows, or the parent resizes. */
static inline void ui_align_label_center(lv_obj_t *label)
{
    lv_obj_set_size(label, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
}

static inline void ui_align_center_y(lv_obj_t *object, int32_t x)
{
    lv_obj_align(object, LV_ALIGN_LEFT_MID, x, 0);
}

static inline void ui_align_single_line_font_changed(lv_event_t *event)
{
    lv_obj_t *label = lv_event_get_target_obj(event);
    const lv_font_t *font = lv_obj_get_style_text_font(label, 0);
    if(font && lv_obj_get_style_height(label, 0) != (int32_t)font->line_height)
        lv_obj_set_height(label, font->line_height);
}

static inline void ui_align_label_single_line(lv_obj_t *label)
{
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_height(label, lv_obj_get_style_text_font(label, 0)->line_height);
    lv_obj_add_event_cb(label, ui_align_single_line_font_changed,
                        LV_EVENT_STYLE_CHANGED, NULL);
}

static inline lv_obj_t *ui_align_text_pair(lv_obj_t *parent, lv_obj_t *title,
                                           lv_obj_t *subtitle, int32_t x,
                                           int32_t width, int32_t gap)
{
    lv_obj_t *stack = lv_obj_create(parent);
    lv_obj_remove_style_all(stack);
    lv_obj_set_size(stack, width, LV_PCT(100));
    ui_align_center_y(stack, x);
    lv_obj_clear_flag(stack, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(stack, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(stack, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(stack, gap, 0);
    lv_obj_set_parent(title, stack);
    lv_obj_set_parent(subtitle, stack);
    lv_obj_set_size(title, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_size(subtitle, LV_PCT(100), LV_SIZE_CONTENT);
    ui_align_label_single_line(title);
    ui_align_label_single_line(subtitle);
    return stack;
}

#endif
