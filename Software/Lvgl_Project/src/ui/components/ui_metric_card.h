/**
 * @file ui_metric_card.h
 * @brief 公路车码表数据指标卡片组件
 *
 * 该组件用于显示 Dashboard 下半区域的数据：
 *
 * 心率
 * 156 bpm
 *
 * 功率
 * 223 W
 *
 * 踏频
 * 85 rpm
 *
 * 坡度
 * 4.3 %
 *
 * 里程
 * 50.25 km
 *
 * 骑行时间
 * 01:30:25
 */

#ifndef UI_METRIC_CARD_H
#define UI_METRIC_CARD_H


/*********************
 *      INCLUDES
 *********************/

#include "lvgl.h"


/**********************
 *      TYPEDEFS
 **********************/

/**
 * @brief 单个 Dashboard 数据指标组件
 */
typedef struct
{
    /**
     * 整个指标区域的根对象
     *
     * 本对象自身没有背景、边框，
     * 只负责管理子对象坐标。
     */
    lv_obj_t *root;


    /**
     * 指标名称
     *
     * 例如：
     *
     * 心率
     * 功率
     * 踏频
     * 坡度
     * 里程
     * 骑行时间
     */
    lv_obj_t *title;


    /**
     * 实时数值
     *
     * 例如：
     *
     * 156
     * 223
     * 85
     * 4.3
     * 50.25
     * 01:30:25
     */
    lv_obj_t *value;


    /**
     * 单位
     *
     * 例如：
     *
     * bpm
     * W
     * rpm
     * %
     * km
     *
     * 骑行时间没有单位，因此可以传入空字符串。
     */
    lv_obj_t *unit;

} ui_metric_card_t;


/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief 创建一个数据指标组件
 *
 * @param card   指标组件结构体
 * @param parent 父 LVGL 对象
 * @param x      相对于父对象的 X 坐标
 * @param y      相对于父对象的 Y 坐标
 * @param w      指标区域宽度
 * @param h      指标区域高度
 * @param title  指标名称
 * @param value  初始数值
 * @param unit       单位
 * @param value_font 数值字体；NULL 时使用 Theme 默认大数字字体
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
    const lv_font_t *value_font
);


/**
 * @brief 修改指标数值
 *
 * @param card  指标组件
 * @param value 新数值字符串
 */
void ui_metric_card_set_value(
    ui_metric_card_t *card,
    const char *value
);


/**
 * @brief 修改单位
 *
 * @param card 指标组件
 * @param unit 新单位字符串
 */
void ui_metric_card_set_unit(
    ui_metric_card_t *card,
    const char *unit
);


#endif /* UI_METRIC_CARD_H */
