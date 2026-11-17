
/*
 * 文件：bsp_gnss.c
 *
 * 功能：
 * STM32H743 USART2 + DMA Circular + IDLE 接收驱动
 *
 * 接收架构：
 *
 * PU-252
 *   ↓ UART
 * USART2
 *   ↓ DMA1 Stream0
 * D2 SRAM 循环缓冲区
 *   ↓
 * GNSSTask
 *
 * 本模块只实现原始字节接收，不解析 NMEA。
 *
 * 重要约束：
 * 1. 只有 GNSSTask 可以调用 GNSS_DMA_Read()
 * 2. DMA 缓冲区由 DMA 写入，CPU 不主动修改
 * 3. DMA 缓冲区必须按 32 字节对齐
 * 4. 该段必须由链接器放入 DMA 可访问的 SRAM
 * 5. ISR 只更新事件计数和发送任务通知
 */

#include "bsp_gnss.h"           // 当前模块头文件
#include "app_rtos.h"               // 已存在的 FreeRTOS GNSS 通知接口

#include <stdint.h>                 // uint32_t、uintptr_t
#include <string.h>                 // memcpy、memset

/*==================== DMA 接收缓冲区 ====================*/

/*
 * 独立 DMA 缓冲区。
 *
 * aligned(32)：
 * 起始地址按 32 字节对齐。
 *
 * section("GNSS_DMA_RX")：
 * 放进独立链接段。
 *
 * 需要在 Keil Scatter 文件中分配到 D2 SRAM。
 *
 * 本数组容量为 4096 字节，
 * 同时也是 32 字节的整数倍。
 */
static uint8_t s_dmaRxBuffer[GNSS_DMA_RX_SIZE]
    __attribute__((aligned(32), section("GNSS_DMA_RX")));

/*==================== 模块运行状态 ====================*/

static UART_HandleTypeDef *s_uart = NULL;   // 保存当前使用的 USART2 句柄

static volatile uint32_t s_halfCount = 0U;  // DMA 已经完整写入的半区总数

static uint32_t s_readTotal = 0U;           // GNSSTask 累计消费位置

static volatile uint8_t s_running = 0U;     // DMA 已成功启动标志

static volatile uint8_t s_needRecovery = 0U;// UART 接收故障恢复请求

static volatile GNSS_DMA_Stats_t s_stats;   // 接收链路运行诊断统计

/*==================== D-Cache 处理 ====================*/

/*
 * 使 DMA 接收数据对应的 D-Cache 缓存行失效。
 *
 * 原理：
 *
 * DMA 写入的是 SRAM。
 * CPU 读取数据时可能优先访问 D-Cache。
 *
 * 如果 Cache 中保存着旧数据，
 * CPU 就可能读不到 DMA 刚写入的新字节。
 *
 * 因此读取之前执行 Invalidate。
 *
 * 注意：
 * 这里使用专用、独占缓存行的 DMA 缓冲区。
 * 不能与 CPU 正在写入的普通变量共享缓存行。
 */
static void GNSS_DMA_Invalidate(
    const uint8_t *address,
    uint32_t size)
{
    uintptr_t start;                  // 对齐后的起始地址
    uintptr_t end;                    // 对齐后的结束地址

    if ((address == NULL) || (size == 0U))
    {
        return;                       // 无效参数无需处理
    }

    if ((SCB->CCR & SCB_CCR_DC_Msk) == 0U)
    {
        return;                       // D-Cache 未开启时直接返回
    }

    start = (uintptr_t)address;       // 指针转换为整数地址

    end = start + (uintptr_t)size;    // 计算结束地址

    start &= ~((uintptr_t)GNSS_DMA_CACHE_LINE - 1U);
                                      // 起始地址向下对齐到 32 字节

    end = (end + GNSS_DMA_CACHE_LINE - 1U)
        & ~((uintptr_t)GNSS_DMA_CACHE_LINE - 1U);
                                      // 结束地址向上对齐到 32 字节

    SCB_InvalidateDCache_by_Addr(
        (uint32_t *)start,            // 对齐后的起始地址
        (int32_t)(end - start)        // 实际需要失效的字节数
    );

    __DMB();                          // 保证缓存维护与后续读取的顺序
}

/*==================== DMA 生产位置计算 ====================*/

/*
 * 计算 DMA 已经写入的绝对字节位置。
 *
 * 4096 字节缓冲区分为两个 2048 字节区域。
 *
 * HT：完整写入前半区，halfCount +1
 * TC：完整写入后半区，halfCount +1
 *
 * DMA NDTR 表示当前剩余传输数量。
 *
 * 利用半区完成次数和 NDTR，
 * 可以推算 DMA 的累计生产位置。
 *
 * 边界条件：
 * 如果 DMA 已越过半区边界，
 * 但对应 HT/TC ISR 尚未更新计数，
 * 则暂时返回已经确认的完整半区位置。
 *
 * 该函数不会暂停 DMA。
 */
static uint32_t GNSS_DMA_GetProducedTotal(void)
{
    uint32_t before;                  // 读取前的半区完成计数
    uint32_t after;                   // 读取后的半区完成计数

    uint32_t remaining;               // DMA NDTR 剩余字节数
    uint32_t position;                // DMA 在 4096 字节数组中的位置

    uint32_t base;                    // 已完成半区对应的累计位置
    uint32_t attempt;                 // 一致性快照重试次数

    if ((s_uart == NULL) || (s_uart->hdmarx == NULL))
    {
        return s_readTotal;           // 无 DMA 句柄，不允许产生新增数据
    }

    for (attempt = 0U; attempt < 3U; attempt++)
    {
        before = s_halfCount;         // 获取第一次 ISR 完成计数

        __DMB();                      // 保证共享变量访问顺序

        remaining = __HAL_DMA_GET_COUNTER(
            s_uart->hdmarx);         // 读取 DMA 硬件剩余数量

        __DMB();                      // 保证 NDTR 读取与后续变量访问顺序

        after = s_halfCount;         // 再次读取 ISR 完成计数

        if (before != after)
        {
            continue;                // 期间发生中断，重新取得快照
        }

        base = before * GNSS_DMA_HALF_SIZE;
                                      // 已完整写入的半区总字节数

        if (remaining > GNSS_DMA_RX_SIZE)
        {
            return base;             // 非法 NDTR，保守返回确认位置
        }

        position = GNSS_DMA_RX_SIZE - remaining;
                                      // 换算 DMA 当前写入偏移

        if ((before & 1U) == 0U)
        {
            /*
             * 偶数个半区完成：
             * DMA 应该处于前半区。
             */
            if (position > GNSS_DMA_HALF_SIZE)
            {
                return base;         // HT 中断尚未确认，延后读取
            }

            return base + position;  // 完整半区字节数 + 当前偏移
        }

        /*
         * 奇数个半区完成：
         * DMA 应该处于后半区。
         */
        if (position < GNSS_DMA_HALF_SIZE)
        {
            return base;             // TC 中断尚未确认，延后读取
        }

        return base
             + position
             - GNSS_DMA_HALF_SIZE;   // 计算当前完整生产位置
    }

    return s_halfCount * GNSS_DMA_HALF_SIZE;
                                      // 多次发生竞争时保守返回
}

/*==================== 初始化 ====================*/

void GNSS_DMA_Init(UART_HandleTypeDef *huart)
{
    configASSERT(huart != NULL);     // 必须传入有效 UART 句柄

    configASSERT(huart->hdmarx != NULL);
                                      // 必须已经建立 USART2 RX DMA 句柄

    s_uart = huart;                  // 绑定当前 UART 外设

    s_halfCount = 0U;                // 初始化半区计数

    s_readTotal = 0U;                // 初始化软件消费位置

    s_running = 0U;                  // 当前尚未启动 DMA

    s_needRecovery = 0U;             // 初始无接收故障

    memset((void *)&s_stats, 0, sizeof(s_stats));
                                      // 清空运行统计
}

/*==================== 启动 DMA ====================*/

HAL_StatusTypeDef GNSS_DMA_Start(void)
{
    HAL_StatusTypeDef status;        // HAL 启动状态

    if ((s_uart == NULL) || (s_uart->hdmarx == NULL))
    {
        return HAL_ERROR;            // 尚未正确初始化
    }

    if (s_running != 0U)
    {
        return HAL_OK;               // 已经运行时不允许重复启动
    }

    s_halfCount = 0U;                // 本次 DMA 启动从零开始统计

    s_readTotal = 0U;                // 清除上一次 DMA 接收的消费位置

    /*
     * 在启动 DMA 前使专用接收缓冲区的旧 Cache 失效。
     *
     * 避免本次 DMA 接收与旧缓存数据混用。
     */
    GNSS_DMA_Invalidate(
        s_dmaRxBuffer,
        GNSS_DMA_RX_SIZE
    );

    /*
     * 正式启动 USART2 Circular DMA + IDLE。
     *
     * CubeMX 中必须已经将 RX DMA 配成 Circular。
     *
     * DMA 不会因为 IDLE 事件自动停止。
     */
    status = HAL_UARTEx_ReceiveToIdle_DMA(
        s_uart,                      // STM32 HAL USART2 句柄
        s_dmaRxBuffer,               // DMA 实际写入的 RAM 数组
        (uint16_t)GNSS_DMA_RX_SIZE   // 本次接收缓冲区大小
    );

    if (status == HAL_OK)
    {
        s_running = 1U;                   // DMA启动成功，标记接收正在运行
        s_needRecovery = 0U;              // 清除之前可能存在的恢复请求
    }
    else
    {
        s_running = 0U;                   // DMA启动失败，接收未正常运行
        s_needRecovery = 1U;              // 请求GNSSTask执行完整恢复流程
    }

    return status;                   // 向上层返回真实启动结果
}

/*==================== 读取 DMA 数据 ====================*/

uint32_t GNSS_DMA_Read(
    uint8_t *dst,
    uint32_t capacity)
{
    uint32_t produced;               // DMA 当前累计生产位置

    uint32_t pending;                // 尚未消费的字节数

    uint32_t length;                 // 本次准备复制的字节数

    uint32_t offset;                 // 当前读取位置在 DMA 数组中的偏移

    uint32_t first;                  // 数组末尾第一段复制长度

    uint32_t second;                 // 回绕到数组开头后的第二段长度

    uint32_t lost;                   // 因积压过多而主动丢弃的字节数

    if ((dst == NULL) || (capacity == 0U))
    {
        return 0U;                   // 无效参数
    }

    if ((s_running == 0U) || (s_needRecovery != 0U))
    {
        return 0U;                   // DMA 没运行或者已经出现接收故障
    }

    produced = GNSS_DMA_GetProducedTotal();
                                      // 取得硬件已写入的累计位置

    pending = produced - s_readTotal;
                                      // 利用 uint32_t 回绕语义计算差值

    /*
     * 如果得到一个非常大的差值，
     * 可能是边界中断尚未确认造成的旧快照。
     *
     * 小于 2^31 字节的真实积压属于正常计数范围。
     */
    if (pending >= 0x80000000UL)
    {
        return 0U;                   // 忽略暂时不一致的生产位置
    }

    if (pending == 0U)
    {
        return 0U;                   // 当前不存在新增数据
    }

    /*
     * 为正在写入的 DMA 保留安全距离。
     *
     * 一旦积压超过 3072 字节：
     * 主动丢弃较老的数据，
     * 只保留最新 2048 字节。
     *
     * 这是有意设计的实时性保护策略。
     * 不表示所有丢弃字节都已被硬件实际覆盖。
     */
    if (pending > GNSS_DMA_SAFE_LIMIT)
    {
        lost = pending - GNSS_DMA_HALF_SIZE;
                                      // 计算需要跳过的旧数据量

        s_readTotal += lost;         // 推进消费位置

        s_stats.dropped_bytes += lost;
                                      // 累加主动丢弃的字节数

        s_stats.overflow_events++;   // 记录一次安全阈值触发

        pending = GNSS_DMA_HALF_SIZE;
                                      // 只保留最新半缓冲区的数据
    }

    length = pending;                // 默认读取当前所有待处理字节

    if (length > capacity)
    {
        length = capacity;           // 不能超过目标数组的实际容量
    }

    offset = s_readTotal % GNSS_DMA_RX_SIZE;
                                      // 转换为 DMA 循环数组中的位置

    first = GNSS_DMA_RX_SIZE - offset;
                                      // 从当前偏移到数组末尾的长度

    if (first > length)
    {
        first = length;              // 第一段不能超过本次目标长度
    }

    second = length - first;         // 计算数组回绕后的第二段长度

    /*
     * 读取第一段数据前执行 Cache Invalidate。
     */
    GNSS_DMA_Invalidate(
        &s_dmaRxBuffer[offset],
        first
    );

    memcpy(
        dst,                         // 目标字节数组
        &s_dmaRxBuffer[offset],      // DMA 缓冲区当前读取位置
        first                        // 本段复制长度
    );

    /*
     * 如果本轮读取越过 DMA 数组末尾，
     * 还需要从数组起点继续复制第二段。
     */
    if (second > 0U)
    {
        GNSS_DMA_Invalidate(
            &s_dmaRxBuffer[0],
            second
        );

        memcpy(
            &dst[first],             // 目标数组后半部分
            &s_dmaRxBuffer[0],       // DMA 数组起始位置
            second                   // 回绕后需要复制的字节数
        );
    }

    s_readTotal += length;           // 更新软件累计消费位置

    s_stats.copied_bytes += length;  // 累加成功交付字节数

    return length;                   // 返回本次真正复制的字节数
}

/*==================== 查询运行状态 ====================*/

uint8_t GNSS_DMA_IsRunning(void)
{
    return s_running;                // 返回 DMA 运行标志
}

uint8_t GNSS_DMA_NeedsRecovery(void)
{
    return s_needRecovery;           // 返回故障恢复请求
}

/*==================== UART 接收异常恢复 ====================*/

HAL_StatusTypeDef GNSS_DMA_Recover(void)
{
    HAL_StatusTypeDef status;        // HAL 恢复结果

    if (s_uart == NULL)
    {
        return HAL_ERROR;            // 尚未完成初始化
    }

    if (s_needRecovery == 0U)
    {
        return HAL_OK;               // 没有故障，无需恢复
    }

    s_stats.recovery_attempts++;     // 记录一次恢复尝试

    s_running = 0U;                  // 先标记接收停止

    /*
     * 在 GNSSTask 上下文停止旧接收。
     *
     * 禁止在 UART 错误中断回调内进行此操作。
     */
    (void)HAL_UART_DMAStop(s_uart);

    /*
     * 清除 PE、FE、NE、ORE 等错误标志。
     */
    __HAL_UART_CLEAR_FLAG(
        s_uart,
        UART_CLEAR_PEF |
        UART_CLEAR_FEF |
        UART_CLEAR_NEF |
        UART_CLEAR_OREF
    );

    status = GNSS_DMA_Start();       // 启动新一轮 Circular DMA 接收

    if (status != HAL_OK)
    {
        s_needRecovery = 1U;         // 失败时保留恢复请求

        s_stats.recovery_failures++;
                                      // 记录一次恢复失败
    }

    return status;                   // 返回实际恢复结果
}

/*==================== 诊断统计 ====================*/

void GNSS_DMA_GetStats(GNSS_DMA_Stats_t *stats)
{
    if (stats == NULL)
    {
        return;                       // 防止空指针访问
    }

    /*
     * 按字段建立运行诊断快照。
     *
     * ISR 可能同时更新中断相关统计，
     * 因此本接口提供的是非事务型监测快照。
     */
    stats->copied_bytes = s_stats.copied_bytes;

    stats->dropped_bytes = s_stats.dropped_bytes;

    stats->overflow_events = s_stats.overflow_events;

    stats->idle_events = s_stats.idle_events;

    stats->half_events = s_stats.half_events;

    stats->complete_events = s_stats.complete_events;

    stats->uart_errors = s_stats.uart_errors;

    stats->last_error_code = s_stats.last_error_code;

    stats->recovery_attempts = s_stats.recovery_attempts;

    stats->recovery_failures = s_stats.recovery_failures;
}

/*==================== HAL 接收事件回调 ====================*/

/*
 * UART IDLE、DMA HT、DMA TC 事件均会进入此回调。
 *
 * 本回调不负责解析 NMEA。
 *
 * 唯一目标：
 * - 更新完成计数
 * - 统计接收事件
 * - 唤醒已经创建的 GNSSTask
 */
void HAL_UARTEx_RxEventCallback(
    UART_HandleTypeDef *huart,
    uint16_t Size)
{
    HAL_UART_RxEventTypeTypeDef eventType;
                                      // HAL 报告的本次接收事件类型

    BaseType_t higherPriorityTaskWoken = pdFALSE;
                                      // FreeRTOS ISR 任务切换请求标志

    (void)Size;                       // 本驱动使用 NDTR 推算实际生产位置

    if ((s_uart == NULL) || (huart != s_uart))
    {
        return;                       // 忽略其他 UART 的接收事件
    }

    eventType = HAL_UARTEx_GetRxEventType(huart);
                                      // 查询 HT / TC / IDLE 类型

    if (eventType == HAL_UART_RXEVENT_HT)
    {
        s_halfCount++;                // 已完成一个 2048 字节半区

        s_stats.half_events++;        // 累计 HT 事件数量
    }
    else if (eventType == HAL_UART_RXEVENT_TC)
    {
        s_halfCount++;                // 已完成第二个 2048 字节半区

        s_stats.complete_events++;    // 累计 TC 事件数量
    }
    else if (eventType == HAL_UART_RXEVENT_IDLE)
    {
        s_stats.idle_events++;        // 累计串口空闲事件
    }
    else
    {
        return;                       // 未知事件不通知上层任务
    }

    __DMB();                          // 先发布最新计数，再进行任务通知

    App_RTOS_NotifyGNSSFromISR(
        &higherPriorityTaskWoken
    );                                // 使用现有 GNSS Task 通知接口

    portYIELD_FROM_ISR(
        higherPriorityTaskWoken
    );                                // 必要时在退出 ISR 后切换任务
}

/*==================== HAL UART 错误回调 ====================*/

/*
 * UART 接收错误处理。
 *
 * 本回调只记录错误和请求恢复，
 * 不能直接停止并重新启动 DMA。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;
                                      // ISR 是否需要触发任务切换

    if ((s_uart == NULL) || (huart != s_uart))
    {
        return;                       // 不是 GNSS USART2，直接退出
    }

    s_stats.uart_errors++;           // UART 错误累计加一

    s_stats.last_error_code = huart->ErrorCode;
                                      // 保存 HAL 报告的实际错误码

    s_needRecovery = 1U;             // 通知任务准备重新建立接收

    s_running = 0U;                  // 当前接收不再视为正常运行

    App_RTOS_NotifyGNSSFromISR(
        &higherPriorityTaskWoken
    );                                // 唤醒 GNSSTask 执行恢复

    portYIELD_FROM_ISR(
        higherPriorityTaskWoken
    );                                // 根据需要请求任务切换
}
