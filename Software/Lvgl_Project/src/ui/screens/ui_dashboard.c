/**
 * @file ui_dashboard.c
 * @brief 公路车码表 Dashboard 首页
 *
 * ============================================================
 * 页面目标
 * ============================================================
 *
 * 屏幕分辨率：
 *
 *      480 × 854
 *
 *
 * 当前页面结构：
 *
 * ┌─────────────────────────────────────────────┐
 * │ 状态栏                                      │
 * │ GNSS / 时间 / WiFi / Bluetooth / 电池       │
 * ├─────────────────────────────────────────────┤
 * │                                             │
 * │ 实时速度                                    │
 * │ CURRENT SPEED                               │
 * │                                             │
 * │                   32.8                      │
 * │                                      KM/H   │
 * │                                             │
 * │     ? ? ? ? ? ? ?                         │
 * │                                             │
 * ├─────────────────────┬───────────────────────┤
 * │ 心率                │ 功率                  │
 * │ 156 bpm             │ 223 W                 │
 * ├─────────────────────┼───────────────────────┤
 * │ 踏频                │ 坡度                  │
 * │ 85 rpm              │ 4.3 %                 │
 * ├─────────────────────┼───────────────────────┤
 * │ 里程                │ 骑行时间              │
 * │ 50.25 km            │ 01:30:25              │
 * └─────────────────────┴───────────────────────┘
 *
 *
 * ============================================================
 * 当前实现说明
 * ============================================================
 *
 * 1. 页面顶部不创建独立标题区域，状态栏下面直接进入速度区域。
 *
 * 2. 状态栏直接放在屏幕顶部。
 *
 * 3. 实时速度区域获得更多垂直空间。
 *
 * 4. 下方使用 2 × 3 数据布局。
 *
 * 5. 速度条使用斜切平行四边形。
 *
 * 6. 当前字体由 ui_theme.h 统一管理。
 *
 * 7. 已经包含速度平滑动画接口，
 *    后续 GNSS 速度变化时可以获得更平滑的视觉效果。
 */


/*********************
 *      INCLUDES
 *********************/

#include "ui_dashboard.h"

#include "../components/ui_status_bar.h"
#include "../pages/ui_page_common.h"
#include "../components/ui_metric_card.h"
#include "../components/ui_dynamic_card_manager.h"

#include "../core/ui_colors.h"
#include "../core/ui_background.h"
#include "../core/ui_bottom_surface.h"
#include "../core/ui_sizes.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"

#include "lvgl/src/draw/lv_draw_triangle.h"

#include <stdio.h>
#include <string.h>


/**********************
 *      DEFINES
 **********************/

/*
 * ============================================================
 * 页面主要坐标
 * ============================================================
 *
 * 这里先直接定义在 Dashboard 内部。
 *
 * 等首页设计最终完全确定以后，
 * 可以进一步移动到 ui_sizes.h 统一管理。
 */


/*
 * 页面左右边界
 */
#define DASH_LEFT_X                 UI_LEFT
#define DASH_RIGHT_X                UI_RIGHT

#define DASH_CONTENT_W              UI_CONTENT_W

#define DASH_CENTER_X               UI_CENTER_X


/*
 * ============================================================
 * Status Bar
 * ============================================================
 */

/*
 * 状态栏直接位于顶部。
 */
#define DASH_STATUS_X               UI_LEFT
#define DASH_STATUS_Y               UI_STATUS_TOP

#define DASH_STATUS_W               UI_CONTENT_W
#define DASH_STATUS_H               UI_STATUS_H


/*
 * ============================================================
 * Speed Area
 * ============================================================
 */

/*
 * 速度区域起点。
 *
 * Status Bar 底部约：
 *
 *      Y = 63
 *
 * 再留约 12 px 空间。
 */
#define DASH_SPEED_TOP              UI_SPEED_TOP


/*
 * 速度区域底部。
 */
#define DASH_SPEED_BOTTOM           UI_SPEED_BOTTOM


/*
 * ============================================================
 * Metrics Grid
 * ============================================================
 */

/*
 * 四条横线。
 *
 * 三行数据区域基本等高。
 */
#define DASH_GRID_Y1                UI_GRID_Y1
#define DASH_GRID_Y2                UI_GRID_Y2
#define DASH_GRID_Y3                UI_GRID_Y3
#define DASH_GRID_Y4                UI_GRID_Y4

/*
 * ============================================================
 * Speed animation
 * ============================================================
 */

/*
 * 约 60Hz 动画刷新周期。
 *
 * 16ms ≈ 62.5Hz
 */
#define DASH_SPEED_ANIM_PERIOD_MS    16


/*
 * 速度一阶平滑系数。
 *
 * 越大：
 *      响应越快
 *
 * 越小：
 *      动画越平缓
 */
#define DASH_SPEED_SMOOTH_ALPHA      0.15f

#define DASH_SPEED_TARGET_EPSILON    0.0001f


/**********************
 *  STATIC VARIABLES
 **********************/


/*
 * ============================================================
 * Screen
 * ============================================================
 */

/**
 * Dashboard 根 Screen。
 */
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_bottom_surface_slot = NULL;


/*
 * ============================================================
 * Status Bar
 * ============================================================
 */

static ui_status_bar_t s_status_bar;


/*
 * ============================================================
 * Speed Area
 * ============================================================
 */

/**
 * 主速度数字，例如：
 *
 * 32.8
 */
static lv_obj_t *s_speed_label = NULL;


/**
 * KM/H
 */
static lv_obj_t *s_speed_unit = NULL;


/**
 * 七段速度指示条。
 */
static lv_obj_t *s_speed_segments[7] =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};


/*
 * ============================================================
 * Metric Cards
 * ============================================================
 */

static ui_metric_card_t s_hr_card;

static ui_metric_card_t s_power_card;

static ui_metric_card_t s_cadence_card;

static ui_metric_card_t s_grade_card;

static ui_metric_card_t s_distance_card;

static ui_metric_card_t s_time_card;


/* 底部骑行状态条：只展示可由现有 Dashboard 数据真实推导的状态。 */


/*
 * ============================================================
 * Speed animation data
 * ============================================================
 */

/**
 * 传感器/业务层提供的目标速度。
 */
static float s_target_speed = 32.8f;


/**
 * 当前屏幕实际显示速度。
 */
static float s_display_speed = 32.8f;


/**
 * 速度平滑动画 Timer。
 */
static lv_timer_t *s_speed_anim_timer = NULL;
static bool s_dashboard_active = false;


/* 已绘制的速度分段数量；-1 表示尚未绘制。 */
static int32_t s_speed_active_segments = -1;


/**********************
 *  STATIC PROTOTYPES
 **********************/


/*
 * 基础绘制函数
 */
static lv_obj_t *dashboard_create_rect(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    int32_t w,
    int32_t h,
    lv_color_t color
);


static lv_obj_t *dashboard_create_label(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    const char *text,
    const lv_font_t *font,
    lv_color_t color
);


/*
 * 页面区域创建
 */
static void dashboard_create_status_bar(void);

static void dashboard_create_speed_area(void);

static void dashboard_create_grid(void);

static void dashboard_create_metrics(void);

/*
 * 速度更新
 */
static void dashboard_update_speed_visual(
    float speed
);

static void dashboard_layout_speed_readout(void);


static void dashboard_update_speed_segments(
    float speed
);


static void dashboard_speed_anim_cb(
    lv_timer_t *timer
);


/**********************
 *   STATIC FUNCTIONS
 **********************/


/**
 * @brief 创建一个纯色矩形
 *
 * 可以用于：
 *
 * - 横线
 * - 竖线
 * - 黄色强调线
 * - 速度指示条
 *
 * @param parent 父对象
 * @param x      X 坐标
 * @param y      Y 坐标
 * @param w      宽度
 * @param h      高度
 * @param color  填充颜色
 *
 * @return 新创建的 LVGL 对象
 */
static lv_obj_t *dashboard_create_rect(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    int32_t w,
    int32_t h,
    lv_color_t color)
{
    /*
     * 创建普通 LVGL Object。
     */
    lv_obj_t *obj =
        lv_obj_create(parent);


    /*
     * 清除默认样式。
     *
     * 避免默认：
     *
     * - 圆角
     * - 边框
     * - Padding
     * - 阴影
     *
     * 干扰像素级 UI。
     */
    lv_obj_remove_style_all(
        obj
    );


    /*
     * 设置坐标。
     */
    lv_obj_set_pos(
        obj,
        x,
        y
    );


    /*
     * 设置尺寸。
     */
    lv_obj_set_size(
        obj,
        w,
        h
    );


    /*
     * 设置背景颜色。
     */
    lv_obj_set_style_bg_color(
        obj,
        color,
        0
    );


    /*
     * 背景完全不透明。
     */
    lv_obj_set_style_bg_opa(
        obj,
        LV_OPA_COVER,
        0
    );


    /*
     * 当前对象仅作为视觉元素，
     * 不需要滚动。
     */
    lv_obj_clear_flag(
        obj,
        LV_OBJ_FLAG_SCROLLABLE
    );


    return obj;
}


static void dashboard_segment_draw_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN)
        return;

    lv_obj_t *obj = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;

    lv_obj_get_coords(obj, &area);

    const int32_t slant = 7;
    const lv_color_t color = lv_obj_get_style_bg_color(obj, 0);

    lv_point_precise_t points[4] = {
        { area.x1 + slant, area.y1 },
        { area.x2, area.y1 },
        { area.x2 - slant, area.y2 },
        { area.x1, area.y2 }
    };

    lv_draw_triangle_dsc_t triangle;
    lv_draw_triangle_dsc_init(&triangle);
    triangle.bg_color = color;
    triangle.bg_opa = LV_OPA_COVER;

    triangle.p[0] = points[0];
    triangle.p[1] = points[1];
    triangle.p[2] = points[2];
    lv_draw_triangle(layer, &triangle);

    triangle.p[0] = points[0];
    triangle.p[1] = points[2];
    triangle.p[2] = points[3];
    lv_draw_triangle(layer, &triangle);
}


/**
 * @brief 创建统一的 Label
 *
 * @param parent 父对象
 * @param x      X坐标
 * @param y      Y坐标
 * @param text   文本
 * @param font   字体
 * @param color  文本颜色
 *
 * @return 创建后的 Label
 */
static lv_obj_t *dashboard_create_label(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    const char *text,
    const lv_font_t *font,
    lv_color_t color)
{
    /*
     * 创建 Label。
     */
    lv_obj_t *label =
        lv_label_create(parent);


    /*
     * 设置文字。
     */
    lv_label_set_text(
        label,
        text
    );


    /*
     * 设置位置。
     */
    lv_obj_set_pos(
        label,
        x,
        y
    );


    /*
     * 设置字体。
     */
    lv_obj_set_style_text_font(
        label,
        font,
        0
    );


    /*
     * 设置颜色。
     */
    lv_obj_set_style_text_color(
        label,
        color,
        0
    );


    return label;
}


/**
 * @brief 创建顶部状态栏
 *
 * 页面顶部现在直接显示：
 *
 * GNSS / 卫星数 / 时间 / WiFi / Bluetooth / 电量
 */
static void dashboard_create_status_bar(void)
{
    /*
     * 先调用 Status Bar 组件自身的创建函数。
     */
    ui_status_bar_create(
        &s_status_bar,
        s_screen
    );


    /*
     * --------------------------------------------------------
     * 强制重新定位 Status Bar
     * --------------------------------------------------------
     *
     * 这样即使 ui_status_bar.c 里面目前仍然写着：
     *
     *      Y = 81
     *
     * Dashboard 也会将它直接移动到顶部。
     *
     * 因此当前不用马上修改 ui_status_bar.c。
     */
    if(s_status_bar.root != NULL)
    {
        lv_obj_set_pos(
            s_status_bar.root,
            DASH_STATUS_X,
            DASH_STATUS_Y
        );


        lv_obj_set_size(
            s_status_bar.root,
            DASH_STATUS_W,
            DASH_STATUS_H
        );
    }
}


/**
 * @brief 创建实时速度主区域
 */
static void dashboard_create_speed_area(void)
{
    /*
     * ========================================================
     * “实时速度”
     * ========================================================
     */

    dashboard_create_label(
        s_screen,

        28,
        73,

        "实时速度",

        UI_FONT_SPEED_TITLE,

        UI_COLOR_WHITE
    );


    /*
     * ========================================================
     * CURRENT SPEED
     * ========================================================
     */

    lv_obj_t *subtitle =
        dashboard_create_label(
            s_screen,

            28,
            101,

            "CURRENT SPEED",

            UI_FONT_SPEED_SUBTITLE,

            UI_COLOR_WHITE
        );


    /*
     * 增加英文字符间距。
     */
    lv_obj_set_style_text_letter_space(
        subtitle,
        1,
        0
    );


    /*
     * ========================================================
     * 主速度数字
     * ========================================================
     *
     * 使用 140 px Heavy 数字字体，给标题和速度条留出明确的
     * 上下边界，避免数字压住进度条。
     */

    s_speed_label =
        dashboard_create_label(
            s_screen,

            0,
            142,

            "32.8",

            UI_FONT_SPEED_VALUE,

            UI_COLOR_WHITE
        );


    /*
     * ========================================================
     * KM/H
     * ========================================================
     */

    s_speed_unit =
        dashboard_create_label(
            s_screen,

            0,
            0,

            "KM/H",

            UI_FONT_SPEED_UNIT,

            UI_COLOR_WHITE
        );

    dashboard_layout_speed_readout();


    /*
     * ========================================================
     * 七段速度条
     * ========================================================
     *
     * 每一格由两个三角形组成斜切平行四边形，
     * 点亮和未点亮状态使用同一套几何形状。
     */

    static const int32_t segment_x[7] =
    {
        24,
        82,
        140,
        198,
        256,
        314,
        372
    };


    for(int32_t i = 0; i < 7; i++)
    {
        /*
         * 初始速度为 32.8 km/h。
         *
         * 对应点亮约 4 格。
         */
        lv_color_t color =
            (i < 4)
            ? UI_COLOR_YELLOW
            : UI_COLOR_SEGMENT_OFF;


        s_speed_segments[i] =
            dashboard_create_rect(
                s_screen,

                segment_x[i],
                278,

                52,
                15,

                color
            );

        /* 用两个三角形绘制真正的平行四边形，避免旋转矩形造成间隙和重叠。 */
        lv_obj_set_style_bg_opa(
            s_speed_segments[i],
            LV_OPA_TRANSP,
            0
        );

        lv_obj_add_event_cb(
            s_speed_segments[i],
            dashboard_segment_draw_cb,
            LV_EVENT_DRAW_MAIN,
            NULL
        );
    }
}


/**
 * @brief 创建 2 × 3 数据区域的结构线
 */
static void dashboard_create_grid(void)
{
    /*
     * ========================================================
     * 横线 1
     * ========================================================
     */

    dashboard_create_rect(
        s_screen,

        DASH_LEFT_X,
        DASH_GRID_Y1,

        DASH_CONTENT_W,
        1,

        UI_COLOR_GRID_LINE
    );


    /*
     * ========================================================
     * 横线 2
     * ========================================================
     */

    dashboard_create_rect(
        s_screen,

        DASH_LEFT_X,
        DASH_GRID_Y2,

        DASH_CONTENT_W,
        1,

        UI_COLOR_GRID_LINE
    );


    /*
     * ========================================================
     * 横线 3
     * ========================================================
     */

    dashboard_create_rect(
        s_screen,

        DASH_LEFT_X,
        DASH_GRID_Y3,

        DASH_CONTENT_W,
        1,

        UI_COLOR_GRID_LINE
    );


    /*
     * ========================================================
     * 横线 4
     * ========================================================
     */

    dashboard_create_rect(
        s_screen,

        DASH_LEFT_X,
        DASH_GRID_Y4,

        DASH_CONTENT_W,
        1,

        UI_COLOR_GRID_LINE
    );


    /*
     * 第一行中线
     */
    dashboard_create_rect(
        s_screen,

        DASH_CENTER_X,
        DASH_GRID_Y1 + 10,

        1,
        DASH_GRID_Y2 - DASH_GRID_Y1 - 20,

        UI_COLOR_GRID_LINE
    );


    /*
     * ========================================================
     * 第二行
     * ========================================================
     */

    dashboard_create_rect(
        s_screen,

        DASH_CENTER_X,
        DASH_GRID_Y2 + 10,

        1,
        DASH_GRID_Y3 - DASH_GRID_Y2 - 20,

        UI_COLOR_GRID_LINE
    );


    /*
     * ========================================================
     * 第三行
     * ========================================================
     */

    dashboard_create_rect(
        s_screen,

        DASH_CENTER_X,
        DASH_GRID_Y3 + 10,

        1,
        DASH_GRID_Y4 - DASH_GRID_Y3 - 20,

        UI_COLOR_GRID_LINE
    );
}


/**
 * @brief 创建六个 Dashboard 数据指标
 */
static void dashboard_create_metrics(void)
{
    /*
     * ========================================================
     * 第一行：心率
     * ========================================================
     */

    ui_metric_card_create(
        &s_hr_card,

        s_screen,

        DASH_LEFT_X + 1,
        DASH_GRID_Y1 + 1,

        237,
        DASH_GRID_Y2 - DASH_GRID_Y1 - 1,

        "心率",

        "156",

        "bpm",

        UI_FONT_METRIC_VALUE
    );


    /*
     * ========================================================
     * 第一行：功率
     * ========================================================
     */

    ui_metric_card_create(
        &s_power_card,

        s_screen,

        DASH_CENTER_X + 1,
        DASH_GRID_Y1 + 1,

        237,
        DASH_GRID_Y2 - DASH_GRID_Y1 - 1,

        "功率",

        "223",

        "W",

        UI_FONT_METRIC_VALUE
    );


    /*
     * ========================================================
     * 第二行：踏频
     * ========================================================
     */

    ui_metric_card_create(
        &s_cadence_card,

        s_screen,

        DASH_LEFT_X + 1,
        DASH_GRID_Y2 + 1,

        237,
        DASH_GRID_Y3 - DASH_GRID_Y2 - 1,

        "踏频",

        "85",

        "rpm",

        UI_FONT_METRIC_VALUE
    );


    /*
     * ========================================================
     * 第二行：坡度
     * ========================================================
     */

    ui_metric_card_create(
        &s_grade_card,

        s_screen,

        DASH_CENTER_X + 1,
        DASH_GRID_Y2 + 1,

        237,
        DASH_GRID_Y3 - DASH_GRID_Y2 - 1,

        "坡度",

        "4.3",

        "%",

        UI_FONT_METRIC_VALUE
    );


    /*
     * ========================================================
     * 第三行：里程
     * ========================================================
     */

    ui_metric_card_create(
        &s_distance_card,

        s_screen,

        DASH_LEFT_X + 1,
        DASH_GRID_Y3 + 1,

        237,
        DASH_GRID_Y4 - DASH_GRID_Y3 - 1,

        "里程",

        "50.25",

        "km",

        UI_FONT_METRIC_VALUE_SMALL
    );


    /*
     * ========================================================
     * 第三行：骑行时间
     * ========================================================
     */

    ui_metric_card_create(
        &s_time_card,

        s_screen,

        DASH_CENTER_X + 1,
        DASH_GRID_Y3 + 1,

        237,
        DASH_GRID_Y4 - DASH_GRID_Y3 - 1,

        "骑行时间",

        "01:30:25",

        "",

        UI_FONT_METRIC_VALUE_SMALL
    );
}


/**
 * @brief 根据当前速度更新七段速度条
 *
 * 当前按照：
 *
 *      0 ~ 60 km/h
 *
 * 映射到：
 *
 *      0 ~ 7 格
 *
 * @param speed 当前速度
 */
static void dashboard_update_speed_segments(
    float speed)
{
    /*
     * ========================================================
     * 输入限幅
     * ========================================================
     */

    if(speed < 0.0f)
    {
        speed = 0.0f;
    }


    if(speed > 60.0f)
    {
        speed = 60.0f;
    }


    /*
     * ========================================================
     * 计算点亮格数
     * ========================================================
     *
     * 例如：
     *
     * 32.8 / 60 × 7
     *
     * ≈ 3.83
     *
     * 加 0.5 后转整数：
     *
     * = 4
     */

    int32_t active_count =
        (int32_t)(
            speed
            / 60.0f
            * 7.0f
            + 0.5f
        );


    /*
     * 防御性限幅。
     */

    if(active_count < 0)
    {
        active_count = 0;
    }


    if(active_count > 7)
    {
        active_count = 7;
    }

    if(active_count == s_speed_active_segments)
    {
        return;
    }

    s_speed_active_segments = active_count;


    /*
     * ========================================================
     * 更新七个 Segment
     * ========================================================
     */

    for(int32_t i = 0; i < 7; i++)
    {
        /*
         * 防止对象尚未创建。
         */
        if(s_speed_segments[i] == NULL)
        {
            continue;
        }


        /*
         * 已激活：
         *
         * 黄色
         *
         * 未激活：
         *
         * 深蓝灰
         */
        lv_color_t color =
            (i < active_count)
            ? UI_COLOR_YELLOW
            : UI_COLOR_SEGMENT_OFF;


        lv_obj_set_style_bg_color(
            s_speed_segments[i],
            color,
            0
        );
    }
}


/**
 * @brief 更新当前速度视觉显示
 *
 * @param speed 当前显示速度
 */
static void dashboard_update_speed_visual(
    float speed)
{
    /*
     * 防止页面还没创建。
     */
    if(s_speed_label == NULL)
    {
        return;
    }


    /*
     * 格式化速度。
     *
     * 固定显示一位小数：
     *
     * 32.8
     */
    char buffer[16];


    snprintf(
        buffer,
        sizeof(buffer),
        "%.1f",
        speed
    );


    if(strcmp(lv_label_get_text(s_speed_label), buffer) != 0)
    {
        lv_label_set_text(s_speed_label, buffer);
        dashboard_layout_speed_readout();
    }


    /*
     * 更新速度条。
     */
    dashboard_update_speed_segments(
        speed
    );
}


/*
 * 数字本身优先保持在速度区域中线；单位紧跟在右侧并与数字底部对齐。
 * 当五位速度值（例如 105.2）加上单位将越界时，退回为整组居中，避免裁切。
 */
static void dashboard_layout_speed_readout(void)
{
    if(s_speed_label == NULL || s_speed_unit == NULL)
    {
        return;
    }

    lv_obj_update_layout(s_screen);

    const int32_t value_width = lv_obj_get_width(s_speed_label);
    const int32_t value_height = lv_obj_get_height(s_speed_label);
    const int32_t unit_width = lv_obj_get_width(s_speed_unit);
    const int32_t unit_height = lv_obj_get_height(s_speed_unit);
    const int32_t unit_gap = 8;
    const int32_t value_y = 122;
    int32_t value_x = UI_LEFT + (UI_CONTENT_W - value_width) / 2;
    int32_t unit_x = value_x + value_width + unit_gap;

    if(unit_x + unit_width > UI_RIGHT - 8)
    {
        const int32_t group_width = value_width + unit_gap + unit_width;
        value_x = UI_LEFT + (UI_CONTENT_W - group_width) / 2;
        unit_x = value_x + value_width + unit_gap;
    }

    lv_obj_set_pos(s_speed_label, value_x, value_y);
    lv_obj_set_pos(
        s_speed_unit,
        unit_x,
        value_y + value_height - unit_height
    );
}


/**
 * @brief 速度平滑动画 Timer
 *
 * 每约 16 ms 调用一次。
 *
 * 页面实际显示速度：
 *
 *      s_display_speed
 *
 * 会逐渐逼近：
 *
 *      s_target_speed
 */
static void dashboard_speed_anim_cb(
    lv_timer_t *timer)
{
    if(!s_dashboard_active)
    {
        lv_timer_pause(timer);
        return;
    }

    /*
     * 目标值与当前显示值之间的差值。
     */
    float delta =
        s_target_speed
        - s_display_speed;


    /*
     * ========================================================
     * 差值已经非常小时直接吸附
     * ========================================================
     */

    if(
        delta > -0.01f
        &&
        delta < 0.01f
    )
    {
        s_display_speed =
            s_target_speed;
        dashboard_update_speed_visual(s_display_speed);
        lv_timer_pause(timer);
        return;
    }
    else
    {
        /*
         * ====================================================
         * 一阶低通平滑
         * ====================================================
         *
         * 例如：
         *
         * 当前：
         *
         *      30.0
         *
         * GNSS 新速度：
         *
         *      35.0
         *
         * 页面不会瞬间跳到 35.0，
         * 会逐帧平滑接近。
         */

        s_display_speed +=
            delta
            * DASH_SPEED_SMOOTH_ALPHA;
    }


    dashboard_update_speed_visual(
        s_display_speed
    );
}


/**********************
 *   GLOBAL FUNCTIONS
 **********************/


/**
 * @brief 创建 Dashboard 首页
 */
void ui_dashboard_create(void)
{
    /*
     * ========================================================
     * 防止重复创建
     * ========================================================
     */

    if(s_screen != NULL)
    {
        return;
    }


    /*
     * ========================================================
     * 创建 Screen
     * ========================================================
     */

    s_screen =
        lv_obj_create(NULL);


    /*
     * 去掉 LVGL 默认样式。
     */
    lv_obj_remove_style_all(
        s_screen
    );


    /*
     * 设置实际目标分辨率。
     */
    lv_obj_set_size(
        s_screen,
        UI_SCREEN_W,
        UI_SCREEN_H
    );


    /* 每个页面根节点直接绘制同一份共享背景数据，避免局部刷新残留旧页面。 */
    ui_background_apply(s_screen);


    /*
     * 页面禁止滚动。
     */
    lv_obj_clear_flag(
        s_screen,
        LV_OBJ_FLAG_SCROLLABLE
    );


    /*
     * ========================================================
     * 创建页面组件
     * ========================================================
     */


    /*
     * 1. 顶部状态栏
     */
    dashboard_create_status_bar();
    (void)ui_page_status_register(&s_status_bar);


    /*
     * 2. 实时速度区域
     */
    dashboard_create_speed_area();


    /*
     * 3. 数据结构线
     */
    dashboard_create_grid();


    /*
     * 4. 六个实时数据
     */
    dashboard_create_metrics();


    /* 5. 首页底部真实槽位：Surface 在这里原位替换内容，不作为弹窗叠加。 */
    s_bottom_surface_slot = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_bottom_surface_slot);
    lv_obj_set_pos(s_bottom_surface_slot, UI_BOTTOM_SURFACE_X,
                   UI_BOTTOM_SURFACE_NORMAL_Y);
    lv_obj_set_size(s_bottom_surface_slot, UI_BOTTOM_SURFACE_WIDTH,
                    UI_BOTTOM_SURFACE_NORMAL_HEIGHT);
    lv_obj_clear_flag(s_bottom_surface_slot,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_bottom_surface_slot, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    if(ui_bottom_surface_create(s_bottom_surface_slot)) {
        ui_dynamic_card_manager_create(
            ui_bottom_surface_dynamic_host());
    }


    /*
     * ========================================================
     * 创建速度平滑动画 Timer
     * ========================================================
     */

    if(s_speed_anim_timer == NULL)
    {
        s_speed_anim_timer =
            lv_timer_create(
                dashboard_speed_anim_cb,
                DASH_SPEED_ANIM_PERIOD_MS,
                NULL
            );
        lv_timer_pause(s_speed_anim_timer);
    }
}


/**
 * @brief 获取 Dashboard Screen
 *
 * @return Dashboard LVGL Screen
 */
lv_obj_t *ui_dashboard_get_screen(void)
{
    return s_screen;
}


/**
 * @brief 单独设置目标速度
 *
 * @param speed 新速度，单位 km/h
 */
void ui_dashboard_set_speed(
    float speed)
{
    /*
     * 禁止负数速度。
     */
    if(speed < 0.0f)
    {
        speed = 0.0f;
    }


    /*
     * 当前不直接修改 Label。
     *
     * 这里只更新目标值。
     *
     * dashboard_speed_anim_cb()
     * 会平滑地将当前显示值移动到目标值。
     */
    float target_delta = speed - s_target_speed;
    if(target_delta > -DASH_SPEED_TARGET_EPSILON && target_delta < DASH_SPEED_TARGET_EPSILON)
    {
        return;
    }

    s_target_speed = speed;
    if(s_speed_anim_timer != NULL && s_dashboard_active)
    {
        lv_timer_resume(s_speed_anim_timer);
    }
}

void ui_dashboard_set_active(bool active)
{
    s_dashboard_active = active;
    if(s_speed_anim_timer == NULL) return;
    if(active) lv_timer_resume(s_speed_anim_timer);
    else lv_timer_pause(s_speed_anim_timer);
}


/**
 * @brief 一次性更新 Dashboard 全部数据
 *
 * @param data Dashboard 数据
 */
void ui_dashboard_set_data(
    const ui_dashboard_data_t *data)
{
    /*
     * ========================================================
     * 空指针保护
     * ========================================================
     */

    if(data == NULL || s_screen == NULL)
    {
        return;
    }


    /*
     * ========================================================
     * 1. 实时速度
     * ========================================================
     */

    ui_dashboard_set_speed(
        data->speed
    );


    /* 顶部状态栏由 ui_app_state 统一驱动，不再从骑行数据重复写入。 */


    /*
     * ========================================================
     * 数据格式化缓冲区
     * ========================================================
     */

    char buffer[32];


    /*
     * ========================================================
     * 3. 心率
     * ========================================================
     */

    snprintf(
        buffer,
        sizeof(buffer),
        "%u",
        (unsigned int)data->heart_rate
    );


    ui_metric_card_set_value(
        &s_hr_card,
        buffer
    );


    /*
     * ========================================================
     * 4. 功率
     * ========================================================
     */

    snprintf(
        buffer,
        sizeof(buffer),
        "%u",
        (unsigned int)data->power
    );


    ui_metric_card_set_value(
        &s_power_card,
        buffer
    );


    /*
     * ========================================================
     * 5. 踏频
     * ========================================================
     */

    snprintf(
        buffer,
        sizeof(buffer),
        "%u",
        (unsigned int)data->cadence
    );


    ui_metric_card_set_value(
        &s_cadence_card,
        buffer
    );


    /*
     * ========================================================
     * 6. 坡度
     * ========================================================
     */

    snprintf(
        buffer,
        sizeof(buffer),
        "%.1f",
        data->grade
    );


    ui_metric_card_set_value(
        &s_grade_card,
        buffer
    );


    /*
     * ========================================================
     * 7. 里程
     * ========================================================
     */

    snprintf(
        buffer,
        sizeof(buffer),
        "%.2f",
        data->distance
    );


    ui_metric_card_set_value(
        &s_distance_card,
        buffer
    );


    /*
     * ========================================================
     * 8. 骑行时间
     * ========================================================
     *
     * ride_seconds：
     *
     *      总秒数
     *
     * 转换：
     *
     *      HH:MM:SS
     */


    uint32_t hours =
        data->ride_seconds
        / 3600U;


    uint32_t minutes =
        (
            data->ride_seconds
            % 3600U
        )
        / 60U;


    uint32_t seconds =
        data->ride_seconds
        % 60U;


    snprintf(
        buffer,
        sizeof(buffer),

        "%02lu:%02lu:%02lu",

        (unsigned long)hours,
        (unsigned long)minutes,
        (unsigned long)seconds
    );


    ui_metric_card_set_value(
        &s_time_card,
        buffer
    );
}
