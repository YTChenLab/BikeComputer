/**
 * @file    bsp_uart.h
 * @brief   STM32H743 USART1 底层串口驱动接口
 *
 * @details
 * 本模块负责：
 *
 * 1. USART1 串口底层收发接口。
 * 2. 资源安装协议的可靠数据发送。
 * 3. USART1 单字节接收中断。
 * 4. SPSC 环形缓冲区读取。
 * 5. UART 接收异常计数。
 *
 * 软件架构：
 *
 * USART1
 *    |
 *    v
 * BSP_UART
 *    |
 *    v
 * ResourceSystem
 *    |
 *    v
 * ResourceInstall
 *
 * 说明：
 * USART1 的时钟、GPIO、波特率由 CubeMX 初始化。
 * 本模块不重复配置 UART 硬件。
 *
 * GNSS 使用独立的 USART2 + DMA 驱动，
 * 不受本模块修改影响。
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#include <stddef.h>                 // size_t 类型
#include <stdint.h>                 // 固定宽度整数类型

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------
 * 一、UART 初始化
 *---------------------------------------------------------*/

void BSP_UART_Init(void);           // BSP 初始化入口


/*----------------------------------------------------------
 * 二、UART 接收接口
 *---------------------------------------------------------*/

int BSP_UART_ReceiveStart(void);    // 启动 USART1 接收中断

size_t BSP_UART_Read(
    uint8_t *data,                 // 接收数据的目标缓冲区
    size_t capacity                // 缓冲区最大容量
);

uint32_t BSP_UART_ReceiveErrors(void); // 获取累计接收错误次数

void BSP_UART_ServiceIRQ(void);    // USART1 中断服务入口


/*----------------------------------------------------------
 * 三、UART 发送接口
 *---------------------------------------------------------*/

int BSP_UART_Write(
    const uint8_t *data,           // 需要发送的数据
    size_t len                     // 发送字节数量
);

int BSP_UART_WriteString(
    const char *str                // 需要发送的字符串
);

#ifdef __cplusplus
}
#endif

#endif /* BSP_UART_H */