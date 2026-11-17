/**
 * @file    altitude_service.h
 * @brief   公路车码表气压高度计算与校准服务接口
 *
 * @details
 * 本文件定义：
 * 1. 气压高度服务状态码。
 * 2. 高度数据快照结构体。
 * 3. 高度滤波、参考气压和海拔校准接口。
 * 4. 相对高度零点管理接口。
 *
 * 软件数据流：
 * SPL06 -> SensorService -> AltitudeService
 *
 * 线程约束：
 * 1. SensorTask 是高度服务唯一的数据生产者。
 * 2. UITask、RideService 等其他任务只允许读取快照。
 * 3. 改变校准参数的接口必须在 SensorTask 中执行。
 * 4. 后续 UI 校准命令应通过队列发送至 SensorTask。
 */

#ifndef ALTITUDE_SERVICE_H                    // 防止头文件重复包含
#define ALTITUDE_SERVICE_H                    // 定义头文件保护宏

#include <stdbool.h>                         // bool、true、false
#include <stdint.h>                          // uint32_t 等固定宽度类型

#ifdef __cplusplus                           // 兼容 C++ 调用
extern "C" {                                 // 使用 C 语言链接方式
#endif


/* ---------- 1. 高度服务公共配置 ---------- */

#define ALTITUDE_DEFAULT_REFERENCE_PA 101325.0f  // 默认标准海平面气压，单位 Pa

#define ALTITUDE_DATA_TIMEOUT_MS    1500U    // 超过 1500ms 未更新则数据失效


/* ---------- 2. 高度服务状态码 ---------- */

typedef enum
{
    ALTITUDE_STATUS_OK = 0,                  // 操作成功

    ALTITUDE_STATUS_NOT_READY,               // 高度服务尚未初始化

    ALTITUDE_STATUS_NO_DATA,                 // 没有可用的有效气压数据

    ALTITUDE_STATUS_INVALID_ARG,             // 输入参数不合法

    ALTITUDE_STATUS_PRESSURE_INVALID,        // 气压超出有效范围

    ALTITUDE_STATUS_SPIKE_REJECTED,          // 本次气压突变被暂时拒绝

    ALTITUDE_STATUS_MATH_ERROR               // 高度计算出现数学异常

} altitude_service_status_t;


/* ---------- 3. 高度校准模式 ---------- */

typedef enum
{
    ALTITUDE_CALIBRATION_STANDARD = 0,       // 使用默认标准海平面气压

    ALTITUDE_CALIBRATION_REFERENCE,          // 用户设置参考海平面气压

    ALTITUDE_CALIBRATION_KNOWN_HEIGHT        // 用户输入已知海拔进行校准

} altitude_calibration_mode_t;


/* ---------- 4. 高度服务状态快照 ---------- */

typedef struct
{
    float raw_pressure_pa;                   // 最近接受的原始补偿气压，单位 Pa

    float filtered_pressure_pa;              // 低通滤波后的气压，单位 Pa

    float reference_pressure_pa;             // 当前参考海平面气压，单位 Pa

    float altitude_m;                        // 根据参考气压计算的高度，单位 m

    float relative_altitude_m;               // 相对于零点的高度变化，单位 m

    uint32_t updated_ms;                     // 最近一次有效高度更新时间

    uint32_t sample_count;                   // 成功接收并处理的气压样本数

    uint32_t rejected_count;                 // 无效或异常气压被拒绝的次数

    uint32_t revision;                       // 状态快照修订号

    bool valid;                              // 当前高度数据是否有效

    bool relative_zero_valid;                // 相对高度零点是否已经建立

    altitude_calibration_mode_t mode;         // 当前高度校准模式

} altitude_service_snapshot_t;


/* ---------- 5. 初始化高度服务 ---------- */

/**
 * @brief 初始化高度服务
 *
 * @note
 * 由 SensorService_Init() 调用一次。
 *
 * 初始化后：
 * 参考气压为 101325 Pa。
 * 高度数据标记为无效。
 * 等待第一组 SPL06 气压数据。
 */
void AltitudeService_Init(void);


/* ---------- 6. 输入新的气压测量 ---------- */

/**
 * @brief 根据新气压更新滤波器和高度数据
 *
 * @param pressure_pa  SPL06 补偿气压，单位 Pa
 * @param sampled_at_ms 本次气压测量时间戳，单位 ms
 *
 * @return 高度服务状态码
 *
 * @note
 * 只能由 SensorTask 通过 SensorService 调用。
 * 不能在中断中调用。
 */
altitude_service_status_t AltitudeService_Update(
    float pressure_pa,
    uint32_t sampled_at_ms
);


/* ---------- 7. 周期维护高度有效性 ---------- */

/**
 * @brief 检查高度数据是否超时
 *
 * @param now_ms 当前系统毫秒时间
 *
 * @note
 * 由 SensorTask 周期调用。
 */
void AltitudeService_Tick(uint32_t now_ms);


/* ---------- 8. 设置参考海平面气压 ---------- */

/**
 * @brief 使用已知海平面气压进行高度校准
 *
 * @param reference_pa 海平面参考气压，单位 Pa
 *
 * @return 高度服务状态码
 *
 * @note
 * 当前无有效测量时允许预先设置参考气压。
 * 有有效测量时立即重新计算高度，并重设相对高度零点。
 * 必须在 SensorTask 中调用。
 */
altitude_service_status_t AltitudeService_SetReferencePressure(
    float reference_pa
);


/* ---------- 9. 使用已知海拔进行校准 ---------- */

/**
 * @brief 根据当前气压和已知海拔反算参考气压
 *
 * @param known_altitude_m 已知海拔，单位 m
 *
 * @return 高度服务状态码
 *
 * @note
 * 必须已经获得有效气压。
 * 校准成功后当前相对高度重设为 0。
 * 必须在 SensorTask 中调用。
 */
altitude_service_status_t AltitudeService_CalibrateKnownAltitude(
    float known_altitude_m
);


/* ---------- 10. 重新设置相对高度零点 ---------- */

/**
 * @brief 将当前位置作为相对高度零点
 *
 * @return 高度服务状态码
 *
 * @note
 * 不改变绝对气压高度，也不改变参考海平面气压。
 * 必须在 SensorTask 中调用。
 */
altitude_service_status_t AltitudeService_ResetRelativeZero(void);


/* ---------- 11. 获取最新高度状态快照 ---------- */

/**
 * @brief 获取线程安全的高度数据快照
 *
 * @param out 用户提供的输出结构体
 *
 * @retval true  成功取得快照
 * @retval false 参数错误或服务尚未初始化
 *
 * @note
 * 允许在普通 FreeRTOS 任务中调用。
 * 不允许在中断上下文调用。
 * 使用高度前必须检查 out->valid。
 */
bool AltitudeService_GetSnapshot(
    altitude_service_snapshot_t *out
);


#ifdef __cplusplus                           // 结束 C++ 兼容定义
}                                              // 结束 extern "C"
#endif

#endif /* ALTITUDE_SERVICE_H */              // 结束头文件保护
