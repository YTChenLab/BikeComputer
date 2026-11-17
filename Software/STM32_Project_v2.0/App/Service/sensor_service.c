/**
 * @file    sensor_service.c
 * @brief   公路车码表传感器状态服务实现
 *
 * @details
 * 主要功能：
 * 1. 周期检查 SPL06 初始化状态。
 * 2. 初始化失败后自动重试。
 * 3. 获取新的补偿气压和温度数据。
 * 4. 管理数据新鲜度与测量有效性。
 * 5. 通过任务安全的状态快照向其他模块提供数据。
 *
 * 线程所有权：
 * SensorTask：唯一数据生产者。
 * UITask / RideService：只读取快照。
 *
 * 本服务不直接调用 LVGL。
 */

#include "sensor_service.h"                  // 本模块公开接口
#include "altitude_service.h"                // 气压高度计算、滤波和校准服务
#include "i2c.h"                             // CubeMX I2C1 句柄
#include "FreeRTOS.h"                        // FreeRTOS 核心定义
#include "task.h"                            // FreeRTOS 临界区接口

#include <string.h>                          // memset() 内存初始化


/* ---------- 1. 传感器服务内部状态 ---------- */

static sensor_service_snapshot_t s_snapshot; // 最新传感器状态快照

static bool s_initialized = false;           // 服务是否已经初始化

static uint32_t s_next_retry_ms = 0U;        // 下次允许初始化的时间


/* ---------- 2. 更新状态版本号 ---------- */

static void SensorService_UpdateStatus(
    bsp_spl06_status_t status,
    bool ready
)
{
    taskENTER_CRITICAL();                     // 禁止任务切换，保持快照一致

    s_snapshot.last_status = status;          // 保存最近一次驱动状态

    s_snapshot.spl06_ready = ready;           // 更新设备就绪状态

    s_snapshot.revision++;                    // 状态发生变化，增加版本号

    taskEXIT_CRITICAL();                      // 恢复正常任务调度
}


/* ---------- 3. 初始化传感器服务 ---------- */

void SensorService_Init(void)
{
    /* ---------- 3.1 清空历史状态 ---------- */

    taskENTER_CRITICAL();                     // 进入 FreeRTOS 临界区

    memset(
        &s_snapshot,                          // 状态快照起始地址
        0,                                    // 全部清零
        sizeof(s_snapshot)                    // 状态快照实际大小
    );

    s_snapshot.last_status =
        BSP_SPL06_STATUS_NOT_READY;           // 初始状态为尚未就绪

    s_initialized = true;                     // 标记服务已经完成初始化

    taskEXIT_CRITICAL();                      // 退出临界区

    /* ---------- 3.2 允许首次立即初始化 ---------- */

    s_next_retry_ms = 0U;                    // 首次 Process() 可以直接探测
        /* ---------- 3.3 初始化气压高度计算服务 ---------- */

    AltitudeService_Init();                  // 初始化参考气压、滤波状态和高度快照
}


/* ---------- 4. 传感器服务周期处理 ---------- */

void SensorService_Process(uint32_t now_ms)
{
    bsp_spl06_status_t status;                // 保存本次驱动操作状态

    bsp_spl06_sample_t sample = {0};          // SPL06 本次测量结果

    /* ---------- 4.1 检查服务是否已经初始化 ---------- */

    if (!s_initialized)                       // 禁止在初始化前运行采样
    {
        return;                               // 未初始化时直接退出
    }
    
    /* ---------- 4.2 周期维护高度数据有效性 ---------- */

    AltitudeService_Tick(HAL_GetTick());      // 使用当前最新时间检查高度是否超时

    /* ---------- 4.2 SPL06 尚未就绪时尝试初始化 ---------- */

    if (!BSP_SPL06_IsReady())                 // 底层驱动尚未准备好
    {
        if ((int32_t)(now_ms -
            s_next_retry_ms) < 0)            // 尚未到达下一次允许重试的时间
        {
            return;                          // 保持等待，避免频繁操作 I2C
        }

        status = BSP_SPL06_Init(&hi2c1);      // 使用已有 I2C1 初始化 SPL06

        if (status != BSP_SPL06_STATUS_OK)    // 判断初始化是否成功
        {
            taskENTER_CRITICAL();             // 保护共享状态

            s_snapshot.spl06_ready = false;   // 标记气压计未就绪

            s_snapshot.measurement_valid =
                false;                       // 初始化失败时撤销测量有效性

            s_snapshot.last_status = status;  // 保存失败原因

            if (status != BSP_SPL06_STATUS_NOT_READY)
            {
                s_snapshot.error_count++;     // 非暂时未就绪状态计入错误
            }

            s_snapshot.revision++;            // 更新状态版本

            taskEXIT_CRITICAL();              // 退出临界区

            s_next_retry_ms =
                now_ms +
                SENSOR_SERVICE_RETRY_MS;      // 500ms 后重新初始化

            return;                           // 当前周期不再继续采样
        }

        /* ---------- 4.3 保存芯片初始化结果 ---------- */

        taskENTER_CRITICAL();                 // 进入状态保护区

        s_snapshot.chip_id =
            BSP_SPL06_GetChipId();           // 保存产品及修订版本 ID

        s_snapshot.i2c_address =
            BSP_SPL06_GetAddress();          // 保存实际 I2C 地址

        taskEXIT_CRITICAL();                  // 退出临界区

        SensorService_UpdateStatus(
            BSP_SPL06_STATUS_OK,             // 初始化已经成功
            true                             // SPL06 已经准备好
        );

        return;                               // 下一次周期再读取测量
    }

    /* ---------- 4.4 读取新的 SPL06 测量数据 ---------- */

    status = BSP_SPL06_Read(&sample);         // 获取补偿气压和温度

    /* ---------- 4.5 成功采集新的气压和温度 ---------- */

    if (status == BSP_SPL06_STATUS_OK)             // SPL06 返回有效的气压和温度
    {
        /* ---------- 4.5.1 将新气压交给高度计算服务 ---------- */

        (void)AltitudeService_Update(
            sample.pressure_pa,                    // SPL06 当前补偿气压，单位 Pa
            sample.sampled_at_ms                   // 本次实际采样完成时间
        );

        /*
         * 高度服务内部负责：
         * 1. 检查气压范围。
         * 2. 拒绝瞬时异常值。
         * 3. 根据时间间隔进行低通滤波。
         * 4. 换算绝对气压高度。
         * 5. 更新相对高度及状态快照。
         *
         * 高度服务拒绝一次异常气压时，
         * 不影响 SensorService 已经成功采集的气压和温度。
         */

        /* ---------- 4.5.2 更新原有传感器状态快照 ---------- */

        taskENTER_CRITICAL();                      // 开始更新传感器共享状态

        s_snapshot.pressure_pa =
            sample.pressure_pa;               // 更新补偿气压

        s_snapshot.temperature_c =
            sample.temperature_c;             // 更新补偿温度

        s_snapshot.last_valid_ms =
            sample.sampled_at_ms;             // 更新最近有效采样时间

        s_snapshot.measurement_valid = true;  // 本次测量有效

        s_snapshot.spl06_ready = true;        // 设备正常工作

        s_snapshot.sample_count++;            // 累加成功采样次数

        s_snapshot.last_status =
            BSP_SPL06_STATUS_OK;              // 保存成功状态

        s_snapshot.revision++;                // 更新快照版本号

        taskEXIT_CRITICAL();                  // 退出临界区
    }

    /* ---------- 4.6 没有新数据属于正常情况 ---------- */

    else if (status == BSP_SPL06_STATUS_NO_SAMPLE)
    {
        /*
         * SPL06 配置为 4Hz，约 250ms 才会更新一组数据。
         *
         * SensorTask 的周期是 100ms，因此不是每次都能读到新值。
         *
         * NO_SAMPLE 不计入错误次数，也不立即清除上一组有效数据。
         */
    }

    /* ---------- 4.7 处理其他实际错误 ---------- */

    else
    {
        taskENTER_CRITICAL();                 // 进入状态保护区

        s_snapshot.error_count++;            // 累加测量错误次数

        s_snapshot.last_status = status;      // 保存最近一次错误码

        s_snapshot.revision++;                // 更新状态版本

        if (!BSP_SPL06_IsReady())             // 底层驱动已因通信错误失效
        {
            s_snapshot.spl06_ready = false;   // 同步设备未就绪状态

            s_snapshot.measurement_valid =
                false;                       // 不继续使用已失效测量
        }

        taskEXIT_CRITICAL();                  // 退出临界区

        if (!BSP_SPL06_IsReady())             // 需要重新识别并配置芯片
        {
            s_next_retry_ms =
                now_ms +
                SENSOR_SERVICE_RETRY_MS;      // 延迟 500ms 后重新初始化
        }
    }

    /* ---------- 4.8 使用最新系统时间检查测量数据是否超时 ---------- */

    /*
     * 设计说明：
     *
     * 不能直接使用传入的 now_ms 判断本次测量是否超时。
     *
     * 原因：
     * now_ms 是 SensorTask 调用本函数前获取的时间，
     * 而 last_valid_ms 是底层 I2C 读取结束后记录的时间。
     *
     * 如果 I2C 读取期间 HAL Tick 增加，
     * 就会出现 now_ms 早于 last_valid_ms 的情况。
     *
     * 两者相减发生 uint32_t 回绕，
     * 可能把刚采集成功的数据错误地判定为过期。
     *
     * 解决方法：
     * 在完成本轮传感器处理后，重新获取当前系统时间，
     * 然后使用最新时间检查数据有效期。
     */

    uint32_t checkNowMs = HAL_GetTick();      // 在完成本轮采集后重新获取最新系统时间

    taskENTER_CRITICAL();                     // 进入临界区，保护传感器共享快照

    if (s_snapshot.measurement_valid &&       // 只有当前有效的数据才需要检查超时
        (uint32_t)(checkNowMs -               // 使用本轮处理完成后的最新时间
        s_snapshot.last_valid_ms) >           // 计算距离最近一次有效测量的时间间隔
        SENSOR_SERVICE_TIMEOUT_MS)            // 判断是否超过允许的 1500ms
    {
        s_snapshot.measurement_valid = false; // 超时后撤销测量有效性

        s_snapshot.revision++;                // 数据状态变化，更新快照版本号
    }

    taskEXIT_CRITICAL();                      // 退出临界区，恢复正常任务调度
}


/* ---------- 5. 获取传感器状态快照 ---------- */

bool SensorService_GetSnapshot(
    sensor_service_snapshot_t *out
)
{
    /* ---------- 5.1 检查调用参数和初始化状态 ---------- */

    if (out == NULL ||                         // 调用者没有提供输出空间
        !s_initialized)                       // 服务尚未初始化
    {
        return false;                         // 禁止读取无效状态
    }

    /* ---------- 5.2 复制完整状态快照 ---------- */

    taskENTER_CRITICAL();                     // 暂停任务切换，避免复制中途被修改

    *out = s_snapshot;                        // 一次复制完整状态

    taskEXIT_CRITICAL();                      // 恢复任务调度

    return true;                              // 返回快照获取成功
}
