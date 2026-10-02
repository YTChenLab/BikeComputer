/**
 * @file ui_metric_card.c
 * @brief 公路车码表 Dashboard 数据指标组件实现
 */


/*********************
 *      INCLUDES
 *********************/

#include "ui_metric_card.h"

#include "../core/ui_colors.h"
#include "../core/ui_theme.h"
#include "lvgl/src/misc/lv_text_private.h"

#include <string.h>


/**********************
 *   STATIC FUNCTIONS
 **********************/


/*
 * 标题与“数值 + 单位”是两组独立视觉元素。
 *
 * 数字宽度会随内容变化；每次更新后都重新计算组宽，保证每张卡片
 * 的读数在自身 237 px 区域内居中，同时使单位的底部与数字底部对齐。
 */
static bool font_supports_value(const lv_font_t *font, const char *text)
{
    uint32_t offset=0;
    while(text[offset]) {
        uint32_t letter=lv_text_encoded_next(text,&offset);
        lv_font_glyph_dsc_t glyph;
        if(!lv_font_get_glyph_dsc(font,&glyph,letter,0) || glyph.is_placeholder) return false;
    }
    return true;
}

static void ui_metric_card_layout(ui_metric_card_t *card)
{
    if(card == NULL || card->root == NULL || card->title == NULL ||
       card->value == NULL || card->unit == NULL)
    {
        return;
    }

    lv_obj_update_layout(card->root);

    const int32_t card_width = lv_obj_get_width(card->root);
    const int32_t card_height = lv_obj_get_height(card->root);
    const char *unit_text = lv_label_get_text(card->unit);
    const bool has_unit = unit_text != NULL && unit_text[0] != '\0';
    const int32_t unit_gap = has_unit ? 7 : 0;
    const int32_t available = card_width - unit_gap -
        (has_unit ? lv_obj_get_width(card->unit) : 0) - (has_unit ? 16 : 8);
    const lv_font_t *fonts[] = {card->preferred_value_font,
        UI_FONT_METRIC_VALUE_SMALL, &lv_font_montserrat_48, &lv_font_montserrat_32,
        &lv_font_montserrat_24, &lv_font_montserrat_16};
    for(unsigned i = 0; i < sizeof(fonts) / sizeof(fonts[0]); ++i) {
        if(fonts[i] == NULL || !font_supports_value(fonts[i],lv_label_get_text(card->value))) continue;
        lv_obj_set_style_text_font(card->value, fonts[i], 0);
        lv_obj_update_layout(card->root);
        if(lv_obj_get_width(card->value) <= available) break;
    }
    const int32_t value_width = lv_obj_get_width(card->value);
    const int32_t value_height = lv_obj_get_height(card->value);
    const int32_t unit_width = has_unit ? lv_obj_get_width(card->unit) : 0;
    const int32_t unit_height = has_unit ? lv_obj_get_height(card->unit) : 0;
    const int32_t group_width = value_width + unit_gap + unit_width;
    const int32_t value_x = (card_width - group_width) / 2;
    /*
     * 标题和读数组合在可用区域内垂直居中。
     * 紧凑卡片底部的 12 px 用于训练区间色条，不属于性能浮层。
     */
    const bool compact_card = card_height <= 105;
    const int32_t title_height = lv_obj_get_height(card->title);
    const int32_t usable_height = card_height - (compact_card ? 12 : 0);
    const int32_t group_top = LV_MAX(0, (usable_height - title_height - 6 - value_height) / 2);
    const int32_t value_y = group_top + title_height + 6;

    lv_obj_set_pos(card->title, 0, group_top);
    lv_obj_set_width(card->title, card_width);
    lv_obj_set_style_text_align(card->title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_set_pos(card->value, value_x, value_y);

    if(has_unit)
    {
        lv_obj_set_pos(
            card->unit,
            value_x + value_width + unit_gap,
            value_y + value_height - unit_height
        );
    }
    else
    {
        lv_obj_set_pos(card->unit, 0, 0);
    }
}


/**********************
 *   GLOBAL FUNCTIONS
 **********************/


/**
 * @brief 创建一个 Dashboard 数据指标组件
 *
 * 典型布局：
 *
 * ┌────────────────────┐
 * │ 心率               │
 * │                    │
 * │ 156 bpm            │
 * │                    │
 * └────────────────────┘
 *
 * 本组件本身不绘制外框。
 *
 * Dashboard 的横线、竖线由 ui_dashboard.c 统一绘制。
 */
void ui_metric_card_create(
    ui_metric_card_t *card,
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    int32_t w,
    int32_t h,
    const char *title,
    const char *value,
    const char *unit,
    const lv_font_t *value_font)
{
    /*
     * ========================================================
     * 参数检查
     * ========================================================
     */

    if(card == NULL)
    {
        return;
    }

    if(parent == NULL)
    {
        return;
    }
    card->preferred_value_font = value_font != NULL ? value_font : UI_FONT_METRIC_VALUE;


    /*
     * ========================================================
     * 1. 创建根对象
     * ========================================================
     *
     * root 主要负责：
     *
     * - 保存该指标区域的位置
     * - 管理 title / value / unit
     *
     * root 自己不绘制背景和边框。
     */

    card->root =
        lv_obj_create(parent);


    /*
     * 去掉 LVGL lv_obj 默认样式。
     *
     * 默认 lv_obj 通常会带：
     *
     * - Background
     * - Border
     * - Radius
     * - Padding
     *
     * 这里全部清除，
     * 保证像素坐标完全可控。
     */
    lv_obj_remove_style_all(
        card->root
    );


    /*
     * 设置整个指标区域的位置。
     */
    lv_obj_set_pos(
        card->root,
        x,
        y
    );


    /*
     * 设置区域尺寸。
     */
    lv_obj_set_size(
        card->root,
        w,
        h
    );


    /*
     * Dashboard 为固定布局，
     * 指标组件不允许滚动。
     */
    lv_obj_clear_flag(
        card->root,
        LV_OBJ_FLAG_SCROLLABLE
    );


    /*
     * ========================================================
     * 2. 创建标题
     * ========================================================
     *
     * 例如：
     *
     * 心率
     * 功率
     * 踏频
     */

    card->title =
        lv_label_create(
            card->root
        );


    /*
     * 设置标题文字。
     */
    lv_label_set_text(
        card->title,
        title != NULL
            ? title
            : ""
    );


    /*
     * 标题在数据区域内部的位置。
     *
     * 后续接入真实字体后，
     * 这里可能再进行 1~5 px 级微调。
     */
    /* 标题使用纯白，保证全屏文字语言一致。 */
    lv_obj_set_style_text_color(
        card->title,
        UI_COLOR_WHITE,
        0
    );


    /*
     * 使用 Theme 中定义的数据标题字体。
     */
    lv_obj_set_style_text_font(
        card->title,
        UI_FONT_METRIC_TITLE,
        0
    );


    /*
     * ========================================================
     * 3. 创建实时数值
     * ========================================================
     *
     * 例如：
     *
     * 156
     * 223
     * 85
     * 4.3
     */

    card->value =
        lv_label_create(
            card->root
        );


    lv_label_set_text(
        card->value,
        value != NULL
            ? value
            : ""
    );


    /*
     * 主数值使用纯白。
     */
    lv_obj_set_style_text_color(
        card->value,
        UI_COLOR_WHITE,
        0
    );


    /*
     * 使用数据数值字体。
     *
     * 默认使用 UI_FONT_METRIC_VALUE；调用者可以传入
     * UI_FONT_METRIC_VALUE_SMALL 等专用字体。
     *
     * 这样可以避免不同字号的数值被统一压缩或裁切。
     */
    lv_obj_set_style_text_font(
        card->value,
        value_font != NULL
            ? value_font
            : UI_FONT_METRIC_VALUE,
        0
    );


    /*
     * ========================================================
     * 4. 创建单位
     * ========================================================
     *
     * 例如：
     *
     * bpm
     * W
     * rpm
     * %
     * km
     */

    card->unit =
        lv_label_create(
            card->root
        );


    lv_label_set_text(
        card->unit,
        unit != NULL
            ? unit
            : ""
    );


    /* 单位与数值同为纯白；以字号区分层级，不再用颜色分层。 */
    lv_obj_set_style_text_color(
        card->unit,
        UI_COLOR_WHITE,
        0
    );


    /*
     * 设置单位字体。
     */
    lv_obj_set_style_text_font(
        card->unit,
        UI_FONT_METRIC_UNIT,
        0
    );


    ui_metric_card_layout(card);
}

void ui_metric_card_set_value_font(ui_metric_card_t *card, const lv_font_t *font)
{
    if(card == NULL || card->value == NULL || font == NULL) return;
    card->preferred_value_font = font;
    ui_metric_card_layout(card);
}


/**
 * @brief 修改一个数据指标的数值
 */
void ui_metric_card_set_value(
    ui_metric_card_t *card,
    const char *value)
{
    /*
     * ========================================================
     * 参数检查
     * ========================================================
     */

    if(card == NULL)
    {
        return;
    }


    if(card->value == NULL)
    {
        return;
    }

    const char *value_text = value != NULL ? value : "";
    if(strcmp(lv_label_get_text(card->value), value_text) == 0)
    {
        return;
    }


    /*
     * ========================================================
     * 更新数值文字
     * ========================================================
     */

    lv_label_set_text(
        card->value,
        value_text
    );


    /*
     * ========================================================
     * 重新计算 Label 大小
     * ========================================================
     *
     * 数值更新以后，
     * Label 宽度可能发生变化。
     *
     * 例如：
     *
     * 85
     *
     * 更新为：
     *
     * 105
     *
     * 单位位置必须跟着变化。
     */

    ui_metric_card_layout(card);
}


/**
 * @brief 修改指标单位
 */
void ui_metric_card_set_unit(
    ui_metric_card_t *card,
    const char *unit)
{
    /*
     * 参数检查。
     */

    if(card == NULL)
    {
        return;
    }


    if(card->unit == NULL)
    {
        return;
    }

    const char *unit_text = unit != NULL ? unit : "";
    if(strcmp(lv_label_get_text(card->unit), unit_text) == 0)
    {
        return;
    }


    /*
     * 更新单位文字。
     */

    lv_label_set_text(
        card->unit,
        unit_text
    );


    /*
     * 更新布局。
     */

    ui_metric_card_layout(card);
}
