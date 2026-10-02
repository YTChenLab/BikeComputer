#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl/lvgl.h"


/*
 * ============================================================
 * MiSans 自定义字体声明
 * ============================================================
 *
 * 字体文件：
 *
 * src/ui/fonts/ui_font_misans_cn_28.c
 * src/ui/fonts/ui_font_misans_cn_23.c
 * src/ui/fonts/ui_font_misans_en_18.c
 * src/ui/fonts/ui_font_misans_en_16.c
 * src/ui/fonts/ui_font_misans_status_20.c
 * src/ui/fonts/ui_font_misans_status_16.c
 * src/ui/fonts/ui_font_misans_unit_25.c
 * src/ui/fonts/ui_font_misans_unit_21.c
 * src/ui/fonts/ui_font_misans_num_48.c
 * src/ui/fonts/ui_font_misans_num_68.c
 * src/ui/fonts/ui_font_misans_num_140.c
 * src/ui/fonts/ui_font_misans_speed_110.c
 * src/ui/fonts/ui_font_misans_metric_60.c
 * src/ui/fonts/ui_font_misans_metric_small_46.c
 */


/*
 * 23 px
 *
 * 用于：
 * 心率
 * 功率
 * 踏频
 * 坡度
 * 里程
 * 骑行时间
 */
LV_FONT_DECLARE(ui_font_misans_cn_23);


/*
 * 23 px
 *
 * 用于：
 * 实时速度
 */
LV_FONT_DECLARE(ui_font_misans_cn_28);
LV_FONT_DECLARE(ui_font_misans_en_18);
LV_FONT_DECLARE(ui_font_misans_en_16);
LV_FONT_DECLARE(ui_font_misans_status_20);
LV_FONT_DECLARE(ui_font_misans_status_16);
LV_FONT_DECLARE(ui_font_misans_ride_status_18);
LV_FONT_DECLARE(ui_font_misans_unit_25);
LV_FONT_DECLARE(ui_font_misans_unit_21);
LV_FONT_DECLARE(ui_font_misans_num_48);
LV_FONT_DECLARE(ui_font_misans_num_68);
LV_FONT_DECLARE(ui_font_misans_num_140);
LV_FONT_DECLARE(ui_font_misans_speed_110);
LV_FONT_DECLARE(ui_font_misans_metric_60);
LV_FONT_DECLARE(ui_font_misans_metric_small_46);
LV_FONT_DECLARE(ui_font_misans_ui_20);
LV_FONT_DECLARE(ui_font_misans_ui_24);
LV_FONT_DECLARE(ui_font_misans_zone_24);
LV_FONT_DECLARE(ui_font_misans_nav_metric_40);
LV_FONT_DECLARE(ui_font_misans_nav_action_32);
LV_FONT_DECLARE(ui_font_misans_nav_unit_16);

/* P0-P7 共用字体：覆盖当前中文、完整可打印 ASCII 和常用界面标点。 */
#define UI_FONT_PAGE_TEXT \
    (&ui_font_misans_ui_20)


/*
 * ============================================================
 * 顶部状态栏字体
 * ============================================================
 *
 * 状态栏数字和时间。
 */
#define UI_FONT_STATUS \
    (&ui_font_misans_status_16)

/* GNSS 数量与时钟使用较大一档，仅增强关键读数。 */
#define UI_FONT_STATUS_EMPHASIS \
    (&ui_font_misans_status_20)

/* 底部状态条：按需包含中文状态词、数字和分隔符。 */
#define UI_FONT_RIDE_STATUS \
    (&ui_font_misans_ride_status_18)


/*
 * ============================================================
 * Speed Area
 * ============================================================
 */


/*
 * “实时速度”
 *
 * MiSans Semibold
 * 23 px
 */
#define UI_FONT_SPEED_TITLE \
    (&ui_font_misans_cn_23)


/*
 * CURRENT SPEED
 *
 * MiSans Semibold 16 px。
 */
#define UI_FONT_SPEED_SUBTITLE \
    (&ui_font_misans_en_16)


/*
 * 主速度：
 *
 * 32.8
 *
 * MiSans Heavy 140 px。
 */
#define UI_FONT_SPEED_VALUE \
    (&ui_font_misans_speed_110)


/*
 * KM/H
 *
 * MiSans Semibold 21 px。
 */
#define UI_FONT_SPEED_UNIT \
    (&ui_font_misans_unit_21)


/*
 * ============================================================
 * Metric Card
 * ============================================================
 */


/*
 * 中文标题：
 *
 * 心率
 * 功率
 * 踏频
 * 坡度
 * 里程
 * 骑行时间
 *
 * MiSans Semibold
 * 23 px
 */
#define UI_FONT_METRIC_TITLE \
    (&ui_font_misans_cn_23)


/*
 * 数据：
 *
 * 156
 * 223
 * 85
 * 4.3
 * 50.25
 * 01:30:25
 *
 * 常规指标使用 68 px；里程和骑行时间使用 48 px。
 *
 * 骑行时间有 8 个字符，保持较小一档以避免在 237 px 卡片中裁切。
 */
#define UI_FONT_METRIC_VALUE \
    (&ui_font_misans_metric_60)

#define UI_FONT_METRIC_VALUE_SMALL \
    (&ui_font_misans_num_48)


/*
 * 单位：
 *
 * bpm
 * W
 * rpm
 * %
 * km
 *
 * MiSans Semibold 21 px。
 */
#define UI_FONT_METRIC_UNIT \
    (&ui_font_misans_unit_21)

/*
 * Bottom Interaction Surface typography roles.
 *
 * Body/action copy stays on the shared 20 px face. Titles and navigation
 * instructions use a generated 24 px face with the same runtime glyph set.
 */
#define UI_FONT_SURFACE_TITLE   (&ui_font_misans_ui_24)
#define UI_FONT_SURFACE_BODY    UI_FONT_PAGE_TEXT
#define UI_FONT_SURFACE_EMPHASIS (&ui_font_misans_ui_24)
/* Dock type badges are ASCII words (INFO/ACTION/CONFIRM/...).  The compact
 * status font only contains digits, ':' and '%', so use a complete ASCII
 * face here instead of relying on missing-glyph boxes. */
#define UI_FONT_SURFACE_BADGE   (&lv_font_montserrat_14)
#define UI_FONT_SURFACE_ACTION  UI_FONT_PAGE_TEXT

/* Navigation V2 compact type roles.  The 40 px numeric face fits a value and
 * its unit inside one 153 px data-strip column.  The 32 px action face only
 * contains the fixed maneuver vocabulary, keeping the embedded cost small. */
#define UI_FONT_NAV_STRIP_VALUE (&ui_font_misans_nav_metric_40)
#define UI_FONT_NAV_STRIP_UNIT  (&ui_font_misans_nav_unit_16)
#define UI_FONT_NAV_ACTION      (&ui_font_misans_nav_action_32)


/*
 * ============================================================
 * Theme API
 * ============================================================
 */

void ui_theme_init(void);


#endif /* UI_THEME_H */
