#include "ui_nav_data_strip.h"

#include "ui_nav_layout.h"
#include "../../core/ui_colors.h"
#include "../../core/ui_theme.h"

#include <stdio.h>
#include <string.h>

#define NAV_DATA_COLUMN_W       153
#define NAV_DATA_VALUE_TOP        5
#define NAV_DATA_VALUE_H         47
#define NAV_DATA_TITLE_TOP       58
#define NAV_DATA_TITLE_H         28
#define NAV_DATA_UNIT_GAP         4
#define NAV_DATA_DIVIDER_TOP     14
#define NAV_DATA_DIVIDER_H       64

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

static void create_group(ui_nav_data_strip_t *strip, uint8_t index,
                         int32_t x, int32_t width, const char *title,
                         const char *unit)
{
    strip->groups[index] = plain_obj(strip->root);
    lv_obj_set_pos(strip->groups[index], x, 0);
    lv_obj_set_size(strip->groups[index], width, UI_NAV_DATA_HEIGHT);

    strip->value_rows[index] = plain_obj(strip->groups[index]);
    lv_obj_set_pos(strip->value_rows[index], 0, NAV_DATA_VALUE_TOP);
    lv_obj_set_size(strip->value_rows[index], width, NAV_DATA_VALUE_H);
    lv_obj_set_layout(strip->value_rows[index], LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(strip->value_rows[index], LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip->value_rows[index], LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(strip->value_rows[index], NAV_DATA_UNIT_GAP, 0);

    strip->values[index] = make_label(strip->value_rows[index], "--",
                                      UI_FONT_NAV_STRIP_VALUE,
                                      UI_COLOR_WHITE);
    strip->units[index] = make_label(strip->value_rows[index], unit,
                                     UI_FONT_NAV_STRIP_UNIT,
                                     UI_TOKEN_COLOR_TEXT_SECONDARY);
    if(unit == NULL || unit[0] == '\0')
        lv_obj_add_flag(strip->units[index], LV_OBJ_FLAG_HIDDEN);

    strip->titles[index] = make_label(strip->groups[index], title,
                                      UI_FONT_PAGE_TEXT,
                                      UI_TOKEN_COLOR_TEXT_SECONDARY);
    lv_obj_set_pos(strip->titles[index], 0, NAV_DATA_TITLE_TOP);
    lv_obj_set_size(strip->titles[index], width, NAV_DATA_TITLE_H);
}

void ui_nav_data_strip_create(ui_nav_data_strip_t *strip, lv_obj_t *parent)
{
    if(strip == NULL || parent == NULL) return;
    memset(strip, 0, sizeof(*strip));

    strip->root = plain_obj(parent);
    lv_obj_set_pos(strip->root, UI_NAV_PAGE_MARGIN, UI_NAV_DATA_TOP);
    lv_obj_set_size(strip->root, UI_NAV_DATA_WIDTH, UI_NAV_DATA_HEIGHT);
    lv_obj_set_style_bg_color(strip->root, lv_color_hex(0x050706), 0);
    lv_obj_set_style_bg_opa(strip->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(strip->root, lv_color_hex(0x303431), 0);
    lv_obj_set_style_border_opa(strip->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(strip->root, 1, 0);
    lv_obj_set_style_border_side(strip->root,
                                 LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM,
                                 0);

    create_group(strip, 0, 0, NAV_DATA_COLUMN_W, "实时速度", "km/h");
    create_group(strip, 1, NAV_DATA_COLUMN_W, NAV_DATA_COLUMN_W + 1,
                 "剩余里程", "km");
    create_group(strip, 2, (NAV_DATA_COLUMN_W * 2) + 1,
                 UI_NAV_DATA_WIDTH - ((NAV_DATA_COLUMN_W * 2) + 1),
                 "预计到达", "");

    for(uint8_t index = 0; index < 2U; index++) {
        strip->dividers[index] = plain_obj(strip->root);
        lv_obj_set_pos(strip->dividers[index],
                       (index + 1) * NAV_DATA_COLUMN_W,
                       NAV_DATA_DIVIDER_TOP);
        lv_obj_set_size(strip->dividers[index], 1, NAV_DATA_DIVIDER_H);
        lv_obj_set_style_bg_color(strip->dividers[index],
                                  lv_color_hex(0x303431), 0);
        lv_obj_set_style_bg_opa(strip->dividers[index], LV_OPA_COVER, 0);
    }
}

void ui_nav_data_strip_update(ui_nav_data_strip_t *strip,
                              const nav_model_t *model)
{
    if(strip == NULL || strip->root == NULL || model == NULL) return;
    lv_label_set_text_fmt(strip->values[0], "%.1f", (double)model->speed);
    lv_label_set_text_fmt(strip->values[1], "%.1f", (double)model->distance);
    lv_label_set_text_fmt(strip->values[2], "%02u:%02u",
                          (unsigned)model->eta_hour,
                          (unsigned)model->eta_min);
}
