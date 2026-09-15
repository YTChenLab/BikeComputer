#ifndef UI_COLORS_H
#define UI_COLORS_H

#include "lvgl.h"

/*
 * ============================================================
 * 公路车码表 UI 全局配色
 * ============================================================
 *
 * 后面如果需要调整整个界面的色彩，只修改本文件即可。
 */


/* 页面纯黑背景 */
#define UI_COLOR_BG              lv_color_hex(0x000000)

/* 主文字、数字 */
#define UI_COLOR_WHITE           lv_color_hex(0xFFFFFF)

/* 冰蓝色辅助文字 */
#define UI_COLOR_CYAN            lv_color_hex(0xBDE7FF)

/* 表格以及结构线 */
#define UI_COLOR_LINE            lv_color_hex(0xA7D8EF)

/* 下方指标区域边框，使用纯白提高商用仪表的层次和可读性 */
#define UI_COLOR_GRID_LINE       UI_COLOR_WHITE

/* 速度区域黄色强调 */
#define UI_COLOR_YELLOW          lv_color_hex(0xFFD900)

/* 速度条未点亮状态 */
#define UI_COLOR_SEGMENT_OFF     lv_color_hex(0x30424F)

/* 深蓝灰装饰 */
#define UI_COLOR_DECORATION      lv_color_hex(0x47677A)

#endif
