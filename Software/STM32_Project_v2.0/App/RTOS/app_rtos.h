#ifndef APP_RTOS_H
#define APP_RTOS_H


/*====================================================================
 * File    : app_rtos.h
 * Brief   : Bike Computer application FreeRTOS interface
 *
 * 这个文件只负责：
 *
 * 1. 向其他模块提供 RTOS 层公共接口
 * 2. 定义整个系统共享的 EventGroup 状态位
 * 3. 提供 Input Queue 的发送 / 接收接口
 * 4. 提供 UART DMA / ISR 唤醒任务的接口
 *
 * 注意：
 *
 * Task 入口函数、Task Handle、TCB、Stack 等都属于 RTOS 内部实现，
 * 不应该暴露到 .h 中。
 *
 *====================================================================*/


#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

#include "app_events.h"

#include <stdint.h>
#include "ui/core/ui_physical_input.h"  // 引入12字节UI标准事件结构
#include <stdbool.h>  // 提供bool、true、false类型


#ifdef __cplusplus
extern "C" {
#endif


/*====================================================================
 *                         System Event Bits
 *====================================================================*/

/*
 * EventGroup 用来描述：
 *
 *              “系统当前处于什么状态”
 *
 * 而不是传输具体数据。
 *
 * 例如：
 *
 * GNSS 是否初始化完成
 * GNSS 是否定位成功
 * ESP32 是否连接
 * TF 卡是否挂载
 * LCD 是否准备完成
 * 当前是否正在骑行
 *
 *
 * 一个 EventGroup 本质上是一组 bit。
 *
 * bit = 1
 *      表示某状态成立
 *
 * bit = 0
 *      表示某状态不成立
 *
 *
 * 注意：
 *
 * FreeRTOS EventGroup 的最高若干 bit 被内核保留，
 * 因此我们只使用低位。
 */


/* GNSS 模块初始化完成 */
#define APP_EVENT_GNSS_READY          (1UL << 0)

/* GNSS 当前已经获得有效定位 */
#define APP_EVENT_GNSS_FIX            (1UL << 1)

/* SPL06 / ADC 等传感器初始化完成 */
#define APP_EVENT_SENSOR_READY        (1UL << 2)

/* STM32 与 ESP32 通信链路已经建立 */
#define APP_EVENT_ESP32_CONNECTED     (1UL << 3)

/* TF 卡 / FatFs 已经准备完成 */
#define APP_EVENT_STORAGE_READY       (1UL << 4)

/* LCD + LVGL 初始化完成 */
#define APP_EVENT_DISPLAY_READY       (1UL << 5)

/* 当前处于骑行记录状态 */
#define APP_EVENT_RIDING_ACTIVE       (1UL << 6)



/*====================================================================
 *                         RTOS Initialization
 *====================================================================*/

/**
 * @brief  创建应用层所有 FreeRTOS 对象
 *
 * 创建内容包括：
 *
 * - Input Queue
 * - System EventGroup
 * - SystemTask
 * - InputTask
 * - GNSSTask
 * - SensorTask
 * - CommTask
 * - StorageTask
 * - UITask
 *
 *
 * 注意：
 *
 * 该函数只负责“创建 RTOS 对象”。
 *
 * 它不会启动 Scheduler。
 *
 * Scheduler 仍由 CubeMX / CMSIS-RTOS：
 *
 *      osKernelStart()
 *
 * 启动。
 */
void App_RTOS_Init(void);



/*====================================================================
 *                           Input Queue
 *====================================================================*/

/**
 * @brief  向输入事件队列发送一个事件
 *
 * @param  event
 *         指向需要发送的 AppInputEvent_t
 *
 * @param  timeout
 *         Queue 满时允许等待的最大 Tick 数
 *
 *         例如：
 *
 *         0
 *              不等待
 *
 *         pdMS_TO_TICKS(10)
 *              最多等待 10 ms
 *
 *         portMAX_DELAY
 *              一直等待
 *
 *
 * @return
 *
 * pdPASS
 *      发送成功
 *
 * pdFAIL
 *      参数错误或发送失败
 */
BaseType_t App_RTOS_PostInputEvent(
    const AppInputEvent_t *event,
    TickType_t timeout
);


/**
 * @brief  从输入 Queue 中获取一个输入事件
 *
 * @param  event
 *         接收数据的目标结构体
 *
 * @param  timeout
 *         Queue 为空时允许等待多长时间
 *
 *
 * @return
 *
 * pdPASS
 *      成功收到事件
 *
 * pdFAIL
 *      超时或参数错误
 */
BaseType_t App_RTOS_GetInputEvent(
    ui_physical_input_event_t *event, // 输出：UI标准物理输入事件
    TickType_t timeout                // 队列为空时的最长等待Tick数
);


/* UI输入事件处理函数类型 */
typedef bool (*AppUiInputHandler_t)(
    const ui_physical_input_event_t *event);

/* 由UITask在UI初始化成功后注册输入处理函数 */
BaseType_t App_RTOS_RegisterUiInputHandler(
    AppUiInputHandler_t handler
);
/* 获取已经交给UI但被拒绝的事件数量 */
uint32_t App_RTOS_GetUiInputRejectedCount(void);


/*
 * 将POWER操作提交给SystemTask。
 *
 * 只接受：
 * APP_INPUT_ACTION_SHORT
 * APP_INPUT_ACTION_LONG
 *
 * 不进入普通的UI InputQueue。
 */
BaseType_t App_RTOS_PostPowerEvent(
    AppInputAction_t action, // 输入服务已经识别好的动作
    uint32_t timestamp_ms    // 事件产生时的毫秒时间戳
);



/*====================================================================
 *                         System EventGroup
 *====================================================================*/

/**
 * @brief 设置一个或多个系统状态位
 *
 * 示例：
 *
 * App_RTOS_SetSystemEvent(APP_EVENT_GNSS_FIX);
 */
void App_RTOS_SetSystemEvent(EventBits_t bits);


/**
 * @brief 清除一个或多个系统状态位
 *
 * 示例：
 *
 * App_RTOS_ClearSystemEvent(APP_EVENT_GNSS_FIX);
 */
void App_RTOS_ClearSystemEvent(EventBits_t bits);


/**
 * @brief 获取当前全部系统状态位
 *
 * @return 当前 EventGroup bit 值
 */
EventBits_t App_RTOS_GetSystemEvents(void);



/*====================================================================
 *                       ISR -> Task Notification
 *====================================================================*/

/**
 * @brief  从中断中通知 GNSSTask
 *
 * 典型用途：
 *
 * UART DMA + IDLE
 *
 * USART ISR / HAL Callback
 *        ↓
 * App_RTOS_NotifyGNSSFromISR()
 *        ↓
 * GNSSTask 被唤醒
 *        ↓
 * 在 Task 上下文中解析 GNSS 数据
 *
 *
 * @param higherPriorityTaskWoken
 *
 * ISR 中声明：
 *
 * BaseType_t higherPriorityTaskWoken = pdFALSE;
 *
 * 调用本函数后：
 *
 * portYIELD_FROM_ISR(higherPriorityTaskWoken);
 */
void App_RTOS_NotifyGNSSFromISR(
    BaseType_t *higherPriorityTaskWoken
);


/**
 * @brief 从中断中通知 CommTask
 *
 * 用于未来 STM32 <-> ESP32 UART DMA 接收链。
 */
void App_RTOS_NotifyCommFromISR(
    BaseType_t *higherPriorityTaskWoken
);



/*====================================================================
 *                       System Health Interface
 *====================================================================*/

/**
 * @brief 获取 SystemTask 心跳计数
 *
 * SystemTask 每秒 +1。
 *
 * 后续可作为：
 *
 * - 系统健康监控
 * - Watchdog 辅助
 * - Debug
 * - 运行状态检测
 */
uint32_t App_RTOS_GetSystemHeartbeat(void);



#ifdef __cplusplus
}
#endif


#endif /* APP_RTOS_H */

