#ifndef UI_DISPLAY_H
#define UI_DISPLAY_H

#include <stdint.h>
#include "platform/stm32/ui_stm32_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================
 * 文件：app_ui_display.h
 *
 * 功能：
 * 将 STM32H743 的 LTDC + DMA2D 显示系统
 * 适配到现有 LVGL 9.3 UI 平台接口。
 *
 * 使用：
 * 在 SDRAM、DMA2D、LCD 初始化成功以后，
 * 由 UI Task 调用本模块建立显示接口。
 *============================================================*/

/* LVGL 局部绘制缓冲高度：80 行 */
#define APP_UI_DRAW_LINES          (80U)

/* 每块 LVGL 绘制缓冲的字节数 */
#define APP_UI_DRAW_BUFFER_BYTES   \
    (480U * APP_UI_DRAW_LINES * 2U)

/*
 * 构建现有 UI 库要求的 Display Port（显示接口）。
 *
 * 返回值：
 * 1U：成功。
 * 0U：显示底层尚未就绪或参数无效。
 */
uint8_t App_UI_DisplayBuildPort(
    ui_stm32_display_port_t *port
);

#ifdef __cplusplus
}
#endif

#endif
