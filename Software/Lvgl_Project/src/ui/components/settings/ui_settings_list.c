#include "ui_settings_list.h"

#include "../../core/ui_colors.h"
#include "../../core/ui_motion.h"
#include "../../core/ui_sound.h"
#include "../../core/ui_theme.h"
#include "../../design/ui_design_tokens.h"

#include <stdio.h>
#include <string.h>

#define ROW_HEIGHT 58
#define ROW_GAP     6

static ui_settings_row_t *find_row(ui_settings_list_t *list, uint16_t id)
{
    if(list == NULL) return NULL;
    for(uint8_t i = 0; i < list->row_count; i++)
        if(list->rows[i].id == id) return &list->rows[i];
    return NULL;
}

static lv_obj_t *make_label(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t width, const char *text,
                            const lv_font_t *font, lv_color_t color,
                            lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

static void apply_selection(ui_settings_list_t *list)
{
    if(list == NULL) return;
    for(uint8_t i = 0; i < list->row_count; i++) {
        bool selected = i == list->selection;
        lv_obj_set_style_border_color(list->rows[i].root,
                                      selected ? UI_COLOR_YELLOW :
                                      UI_TOKEN_COLOR_SURFACE_BORDER, 0);
        lv_obj_set_style_border_width(list->rows[i].root,
                                      selected ? UI_TOKEN_BORDER_FOCUS :
                                      UI_TOKEN_BORDER_THIN, 0);
    }
}

static void activate_row(ui_settings_row_t *row)
{
    if(row == NULL || row->owner == NULL) return;
    ui_settings_list_t *list = row->owner;
    for(uint8_t i = 0; i < list->row_count; i++) {
        if(&list->rows[i] == row) {
            list->selection = i;
            break;
        }
    }
    apply_selection(list);
    ui_sound_play(UI_SFX_PRESS);
    if(list->callback != NULL) list->callback(row->id, list->user_data);
}

static void row_event(lv_event_t *event)
{
    ui_settings_row_t *row = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if(row == NULL || row->root == NULL) return;
    if(code == LV_EVENT_PRESSED) ui_motion_button_press(row->root);
    else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
        ui_motion_button_release(row->root);
    else if(code == LV_EVENT_CLICKED) activate_row(row);
}

void ui_settings_list_create(ui_settings_list_t *list, lv_obj_t *parent,
                             int32_t x, int32_t y,
                             int32_t width, int32_t height,
                             ui_settings_list_action_cb_t callback,
                             void *user_data)
{
    if(list == NULL || parent == NULL) return;
    memset(list, 0, sizeof(*list));
    list->callback = callback;
    list->user_data = user_data;
    list->root = lv_obj_create(parent);
    lv_obj_remove_style_all(list->root);
    lv_obj_set_pos(list->root, x, y);
    lv_obj_set_size(list->root, width, height);
    lv_obj_set_flex_flow(list->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list->root, ROW_GAP, 0);
    lv_obj_set_scroll_dir(list->root, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list->root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(list->root, LV_OBJ_FLAG_SCROLLABLE);
}

bool ui_settings_list_add(ui_settings_list_t *list, uint16_t id,
                          ui_settings_row_type_t type,
                          const char *title, const char *subtitle,
                          const char *value)
{
    if(list == NULL || list->root == NULL ||
       list->row_count >= UI_SETTINGS_LIST_MAX_ROWS) return false;
    ui_settings_row_t *row = &list->rows[list->row_count++];
    memset(row, 0, sizeof(*row));
    row->owner = list;
    row->id = id;
    row->type = type;
    row->root = lv_obj_create(list->root);
    lv_obj_remove_style_all(row->root);
    lv_obj_set_size(row->root, LV_PCT(100), ROW_HEIGHT);
    lv_obj_set_flex_grow(row->root, 0);
    lv_obj_set_style_bg_color(row->root, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(row->root, LV_OPA_90, 0);
    lv_obj_set_style_border_color(row->root,
                                  UI_TOKEN_COLOR_SURFACE_BORDER, 0);
    lv_obj_set_style_border_width(row->root, UI_TOKEN_BORDER_THIN, 0);
    lv_obj_set_style_radius(row->root, UI_TOKEN_RADIUS_MEDIUM, 0);
    lv_obj_add_flag(row->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(row->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row->root, row_event, LV_EVENT_ALL, row);

    row->title = make_label(row->root, 16, 7, 218, title,
                            UI_FONT_PAGE_TEXT, UI_COLOR_WHITE,
                            LV_TEXT_ALIGN_LEFT);
    row->subtitle = make_label(row->root, 16, 33, 255, subtitle,
                               &lv_font_montserrat_12,
                               UI_TOKEN_COLOR_TEXT_SECONDARY,
                               LV_TEXT_ALIGN_LEFT);
    row->value = make_label(row->root, 264, 18,
                            type == UI_SETTINGS_ROW_TOGGLE ? 86 : 130,
                            value, &lv_font_montserrat_16,
                            UI_TOKEN_COLOR_TEXT_SECONDARY,
                            LV_TEXT_ALIGN_RIGHT);

    if(type == UI_SETTINGS_ROW_TOGGLE) {
        row->toggle_track = lv_obj_create(row->root);
        lv_obj_remove_style_all(row->toggle_track);
        lv_obj_set_pos(row->toggle_track, 372, 15);
        lv_obj_set_size(row->toggle_track, 50, 28);
        lv_obj_set_style_radius(row->toggle_track, LV_RADIUS_CIRCLE, 0);
        row->toggle_knob = lv_obj_create(row->toggle_track);
        lv_obj_remove_style_all(row->toggle_knob);
        lv_obj_set_size(row->toggle_knob, 22, 22);
        lv_obj_set_y(row->toggle_knob, 3);
        lv_obj_set_style_radius(row->toggle_knob, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(row->toggle_knob, UI_COLOR_WHITE, 0);
        lv_obj_set_style_bg_opa(row->toggle_knob, LV_OPA_COVER, 0);
        (void)ui_settings_list_set_toggle(list, id, false);
    }
    else if(type != UI_SETTINGS_ROW_INFO) {
        make_label(row->root, 408, 17, 20, LV_SYMBOL_RIGHT,
                   LV_FONT_DEFAULT, UI_COLOR_WHITE, LV_TEXT_ALIGN_RIGHT);
    }
    apply_selection(list);
    return true;
}

bool ui_settings_list_set_value(ui_settings_list_t *list, uint16_t id,
                                const char *value)
{
    ui_settings_row_t *row = find_row(list, id);
    if(row == NULL || row->value == NULL) return false;
    lv_label_set_text(row->value, value != NULL ? value : "");
    return true;
}

bool ui_settings_list_set_toggle(ui_settings_list_t *list, uint16_t id,
                                 bool enabled)
{
    ui_settings_row_t *row = find_row(list, id);
    if(row == NULL || row->type != UI_SETTINGS_ROW_TOGGLE ||
       row->toggle_track == NULL || row->toggle_knob == NULL) return false;
    row->enabled = enabled;
    lv_obj_set_style_bg_color(row->toggle_track,
                              enabled ? UI_COLOR_YELLOW :
                              UI_COLOR_SEGMENT_OFF, 0);
    lv_obj_set_style_bg_opa(row->toggle_track, LV_OPA_COVER, 0);
    lv_obj_set_x(row->toggle_knob, enabled ? 25 : 3);
    lv_label_set_text(row->value, "");
    return true;
}

bool ui_settings_list_set_feedback(ui_settings_list_t *list, uint16_t id,
                                   ui_settings_row_feedback_t feedback)
{
    ui_settings_row_t *row = find_row(list, id);
    if(row == NULL || row->subtitle == NULL) return false;
    lv_obj_set_style_text_color(row->subtitle,
        feedback == UI_SETTINGS_ROW_FAILED ? UI_TOKEN_COLOR_CRITICAL :
        (feedback == UI_SETTINGS_ROW_PENDING ? UI_COLOR_YELLOW :
                                               UI_TOKEN_COLOR_TEXT_SECONDARY),
        0);
    if(row->type == UI_SETTINGS_ROW_TOGGLE && row->value != NULL) {
        lv_label_set_text(row->value,
            feedback == UI_SETTINGS_ROW_PENDING ? "WAIT" :
            (feedback == UI_SETTINGS_ROW_FAILED ? "FAILED" : ""));
        lv_obj_set_style_text_color(row->value,
            feedback == UI_SETTINGS_ROW_FAILED ? UI_TOKEN_COLOR_CRITICAL :
            UI_COLOR_YELLOW, 0);
    }
    return true;
}

bool ui_settings_list_handle_input(ui_settings_list_t *list,
                                   ui_input_event_t input)
{
    if(list == NULL || list->row_count == 0U) return false;
    if(input == UI_INPUT_ENCODER_CW) {
        if(list->selection + 1U < list->row_count) list->selection++;
        else return true;
    }
    else if(input == UI_INPUT_ENCODER_CCW) {
        if(list->selection > 0U) list->selection--;
        else return true;
    }
    else if(input == UI_INPUT_ENCODER_PRESS) {
        activate_row(&list->rows[list->selection]);
        return true;
    }
    else return false;

    apply_selection(list);
    lv_obj_scroll_to_view(list->rows[list->selection].root, LV_ANIM_ON);
    ui_sound_play(UI_SFX_FOCUS);
    return true;
}

bool ui_settings_list_contains_point(const ui_settings_list_t *list,
                                     int32_t x, int32_t y)
{
    if(list == NULL || list->root == NULL) return false;
    lv_area_t area;
    lv_obj_get_coords(list->root, &area);
    return x >= area.x1 && x <= area.x2 &&
           y >= area.y1 && y <= area.y2;
}
