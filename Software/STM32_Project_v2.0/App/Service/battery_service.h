#ifndef BATTERY_SERVICE_H                         // 防止头文件被重复包含
#define BATTERY_SERVICE_H                         // 定义头文件保护宏

#include <stdbool.h>                             // 提供 bool、true、false
#include <stdint.h>                              // 提供固定宽度整数类型

#ifdef __cplusplus                               // 兼容 C++ 工程
extern "C" {                                     // 按 C 语言链接规则导出函数
#endif

/* 电池 ADC 采样周期：1000ms，即每秒一次 */
#define BATTERY_SERVICE_SAMPLE_PERIOD_MS (1000U)

/**
 * @brief 电池外部供电与充电指示状态
 *
 * @note
 * CHG# 未拉低不能证明电池已经充满。
 * 因此 EXTERNAL 只表示外部输入有效，
 * 不额外推断充电是否已经完成。
 */
typedef enum
{
    BATTERY_POWER_UNKNOWN = 0,                   // 尚未稳定识别当前供电状态

    BATTERY_POWER_BATTERY,                       // 未检测到有效外部输入电源

    BATTERY_POWER_EXTERNAL,                      // 外部输入有效，但充电指示未拉低

    BATTERY_POWER_CHARGING                       // 外部输入有效，且充电指示已拉低

} battery_power_state_t;


/**
 * @brief 电池管理服务的完整状态快照
 *
 * @details
 * 快照是一次完整的电池状态记录。
 *
 * Sensor Task 负责更新。
 * UI Task 及其他任务通过 GetSnapshot() 读取副本。
 *
 * 不允许其他任务直接修改服务内部状态。
 */
typedef struct
{
    uint32_t revision;                           // 状态版本号，数据更新时递增

    uint32_t battery_raw_mv;                     // 最新一次 ADC 测量的电池电压，单位 mV

    uint32_t battery_filtered_mv;                // 经过软件滤波的电池电压，单位 mV

    uint32_t last_valid_ms;                      // 最近一次有效电压采样的时间戳

    uint32_t error_count;                        // ADC 采样失败或数据异常的累计次数

    uint8_t percent;                             // 0～100，使用电压估算的剩余电量百分比

    bool voltage_valid;                          // 电池电压有效且未超过数据有效期

    bool external_power;                         // 当前检测到有效外部输入电源

    bool charging;                               // BQ24075 当前正在输出充电指示

    bool low_battery;                            // 电池电压进入低电量提醒区间

    bool critical_battery;                       // 电池电压进入严重低电量提醒区间

    battery_power_state_t power_state;           // 当前外部供电与充电状态

} battery_service_snapshot_t;


/**
 * @brief 初始化电池管理服务
 *
 * @note
 * 只能在 Sensor Task 中调用。
 * 调用前必须已经完成 CubeMX ADC1/GPIO 初始化。
 *
 * ADC 校准失败不会使整个系统停止。
 * Process() 会按采样周期重新尝试初始化。
 */
void BatteryService_Init(void);


/**
 * @brief 执行电池管理服务的周期处理
 *
 * @param now_ms 当前 HAL 毫秒时间戳
 *
 * @note
 * 建议由 Sensor Task 每 100ms 调用一次。
 *
 * 内部会根据独立时间戳判断：
 * - 是否应当进行 ADC 采样
 * - GPIO 充电状态是否稳定
 * - 当前电池数据是否过期
 *
 * 只能由 Sensor Task 调用，禁止 ISR 调用。
 */
void BatteryService_Process(uint32_t now_ms);


/**
 * @brief 读取电池管理服务的最新完整快照
 *
 * @param out 用于接收电池状态的结构体地址
 *
 * @retval true  成功复制状态
 * @retval false 参数无效或服务尚未初始化
 *
 * @note
 * 本函数使用 FreeRTOS 临界区保护结构体复制。
 * 可由正常运行的 FreeRTOS 任务调用。
 * 不允许从中断中调用。
 *
 * voltage_valid == false 时，百分比不能作为
 * 当前有效电量使用。
 */
bool BatteryService_GetSnapshot(
    battery_service_snapshot_t *out
);


#ifdef __cplusplus                               // 兼容 C++ 编译器
}                                               // 结束 C 语言链接声明
#endif

#endif                                          // BATTERY_SERVICE_H
