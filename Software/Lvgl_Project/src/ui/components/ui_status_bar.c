#include "ui_status_bar.h"

#include "../core/ui_colors.h"
#include "../core/ui_sizes.h"
#include "../core/ui_theme.h"
#include "../design/ui_alignment.h"

#include <stdio.h>
#include <string.h>


/*
 * 顶部三枚连接图标使用 LVGL 内置的同一套 20 px 符号字体。
 * 同一字形系统共享基线和字框，避免外部 SVG 在缩放后出现断笔、裁切
 * 或视觉高度不一致。
 */
static lv_obj_t *create_status_symbol(
    lv_obj_t *parent,
    const char *symbol,
    int32_t x,
    int32_t width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, symbol);
    /* Use a vertical anchor so changing the status font retains alignment. */
    lv_obj_set_pos(label, x, 16);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_WHITE, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);

    return label;
}


/*
 * 信号.svg 的小尺寸等价绘制：两层左右弧线加中心定位环。
 * 直接以 LVGL 矢量绘制，避免把 1024 px SVG 缩到 20 px 时丢失笔画。
 */
static void create_signal_arc(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    int32_t size,
    int32_t start_angle,
    int32_t end_angle)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_set_pos(arc, x, y);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_angles(arc, start_angle, end_angle);
    lv_obj_set_style_arc_color(arc, UI_COLOR_WHITE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}


static lv_obj_t *create_signal_icon(lv_obj_t *parent, int32_t x, int32_t y)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_pos(root, x, y);
    lv_obj_set_size(root, 22, 22);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* Outer and inner left/right signal arcs, matching 信号.svg. */
    create_signal_arc(root, 0, 0, 22, 125, 235);
    create_signal_arc(root, 0, 0, 22, 305, 415);
    create_signal_arc(root, 4, 4, 14, 125, 235);
    create_signal_arc(root, 4, 4, 14, 305, 415);

    lv_obj_t *center = lv_obj_create(root);
    lv_obj_remove_style_all(center);
    lv_obj_set_pos(center, 8, 8);
    lv_obj_set_size(center, 6, 6);
    lv_obj_set_style_bg_opa(center, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(center, UI_COLOR_WHITE, 0);
    lv_obj_set_style_border_opa(center, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(center, 2, 0);
    lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_SCROLLABLE);

    return root;
}


static void update_battery_fill(ui_status_bar_t *bar)
{
    if(bar == NULL || bar->battery_fill == NULL)
        return;

    const int32_t inner_width = 23;
    int32_t width =
        (inner_width * (int32_t)bar->battery_percent + 50) / 100;

    if(width < 0)
        width = 0;
    if(width > inner_width)
        width = inner_width;

    lv_obj_set_width(bar->battery_fill, width);
}


/*
 * ============================================================
 * 创建 Status Bar
 * ============================================================
 */

void ui_status_bar_create(
    ui_status_bar_t *bar,
    lv_obj_t *parent)
{
    if(bar == NULL || parent == NULL)
        return;

    /*
     * --------------------------------------------------------
     * Root
     * --------------------------------------------------------
     */

    bar->root =
        lv_obj_create(parent);

    lv_obj_remove_style_all(
        bar->root
    );


    /*
     * Status Bar 在整�? 480×854 页面�?的位�?�?
     *
     * X = 34
     * Y = 81
     *
     * W = 413
     * H = 49
     */
    lv_obj_set_pos(
        bar->root,
        UI_LEFT,
        UI_STATUS_TOP
    );

    lv_obj_set_size(
        bar->root,
        UI_CONTENT_W,
        UI_STATUS_H
    );


    lv_obj_clear_flag(
        bar->root,
        LV_OBJ_FLAG_SCROLLABLE
    );


    /*
     * --------------------------------------------------------
     * 顶部�?�?
     * --------------------------------------------------------
     */

    lv_obj_t *top_line =
        lv_obj_create(bar->root);

    lv_obj_remove_style_all(
        top_line
    );

    lv_obj_set_pos(
        top_line,
        0,
        0
    );

    lv_obj_set_size(
        top_line,
        UI_CONTENT_W,
        1
    );

    lv_obj_set_style_bg_color(
        top_line,
        UI_COLOR_GRID_LINE,
        0
    );

    lv_obj_set_style_bg_opa(
        top_line,
        LV_OPA_COVER,
        0
    );


    /*
     * --------------------------------------------------------
     * 底部�?�?
     * --------------------------------------------------------
     */

    lv_obj_t *bottom_line =
        lv_obj_create(bar->root);

    lv_obj_remove_style_all(
        bottom_line
    );

    lv_obj_set_pos(
        bottom_line,
        0,
        57
    );

    lv_obj_set_size(
        bottom_line,
        UI_CONTENT_W,
        1
    );

    lv_obj_set_style_bg_color(
        bottom_line,
        UI_COLOR_GRID_LINE,
        0
    );

    lv_obj_set_style_bg_opa(
        bottom_line,
        LV_OPA_COVER,
        0
    );


    /*
     * --------------------------------------------------------
     * 左侧黄色�?�?
     * --------------------------------------------------------
     */

    lv_obj_t *yellow_line =
        lv_obj_create(bar->root);

    lv_obj_remove_style_all(
        yellow_line
    );

    lv_obj_set_pos(
        yellow_line,
        0,
        13
    );

    lv_obj_set_size(
        yellow_line,
        2,
        32
    );

    lv_obj_set_style_bg_color(
        yellow_line,
        UI_COLOR_YELLOW,
        0
    );

    lv_obj_set_style_bg_opa(
        yellow_line,
        LV_OPA_COVER,
        0
    );


    /* No internal divider lines: each group is defined by fixed slots. */

    /* 采用用户提供的 信号.svg 对应的 GNSS 信号图形。 */
    bar->sat_icon = create_signal_icon(bar->root, 8, 15);


    /*
     * �?星数�?
     */
    bar->sat_label =
        lv_label_create(bar->root);

    lv_label_set_text(
        bar->sat_label,
        "12"
    );

    lv_obj_set_pos(
        bar->sat_label,
        36,
        18
    );

    lv_obj_set_width(bar->sat_label, 30);

    lv_obj_set_style_text_color(
        bar->sat_label,
        UI_COLOR_WHITE,
        0
    );

    lv_obj_set_style_text_font(
        bar->sat_label,
        UI_FONT_STATUS_EMPHASIS,
        0
    );


    /*
     * --------------------------------------------------------
     * 当前时间
     * --------------------------------------------------------
     */

    bar->time_label =
        lv_label_create(bar->root);

    lv_label_set_text(
        bar->time_label,
        "10:24"
    );

    lv_obj_set_pos(
        bar->time_label,
        160,
        18
    );

    lv_obj_set_width(bar->time_label, 160);

    lv_obj_set_style_text_align(
        bar->time_label,
        LV_TEXT_ALIGN_CENTER,
        0
    );

    lv_obj_set_style_text_color(
        bar->time_label,
        UI_COLOR_WHITE,
        0
    );

    lv_obj_set_style_text_font(
        bar->time_label,
        UI_FONT_STATUS_EMPHASIS,
        0
    );


    /* 右侧无线组紧贴电量数值，避免形成视觉断层。 */
    bar->wifi_icon = create_status_symbol(bar->root, LV_SYMBOL_WIFI, 340, 24);


    /*
     * --------------------------------------------------------
     * Bluetooth
     * --------------------------------------------------------
     */

    bar->bt_icon = create_status_symbol(bar->root, LV_SYMBOL_BLUETOOTH, 368, 22);


    /*
     * --------------------------------------------------------
     * 电量百分�?
     * --------------------------------------------------------
     */

    bar->battery_label =
        lv_label_create(bar->root);

    lv_label_set_text(
        bar->battery_label,
        "82%"
    );

    lv_obj_set_pos(
        bar->battery_label,
        374,
        18
    );

    /* status_20 下 100% 约 54 px；60 px 可保证 0%~100% 始终单行。 */
    lv_obj_set_width(bar->battery_label, 60);
    lv_obj_set_style_text_align(bar->battery_label, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_set_style_text_color(
        bar->battery_label,
        UI_COLOR_WHITE,
        0
    );

    lv_obj_set_style_text_font(
        bar->battery_label,
        UI_FONT_STATUS_EMPHASIS,
        0
    );


    /*
     * --------------------------------------------------------
     * 电池图标
     * --------------------------------------------------------
     *
     * 电池使用 LVGL 内置小号符号，并根据电量动态切换外形。
     */

    bar->battery_percent = 82;

    bar->battery_icon =
        lv_obj_create(bar->root);

    lv_obj_remove_style_all(bar->battery_icon);
    lv_obj_set_pos(bar->battery_icon, 442, 17);
    lv_obj_set_size(bar->battery_icon, 34, 18);
    lv_obj_set_style_bg_opa(bar->battery_icon, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(bar->battery_icon, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *battery_body = lv_obj_create(bar->battery_icon);
    lv_obj_remove_style_all(battery_body);
    lv_obj_set_pos(battery_body, 0, 0);
    lv_obj_set_size(battery_body, 31, 18);
    lv_obj_set_style_bg_opa(battery_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(battery_body, UI_COLOR_WHITE, 0);
    lv_obj_set_style_border_opa(battery_body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(battery_body, 2, 0);
    lv_obj_set_style_radius(battery_body, 2, 0);
    lv_obj_clear_flag(battery_body, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *battery_terminal = lv_obj_create(bar->battery_icon);
    lv_obj_remove_style_all(battery_terminal);
    lv_obj_set_pos(battery_terminal, 31, 5);
    lv_obj_set_size(battery_terminal, 3, 6);
    lv_obj_set_style_bg_color(battery_terminal, UI_COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(battery_terminal, LV_OPA_COVER, 0);

    bar->battery_fill = lv_obj_create(battery_body);
    lv_obj_remove_style_all(bar->battery_fill);
    lv_obj_set_pos(bar->battery_fill, 3, 3);
    lv_obj_set_size(bar->battery_fill, 23, 10);
    lv_obj_set_style_bg_color(bar->battery_fill, UI_COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(bar->battery_fill, LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar->battery_fill, LV_OBJ_FLAG_SCROLLABLE);

    ui_align_center_y(bar->sat_icon, 8);
    ui_align_center_y(bar->sat_label, 36);
    ui_align_center_y(bar->time_label, 160);
    ui_align_center_y(bar->wifi_icon, 340);
    ui_align_center_y(bar->bt_icon, 368);
    ui_align_center_y(bar->battery_label, 374);
    ui_align_center_y(bar->battery_icon, 442);
    update_battery_fill(bar);
}


/*
 * ============================================================
 * 更新�?星数�?
 * ============================================================
 */

void ui_status_bar_set_satellites(
    ui_status_bar_t *bar,
    uint8_t count)
{
    if(bar == NULL || bar->sat_label == NULL)
        return;

    char text[4];
    snprintf(text, sizeof(text), "%u", count);
    if(strcmp(lv_label_get_text(bar->sat_label), text) != 0)
        lv_label_set_text(bar->sat_label, text);
}


/*
 * ============================================================
 * 更新时间
 * ============================================================
 */

void ui_status_bar_set_time(
    ui_status_bar_t *bar,
    uint8_t hour,
    uint8_t minute)
{
    if(bar == NULL || bar->time_label == NULL)
        return;

    char text[6];
    snprintf(text, sizeof(text), "%02u:%02u", hour, minute);
    if(strcmp(lv_label_get_text(bar->time_label), text) != 0)
        lv_label_set_text(bar->time_label, text);
}


/*
 * ============================================================
 * 更新电池百分�?
 * ============================================================
 */

void ui_status_bar_set_battery(
    ui_status_bar_t *bar,
    uint8_t percent)
{
    if(bar == NULL || bar->battery_label == NULL)
        return;


    /*
     * 避免超过 100%
     */
    if(percent > 100)
        percent = 100;

    char text[5];
    snprintf(text, sizeof(text), "%u%%", percent);
    if(bar->battery_percent == percent && strcmp(lv_label_get_text(bar->battery_label), text) == 0)
        return;

    lv_label_set_text(bar->battery_label, text);

    bar->battery_percent = percent;
    update_battery_fill(bar);
}


/*
 * ============================================================
 * 设置 WiFi 状�?
 * ============================================================
 */

void ui_status_bar_set_wifi(
    ui_status_bar_t *bar,
    bool connected)
{
    if(bar == NULL || bar->wifi_icon == NULL)
        return;

    if(bar->wifi_state_valid && bar->wifi_connected == connected)
        return;


    /*
     * 已连接：完全显示
     *
     * �?连接：降低透明�?
     */
    lv_obj_set_style_text_opa(
        bar->wifi_icon,
        connected ?
        LV_OPA_COVER :
        LV_OPA_30,
        0
    );

    bar->wifi_connected = connected;
    bar->wifi_state_valid = true;
}


/*
 * ============================================================
 * 设置 Bluetooth 状�?
 * ============================================================
 */

void ui_status_bar_set_bluetooth(
    ui_status_bar_t *bar,
    bool connected)
{
    if(bar == NULL || bar->bt_icon == NULL)
        return;

    if(bar->bluetooth_state_valid && bar->bluetooth_connected == connected)
        return;


    lv_obj_set_style_text_opa(
        bar->bt_icon,
        connected ?
        LV_OPA_COVER :
        LV_OPA_30,
        0
    );

    bar->bluetooth_connected = connected;
    bar->bluetooth_state_valid = true;
}
