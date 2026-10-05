/**
 * @file main.c
 * @brief 公路车码表 LVGL PC 模拟器主程序入口
 *
 * ============================================================
 * 当前程序主要功能
 * ============================================================
 *
 * 1. 初始化 LVGL
 * 2. 初始化 SDL PC 显示模拟器
 * 3. 创建 480 × 854 虚拟 LCD
 * 4. 初始化公路车码表 UI
 * 5. 周期性执行 LVGL Timer Handler
 * 6. 为后续约 60 FPS UI 刷新提供稳定调度
 *
 *
 * ============================================================
 * 当前 PC 模拟器配置
 * ============================================================
 *
 * LCD 分辨率：
 *
 *     480 × 854
 *
 * UI 目标刷新率：
 *
 *     约 60 FPS
 *
 * LVGL 推荐刷新周期：
 *
 *     LV_DEF_REFR_PERIOD = 16 ms
 *
 *
 * ============================================================
 * 当前 UI 调用关系
 * ============================================================
 *
 * main.c
 *    │
 *    ├── lv_init()
 *    │
 *    ├── sdl_hal_init()
 *    │
 *    ├── ui_init()
 *    │      │
 *    │      └── ui_dashboard_create()
 *    │
 *    └── lv_timer_handler()
 *
 *
 * ============================================================
 * 后续 STM32 移植
 * ============================================================
 *
 * PC 阶段：
 *
 *     SDL
 *      ↓
 *     LVGL
 *      ↓
 *     UI
 *
 * STM32 阶段：
 *
 *     LTDC / DMA2D / SDRAM
 *              ↓
 *             LVGL
 *              ↓
 *             UI
 *
 * UI 层代码原则上可以继续沿用。
 */


/*********************
 *      DEFINES
 *********************/

/*
 * 部分 GCC / MinGW 环境下，
 * usleep() 需要启用 _DEFAULT_SOURCE。
 */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif


/*
 * ============================================================
 * 模拟屏幕尺寸
 * ============================================================
 *
 * 与最终码表 LCD 保持一致。
 */

#define UI_SCREEN_WIDTH             480
#define UI_SCREEN_HEIGHT            854


/*
 * ============================================================
 * LVGL Handler 最大休眠时间
 * ============================================================
 *
 * 即使 lv_timer_handler() 告诉我们：
 *
 *     下一任务还有 16 ms
 *
 * PC 主循环也最多休眠 5 ms。
 *
 * 好处：
 *
 * - 输入响应更及时
 * - 动画调度更稳定
 * - 更适合后续 60 FPS 页面动画
 *
 * 注意：
 *
 * 这里并不表示屏幕每 5 ms 刷新一次。
 *
 * LVGL 内部仍按照：
 *
 *     LV_DEF_REFR_PERIOD
 *
 * 决定什么时候真正重绘 Display。
 */

#define UI_HANDLER_MAX_SLEEP_MS     5U


/*
 * 最短休眠 1 ms。
 *
 * 避免 lv_timer_handler() 返回 0 时，
 * PC 主循环无限空转导致 CPU 长时间接近 100%。
 */

#define UI_HANDLER_MIN_SLEEP_MS     1U


/*********************
 *      INCLUDES
 *********************/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


/*
 * ============================================================
 * 操作系统相关头文件
 * ============================================================
 */

#ifdef _MSC_VER

/*
 * Visual Studio / MSVC
 */
#include <Windows.h>

#else

/*
 * MinGW / MSYS2 / Linux
 */
#include <unistd.h>

#endif


/*
 * ============================================================
 * LVGL
 * ============================================================
 */

#include "lvgl/lvgl.h"


/*
 * ============================================================
 * SDL
 * ============================================================
 *
 * PC 模拟器使用 SDL 创建窗口。
 */

#include <SDL.h>


/*
 * ============================================================
 * LVGL PC HAL
 * ============================================================
 *
 * sdl_hal_init() 一般负责：
 *
 * - SDL Display 初始化
 * - LVGL Display 注册
 * - 鼠标输入注册
 * - Tick 初始化
 */

#include "hal/hal.h"
#include "perf_benchmark.h"
#include "platform/pc/ui_pc_services.h"
#include "platform/pc/ui_pc_offline_navigation.h"


/*
 * ============================================================
 * 公路车码表 UI
 * ============================================================
 *
 * ui_init() 是整个 UI 系统唯一的外部初始化入口。
 */

#include "ui/ui.h"


/**********************
 *  STATIC PROTOTYPES
 **********************/

/**
 * @brief PC 平台毫秒延时
 *
 * @param ms 延时时间，单位 ms
 */
static void app_delay_ms(uint32_t ms);


/**
 * @brief 根据 LVGL 返回值计算下一次主循环休眠时间
 *
 * @param lvgl_wait_ms lv_timer_handler() 返回值
 *
 * @return 实际休眠时间，单位 ms
 */
static uint32_t app_get_sleep_time(uint32_t lvgl_wait_ms);


/**********************
 *   GLOBAL FUNCTIONS
 **********************/


/*
 * ============================================================
 * PC 模拟器入口
 * ============================================================
 *
 * 当前项目 PC 阶段使用：
 *
 *     LV_USE_OS = LV_OS_NONE
 *
 * FreeRTOS 模式下 freertos_main.c 会承担入口功能。
 */

#if LV_USE_OS != LV_OS_FREERTOS


/**
 * @brief 主程序入口
 *
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 *
 * @return 程序退出状态
 */
int main(int argc, char **argv)
{
    /*
     * ========================================================
     * 0. 当前程序不使用命令行参数
     * ========================================================
     *
     * 消除编译器 unused parameter 警告。
     */

    app_perf_benchmark_mode_t benchmark_mode =
        argc > 1 ? app_perf_benchmark_parse_arg(argv[1]) : APP_PERF_BENCHMARK_NONE;
    /* 默认运行不启用 benchmark；必须显式零初始化避免随机栈值被当作 timer。 */
    app_perf_benchmark_t benchmark = {0};


    /*
     * ========================================================
     * 1. 初始化 LVGL
     * ========================================================
     *
     * 所有 LVGL API 使用之前，
     * lv_init() 必须首先执行。
     *
     * 主要初始化：
     *
     * - LVGL 对象系统
     * - Display 系统
     * - Input Device 系统
     * - Timer
     * - Animation
     * - Style
     * - 内存管理
     * - Event 系统
     */

    lv_init();


    /*
     * ========================================================
     * 2. 初始化 SDL PC 模拟器
     * ========================================================
     *
     * 创建：
     *
     *     480 × 854
     *
     * 的 PC 虚拟 LCD。
     *
     * 这个分辨率与最终实体屏幕保持一致。
     *
     * 因此：
     *
     * - X/Y 坐标
     * - 控件大小
     * - 字号
     * - 图标尺寸
     *
     * 后期移植到 STM32 时可以基本保持一致。
     */

    lv_display_t *display = sdl_hal_init(
        UI_SCREEN_WIDTH,
        UI_SCREEN_HEIGHT
    );


    /*
     * ========================================================
     * 3. 初始化公路车码表 UI
     * ========================================================
     *
     * 当前 ui_init() 内部主要执行：
     *
     *     ui_theme_init()
     *          ↓
     *     ui_dashboard_create()
     *          ↓
     *     lv_screen_load()
     *
     * 最终显示 Dashboard 首页。
     */

    /* Register the PC-only asynchronous GNSS/TF simulator backend. */
    ui_pc_services_register();
    (void)ui_pc_offline_navigation_register("assets/maps/xian");
    atexit(ui_pc_offline_navigation_shutdown);
    ui_init();


    /*
     * 仅用于可复现的 10 秒性能采样；默认启动路径完全不启用。
     * --perf-benchmark=static：静态界面
     * --perf-benchmark=data：100 ms 骑行数据持续变化
     */
    app_perf_benchmark_start(&benchmark, display, benchmark_mode);


    /*
     * ========================================================
     * 4. LVGL 主循环
     * ========================================================
     *
     * PC 模拟器持续运行。
     *
     * 每次循环：
     *
     *     lv_timer_handler()
     *
     * LVGL 会处理：
     *
     * - Display Refresh
     * - Animation
     * - LVGL Timer
     * - 输入设备
     * - Event
     * - 对象状态变化
     * - 页面重绘
     */

    while(1)
    {
        /*
         * ----------------------------------------------------
         * 执行 LVGL Timer Handler
         * ----------------------------------------------------
         *
         * 返回值：
         *
         * LVGL 建议距离下一次 Handler 执行还有多少毫秒。
         */

        uint32_t lvgl_wait_ms =
            lv_timer_handler();


        if(app_perf_benchmark_sample(&benchmark))
        {
            return 0;
        }


        /*
         * ----------------------------------------------------
         * 计算实际休眠时间
         * ----------------------------------------------------
         *
         * 我们限制最大休眠：
         *
         *     5 ms
         *
         * 这样 LVGL Handler 每秒大约拥有：
         *
         *     ≤ 200 次
         *
         * 的调度机会。
         *
         * Display 真实刷新频率仍由：
         *
         *     LV_DEF_REFR_PERIOD
         *
         * 控制。
         */

        uint32_t sleep_ms =
            app_get_sleep_time(
                lvgl_wait_ms
            );


        /*
         * ----------------------------------------------------
         * PC 主线程休眠
         * ----------------------------------------------------
         *
         * 减少 CPU 空转。
         */

        app_delay_ms(
            sleep_ms
        );
    }


    /*
     * 正常运行状态不会执行到这里。
     */

    return 0;
}


#endif /* LV_USE_OS != LV_OS_FREERTOS */


/**********************
 *   STATIC FUNCTIONS
 **********************/


/**
 * @brief 计算 LVGL 主循环实际休眠时间
 *
 * LVGL 的 lv_timer_handler() 会告诉应用：
 *
 *     距离下一次 LVGL 任务还有多少毫秒
 *
 * 为了保证 PC 模拟器动画和输入响应，
 * 这里增加最大 5 ms 限制。
 *
 * @param lvgl_wait_ms LVGL 推荐等待时间
 *
 * @return 实际等待时间
 */
static uint32_t app_get_sleep_time(
    uint32_t lvgl_wait_ms)
{
    uint32_t sleep_ms;


    /*
     * --------------------------------------------------------
     * LVGL 当前没有 Timer 准备运行
     * --------------------------------------------------------
     *
     * LV_NO_TIMER_READY 通常对应一个特殊值。
     *
     * 此时使用最大 Handler Sleep 时间。
     */

    if(lvgl_wait_ms == LV_NO_TIMER_READY)
    {
        sleep_ms =
            UI_HANDLER_MAX_SLEEP_MS;
    }
    else
    {
        /*
         * 使用 LVGL 推荐等待时间。
         */

        sleep_ms =
            lvgl_wait_ms;
    }


    /*
     * --------------------------------------------------------
     * 限制最大等待时间
     * --------------------------------------------------------
     */

    if(sleep_ms >
       UI_HANDLER_MAX_SLEEP_MS)
    {
        sleep_ms =
            UI_HANDLER_MAX_SLEEP_MS;
    }


    /*
     * --------------------------------------------------------
     * 限制最小等待时间
     * --------------------------------------------------------
     *
     * lv_timer_handler() 有可能返回 0。
     *
     * 如果直接：
     *
     *     while(1)
     *         lv_timer_handler();
     *
     * CPU 容易长时间满载。
     */

    if(sleep_ms <
       UI_HANDLER_MIN_SLEEP_MS)
    {
        sleep_ms =
            UI_HANDLER_MIN_SLEEP_MS;
    }


    return sleep_ms;
}


/**
 * @brief PC 平台毫秒延时
 *
 * 同时兼容：
 *
 * - Windows MSVC
 * - MinGW / MSYS2
 * - Linux
 *
 * @param ms 延时时间，单位毫秒
 */
static void app_delay_ms(uint32_t ms)
{
#ifdef _MSC_VER

    /*
     * ========================================================
     * Windows / MSVC
     * ========================================================
     *
     * Sleep() 参数单位：
     *
     *     ms
     */

    Sleep(ms);

#else

    /*
     * ========================================================
     * GCC / MinGW / MSYS2 / Linux
     * ========================================================
     *
     * usleep() 参数单位：
     *
     *     μs
     *
     * 所以：
     *
     *     1 ms = 1000 μs
     */

    usleep(
        (useconds_t)(
            ms * 1000U
        )
    );

#endif
}
