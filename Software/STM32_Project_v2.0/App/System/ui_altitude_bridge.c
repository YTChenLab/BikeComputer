/**
 * @file    ui_altitude_bridge.c
 * @brief   SPL06 气压高度与 LVGL 骑行数据模型桥接实现
 *
 * @details
 * 本文件实现：
 * 1. 读取 AltitudeService 高度快照。
 * 2. 检查高度有效性和采样时间。
 * 3. 将浮点高度四舍五入为整数米。
 * 4. 设置 UI 海拔数据来源为 BAROMETER。
 * 5. 将数据提交到现有 UI 骑行数据模型。
 * 6. 对数据失效进行显式通知。
 * 7. 避免重复发布相同的高度样本。
 *
 * 设计原则：
 * 1. SPL06 是 UI 海拔字段的唯一数据源。
 * 2. GNSS 不再直接更新 UI 海拔字段。
 * 3. 不创建额外的 FreeRTOS 任务。
 * 4. 不直接调用 LVGL 控件操作函数。
 * 5. 高度无效时，不把 0m 作为有效测量提交。
 *
 * 调用位置：
 * UI_BoardPoll()，属于 UITask 上下文。
 */

#include "ui_altitude_bridge.h"               // 本模块公开接口

#include "altitude_service.h"                // 获取已滤波的气压高度

#include "platform/stm32/ui_stm32_services.h" // STM32 到 LVGL 的正式数据接口

#include <math.h>                            // isfinite() 浮点有效性判断
#include <stdbool.h>                         // bool 布尔类型
#include <stdint.h>                          // 固定宽度整数类型
#include <string.h>                          // memset() 内存初始化


/* ---------- 1. UI 海拔允许范围 ---------- */

#define UI_ALTITUDE_MIN_M         (-1500.0f)  // UI 允许的最低高度

#define UI_ALTITUDE_MAX_M         12000.0f    // UI 允许的最高高度


/* ---------- 2. UI 海拔桥接内部状态 ---------- */

static bool s_has_published = false;         // 是否曾经成功向 UI 发布高度

static bool s_previous_valid = false;        // 上一次发布的高度是否有效

static uint32_t s_previous_sample_ms = 0U;   // 上一次成功发布的采样时间

static int32_t s_previous_altitude_m = 0;    // 上一次成功发布的整数高度

static volatile uint32_t
    s_publish_success_count = 0U;             // UI 数据提交成功次数，供 Keil 调试

static volatile uint32_t
    s_publish_failure_count = 0U;             // UI 数据提交失败次数，供 Keil 调试


/* ---------- 3. 将浮点高度转换成整数米 ---------- */

static int32_t UI_AltitudeBridge_RoundMeters(
    float altitude_m
)
{
    /* ---------- 3.1 判断高度的正负 ---------- */

    if (altitude_m >= 0.0f)                  // 当前气压高度大于等于 0m
    {
        return (int32_t)(altitude_m + 0.5f); // 正数采用四舍五入
    }

    /* ---------- 3.2 处理负海拔高度 ---------- */

    return (int32_t)(altitude_m - 0.5f);     // 负数按照绝对值四舍五入
}


/* ---------- 4. 周期更新 UI 海拔数据 ---------- */

void UI_AltitudeBridge_Poll(uint32_t now_ms)
{
    altitude_service_snapshot_t altitude = {0}; // 保存高度服务的最新快照

    ui_ride_data_t patch = {0};                // 本次向 UI 提交的字段补丁

    ui_ride_data_t current = {0};              // 保存 UI 当前已处理的数据状态

    bool altitudeValid;                       // 当前高度是否符合发布条件

    int32_t displayAltitude;                   // 最终提交给 UI 的整数高度

    uint32_t sampledAtMs;                     // UI 质量信息中的测量时间戳

    uint32_t fields;                          // 本次更新的 UI 字段掩码


    /* ---------- 4.1 从高度服务读取最新状态 ---------- */

    if (!AltitudeService_GetSnapshot(&altitude)) // 高度服务尚未初始化或读取失败
    {
        return;                               // 不提交任何无效数据
    }


    /* ---------- 4.2 检查高度数据有效性 ---------- */

    altitudeValid =
        altitude.valid &&                     // 高度服务明确报告有效
        altitude.sample_count > 0U &&         // 已经完成至少一次有效高度计算
        isfinite(altitude.altitude_m) &&       // 高度不能为 NaN 或无穷大
        altitude.altitude_m >=
            UI_ALTITUDE_MIN_M &&              // 高度不能低于 UI 支持范围
        altitude.altitude_m <=
            UI_ALTITUDE_MAX_M &&              // 高度不能高于 UI 支持范围
        (uint32_t)(now_ms -
            altitude.updated_ms) <=
            ALTITUDE_DATA_TIMEOUT_MS;         // 本次数据不能超过允许有效期


    /* ---------- 4.3 有效高度只在新采样到来时更新 ---------- */

    if (altitudeValid)                         // 当前拥有有效气压高度
    {
        if (s_has_published &&                 // 之前已经成功向 UI 发布数据
            s_previous_valid &&               // 上一次发布的高度也有效
            altitude.updated_ms ==
                s_previous_sample_ms)         // 新旧测量时间完全一致
        {
            return;                           // 没有新采样，避免重复提交
        }

        displayAltitude =
            UI_AltitudeBridge_RoundMeters(
                altitude.altitude_m           // 将浮点气压高度转换为整数米
            );

        sampledAtMs = altitude.updated_ms;    // 保存真实高度采样完成时间
    }

    /* ---------- 4.4 无效高度只在状态切换时通知 UI ---------- */

    else
    {
        if (!s_has_published ||                // 尚未向 UI 发布过有效高度
            !s_previous_valid)                 // 上一次已经通知过无效状态
        {
            return;                            // 无需重复发送无效通知
        }

        displayAltitude =
            s_previous_altitude_m;             // 保留最后一次高度值，不伪造 0m

        sampledAtMs = now_ms;                  // 本次无效状态以当前时间发布
    }


    /* ---------- 4.5 读取 UI 当前已处理的数据公共状态 ---------- */

    /*
     * ui_ride_data_t 除具体测量字段外，
     * 还包含骑行会话、计时和 UTC 等公共头字段。
     *
     * 因此在提交独立高度补丁前，
     * 尽量保留已经处理的骑行状态。
     *
     * 当前项目尚未接入正式 RideService。
     * 后续 RideService 将统一负责会话和计时字段的发布。
     */

    if (ui_ride_data_model_get(&current, NULL)) // 读取 UI 当前已处理的数据
    {
        patch.timer_ms =
            current.timer_ms;                  // 保留已有的骑行计时信息

        patch.ride_seconds =
            current.ride_seconds;              // 保留已有骑行秒数

        patch.session_id =
            current.session_id;                // 保留当前骑行会话 ID

        patch.timestamp_utc =
            current.timestamp_utc;             // 保留已有 UTC 时间
    }


    /* ---------- 4.6 填写本次 UI 海拔字段 ---------- */

    patch.altitude_m =
        displayAltitude;                       // 设置本次需要显示的海拔

    patch.sample_time_ms =
        now_ms;                                // 使用 UI 同源 HAL 毫秒时钟


    /* ---------- 4.7 标记海拔数据来源和有效性 ---------- */

    ui_ride_data_set_quality(
        &patch,                               // 本次要提交的 UI 数据结构
        UI_RIDE_FIELD_ALTITUDE,               // 指定本次质量信息属于海拔字段
        true,                                 // 当前设备支持气压高度测量
        altitudeValid,                        // 根据快照状态设置有效标志
        sampledAtMs,                          // 使用测量时间或失效通知时间
        ALTITUDE_DATA_TIMEOUT_MS,             // 超过 1500ms 不再视为新鲜数据
        UI_DATA_SOURCE_BAROMETER              // 明确标记数据来自 SPL06 气压计
    );


    /* ---------- 4.8 构造仅更新海拔字段的掩码 ---------- */

    fields =
        UI_RIDE_FIELD_BIT(
            UI_RIDE_FIELD_ALTITUDE            // 只允许本桥接更新海拔字段
        );


    /* ---------- 4.9 向 LVGL 数据模型提交海拔补丁 ---------- */

    if (!ui_stm32_services_submit_ride_patch(
            &patch,                           // 本次高度数据和质量信息
            fields                            // 仅修改 UI_RIDE_FIELD_ALTITUDE
        ))
    {
        s_publish_failure_count++;            // 记录一次提交失败

        return;                               // 保留旧状态，下次继续尝试
    }


    /* ---------- 4.10 更新本地发布状态 ---------- */

    s_has_published = true;                   // 已经成功提交至少一次

    s_previous_valid = altitudeValid;         // 保存本次高度有效性

    if (altitudeValid)                        // 本次成功提交的是有效高度
    {
        s_previous_sample_ms =
            altitude.updated_ms;              // 保存本次有效采样时间

        s_previous_altitude_m =
            displayAltitude;                  // 保存最后一次有效整数海拔
    }

    s_publish_success_count++;                // 本次 UI 提交成功次数加一
}