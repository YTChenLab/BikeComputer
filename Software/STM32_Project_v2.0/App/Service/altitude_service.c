/**
 * @file    altitude_service.c
 * @brief   公路车码表气压高度计算、滤波与校准服务
 *
 * @details
 * 本文件实现：
 * 1. 标准大气模型气压高度计算。
 * 2. 基于采样时间间隔的一阶 IIR 气压滤波。
 * 3. 气压无效值及突变数据处理。
 * 4. 参考海平面气压设置。
 * 5. 已知海拔反向校准。
 * 6. 相对高度零点建立和重置。
 * 7. 数据超时失效管理。
 * 8. FreeRTOS 多任务安全快照读取。
 *
 * 设计原则：
 * 1. 本服务不直接调用 SPL06 驱动。
 * 2. 本服务不创建 FreeRTOS 任务。
 * 3. 所有状态修改都由 SensorTask 串行执行。
 * 4. 其他任务通过快照读取高度数据。
 * 5. 不在临界区内执行 powf() 等较耗时计算。
 */

#include "altitude_service.h"                // 本模块公共接口

#include "FreeRTOS.h"                        // FreeRTOS 核心接口
#include "task.h"                            // 临界区保护接口

#include <math.h>                            // powf()、fabsf()、isfinite()
#include <string.h>                          // memset() 数据初始化


/* ---------- 1. 标准大气模型参数 ---------- */

#define ALTITUDE_MODEL_HEIGHT_M      44330.0f     // 标准大气高度比例系数

#define ALTITUDE_MODEL_EXPONENT      0.19029495f  // 气压比换算高度的指数


/* ---------- 2. 气压输入有效范围 ---------- */

#define ALTITUDE_PRESSURE_MIN_PA     30000.0f     // 最低允许环境气压

#define ALTITUDE_PRESSURE_MAX_PA     110000.0f    // 最高允许环境气压


/* ---------- 3. 参考海平面气压范围 ---------- */

#define ALTITUDE_REFERENCE_MIN_PA    80000.0f     // 最小允许参考气压

#define ALTITUDE_REFERENCE_MAX_PA    110000.0f    // 最大允许参考气压


/* ---------- 4. 已知海拔校准范围 ---------- */

#define ALTITUDE_KNOWN_MIN_M          (-500.0f)   // 已知海拔下限

#define ALTITUDE_KNOWN_MAX_M          9000.0f     // 已知海拔上限


/* ---------- 5. 气压滤波参数 ---------- */

#define ALTITUDE_FILTER_TAU_MS        1500.0f     // 一阶低通滤波时间常数

#define ALTITUDE_FILTER_RESET_GAP_MS  5000U       // 数据中断超过 5 秒时重建滤波状态


/* ---------- 6. 气压异常值处理参数 ---------- */

#define ALTITUDE_SPIKE_THRESHOLD_PA   150.0f      // 相对滤波值超过 150Pa 视为大幅突变

#define ALTITUDE_SPIKE_MATCH_PA       80.0f       // 连续异常值之间允许的差异

#define ALTITUDE_SPIKE_CONFIRM_COUNT  3U          // 三次相近异常读数确认新气压


/* ---------- 7. 高度服务内部状态 ---------- */

static altitude_service_snapshot_t
    s_altitude_snapshot;                            // 对外提供的最新高度快照

static bool s_initialized = false;                  // 高度服务初始化标志

static bool s_has_sample = false;                   // 是否曾经接收过气压样本

static float s_filtered_pressure_pa = 0.0f;         // 滤波器内部气压状态

static float s_relative_zero_m = 0.0f;              // 当前相对高度零点

static uint32_t s_last_sample_ms = 0U;              // 最近一次接受样本的时间

static float s_spike_candidate_pa = 0.0f;           // 待确认的异常气压候选值

static uint8_t s_spike_count = 0U;                  // 连续相近异常气压次数


/* ---------- 8. 将气压转换为标准大气模型高度 ---------- */

static float AltitudeService_CalculateHeight(
    float pressure_pa,
    float reference_pa
)
{
    float ratio;                                    // 当前气压与参考气压的比值

    float height;                                   // 最终计算的气压高度

    /* ---------- 8.1 计算无量纲气压比 ---------- */

    ratio = pressure_pa / reference_pa;             // 将实测气压除以海平面参考气压

    /* ---------- 8.2 应用标准大气模型 ---------- */

    height = ALTITUDE_MODEL_HEIGHT_M *              // 标准大气模型高度系数
             (1.0f - powf(                          // 根据气压比计算高度变化
                 ratio,                             // 当前气压与参考气压的比值
                 ALTITUDE_MODEL_EXPONENT));         // 标准大气模型指数

    return height;                                  // 返回计算高度，单位 m
}


/* ---------- 9. 初始化高度服务 ---------- */

void AltitudeService_Init(void)
{
    /* ---------- 9.1 清空滤波器内部状态 ---------- */

    s_initialized = false;                          // 初始化期间禁止读取服务

    s_has_sample = false;                           // 当前没有历史气压样本

    s_filtered_pressure_pa = 0.0f;                   // 清空滤波器气压状态

    s_relative_zero_m = 0.0f;                        // 清空相对高度零点

    s_last_sample_ms = 0U;                          // 清空历史采样时间

    s_spike_candidate_pa = 0.0f;                     // 清空异常值候选气压

    s_spike_count = 0U;                             // 清空连续异常计数


    /* ---------- 9.2 初始化共享高度快照 ---------- */

    taskENTER_CRITICAL();                           // 进入临界区，保护共享状态

    memset(
        &s_altitude_snapshot,                       // 高度快照起始地址
        0,                                          // 将全部字段清零
        sizeof(s_altitude_snapshot)                 // 清空完整结构体
    );

    s_altitude_snapshot.reference_pressure_pa =
        ALTITUDE_DEFAULT_REFERENCE_PA;              // 默认使用 101325Pa 参考气压

    s_altitude_snapshot.mode =
        ALTITUDE_CALIBRATION_STANDARD;              // 初始校准模式为标准气压

    s_altitude_snapshot.valid = false;              // 尚未采集，不允许使用高度

    s_altitude_snapshot.relative_zero_valid =
        false;                                      // 尚未建立相对高度零点

    s_initialized = true;                           // 高度服务软件初始化完成

    taskEXIT_CRITICAL();                            // 退出临界区
}


/* ---------- 10. 更新气压滤波和高度 ---------- */

altitude_service_status_t AltitudeService_Update(
    float pressure_pa,
    uint32_t sampled_at_ms
)
{
    uint32_t deltaMs = 0U;                          // 相邻有效样本的时间间隔

    float alpha = 1.0f;                             // 当前滤波系数

    float pressureDifference;                        // 原始气压与滤波气压的差值

    float filteredPressure;                          // 本轮计算得到的滤波气压

    float altitude;                                  // 本轮计算得到的绝对气压高度

    float relativeAltitude;                          // 本轮计算得到的相对高度

    bool resetFilter;                                // 是否需要重建滤波器状态


    /* ---------- 10.1 检查高度服务是否初始化 ---------- */

    if (!s_initialized)                              // 服务尚未建立内部状态
    {
        return ALTITUDE_STATUS_NOT_READY;            // 禁止在初始化前处理气压
    }


    /* ---------- 10.2 检查新气压是否有效 ---------- */

    if (!isfinite(pressure_pa) ||                    // 禁止 NaN 或无穷大
        pressure_pa < ALTITUDE_PRESSURE_MIN_PA ||    // 气压低于允许范围
        pressure_pa > ALTITUDE_PRESSURE_MAX_PA)      // 气压高于允许范围
    {
        taskENTER_CRITICAL();                        // 进入共享状态保护区

        s_altitude_snapshot.rejected_count++;        // 记录一次无效气压输入

        s_altitude_snapshot.revision++;              // 更新状态修订号

        taskEXIT_CRITICAL();                         // 退出临界区

        return ALTITUDE_STATUS_PRESSURE_INVALID;     // 本次气压不参与计算
    }


    /* ---------- 10.3 防止同一时间戳的样本重复处理 ---------- */

    if (s_has_sample &&                              // 之前已经收到过有效气压
        sampled_at_ms == s_last_sample_ms)            // 新样本时间戳与上次相同
    {
        return ALTITUDE_STATUS_NO_DATA;              // 不重复更新滤波器
    }


    /* ---------- 10.4 判断滤波器是否需要重新建立 ---------- */

    resetFilter = !s_has_sample ||                    // 第一组有效气压直接作为滤波初值
                  !s_altitude_snapshot.valid;         // 之前高度失效时重新建立滤波状态

    if (s_has_sample)                                // 只有存在历史样本才计算时间差
    {
        deltaMs = (uint32_t)(sampled_at_ms -
                             s_last_sample_ms);     // 无符号减法支持毫秒计数回绕

        if (deltaMs > ALTITUDE_FILTER_RESET_GAP_MS)  // 两次有效样本间隔超过 5 秒
        {
            resetFilter = true;                      // 放弃过期滤波状态
        }
    }


    /* ---------- 10.5 初始化或恢复滤波器 ---------- */

    if (resetFilter)                                 // 没有可用的连续历史数据
    {
        filteredPressure = pressure_pa;              // 直接使用当前气压重建滤波状态

        s_spike_count = 0U;                          // 清除历史异常候选次数

        s_spike_candidate_pa = 0.0f;                  // 清除历史异常候选气压
    }

    /* ---------- 10.6 检查正常采样过程中的气压突变 ---------- */

    else
    {
        pressureDifference =
            pressure_pa - s_filtered_pressure_pa;   // 比较原始气压与当前滤波状态

        if (fabsf(pressureDifference) >
            ALTITUDE_SPIKE_THRESHOLD_PA)            // 气压变化超过设定突变阈值
        {
            /* ---------- 10.6.1 判断新异常值是否与上次相近 ---------- */

            if (s_spike_count == 0U ||                // 当前尚无待确认的异常气压
                fabsf(pressure_pa -
                      s_spike_candidate_pa) >
                ALTITUDE_SPIKE_MATCH_PA)            // 新旧异常气压不属于同一组
            {
                s_spike_count = 1U;                  // 重新开始累计异常候选次数
            }
            else
            {
                if (s_spike_count <
                    ALTITUDE_SPIKE_CONFIRM_COUNT)   // 避免计数器无意义增长
                {
                    s_spike_count++;                 // 连续相近异常值累计加一
                }
            }

            s_spike_candidate_pa = pressure_pa;      // 保存本次异常气压供下次比较

            /* ---------- 10.6.2 未达到确认次数时拒绝突变 ---------- */

            if (s_spike_count <
                ALTITUDE_SPIKE_CONFIRM_COUNT)       // 尚不足以证明气压发生真实变化
            {
                taskENTER_CRITICAL();                // 进入快照状态保护区

                s_altitude_snapshot.rejected_count++; // 记录一次被过滤的异常气压

                s_altitude_snapshot.revision++;      // 记录快照状态变化

                taskEXIT_CRITICAL();                 // 退出临界区

                return ALTITUDE_STATUS_SPIKE_REJECTED; // 暂时保持上一组高度
            }

            /* ---------- 10.6.3 连续三次确认后接受新气压 ---------- */

            filteredPressure = pressure_pa;          // 认为真实压力环境已经变化

            s_spike_count = 0U;                      // 清空突变确认计数

            s_spike_candidate_pa = 0.0f;             // 清空异常候选气压
        }

        /* ---------- 10.7 正常气压变化执行 IIR 低通滤波 ---------- */

        else
        {
            alpha = (float)deltaMs /                 // 使用相邻样本的真实时间间隔
                    (ALTITUDE_FILTER_TAU_MS +       // 时间常数越大，输出越平稳
                     (float)deltaMs);               // 确保滤波系数始终在 0 到 1 之间

            filteredPressure =
                s_filtered_pressure_pa +            // 从上一轮滤波气压开始
                alpha * pressureDifference;          // 按滤波系数吸收本次气压变化

            s_spike_count = 0U;                      // 正常气压变化清除异常候选

            s_spike_candidate_pa = 0.0f;             // 清除异常候选气压
        }
    }


    /* ---------- 10.8 将滤波气压转换为高度 ---------- */

    altitude = AltitudeService_CalculateHeight(
        filteredPressure,                            // 本轮滤波后的气压
        s_altitude_snapshot.reference_pressure_pa    // 当前海平面参考气压
    );

    if (!isfinite(altitude))                         // 检查高度计算结果是否有效
    {
        taskENTER_CRITICAL();                        // 保护共享计数器

        s_altitude_snapshot.rejected_count++;        // 记录一次高度计算失败

        s_altitude_snapshot.revision++;              // 更新状态修订号

        taskEXIT_CRITICAL();                         // 退出临界区

        return ALTITUDE_STATUS_MATH_ERROR;           // 不发布无效高度
    }


    /* ---------- 10.9 首次有效测量建立相对高度零点 ---------- */

    if (!s_altitude_snapshot.relative_zero_valid)    // 尚未记录相对高度参考点
    {
        s_relative_zero_m = altitude;                // 将当前计算高度设为零点
    }

    relativeAltitude = altitude - s_relative_zero_m; // 计算当前位置相对零点的高度差


    /* ---------- 10.10 更新滤波器内部状态 ---------- */

    s_filtered_pressure_pa = filteredPressure;        // 保存本轮滤波结果

    s_last_sample_ms = sampled_at_ms;                 // 保存本轮真实采样时间

    s_has_sample = true;                             // 标记已经接收有效气压


    /* ---------- 10.11 发布新的高度状态快照 ---------- */

    taskENTER_CRITICAL();                            // 保证其他任务读取到一致的数据

    s_altitude_snapshot.raw_pressure_pa =
        pressure_pa;                                 // 更新本次接受的气压

    s_altitude_snapshot.filtered_pressure_pa =
        filteredPressure;                            // 更新滤波后的气压

    s_altitude_snapshot.altitude_m =
        altitude;                                    // 更新绝对气压高度

    s_altitude_snapshot.relative_altitude_m =
        relativeAltitude;                            // 更新相对高度

    s_altitude_snapshot.updated_ms =
        sampled_at_ms;                               // 更新最近成功采样时间

    s_altitude_snapshot.sample_count++;               // 累计成功处理的气压样本

    s_altitude_snapshot.relative_zero_valid = true;   // 相对高度零点已经有效

    s_altitude_snapshot.valid = true;                 // 本轮高度数据可以使用

    s_altitude_snapshot.revision++;                   // 发布新的状态修订号

    taskEXIT_CRITICAL();                             // 退出临界区

    return ALTITUDE_STATUS_OK;                       // 返回高度更新成功
}


/* ---------- 11. 周期检查高度数据有效期 ---------- */

void AltitudeService_Tick(uint32_t now_ms)
{
    /* ---------- 11.1 检查高度服务是否初始化 ---------- */

    if (!s_initialized)                              // 高度服务尚未初始化
    {
        return;                                      // 不处理未建立的数据状态
    }


    /* ---------- 11.2 根据最后一次有效采样时间检查超时 ---------- */

    taskENTER_CRITICAL();                            // 保护共享快照

    if (s_altitude_snapshot.valid &&                 // 只检查当前标记有效的数据
        (uint32_t)(now_ms -
        s_altitude_snapshot.updated_ms) >
        ALTITUDE_DATA_TIMEOUT_MS)                     // 超过 1500ms 没有新高度
    {
        s_altitude_snapshot.valid = false;           // 撤销过期高度的有效标志

        s_altitude_snapshot.revision++;              // 记录一次有效性状态变化
    }

    taskEXIT_CRITICAL();                             // 退出临界区
}


/* ---------- 12. 设置参考海平面气压 ---------- */

altitude_service_status_t AltitudeService_SetReferencePressure(
    float reference_pa
)
{
    float newAltitude = 0.0f;                        // 使用新参考气压重新计算的高度


    /* ---------- 12.1 检查高度服务是否初始化 ---------- */

    if (!s_initialized)                              // 尚未执行高度服务初始化
    {
        return ALTITUDE_STATUS_NOT_READY;            // 禁止修改未初始化服务
    }


    /* ---------- 12.2 检查参考气压是否合法 ---------- */

    if (!isfinite(reference_pa) ||                   // 不允许非有限浮点数
        reference_pa < ALTITUDE_REFERENCE_MIN_PA ||  // 参考气压不得过低
        reference_pa > ALTITUDE_REFERENCE_MAX_PA)    // 参考气压不得过高
    {
        return ALTITUDE_STATUS_INVALID_ARG;          // 拒绝非法参考气压
    }


    /* ---------- 12.3 已有有效气压时立即重新计算高度 ---------- */

    if (s_altitude_snapshot.valid)                   // 当前拥有有效的滤波气压
    {
        newAltitude = AltitudeService_CalculateHeight(
            s_filtered_pressure_pa,                  // 使用当前滤波后的真实气压
            reference_pa                             // 使用新的海平面参考气压
        );

        if (!isfinite(newAltitude))                  // 防止产生无效高度
        {
            return ALTITUDE_STATUS_MATH_ERROR;       // 本次校准不改变历史状态
        }
    }


    /* ---------- 12.4 保存新的参考气压和校准模式 ---------- */

    taskENTER_CRITICAL();                            // 开始更新共享校准状态

    s_altitude_snapshot.reference_pressure_pa =
        reference_pa;                                // 保存用户设置的参考气压

    s_altitude_snapshot.mode =
        ALTITUDE_CALIBRATION_REFERENCE;              // 标记参考气压校准模式


    /* ---------- 12.5 有效数据存在时同步更新高度 ---------- */

    if (s_altitude_snapshot.valid)                   // 当前气压数据可以用于校准
    {
        s_altitude_snapshot.altitude_m =
            newAltitude;                             // 使用新参考气压更新绝对高度

        s_relative_zero_m = newAltitude;             // 将校准时刻设为新的相对零点

        s_altitude_snapshot.relative_altitude_m =
            0.0f;                                   // 校准位置的相对高度为 0

        s_altitude_snapshot.relative_zero_valid =
            true;                                   // 标记相对高度零点有效
    }
    else
    {
        s_altitude_snapshot.relative_zero_valid =
            false;                                  // 等待下一组有效气压建立零点
    }

    s_altitude_snapshot.revision++;                   // 更新状态修订号

    taskEXIT_CRITICAL();                             // 结束共享状态保护

    return ALTITUDE_STATUS_OK;                       // 参考气压设置成功
}


/* ---------- 13. 根据已知海拔校准高度 ---------- */

altitude_service_status_t AltitudeService_CalibrateKnownAltitude(
    float known_altitude_m
)
{
    float base;                                      // 反向气压计算中的无量纲底数

    float referencePressure;                         // 反算得到的海平面参考气压


    /* ---------- 13.1 检查服务初始化及测量有效性 ---------- */

    if (!s_initialized)                              // 高度服务尚未初始化
    {
        return ALTITUDE_STATUS_NOT_READY;            // 禁止执行校准
    }

    if (!s_altitude_snapshot.valid)                  // 目前没有有效气压测量
    {
        return ALTITUDE_STATUS_NO_DATA;              // 不能使用过期气压反算参考值
    }


    /* ---------- 13.2 检查用户输入的已知海拔 ---------- */

    if (!isfinite(known_altitude_m) ||                // 拒绝 NaN 和无穷大
        known_altitude_m < ALTITUDE_KNOWN_MIN_M ||   // 不允许低于校准范围
        known_altitude_m > ALTITUDE_KNOWN_MAX_M)     // 不允许高于校准范围
    {
        return ALTITUDE_STATUS_INVALID_ARG;          // 返回输入参数错误
    }


    /* ---------- 13.3 计算海拔反算公式的底数 ---------- */

    base =
        1.0f -
        known_altitude_m / ALTITUDE_MODEL_HEIGHT_M;  // 计算 1 - h / 44330

    if (base <= 0.0f)                                // 防止浮点幂运算出现非法底数
    {
        return ALTITUDE_STATUS_MATH_ERROR;           // 拒绝本次校准
    }


    /* ---------- 13.4 通过已知高度反算海平面参考气压 ---------- */

    referencePressure =
        s_filtered_pressure_pa /                     // 使用最新滤波气压
        powf(
            base,                                    // 标准大气模型反算底数
            1.0f / ALTITUDE_MODEL_EXPONENT           // 使用高度指数的倒数
        );


    /* ---------- 13.5 检查反算参考气压是否合理 ---------- */

    if (!isfinite(referencePressure) ||              // 禁止无效浮点结果
        referencePressure < ALTITUDE_REFERENCE_MIN_PA ||
        referencePressure > ALTITUDE_REFERENCE_MAX_PA)
    {
        return ALTITUDE_STATUS_INVALID_ARG;          // 不采用不合理的参考气压
    }


    /* ---------- 13.6 发布校准后的高度与参考气压 ---------- */

    taskENTER_CRITICAL();                            // 开始更新共享状态

    s_altitude_snapshot.reference_pressure_pa =
        referencePressure;                           // 保存反算海平面气压

    s_altitude_snapshot.altitude_m =
        known_altitude_m;                            // 当前高度与用户已知高度一致

    s_relative_zero_m =
        known_altitude_m;                            // 使用当前已知高度建立相对零点

    s_altitude_snapshot.relative_altitude_m =
        0.0f;                                       // 当前相对高度归零

    s_altitude_snapshot.relative_zero_valid =
        true;                                       // 相对高度零点有效

    s_altitude_snapshot.mode =
        ALTITUDE_CALIBRATION_KNOWN_HEIGHT;           // 标记已知高度校准模式

    s_altitude_snapshot.revision++;                   // 更新状态修订号

    taskEXIT_CRITICAL();                             // 退出临界区

    return ALTITUDE_STATUS_OK;                       // 本次校准完成
}


/* ---------- 14. 重新设置相对高度零点 ---------- */

altitude_service_status_t AltitudeService_ResetRelativeZero(void)
{
    /* ---------- 14.1 检查当前高度是否有效 ---------- */

    if (!s_initialized)                              // 高度服务尚未初始化
    {
        return ALTITUDE_STATUS_NOT_READY;            // 不允许修改零点
    }

    if (!s_altitude_snapshot.valid)                  // 当前没有有效高度
    {
        return ALTITUDE_STATUS_NO_DATA;              // 不能使用过期高度建立零点
    }


    /* ---------- 14.2 使用当前位置重新建立相对高度零点 ---------- */

    taskENTER_CRITICAL();                            // 进入共享状态保护区

    s_relative_zero_m =
        s_altitude_snapshot.altitude_m;              // 保存当前绝对气压高度作为零点

    s_altitude_snapshot.relative_altitude_m =
        0.0f;                                       // 当前相对高度设置为 0

    s_altitude_snapshot.relative_zero_valid =
        true;                                       // 标记新零点有效

    s_altitude_snapshot.revision++;                   // 更新快照版本

    taskEXIT_CRITICAL();                             // 退出临界区

    return ALTITUDE_STATUS_OK;                       // 相对高度零点设置成功
}


/* ---------- 15. 获取当前高度状态快照 ---------- */

bool AltitudeService_GetSnapshot(
    altitude_service_snapshot_t *out
)
{
    /* ---------- 15.1 检查参数和初始化状态 ---------- */

    if (out == NULL ||                               // 调用者没有提供有效输出空间
        !s_initialized)                              // 服务尚未初始化
    {
        return false;                                // 拒绝本次快照读取
    }


    /* ---------- 15.2 一次性复制完整高度快照 ---------- */

    taskENTER_CRITICAL();                            // 进入临界区，防止任务切换

    *out = s_altitude_snapshot;                      // 复制所有高度和校准状态

    taskEXIT_CRITICAL();                             // 恢复正常任务调度

    return true;                                     // 快照复制成功
}
