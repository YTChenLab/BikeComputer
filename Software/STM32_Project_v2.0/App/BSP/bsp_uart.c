/**
 * @file    bsp_uart.c
 * @brief   STM32H743 USART1 底层收发驱动
 *
 * @details
 * 本模块实现：
 *
 * 1. USART1 可靠阻塞发送。
 * 2. USART1 单字节中断接收。
 * 3. 1024 字节 SPSC 环形缓冲区。
 * 4. UART 接收错误检测。
 * 5. 接收异常后的重新挂载。
 *
 * 数据流：
 *
 * PC资源安装工具
 *       |
 *       v
 *     USART1
 *       |
 *       v
 * HAL_UART_RxCpltCallback()
 *       |
 *       v
 * SPSC RingBuffer
 *       |
 *       v
 * BSP_UART_Read()
 *       |
 *       v
 * ResourceSystem_Poll()
 *
 * 注意：
 * USART1 硬件由 CubeMX 初始化。
 * 本模块不重复执行 HAL_UART_Init()。
 *
 * GNSS 的 USART2 DMA 独立运行。
 * 本文件不接管全局 HAL_UART_ErrorCallback。
 */

#include "bsp_uart.h"             // UART BSP 公开接口
#include "usart.h"                // CubeMX huart1 句柄

#include <string.h>               // strlen()
#include <limits.h>               // UINT16_MAX


/*==========================================================
 * 一、UART 配置参数
 *=========================================================*/

#define BSP_UART_TIMEOUT_MS  (100U)     // 单次发送最大阻塞时间 100ms

#define BSP_UART_RX_CAP      (1024U)    // 接收环形缓冲区容量 1024 字节


/*==========================================================
 * 二、UART 接收缓冲区
 *
 * USART1 每次通过中断接收一个字节。
 *
 * 中断负责写入：
 *     s_rx_head
 *
 * CommTask 负责读取：
 *     s_rx_tail
 *
 * 二者分别拥有写入位置和读取位置，
 * 构成单生产者、单消费者环形缓冲区。
 *=========================================================*/

static uint8_t s_rx_byte = 0U;           // HAL 单字节接收存储区

static uint8_t s_rx_ring[
    BSP_UART_RX_CAP
];                                       // UART 接收环形缓冲区

static volatile uint32_t s_rx_head = 0U; // 中断写入位置

static volatile uint32_t s_rx_tail = 0U; // 任务读取位置

static volatile uint32_t s_rx_errors = 0U; // 接收异常累计次数

static uint8_t s_rx_started = 0U;        // 接收中断是否已启动


/*==========================================================
 * 三、BSP_UART_Init()
 *
 * 初始化入口。
 *
 * USART1 的基础硬件配置已经由 CubeMX 完成：
 *
 * MX_USART1_UART_Init()
 *
 * 因此不在 BSP 层重复初始化外设。
 *=========================================================*/

void BSP_UART_Init(void)
{
    /*
     * 当前无须执行额外操作。
     *
     * 保留统一 BSP 初始化接口，
     * 避免影响已有的上层调用关系。
     */
}


/*==========================================================
 * 四、BSP_UART_Write()
 *
 * 通过 USART1 发送完整数据。
 *
 * 参数：
 * data：待发送数据起始地址
 * len ：待发送字节数
 *
 * 返回值：
 *  0：发送成功
 * -1：发送失败
 *
 * 原理：
 * HAL_UART_Transmit() 使用阻塞方式发送。
 *
 * 当前 USART1 只用于资源协议及原有日志，
 * 不再与网页遥测竞争发送通道。
 *
 * 注意：
 * 本接口不允许在中断中调用。
 *=========================================================*/

int BSP_UART_Write(
    const uint8_t *data,
    size_t len
)
{
    HAL_StatusTypeDef status;             // HAL 发送状态

    /* 一、检查参数 */

    if ((data == NULL) ||                 // 数据地址无效
        (len == 0U) ||                    // 不允许发送空数据
        (len > UINT16_MAX))               // HAL 发送长度为 16 位
    {
        return -1;                        // 参数错误
    }

    /* 二、检查当前是否处于中断上下文 */

    if (__get_IPSR() != 0U)
    {
        /*
         * IPSR 非零表示当前正在执行异常或中断。
         *
         * 阻塞式 UART 发送可能在中断中造成等待问题。
         *
         * 因此只允许在线程/任务上下文中调用。
         */
        return -1;
    }

    /* 三、执行完整数据发送 */

    status = HAL_UART_Transmit(
        &huart1,                          // USART1 HAL 句柄
        (uint8_t *)data,                  // 发送数据地址
        (uint16_t)len,                    // 发送长度
        BSP_UART_TIMEOUT_MS              // 最长等待时间
    );

    /* 四、返回发送结果 */

    return (status == HAL_OK) ? 0 : -1;
}


/*==========================================================
 * 五、BSP_UART_WriteString()
 *
 * 发送字符串。
 *
 * 主要用于已有的启动日志等文本输出。
 * 最终仍然调用 BSP_UART_Write()。
 *=========================================================*/

int BSP_UART_WriteString(const char *str)
{
    /* 一、检查字符串地址 */

    if (str == NULL)
    {
        return -1;
    }

    /* 二、调用统一发送接口 */

    return BSP_UART_Write(
        (const uint8_t *)str,              // 转换为字节数组
        strlen(str)                        // 计算字符串长度
    );
}


/*==========================================================
 * 六、BSP_UART_ReceiveStart()
 *
 * 启动 USART1 中断接收。
 *
 * HAL_UART_Receive_IT() 每次接收 1 字节。
 *
 * 数据到达后：
 * HAL_UART_RxCpltCallback() 被调用。
 *
 * 然后在回调中重新启动下一字节接收。
 *=========================================================*/

int BSP_UART_ReceiveStart(void)
{
    /* 一、防止重复启动 */

    if (s_rx_started != 0U)
    {
        return 0;                          // 已启动，直接返回
    }

    /* 二、清空接收环形缓冲区状态 */

    s_rx_head = 0U;                        // 清除写入位置

    s_rx_tail = 0U;                        // 清除读取位置

    s_rx_errors = 0U;                      // 清除接收异常计数

    /* 三、启动单字节 UART 接收中断 */

    if (HAL_UART_Receive_IT(
            &huart1,                       // USART1 句柄
            &s_rx_byte,                    // 单字节接收地址
            1U                             // 每次接收 1 字节
        ) != HAL_OK)
    {
        return -1;                         // 接收启动失败
    }

    /* 四、记录接收启动状态 */

    s_rx_started = 1U;

    return 0;
}


/*==========================================================
 * 七、HAL_UART_RxCpltCallback()
 *
 * HAL UART 接收完成回调。
 *
 * 执行环境：中断上下文。
 *
 * 核心要求：
 * 1. 不执行复杂协议解析。
 * 2. 不访问 LVGL。
 * 3. 不执行阻塞操作。
 * 4. 只将字节写入环形缓冲区。
 *
 * 这样可以减少 UART 接收中断占用时间。
 *=========================================================*/

void HAL_UART_RxCpltCallback(
    UART_HandleTypeDef *huart
)
{
    uint32_t next;                          // 下一个写入位置

    /* 一、只处理 USART1 */

    if (huart != &huart1)
    {
        return;                             // 其他 UART 直接返回
    }

    /* 二、计算下一次写入位置 */

    next = (s_rx_head + 1U) % BSP_UART_RX_CAP;

    /* 三、检查环形缓冲区是否已满 */

    if (next == s_rx_tail)
    {
        /*
         * 如果 next == tail，说明没有可写空间。
         *
         * 为了保护还没有被 CommTask 读取的数据，
         * 选择丢弃当前新字节。
         */
        ++s_rx_errors;
    }
    else
    {
        /* 四、将新字节写入环形缓冲区 */

        s_rx_ring[s_rx_head] = s_rx_byte;

        /*
         * DMB：Data Memory Barrier。
         *
         * 保证先写入数据，
         * 再向读取任务发布新的 head。
         */
        __DMB();

        s_rx_head = next;
    }

    /* 五、重新启动下一字节接收 */

    if (HAL_UART_Receive_IT(
            &huart1,
            &s_rx_byte,
            1U
        ) != HAL_OK)
    {
        ++s_rx_errors;                     // 记录重新挂载失败
    }
}


/*==========================================================
 * 八、BSP_UART_ServiceIRQ()
 *
 * 由 Core/Src/stm32h7xx_it.c 中的
 * USART1_IRQHandler() 调用。
 *
 * 负责：
 * 1. 执行 HAL UART 中断处理。
 * 2. 在 UART 接收退出后尝试重新启动接收。
 *
 * 注意：
 * 不修改 GNSS USART2 的 DMA 中断处理。
 *=========================================================*/

void BSP_UART_ServiceIRQ(void)
{
    /* 一、交给 HAL 处理 USART1 中断 */

    HAL_UART_IRQHandler(&huart1);

    /* 二、检查 UART 接收是否意外停止 */

    if ((s_rx_started != 0U) &&
        (huart1.RxState == HAL_UART_STATE_READY))
    {
        /*
         * 接收状态变为 READY，
         * 可能意味着错误导致接收结束。
         *
         * 将本次异常通知 ResourceSystem。
         */
        ++s_rx_errors;

        /* 重新尝试启动接收 */

        (void)HAL_UART_Receive_IT(
            &huart1,
            &s_rx_byte,
            1U
        );
    }
}


/*==========================================================
 * 九、BSP_UART_Read()
 *
 * 从 SPSC 环形缓冲区批量读取数据。
 *
 * 调用者：
 * ResourceSystem_Poll()
 *
 * 返回：
 * 实际读取的字节数量。
 *
 * 只允许一个任务作为读取者。
 *=========================================================*/

size_t BSP_UART_Read(
    uint8_t *data,
    size_t capacity
)
{
    size_t n = 0U;                         // 已读取字节数

    /* 一、检查目标缓冲区 */

    if ((data == NULL) && (capacity != 0U))
    {
        return 0U;
    }

    /* 二、读取接收环形缓冲区 */

    while ((n < capacity) &&
           (s_rx_tail != s_rx_head))
    {
        /*
         * 确保读取最新发布的数据。
         */
        __DMB();

        /* 复制一个字节到调用者缓冲区 */

        data[n++] = s_rx_ring[s_rx_tail];

        /*
         * 读取完成后更新 tail。
         */
        __DMB();

        s_rx_tail =
            (s_rx_tail + 1U) % BSP_UART_RX_CAP;
    }

    /* 三、返回实际读取的字节数 */

    return n;
}


/*==========================================================
 * 十、BSP_UART_ReceiveErrors()
 *
 * 返回 UART 接收错误累计次数。
 *
 * ResourceSystem 会通过此计数判断：
 * 当前资源安装事务是否存在数据丢失。
 *=========================================================*/

uint32_t BSP_UART_ReceiveErrors(void)
{
    return s_rx_errors;                     // 返回当前累计错误次数
}