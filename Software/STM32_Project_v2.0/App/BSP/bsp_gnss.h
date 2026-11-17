
#ifndef BSP_GNSS_H
#define BSP_GNSS_H

/*
 * 文件：bsp_gnss.h
 * 模块：GNSS UART DMA 接收驱动
 *
 * 硬件平台：STM32H743IIT6
 * GNSS 模块：PU-252
 * 通信接口：USART2 / DMA1 Stream0
 *
 * 职责：
 * 1. 管理 Circular DMA 接收
 * 2. 向上层提供连续字节读取接口
 * 3. 统计 DMA、UART 接收事件
 * 4. 检测接收积压及数据丢失
 * 5. 提供异常恢复接口
 *
 * 不负责：
 * NMEA 解析、定位计算、UI 数据更新。
 */

#include <stdint.h>                 // 固定宽度整数类型
#include "stm32h7xx_hal.h"         // STM32H7 HAL 类型

#ifdef __cplusplus
extern "C" {
#endif

#define GNSS_DMA_RX_SIZE     4096U   // DMA 循环接收区：4096 字节
#define GNSS_DMA_HALF_SIZE   2048U   // 每个半缓冲区：2048 字节
#define GNSS_DMA_CACHE_LINE    32U   // Cortex-M7 Cache 行大小
#define GNSS_DMA_SAFE_LIMIT  3072U   // 触发安全保护的积压阈值

/*
 * GNSS 接收运行统计。
 *
 * copied_bytes：真正交付给 GNSSTask 的字节数。
 * dropped_bytes：因接收积压主动丢弃的字节数。
 *
 * 这些计数是 32 位累计计数，允许自然回绕。
 */
typedef struct
{
    uint32_t copied_bytes;          // 成功交付给任务的总字节数
    uint32_t dropped_bytes;         // 接收积压导致丢弃的总字节数
    uint32_t overflow_events;       // 接收积压触发保护的次数

    uint32_t idle_events;           // UART IDLE 事件次数
    uint32_t half_events;           // DMA HT 半传输事件次数
    uint32_t complete_events;       // DMA TC 完成事件次数

    uint32_t uart_errors;           // UART 错误回调次数
    uint32_t last_error_code;       // 最近一次 UART 错误码

    uint32_t recovery_attempts;     // 尝试恢复接收的次数
    uint32_t recovery_failures;     // 接收恢复失败的次数

} GNSS_DMA_Stats_t;

/*
 * 初始化模块。
 *
 * 必须在 USART2 和 DMA 外设初始化完成后调用。
 * 本函数不直接启动接收。
 */
void GNSS_DMA_Init(UART_HandleTypeDef *huart);

/*
 * 启动 Circular DMA + IDLE 接收。
 *
 * 返回 HAL_OK 表示启动成功。
 */
HAL_StatusTypeDef GNSS_DMA_Start(void);

/*
 * 读取当前尚未消费的 GNSS 原始字节。
 *
 * dst：上层提供的目标数组。
 * capacity：目标数组最大容量。
 *
 * 返回实际复制的字节数。
 *
 * 只能由 GNSSTask 调用。
 */
uint32_t GNSS_DMA_Read(
    uint8_t *dst,
    uint32_t capacity
);

/*
 * 查询当前 DMA 接收是否正常运行。
 */
uint8_t GNSS_DMA_IsRunning(void);

/*
 * 查询是否存在需要恢复的接收错误。
 */
uint8_t GNSS_DMA_NeedsRecovery(void);

/*
 * 在任务上下文恢复 UART DMA 接收。
 *
 * 不允许在 UART 错误中断中直接调用。
 */
HAL_StatusTypeDef GNSS_DMA_Recover(void);

/*
 * 获取当前 DMA 接收诊断统计。
 */
void GNSS_DMA_GetStats(GNSS_DMA_Stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* GNSS_UART_DMA_H */
