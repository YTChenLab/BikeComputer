
#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>             /* 提供 uint8_t 等固定宽度整数类型 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  STM32 应用基础启动函数
 *
 * @details
 * 本函数由 Core/Src/main.c 调用。
 *
 * 执行时 FreeRTOS 调度器尚未启动。
 *
 * 当前职责：
 * 1. 记录 STM32 的复位原因。
 * 2. 初始化 W25Q128 QSPI BSP。
 * 3. 输出基础启动日志。
 * 4. 将控制权交还 main()。
 *
 * 注意：
 * 不再在这里初始化 SDRAM、DMA2D 和 LCD。
 * 这些显示硬件由 UI Task 在需要时初始化。
 */
void App_Main(void);

/**
 * @brief  初始化 UI 所需要的全部显示硬件
 *
 * @details
 * 调用者：UI_BoardStart()。
 * 实际运行上下文：FreeRTOS 的 UITask。
 *
 * 初始化顺序：
 * 1. SDRAM 控制器及外部内存。
 * 2. 清零 UI 专用 SDRAM 数据区。
 * 3. DMA2D 图形加速器。
 * 4. LCD / LTDC 显示系统。
 * 5. 可选显示自检。
 * 6. LCD 背光。
 *
 * 安全要求：
 * 1. 只允许初始化一次。
 * 2. 初始化失败时禁止启动 LVGL。
 * 3. 初始化失败不应主动关闭整个系统的中断。
 * 4. 不得破坏已启动的资源安装通信任务。
 *
 * @retval 1U 显示硬件初始化成功。
 * @retval 0U 显示硬件初始化失败。
 */
uint8_t App_DisplayHardwareInit(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */
