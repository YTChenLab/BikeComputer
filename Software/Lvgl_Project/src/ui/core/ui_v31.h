/**
 * @file ui_v31.h
 * @brief V3.1 UI 全局视觉设计系统
 */

#ifndef UI_V31_H
#define UI_V31_H

#include "lvgl/lvgl.h"


/* ============================================================
 * 屏幕尺寸
 * ============================================================ */

#define UI_SCREEN_W              480
#define UI_SCREEN_H              854


/* ============================================================
 * V3.1 色彩系统
 * ============================================================ */

/* 主背景 */
#define UI_V31_BG                lv_color_hex(0x071014)
#define UI_V31_BG_SECOND         lv_color_hex(0x09161B)

/* 模块背景 */
#define UI_V31_PANEL             lv_color_hex(0x0B171C)
#define UI_V31_PANEL_2           lv_color_hex(0x0D1B21)

/* 结构线 */
#define UI_V31_LINE_DARK         lv_color_hex(0x17282F)
#define UI_V31_LINE              lv_color_hex(0x294149)
#define UI_V31_LINE_LIGHT        lv_color_hex(0x40565E)

/* 文字 */
#define UI_V31_TEXT_MAIN         lv_color_hex(0xF4F5F4)
#define UI_V31_TEXT_SECOND       lv_color_hex(0xA3AFB4)
#define UI_V31_TEXT_DIM          lv_color_hex(0x617077)

/* 主强调色 */
#define UI_V31_YELLOW            lv_color_hex(0xF4D90B)

/* 状态色 */
#define UI_V31_CYAN              lv_color_hex(0x13DDE7)
#define UI_V31_GREEN             lv_color_hex(0x24D56A)
#define UI_V31_RED               lv_color_hex(0xFF4E55)
#define UI_V31_PURPLE            lv_color_hex(0xB58BFF)


/* ============================================================
 * 英文字体系统
 * ============================================================ */

#define UI_FONT_TECH             (&lv_font_montserrat_12)
#define UI_FONT_SMALL            (&lv_font_montserrat_14)
#define UI_FONT_NORMAL           (&lv_font_montserrat_16)
#define UI_FONT_VALUE            (&lv_font_montserrat_24)
#define UI_FONT_SPEED            (&lv_font_montserrat_48)


/* ============================================================
 * 中文字体
 * ============================================================
 *
 * 当前阶段先关闭中文字体。
 *
 * 后续会加入：
 *
 * ui_font_cn_14
 * ui_font_cn_16
 *
 * 使用实际 UI 汉字制作子集，
 * 避免完整中文字库占用大量 Flash。
 */

#define UI_V31_HAS_CJK           0

#define UI_FONT_CN               UI_FONT_NORMAL


/* ============================================================
 * 页面文字
 * ============================================================
 *
 * 当前因为中文字体尚未加入，
 * 先使用英文。
 *
 * 中文字体加入后会切换成中英双语。
 */

#if UI_V31_HAS_CJK

    #define UI_TEXT_SPEED        "实时速度"
    #define UI_TEXT_HEART        "心率"
    #define UI_TEXT_POWER        "功率"
    #define UI_TEXT_CADENCE      "踏频"
    #define UI_TEXT_GRADE        "坡度"
    #define UI_TEXT_DISTANCE     "里程"
    #define UI_TEXT_TIME         "骑行时间"
    #define UI_TEXT_ACTIVE       "正在骑行"

#else

    #define UI_TEXT_SPEED        "LIVE SPEED"
    #define UI_TEXT_HEART        "HEART RATE"
    #define UI_TEXT_POWER        "POWER"
    #define UI_TEXT_CADENCE      "CADENCE"
    #define UI_TEXT_GRADE        "GRADE"
    #define UI_TEXT_DISTANCE     "DISTANCE"
    #define UI_TEXT_TIME         "RIDE TIME"
    #define UI_TEXT_ACTIVE       "RIDE ACTIVE"

#endif


#endif /* UI_V31_H */
