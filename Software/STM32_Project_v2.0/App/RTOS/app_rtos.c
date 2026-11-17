/**
 * @file    app_rtos.c
 * @brief   公路车码表 FreeRTOS 应用任务及通信管理
 *
 * @details
 * 系统包含七个正式任务：
 *
 * 1. SystemTask  ：系统心跳、电源事件处理。
 * 2. InputTask   ：实体按键、旋转编码器扫描。
 * 3. GNSSTask    ：PU-252 GNSS DMA 数据接收。
 * 4. SensorTask  ：电池及后续 SPL06 传感器管理。
 * 5. CommTask    ：资源安装通信，后续双 MCU 通信。
 * 6. StorageTask ：后续 TF 卡与骑行记录。
 * 7. UITask      ：LVGL 界面及输入事件消费。
 *
 * 任务间通信：
 *
 * InputQueue  ：普通实体输入事件 -> UITask
 * PowerQueue  ：POWER 电源请求 -> SystemTask
 * EventGroup  ：系统模块就绪状态
 * Notification：UART/DMA 中断唤醒任务
 *
 * 工程原则：
 *
 * 1. 任务、队列和事件组使用静态创建。
 * 2. 中断只负责快速处理数据及通知任务。
 * 3. 复杂业务逻辑在对应 Task 中执行。
 * 4. LVGL API 统一由 UITask 调用。
 * 5. BSP、Service、Application 分层保持不变。
 *
 * 本版本不包含 BikeScope 网页遥测。
 */

#include "app_rtos.h"         // FreeRTOS 应用接口
#include "input_service.h"    // 按键、编码器输入识别
#include "input_router.h"     // 内部输入事件转换
#include "power_service.h"    // 电源管理服务
#include "gnss.h"             // PU-252 NMEA 解析器
#include "gnss_service.h"     // 跨任务 GNSS 状态快照
#include "usart.h"            // USART2 HAL 句柄
#include "resource_system.h"  // QSPI 资源安装与维护
#include "ui_board.h"         // 正式 LVGL UI 适配
#include "bsp_adc.h"          // STM32 ADC 底层驱动
#include "bsp_gnss.h"       // PU-252 USART2 DMA 底层驱动
#include "battery_service.h"  // 电池状态服务
#include "sensor_service.h"   // SPL06 传感器状态管理服务

#include "FreeRTOS.h"         // FreeRTOS 核心配置
#include "task.h"             // Task API
#include "queue.h"            // Queue API
#include "event_groups.h"     // EventGroup API

#include <stdint.h>           // 固定宽度整数
#include <stdbool.h>          // bool 类型
#include <stddef.h>           // NULL


/*==========================================================
 * 一、编译期检查
 *
 * LVGL 物理输入事件必须为 12 字节。
 *
 * 如果后续修改事件结构体导致尺寸变化，
 * 编译阶段会直接报错，防止通信格式不匹配。
 *=========================================================*/

typedef char AppUiEventSizeCheck[
    (sizeof(ui_physical_input_event_t) == 12U) ? 1 : -1
];


/*==========================================================
 * 二、任务栈容量
 *
 * FreeRTOS 的任务栈深度以 StackType_t 为单位。
 *
 * Cortex-M7 中 StackType_t 通常为 32 位，
 * 即每个栈单位占 4 字节。
 *
 * 这里保留原工程任务栈设置。
 *=========================================================*/

#define SYSTEM_TASK_STACK_DEPTH   512U   // 约 2KB
#define INPUT_TASK_STACK_DEPTH    384U   // 约 1.5KB
#define GNSS_TASK_STACK_DEPTH     768U   // 约 3KB
#define SENSOR_TASK_STACK_DEPTH   512U   // 约 2KB
#define COMM_TASK_STACK_DEPTH     768U   // 约 3KB
#define STORAGE_TASK_STACK_DEPTH 1024U   // 约 4KB
#define UI_TASK_STACK_DEPTH      2048U   // 约 8KB


/*==========================================================
 * 三、任务优先级
 *
 * FreeRTOS 数值越大，优先级越高。
 *
 * 保持现有系统已经使用的优先级分配。
 *=========================================================*/

#define SYSTEM_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 2U)

#define INPUT_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 3U)

#define GNSS_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 4U)

#define SENSOR_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 2U)

#define COMM_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 4U)

#define STORAGE_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 1U)

#define UI_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 3U)


/*==========================================================
 * 四、消息队列容量
 *=========================================================*/

#define INPUT_QUEUE_LENGTH 16U     // 输入事件队列容量

#define POWER_QUEUE_LENGTH  8U     // 电源事件队列容量


/*==========================================================
 * 五、静态任务控制块
 *
 * StaticTask_t 保存 FreeRTOS 任务管理信息。
 *
 * 每个任务必须有独立的控制块。
 *=========================================================*/

static StaticTask_t s_systemTaskTCB;
static StaticTask_t s_inputTaskTCB;
static StaticTask_t s_gnssTaskTCB;
static StaticTask_t s_sensorTaskTCB;
static StaticTask_t s_commTaskTCB;
static StaticTask_t s_storageTaskTCB;
static StaticTask_t s_uiTaskTCB;


/*==========================================================
 * 六、静态任务栈
 *
 * 不使用 malloc 动态创建任务栈。
 *
 * 任务栈用于保存：
 * - 函数调用现场
 * - 局部变量
 * - CPU 上下文
 * - 中断切换现场
 *=========================================================*/

static StackType_t
    s_systemTaskStack[SYSTEM_TASK_STACK_DEPTH];

static StackType_t
    s_inputTaskStack[INPUT_TASK_STACK_DEPTH];

static StackType_t
    s_gnssTaskStack[GNSS_TASK_STACK_DEPTH];

static StackType_t
    s_sensorTaskStack[SENSOR_TASK_STACK_DEPTH];

static StackType_t
    s_commTaskStack[COMM_TASK_STACK_DEPTH];

static StackType_t
    s_storageTaskStack[STORAGE_TASK_STACK_DEPTH];

static StackType_t
    s_uiTaskStack[UI_TASK_STACK_DEPTH];


/*==========================================================
 * 七、任务句柄
 *
 * TaskHandle_t 是任务的引用。
 *
 * 任务创建成功后，
 * FreeRTOS 返回对应的 TaskHandle。
 *=========================================================*/

static TaskHandle_t s_systemTaskHandle = NULL;
static TaskHandle_t s_inputTaskHandle = NULL;
static TaskHandle_t s_gnssTaskHandle = NULL;
static TaskHandle_t s_sensorTaskHandle = NULL;
static TaskHandle_t s_commTaskHandle = NULL;
static TaskHandle_t s_storageTaskHandle = NULL;
static TaskHandle_t s_uiTaskHandle = NULL;


/*==========================================================
 * 八、普通物理输入队列
 *
 * InputTask：生产者
 * UITask   ：消费者
 *
 * 每个消息为 12 字节 UI 标准事件。
 *=========================================================*/

static StaticQueue_t s_inputQueueControl;

static uint8_t s_inputQueueStorage[
    INPUT_QUEUE_LENGTH *
    sizeof(ui_physical_input_event_t)
];

static QueueHandle_t s_inputQueueHandle = NULL;


/*==========================================================
 * 九、电源事件队列
 *
 * InputTask ：发送 POWER 输入
 * SystemTask：执行电源状态转换
 *
 * POWER 事件与普通 UI 事件分开传输。
 *=========================================================*/

static StaticQueue_t s_powerQueueControl;

static uint8_t s_powerQueueStorage[
    POWER_QUEUE_LENGTH *
    sizeof(PowerEvent_t)
];

static QueueHandle_t s_powerQueueHandle = NULL;


/*==========================================================
 * 十、系统事件组
 *
 * EventGroup 保存系统模块的状态标志。
 *
 * 例如：
 * APP_EVENT_DISPLAY_READY
 * APP_EVENT_GNSS_READY
 * APP_EVENT_GNSS_FIX
 *=========================================================*/

static StaticEventGroup_t
    s_systemEventGroupBuffer;

static EventGroupHandle_t
    s_systemEventGroup = NULL;


/*==========================================================
 * 十一、系统统计及 UI 回调
 *
 * 这些属于原有业务状态，
 * 与 BikeScope CPU 性能监测无关。
 *=========================================================*/

static volatile uint32_t
    s_systemHeartbeat = 0U;

static volatile uint32_t
    s_uiInputRejectedCount = 0U;

static AppUiInputHandler_t
    s_uiInputHandler = NULL;


/*==========================================================
 * 十二、任务入口函数声明
 *=========================================================*/

static void SystemTask(void *argument);
static void InputTask(void *argument);
static void GNSSTask(void *argument);
static void SensorTask(void *argument);
static void CommTask(void *argument);
static void StorageTask(void *argument);
static void UITask(void *argument);


/*==========================================================
 * 十三、App_RTOS_Init()
 *
 * 由系统启动流程调用。
 *
 * 本函数负责创建任务及通信对象，
 * 不直接启动 FreeRTOS 调度器。
 *=========================================================*/

void App_RTOS_Init(void)
{
    /* 一、初始化电源管理服务 */

    PowerService_Init();


    /* 二、创建电源事件队列 */

    s_powerQueueHandle = xQueueCreateStatic(
        POWER_QUEUE_LENGTH,
        sizeof(PowerEvent_t),
        s_powerQueueStorage,
        &s_powerQueueControl
    );

    configASSERT(s_powerQueueHandle != NULL);


    /* 三、创建普通输入队列 */

    s_inputQueueHandle = xQueueCreateStatic(
        INPUT_QUEUE_LENGTH,
        sizeof(ui_physical_input_event_t),
        s_inputQueueStorage,
        &s_inputQueueControl
    );

    configASSERT(s_inputQueueHandle != NULL);


    /* 四、创建系统 EventGroup */

    s_systemEventGroup = xEventGroupCreateStatic(
        &s_systemEventGroupBuffer
    );

    configASSERT(s_systemEventGroup != NULL);


    /* 五、创建 SystemTask */

    s_systemTaskHandle = xTaskCreateStatic(
        SystemTask,                    // 任务入口
        "System",                      // 任务名称
        SYSTEM_TASK_STACK_DEPTH,       // 栈容量
        NULL,                          // 任务参数
        SYSTEM_TASK_PRIORITY,          // 任务优先级
        s_systemTaskStack,             // 静态任务栈
        &s_systemTaskTCB               // 任务控制块
    );

    configASSERT(s_systemTaskHandle != NULL);


    /* 六、创建 InputTask */

    s_inputTaskHandle = xTaskCreateStatic(
        InputTask,
        "Input",
        INPUT_TASK_STACK_DEPTH,
        NULL,
        INPUT_TASK_PRIORITY,
        s_inputTaskStack,
        &s_inputTaskTCB
    );

    configASSERT(s_inputTaskHandle != NULL);


    /* 七、创建 GNSSTask */

    s_gnssTaskHandle = xTaskCreateStatic(
        GNSSTask,
        "GNSS",
        GNSS_TASK_STACK_DEPTH,
        NULL,
        GNSS_TASK_PRIORITY,
        s_gnssTaskStack,
        &s_gnssTaskTCB
    );

    configASSERT(s_gnssTaskHandle != NULL);


    /* 八、创建 SensorTask */

    s_sensorTaskHandle = xTaskCreateStatic(
        SensorTask,
        "Sensor",
        SENSOR_TASK_STACK_DEPTH,
        NULL,
        SENSOR_TASK_PRIORITY,
        s_sensorTaskStack,
        &s_sensorTaskTCB
    );

    configASSERT(s_sensorTaskHandle != NULL);


    /* 九、创建 CommTask */

    s_commTaskHandle = xTaskCreateStatic(
        CommTask,
        "Comm",
        COMM_TASK_STACK_DEPTH,
        NULL,
        COMM_TASK_PRIORITY,
        s_commTaskStack,
        &s_commTaskTCB
    );

    configASSERT(s_commTaskHandle != NULL);


    /* 十、创建 StorageTask */

    s_storageTaskHandle = xTaskCreateStatic(
        StorageTask,
        "Storage",
        STORAGE_TASK_STACK_DEPTH,
        NULL,
        STORAGE_TASK_PRIORITY,
        s_storageTaskStack,
        &s_storageTaskTCB
    );

    configASSERT(s_storageTaskHandle != NULL);


    /* 十一、创建 UITask */

    s_uiTaskHandle = xTaskCreateStatic(
        UITask,
        "UI",
        UI_TASK_STACK_DEPTH,
        NULL,
        UI_TASK_PRIORITY,
        s_uiTaskStack,
        &s_uiTaskTCB
    );

    configASSERT(s_uiTaskHandle != NULL);
}


/*==========================================================
 * 十四、SystemTask
 *
 * 功能：
 * 1. 维护系统 1Hz 心跳。
 * 2. 接收 POWER 电源事件。
 * 3. 调用 PowerService 执行状态转换。
 *
 * 不直接访问 LVGL。
 *=========================================================*/

static void SystemTask(void *argument)
{
    TickType_t lastHeartbeatTick;
    TickType_t heartbeatPeriod;
    TickType_t currentTick;
    TickType_t elapsedTicks;
    TickType_t elapsedPeriods;
    TickType_t waitTicks;

    PowerEvent_t powerEvent;

    (void)argument;

    /* 一、设置系统心跳周期 */

    heartbeatPeriod = pdMS_TO_TICKS(1000U);

    configASSERT(heartbeatPeriod > 0U);

    lastHeartbeatTick = xTaskGetTickCount();


    /* 二、进入系统任务主循环 */

    for (;;)
    {
        currentTick = xTaskGetTickCount();

        elapsedTicks = (TickType_t)(
            currentTick - lastHeartbeatTick
        );

        /* 三、检查是否达到心跳周期 */

        if (elapsedTicks >= heartbeatPeriod)
        {
            elapsedPeriods =
                elapsedTicks / heartbeatPeriod;

            s_systemHeartbeat +=
                (uint32_t)elapsedPeriods;

            lastHeartbeatTick +=
                elapsedPeriods * heartbeatPeriod;

            continue;
        }

        /* 四、计算下次心跳前剩余时间 */

        waitTicks =
            heartbeatPeriod - elapsedTicks;

        /*
         * 五、阻塞等待电源事件。
         *
         * 有 POWER 事件时立即唤醒。
         * 没有事件时等待到下一个心跳周期。
         */
        if (xQueueReceive(
                s_powerQueueHandle,
                &powerEvent,
                waitTicks
            ) == pdPASS)
        {
            (void)PowerService_HandleEvent(
                &powerEvent
            );
        }
    }
}


/*==========================================================
 * 十五、InputTask
 *
 * 每 10ms 扫描一次实体输入。
 *
 * InputService 负责：
 * 1. GPIO 状态读取。
 * 2. 按键消抖。
 * 3. 单击、长按、双击识别。
 * 4. 编码器旋转增量识别。
 *
 * 普通输入发送到 UITask。
 * POWER 输入发送到 SystemTask。
 *=========================================================*/

static void InputTask(void *argument)
{
    TickType_t lastWakeTime;
    TickType_t pollPeriodTicks;
    TickType_t nowTicks;

    uint32_t nowMs;

    (void)argument;

    /* 一、设置 10ms 扫描周期 */

    pollPeriodTicks = pdMS_TO_TICKS(10U);

    configASSERT(pollPeriodTicks > 0U);


    /* 二、初始化输入服务 */

    nowTicks = xTaskGetTickCount();

    nowMs = (uint32_t)(
        ((uint64_t)nowTicks * 1000ULL) /
        (uint64_t)configTICK_RATE_HZ
    );

    Input_ServiceInit(nowMs);


    /* 三、建立周期调度基准 */

    lastWakeTime = xTaskGetTickCount();


    /* 四、进入输入扫描循环 */

    for (;;)
    {
        nowTicks = xTaskGetTickCount();

        nowMs = (uint32_t)(
            ((uint64_t)nowTicks * 1000ULL) /
            (uint64_t)configTICK_RATE_HZ
        );

        /* 扫描输入并生成对应事件 */

        Input_ServicePoll(nowMs);

        /* 固定周期运行 */

        vTaskDelayUntil(
            &lastWakeTime,
            pollPeriodTicks
        );
    }
}


/**
 * @brief PU-252 GNSS 数据接收与解析任务
 *
 * @details
 * 1. 管理 USART2 Circular DMA 接收。
 * 2. 通过任务通知获取 DMA 接收事件。
 * 3. 批量读取原始 NMEA 数据。
 * 4. 执行 RMC/GGA 校验和解析。
 * 5. 发布 GNSS 定位状态快照。
 * 6. 维护 APP_EVENT_GNSS_FIX 系统状态。
 * 7. 在接收故障时清除定位有效性并恢复 DMA。
 *
 * 运行约束：
 * - 本任务是 GNSS 解码器的唯一写入者。
 * - 不在 UART 中断中解析 NMEA。
 * - 不在本任务中直接调用 LVGL API。
 * - 不使用动态内存分配。
 */
static void GNSSTask(void *argument)
{
    static uint8_t rxBuffer[512U];       // DMA 读取缓冲区
    static gnss_decoder_t decoder;       // NMEA 解析器实例

    gnss_snapshot_t snapshot;            // 本轮解析状态
    GNSS_DMA_Stats_t dmaStats;           // DMA 运行统计

    HAL_StatusTypeDef status;            // HAL 操作结果
    uint32_t receivedLength;             // 本次读取的数据长度
    uint32_t lastDroppedBytes = 0U;      // 上次接收丢失累计字节数
    uint32_t nowMs;                      // 当前系统运行时间

    const TickType_t waitTicks =
        pdMS_TO_TICKS(100U);             // 最长等待 100ms

    const TickType_t retryTicks =
        pdMS_TO_TICKS(200U);             // DMA 故障重试间隔

    (void)argument;                      // 当前无需任务参数

    configASSERT(waitTicks > 0U);
    configASSERT(retryTicks > 0U);

    /*------------------------------------------------------
     * 一、初始化 NMEA 解析器和 GNSS 状态服务
     *-----------------------------------------------------*/

    GNSS_Decoder_Init(&decoder);          // 清空解析器状态
    GnssService_Init();                  // 初始化任务安全状态快照

    /*------------------------------------------------------
     * 二、初始化 USART2 DMA 驱动
     *-----------------------------------------------------*/

    GNSS_DMA_Init(&huart2);              // 绑定 USART2

    App_RTOS_ClearSystemEvent(
        APP_EVENT_GNSS_READY |
        APP_EVENT_GNSS_FIX
    );

    /*------------------------------------------------------
     * 三、GNSS 任务主循环
     *-----------------------------------------------------*/

    for (;;)
    {
        /*--------------------------------------------------
         * 3.1 检查 DMA 接收错误
         *-------------------------------------------------*/

        if (GNSS_DMA_NeedsRecovery() != 0U)
        {
            GNSS_Decoder_Invalidate(
                &decoder
            );                          // 丢弃不可信的旧定位

            GnssService_Invalidate();   // 撤销已发布定位有效性

            App_RTOS_ClearSystemEvent(
                APP_EVENT_GNSS_READY |
                APP_EVENT_GNSS_FIX
            );

            status = GNSS_DMA_Recover(); // 在任务上下文恢复 DMA

            if (status != HAL_OK)
            {
                vTaskDelay(retryTicks);  // 避免连续高速重试
                continue;
            }

            App_RTOS_SetSystemEvent(
                APP_EVENT_GNSS_READY
            );
        }

        /*--------------------------------------------------
         * 3.2 首次启动或重新启动 DMA
         *-------------------------------------------------*/

        if (GNSS_DMA_IsRunning() == 0U)
        {
            GNSS_Decoder_Invalidate(
                &decoder
            );

            GnssService_Invalidate();

            App_RTOS_ClearSystemEvent(
                APP_EVENT_GNSS_READY |
                APP_EVENT_GNSS_FIX
            );

            status = GNSS_DMA_Start();   // 启动 Circular DMA + IDLE

            if (status != HAL_OK)
            {
                vTaskDelay(retryTicks);
                continue;
            }

            App_RTOS_SetSystemEvent(
                APP_EVENT_GNSS_READY
            );
        }

        /*--------------------------------------------------
         * 3.3 等待 UART IDLE / DMA HT / DMA TC 事件
         *
         * 中断通过 Task Notification 唤醒本任务。
         * 100ms 超时也会主动检查数据及定位有效期。
         *-------------------------------------------------*/

        (void)ulTaskNotifyTake(
            pdTRUE,
            waitTicks
        );

        /*--------------------------------------------------
         * 3.4 检查等待期间是否发生接收错误
         *-------------------------------------------------*/

        if ((GNSS_DMA_NeedsRecovery() != 0U) ||
            (GNSS_DMA_IsRunning() == 0U))
        {
            GNSS_Decoder_Invalidate(&decoder);
            GnssService_Invalidate();

            App_RTOS_ClearSystemEvent(
                APP_EVENT_GNSS_READY |
                APP_EVENT_GNSS_FIX
            );

            continue;
        }

        /*--------------------------------------------------
         * 3.5 分批读取 DMA 缓冲区并解析 NMEA
         *
         * 注意：
         * 每次 GNSS_DMA_Read() 不保证得到完整 NMEA 语句。
         * 解析器内部会保存跨批次的不完整语句，
         * 直到收到换行符才执行校验和字段解析。
         *-------------------------------------------------*/

        for (;;)
        {
            receivedLength = GNSS_DMA_Read(
                rxBuffer,
                sizeof(rxBuffer)
            );

            if (receivedLength == 0U)
            {
                break;                  // DMA 暂无新增数据
            }

            nowMs = HAL_GetTick();

            GNSS_Decoder_Feed(
                &decoder,              // 解码器状态
                rxBuffer,              // 本次新增字节
                receivedLength,        // 实际字节数量
                nowMs                  // 接收时间
            );
        }

        /*--------------------------------------------------
         * 3.6 检查 DMA 是否发生过接收数据丢失
         *
         * 如果出现积压丢弃，则取消当前定位有效状态。
         * 等待后续完整、校验正确的 RMC 重新建立定位。
         *-------------------------------------------------*/

        GNSS_DMA_GetStats(&dmaStats);

        if (dmaStats.dropped_bytes != lastDroppedBytes)
        {
            lastDroppedBytes = dmaStats.dropped_bytes;

            GNSS_Decoder_Invalidate(&decoder);
        }

        /*--------------------------------------------------
         * 3.7 执行定位数据新鲜度检查
         *
         * 连续超过 3 秒没有有效 RMC：
         * fix_valid = false
         *
         * GGA 高度也单独检查有效期，
         * 防止显示旧坐标或旧高度。
         *-------------------------------------------------*/

        nowMs = HAL_GetTick();

        GNSS_Decoder_Tick(
            &decoder,
            nowMs
        );

        /*--------------------------------------------------
         * 3.8 获取并发布最新定位快照
         *-------------------------------------------------*/

        GNSS_Decoder_GetSnapshot(
            &decoder,
            &snapshot
        );

        GnssService_Publish(
            &snapshot
        );

        /*--------------------------------------------------
         * 3.9 更新 FreeRTOS 系统定位状态
         *
         * GNSS_READY：DMA 接收链路正常。
         * GNSS_FIX：存在有效且未过期的 RMC 定位。
         *-------------------------------------------------*/

        if (snapshot.fix_valid)
        {
            App_RTOS_SetSystemEvent(
                APP_EVENT_GNSS_FIX
            );
        }
        else
        {
            App_RTOS_ClearSystemEvent(
                APP_EVENT_GNSS_FIX
            );
        }
    }
}


/**
 * @brief STM32 统一传感器采集任务
 *
 * @details
 * 本任务负责：
 * 1. BatteryService 电池测量。
 * 2. SensorService 气压和温度测量。
 * 3. 保持传感器统一调度周期。
 *
 * 本任务不直接访问 LVGL。
 *
 * 当前运行周期：100ms。
 */
static void SensorTask(void *argument)
{
    TickType_t lastWakeTime;                   // FreeRTOS 周期调度基准

    TickType_t pollPeriodTicks;                // 传感器任务调度周期

    uint32_t nowMs;                           // 当前系统运行时间

    (void)argument;                           // 当前任务无需外部参数


    /* ---------- 1. 设置传感器任务周期 ---------- */

    pollPeriodTicks = pdMS_TO_TICKS(100U);    // 每 100ms 调度一次

    configASSERT(pollPeriodTicks > 0U);       // 防止转换后的周期为 0


    /* ---------- 2. 初始化电池管理服务 ---------- */

    BatteryService_Init();                    // 初始化现有 ADC 电池服务


    /* ---------- 3. 初始化气压传感器服务 ---------- */

    SensorService_Init();                     // 初始化 SPL06 服务状态

    /*
     * 这里只初始化 SensorService 软件状态。
     *
     * 真正的 SPL06 I2C 探测和寄存器配置
     * 将由 SensorService_Process() 周期执行。
     *
     * 即使 SPL06 未焊接或者通信异常，
     * 也不会阻止其他 FreeRTOS 任务启动。
     */


    /* ---------- 4. 建立周期任务调度基准 ---------- */

    lastWakeTime = xTaskGetTickCount();        // 获取当前 RTOS Tick


    /* ---------- 5. 进入传感器采集主循环 ---------- */

    for (;;)
    {
        /* ---------- 5.1 获取当前时间 ---------- */

        nowMs = HAL_GetTick();                 // 读取系统单调毫秒计数


        /* ---------- 5.2 更新现有电池服务 ---------- */

        BatteryService_Process(nowMs);         // 执行 ADC 采样和电池状态更新


        /* ---------- 5.3 更新 SPL06 传感器服务 ---------- */

        SensorService_Process(nowMs);          // 检查初始化、采样、错误及数据超时


        /* ---------- 5.4 等待下一次周期 ---------- */

        vTaskDelayUntil(
            &lastWakeTime,                    // 上一次任务唤醒的基准 Tick
            pollPeriodTicks                   // 固定 100ms 调度周期
        );
    }
}


/*==========================================================
 * 十八、CommTask
 *
 * STM32 通信任务。
 *
 * 当前职责：
 * 1. 初始化 QSPI 资源通信系统。
 * 2. 周期处理资源安装协议。
 * 3. 接收来自通信中断的任务通知。
 *
 * 当前不运行 BikeScope 遥测。
 *
 * 后续 STM32 与 ESP32 的正式通信服务
 * 将在对应开发阶段加入。
 *=========================================================*/

static void CommTask(void *argument)
{
    (void)argument;

    /* 一、启动 QSPI 资源通信系统 */

    ResourceSystem_Start();


    /* 二、进入通信任务主循环 */

    for (;;)
    {
        /* 处理资源安装及维护协议 */

        ResourceSystem_Poll();

        /*
         * 等待通信事件。
         *
         * 当前保留 5ms 周期轮询，
         * 兼容原有 ResourceSystem 调度方式。
         */
        (void)ulTaskNotifyTake(
            pdTRUE,
            pdMS_TO_TICKS(5U)
        );
    }
}


/*==========================================================
 * 十九、StorageTask
 *
 * 后续负责：
 * 1. SDMMC 驱动调度。
 * 2. FatFs 文件系统。
 * 3. 骑行记录保存。
 * 4. 历史轨迹读取。
 * 5. 关机前数据同步。
 *
 * 当前没有正式存储业务请求。
 *=========================================================*/

static void StorageTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        /*
         * 没有存储请求时进入阻塞状态。
         *
         * portMAX_DELAY：
         * 在允许无限阻塞的配置下等待任务通知。
         */
        (void)ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY
        );

        /*
         * 后续接入正式 StorageService。
         */
    }
}


/*==========================================================
 * 二十、UITask
 *
 * 正式 LVGL 界面任务。
 *
 * 功能：
 * 1. 等待资源系统允许启动 UI。
 * 2. 启动 LVGL 界面。
 * 3. 注册物理输入回调。
 * 4. 发布 UI 就绪事件。
 * 5. 消费实体按键和编码器事件。
 * 6. 周期执行 LVGL。
 *
 * LVGL 操作统一由 UITask 执行。
 *=========================================================*/

static void UITask(void *argument)
{
    ui_physical_input_event_t event;    // 输入事件缓冲区

    uint32_t processedCount;            // 本轮处理事件数量

    uint32_t nextMs;                    // LVGL 建议等待时间

    TickType_t delayTicks;              // FreeRTOS 等待节拍

    (void)argument;


    /* 一、等待资源系统允许启动 UI */

    for (;;)
    {
        /*
         * 资源系统会决定：
         * 进入正常 UI 运行，
         * 还是继续保持资源维护模式。
         */
        while (!ResourceSystem_UiRequested())
        {
            vTaskDelay(
                pdMS_TO_TICKS(20U)
            );
        }

        /* 二、正式启动 UI */

        bool success = UI_BoardStart();

        /* 三、将 UI 启动结果反馈给资源系统 */

        ResourceSystem_UiResult(success);

        if (success)
        {
            break;
        }
    }


    /* 四、注册实体输入回调 */

    configASSERT(
        App_RTOS_RegisterUiInputHandler(
            UI_BoardInput
        ) == pdPASS
    );


    /* 五、发布显示系统就绪状态 */

    App_RTOS_SetSystemEvent(
        APP_EVENT_DISPLAY_READY
    );


    /* 六、进入 LVGL 主循环 */

    for (;;)
    {
        /*
         * 每一轮最多处理 8 条实体输入事件。
         *
         * 防止输入事件持续占用 UI Task，
         * 导致 LVGL 定时器长时间无法运行。
         */
        for (processedCount = 0U;
             processedCount < 8U;
             processedCount++)
        {
            /* 非阻塞获取输入事件 */

            if (App_RTOS_GetInputEvent(
                    &event,
                    0U
                ) != pdPASS)
            {
                break;
            }

            /* 在 UITask 中执行输入处理 */

            if (UI_BoardInput(&event) == false)
            {
                s_uiInputRejectedCount++;
            }
        }


        /* 七、执行 LVGL 定时器、动画和绘制 */

        nextMs = UI_BoardPoll();


        /*
         * 八、限制最大等待时间。
         *
         * 保留原工程 10ms 上限，
         * 兼顾动画更新和实体输入响应。
         */
        if ((nextMs == 0U) ||
            (nextMs > 10U))
        {
            nextMs = 10U;
        }


        /* 九、将毫秒转换成 FreeRTOS Tick */

        delayTicks = pdMS_TO_TICKS(nextMs);

        if (delayTicks == 0U)
        {
            delayTicks = 1U;
        }


        /* 十、让出 CPU */

        vTaskDelay(delayTicks);
    }
}


/*==========================================================
 * 二十一、UI 输入回调注册
 *
 * 只允许 UITask 注册 UI 事件处理函数。
 *
 * 防止其他 Task 绕过任务所有权，
 * 直接调用 LVGL 输入逻辑。
 *=========================================================*/

BaseType_t App_RTOS_RegisterUiInputHandler(
    AppUiInputHandler_t handler
)
{
    /* 一、检查回调地址 */

    if (handler == NULL)
    {
        return pdFAIL;
    }

    /* 二、检查 UI Task 是否已经创建 */

    if (s_uiTaskHandle == NULL)
    {
        return pdFAIL;
    }

    /* 三、检查是否由 UITask 调用 */

    if (xTaskGetCurrentTaskHandle() !=
        s_uiTaskHandle)
    {
        return pdFAIL;
    }

    /* 四、防止重复注册 */

    if (s_uiInputHandler != NULL)
    {
        return pdFAIL;
    }

    /* 五、保存回调函数 */

    s_uiInputHandler = handler;

    return pdPASS;
}


/*==========================================================
 * 二十二、UI 拒绝事件计数查询
 *=========================================================*/

uint32_t App_RTOS_GetUiInputRejectedCount(void)
{
    return s_uiInputRejectedCount;
}


/*==========================================================
 * 二十三、普通输入事件发送接口
 *
 * InputService 调用。
 *
 * 功能：
 * 1. 将输入事件转换为 12 字节 UI 标准事件。
 * 2. 拒绝非法输入。
 * 3. 对编码器大增量进行分包。
 * 4. 将事件写入 InputQueue。
 *=========================================================*/

BaseType_t App_RTOS_PostInputEvent(
    const AppInputEvent_t *event,
    TickType_t timeout
)
{
    ui_physical_input_event_t uiEvent;  // UI 标准事件

    AppInputEvent_t packetEvent;        // 编码器分包事件

    InputRouterResult_t result;         // 事件转换结果

    int32_t remaining;                  // 剩余编码器档位

    int32_t delta;                      // 当前分包档位

    int64_t magnitude64;                // 64 位安全绝对值

    uint32_t magnitude;                 // 增量绝对值

    uint32_t packetCount;               // 需要发送的分包数量

    const uint32_t maxDelta =
        (uint32_t)UI_PHYSICAL_ENCODER_MAX_DELTA;

    /* 一、检查事件指针和输入队列 */

    if ((event == NULL) ||
        (s_inputQueueHandle == NULL))
    {
        return pdFAIL;
    }


    /* 二、转换成 UI 标准事件 */

    result = Input_RouterConvert(
        event,
        &uiEvent
    );


    /* 三、普通合法事件直接发送 */

    if (result == INPUT_ROUTER_OK)
    {
        return xQueueSend(
            s_inputQueueHandle,
            &uiEvent,
            timeout
        );
    }


    /* 四、检查是否为编码器超范围事件 */

    if (result != INPUT_ROUTER_DELTA_RANGE)
    {
        return pdFAIL;
    }


    /* 五、只允许编码器旋转事件分包 */

    if ((event->type != APP_INPUT_ENCODER_CW) &&
        (event->type != APP_INPUT_ENCODER_CCW))
    {
        return pdFAIL;
    }


    /* 六、计算安全绝对值 */

    magnitude64 = (int64_t)event->value;

    if (magnitude64 < 0)
    {
        magnitude64 = -magnitude64;
    }

    magnitude = (uint32_t)magnitude64;

    if (magnitude == 0U)
    {
        return pdFAIL;
    }


    /* 七、计算分包数量 */

    packetCount = magnitude / maxDelta;

    if ((magnitude % maxDelta) != 0U)
    {
        packetCount++;
    }


    /* 八、限制单批事件数量 */

    if (packetCount > INPUT_QUEUE_LENGTH)
    {
        return pdFAIL;
    }


    /* 九、检查队列剩余容量 */

    if (uxQueueSpacesAvailable(
            s_inputQueueHandle
        ) < (UBaseType_t)packetCount)
    {
        return pdFAIL;
    }


    /* 十、准备分包数据 */

    packetEvent = *event;

    remaining = event->value;


    /* 十一、循环发送编码器分包 */

    while (remaining != 0)
    {
        if (remaining > (int32_t)maxDelta)
        {
            delta = (int32_t)maxDelta;
        }
        else if (remaining < -(int32_t)maxDelta)
        {
            delta = -(int32_t)maxDelta;
        }
        else
        {
            delta = remaining;
        }

        packetEvent.value = delta;


        /* 十二、转换当前分包 */

        result = Input_RouterConvert(
            &packetEvent,
            &uiEvent
        );

        if (result != INPUT_ROUTER_OK)
        {
            return pdFAIL;
        }


        /* 十三、发送当前分包 */

        if (xQueueSend(
                s_inputQueueHandle,
                &uiEvent,
                0U
            ) != pdPASS)
        {
            return pdFAIL;
        }


        /* 十四、更新剩余增量 */

        remaining -= delta;
    }

    return pdPASS;
}


/*==========================================================
 * 二十四、普通输入队列接收接口
 *
 * 由 UITask 调用。
 *=========================================================*/

BaseType_t App_RTOS_GetInputEvent(
    ui_physical_input_event_t *event,
    TickType_t timeout
)
{
    /* 一、检查输出缓冲区 */

    if (event == NULL)
    {
        return pdFAIL;
    }

    /* 二、检查输入队列 */

    if (s_inputQueueHandle == NULL)
    {
        return pdFAIL;
    }

    /* 三、从队列接收事件 */

    return xQueueReceive(
        s_inputQueueHandle,
        event,
        timeout
    );
}


/*==========================================================
 * 二十五、POWER 电源事件发送
 *
 * SHORT -> 切换亮屏/息屏
 * LONG  -> 请求有序关机
 *
 * POWER 事件不进入普通 UI 队列。
 *=========================================================*/

BaseType_t App_RTOS_PostPowerEvent(
    AppInputAction_t action,
    uint32_t timestamp_ms
)
{
    PowerEvent_t powerEvent;


    /* 一、检查电源队列 */

    if (s_powerQueueHandle == NULL)
    {
        return pdFAIL;
    }


    /* 二、转换 POWER 输入动作 */

    if (action == APP_INPUT_ACTION_SHORT)
    {
        powerEvent.action =
            POWER_ACTION_TOGGLE_DISPLAY;
    }
    else if (action == APP_INPUT_ACTION_LONG)
    {
        powerEvent.action =
            POWER_ACTION_REQUEST_SHUTDOWN;
    }
    else
    {
        return pdFAIL;
    }


    /* 三、保存原始事件时间戳 */

    powerEvent.timestamp_ms = timestamp_ms;


    /* 四、发送给 SystemTask */

    return xQueueSend(
        s_powerQueueHandle,
        &powerEvent,
        0U
    );
}


/*==========================================================
 * 二十六、系统 EventGroup 状态置位接口
 *=========================================================*/

void App_RTOS_SetSystemEvent(EventBits_t bits)
{
    if (s_systemEventGroup == NULL)
    {
        return;
    }

    (void)xEventGroupSetBits(
        s_systemEventGroup,
        bits
    );
}


/*==========================================================
 * 二十七、系统 EventGroup 状态清除接口
 *=========================================================*/

void App_RTOS_ClearSystemEvent(EventBits_t bits)
{
    if (s_systemEventGroup == NULL)
    {
        return;
    }

    (void)xEventGroupClearBits(
        s_systemEventGroup,
        bits
    );
}


/*==========================================================
 * 二十八、查询系统 EventGroup 状态
 *=========================================================*/

EventBits_t App_RTOS_GetSystemEvents(void)
{
    if (s_systemEventGroup == NULL)
    {
        return 0U;
    }

    return xEventGroupGetBits(
        s_systemEventGroup
    );
}


/*==========================================================
 * 二十九、GNSS 中断通知接口
 *
 * 由 USART2 / DMA ISR 调用。
 *
 * 使用 FreeRTOS 任务通知，
 * 唤醒 GNSSTask 处理已接收数据。
 *=========================================================*/

void App_RTOS_NotifyGNSSFromISR(
    BaseType_t *higherPriorityTaskWoken
)
{
    /* 一、检查 GNSS 任务句柄 */

    if (s_gnssTaskHandle == NULL)
    {
        return;
    }

    /* 二、发送中断任务通知 */

    vTaskNotifyGiveFromISR(
        s_gnssTaskHandle,
        higherPriorityTaskWoken
    );
}


/*==========================================================
 * 三十、通信中断通知接口
 *
 * 由 USART/DMA 中断调用，
 * 唤醒 CommTask。
 *=========================================================*/

void App_RTOS_NotifyCommFromISR(
    BaseType_t *higherPriorityTaskWoken
)
{
    /* 一、检查通信任务句柄 */

    if (s_commTaskHandle == NULL)
    {
        return;
    }

    /* 二、发送中断任务通知 */

    vTaskNotifyGiveFromISR(
        s_commTaskHandle,
        higherPriorityTaskWoken
    );
}


/*==========================================================
 * 三十一、系统心跳查询
 *
 * 返回 SystemTask 累计的 1Hz 心跳数量。
 *
 * 用于系统运行状态判断，
 * 与 FreeRTOS CPU 占用率统计无关。
 *=========================================================*/

uint32_t App_RTOS_GetSystemHeartbeat(void)
{
    return s_systemHeartbeat;
}
