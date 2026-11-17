/**
 * @file    sensor_service.h
 * @brief   公路车码表传感器服务接口
 *
 * @details
 * 本阶段负责：
 * 1. SPL06 初始化与周期采样。
 * 2. 气压及温度测量状态管理。
 * 3. I2C 故障检测和自动重试。
 * 4. 通过 FreeRTOS 临界区发布一致性快照。
 *
 * 后续扩展：
 * 1. 气压高度计算。
 * 2. 气压高度滤波。
 * 3. GNSS 与气压高度融合。
 * 4. 累计爬升计算。
 */

#ifndef SENSOR_SERVICE_H                       // 防止重复包含
#define SENSOR_SERVICE_H                       // 头文件保护宏

#include "bsp_spl06.h"                       // SPL06 驱动状态定义
#include <stdbool.h>                         // 布尔类型
#include <stdint.h>                          // 固定宽度整数类型

#ifdef __cplusplus                           // 兼容 C++ 编译环境
extern "C" {                                 // 使用 C 链接规则
#endif


/* ---------- 1. 传感器采样与恢复参数 ---------- */

#define SENSOR_SERVICE_RETRY_MS       500U    // 初始化失败后每 500ms 重试

#define SENSOR_SERVICE_TIMEOUT_MS     1500U   // 超过 1.5 秒没有新测量则失效


/* ---------- 2. 传感器状态快照 ---------- */

typedef struct
{
    uint32_t revision;                       // 状态修订号

    uint32_t sample_count;                   // 成功采样累计次数

    uint32_t error_count;                    // 初始化或采样失败累计次数

    uint32_t last_valid_ms;                  // 最近一次有效测量时间戳

    uint8_t chip_id;                         // SPL06 产品及修订版本 ID

    uint8_t i2c_address;                     // 实际 7 位 I2C 地址

    float pressure_pa;                       // 当前补偿气压，单位 Pa

    float temperature_c;                     // 当前补偿温度，单位 °C

    bool spl06_ready;                        // SPL06 初始化是否成功

    bool measurement_valid;                  // 气压与温度是否有效

    bsp_spl06_status_t last_status;          // 最近一次操作返回状态

} sensor_service_snapshot_t;


/* ---------- 3. 传感器服务公开接口 ---------- */

/**
 * @brief 初始化传感器服务
 *
 * @note 仅在 SensorTask 中调用一次。
 */
void SensorService_Init(void);


/**
 * @brief 执行传感器服务周期处理
 *
 * @param now_ms 当前系统运行时间，单位毫秒
 *
 * @note
 * 在 SensorTask 中每 100ms 调用一次。
 */
void SensorService_Process(uint32_t now_ms);


/**
 * @brief 获取传感器最新状态快照
 *
 * @param out 输出缓冲区
 *
 * @retval true  成功复制快照
 * @retval false 服务未初始化或参数错误
 *
 * @note
 * 只能在 FreeRTOS 任务上下文调用，不能在 ISR 中使用。
 */
bool SensorService_GetSnapshot(
    sensor_service_snapshot_t *out
);


#ifdef __cplusplus                           // 结束 C++ 兼容声明
}                                              // 结束 extern "C"
#endif

#endif /* SENSOR_SERVICE_H */                // 结束头文件保护