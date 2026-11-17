/**
 * @file    ui_gnss_bridge.c
 * @brief   PU-252 GNSS 数据与正式 LVGL UI 的桥接适配
 *
 * @details
 * 主要功能：
 *
 * 1. 从 GnssService 获取真实 GNSS 状态快照。
 * 2. 判断定位数据是否有效以及是否超时。
 * 3. 将 GNSS 数据转换成 UI 统一数据格式。
 * 4. 使用现有 UI Service 发布骑行数据补丁。
 * 5. 同步状态栏卫星数量和 GNSS 时间。
 *
 * 线程架构：
 *
 * GNSSTask：
 *     负责 UART DMA 和 NMEA 解析，只发布快照。
 *
 * UITask：
 *     负责读取快照并提交 UI 数据，不解析 NMEA。
 *
 * 特别说明：
 *
 * 本模块不计算骑行里程、平均速度或累计爬升。
 * 这些指标将由后续正式 RideService 负责。
 *
 * GNSS 海拔作为当前临时高度来源。
 * 后续 SPL06 接入后可改用气压高度融合结果。
 */

#include "ui_gnss_bridge.h"               // 本模块公开接口
#include "gnss_service.h"                 // GNSS 跨任务状态快照
#include "platform/stm32/ui_stm32_services.h" // 现有 STM32 UI Service
#include "ui/models/ui_ride_data_model.h" // UI 骑行数据模型
#include "ui/core/ui_app_state.h"         // UI 状态栏和时钟接口

#include <stdbool.h>                      // bool 类型
#include <stdint.h>                       // 固定宽度整数类型


/*==========================================================
 * 一、模块配置
 *=========================================================*/

#define UI_GNSS_POLL_PERIOD_MS       (200U) // 每 200ms 检查一次

#define UI_GNSS_QUALITY_TIMEOUT_MS   \
    GNSS_FIX_TIMEOUT_MS                    // 与 GNSS 解析器超时一致

/*
 * 当前 STM32 UI 默认时区为 UTC+08:00。
 *
 * 该值与 GitHub 中 ui_stm32_defaults.c 一致。
 * 后续接入时区设置服务时，应统一从设置读取。
 */
#define UI_GNSS_UTC_OFFSET_MINUTES   (480)


/*==========================================================
 * 二、静态运行状态
 *
 * 用于判断是否出现新的 GNSS 数据。
 *
 * 没有新数据且有效性没有变化时，
 * 不需要重复提交 UI 更新。
 *=========================================================*/

static bool s_poll_started = false;        // 是否执行过首次轮询

static bool s_published = false;           // 是否提交过有效的模型补丁

static bool s_last_fix = false;            // 上一次定位状态

static bool s_last_altitude = false;       // 上一次高度有效性

static bool s_last_time = false;           // 上一次 UTC 有效性

static bool s_clock_owned = false;         // 是否曾使用 GNSS 设置 UI 时钟

static uint32_t s_last_poll_ms = 0U;       // 上一次轮询时间

static uint32_t s_last_rmc_ms = 0U;        // 上一次 RMC 时间戳

static uint32_t s_last_gga_ms = 0U;        // 上一次 GGA 高度时间戳

static uint8_t s_last_satellites = 0U;     // 上一次显示的卫星数


/*==========================================================
 * 三、闰年判断
 *
 * 公历规则：
 *
 * 能被 4 整除，通常为闰年；
 * 但整百年份必须同时能被 400 整除。
 *
 * 用于后续将 GNSS UTC 转换为 Unix 时间。
 *=========================================================*/

static bool UI_GnssBridge_LeapYear(uint16_t year)
{
    return ((year % 4U) == 0U) &&
           (((year % 100U) != 0U) ||
            ((year % 400U) == 0U));
}


/*==========================================================
 * 四、GNSS UTC 转 Unix 时间戳
 *
 * 输入：
 * gnss_snapshot_t 中的 UTC 年、月、日、时、分、秒。
 *
 * 输出：
 * Unix 时间戳，单位秒。
 *
 * 原理：
 * 从 1970-01-01 UTC 开始，累计完整经过的天数，
 * 再加上当天已经经过的时、分、秒。
 *
 * 使用 int64_t 防止时间计算溢出。
 * 不依赖系统 RTC 或 time.h。
 *=========================================================*/

static bool UI_GnssBridge_ToUnix(
    const gnss_snapshot_t *gnss,
    int64_t *out
)
{
    static const uint8_t days_by_month[12] =
    {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };

    uint32_t days = 0U;                 // 从 Unix 起点累计的天数

    uint8_t month_days;                // 当前月份实际天数

    /* 一、检查指针和日期范围 */

    if (gnss == NULL ||
        out == NULL ||
        gnss->year < 2000U ||
        gnss->year > 2099U ||
        gnss->month < 1U ||
        gnss->month > 12U ||
        gnss->hour >= 24U ||
        gnss->minute >= 60U ||
        gnss->second > 60U)
    {
        return false;
    }

    /* 二、计算当前月份天数 */

    month_days = days_by_month[gnss->month - 1U];

    if ((gnss->month == 2U) &&
        UI_GnssBridge_LeapYear(gnss->year))
    {
        ++month_days;                   // 闰年二月增加一天
    }

    /* 三、检查日期合法性 */

    if (gnss->day < 1U ||
        gnss->day > month_days)
    {
        return false;
    }

    /* 四、累计完整经过的年份 */

    for (uint16_t year = 1970U;
         year < gnss->year;
         ++year)
    {
        days += UI_GnssBridge_LeapYear(year) ?
                366U : 365U;
    }

    /* 五、累计当前年份已经经过的月份 */

    for (uint8_t month = 1U;
         month < gnss->month;
         ++month)
    {
        days += days_by_month[month - 1U];

        if ((month == 2U) &&
            UI_GnssBridge_LeapYear(gnss->year))
        {
            ++days;
        }
    }

    /* 六、加上当前月份已经经过的天数 */

    days += (uint32_t)gnss->day - 1U;

    /* 七、转换为完整 Unix 时间戳 */

    *out =
        (int64_t)days * INT64_C(86400) +
        (int64_t)gnss->hour * 3600 +
        (int64_t)gnss->minute * 60 +
        gnss->second;

    return true;
}


/*==========================================================
 * 五、UI_GnssBridge_Poll()
 *
 * 本模块唯一的周期执行函数。
 *
 * 工作顺序：
 *
 * 1. 限制轮询周期。
 * 2. 获取 GNSS 快照。
 * 3. 判断数据是否有效。
 * 4. 检查数据是否更新。
 * 5. 保留已有骑行会话数据。
 * 6. 生成 GNSS 数据补丁。
 * 7. 设置数据来源与有效性。
 * 8. 发布到 UI 骑行数据模型。
 * 9. 更新状态栏卫星数量。
 * 10. 更新 GNSS 时间。
 *
 * 只允许在 UITask 中调用。
 *=========================================================*/

void UI_GnssBridge_Poll(uint32_t now_ms)
{
    gnss_snapshot_t gnss = {0};              // GNSS 服务快照

    ui_ride_data_t existing = {0};           // 当前 UI 骑行数据

    ui_ride_data_t patch = {0};              // 本轮局部数据更新

    ui_app_state_t app_state = {0};           // 当前 UI 系统状态

    ui_state_adapter_snapshot_t state_patch = {0}; // 状态栏补丁

    int64_t unix_seconds = 0;                // 当前 GNSS UTC 时间

    bool fix_ok;                              // 定位有效性

    bool altitude_ok;                         // 高度有效性

    bool utc_ok;                              // UTC 时间有效性

    uint8_t visible_satellites;               // 需要显示的卫星数

    uint32_t fields;                          // 需要更新的字段掩码


    /*------------------------------------------------------
     * 第一步：限制轮询频率
     *
     * UITask 可能每 10ms 执行一次，
     * 但 GNSS 状态没有必要每 10ms 重复处理。
     *
     * 当前每 200ms 检查一次，即 5Hz。
     *-----------------------------------------------------*/

    if (s_poll_started &&
        (uint32_t)(now_ms - s_last_poll_ms) <
            UI_GNSS_POLL_PERIOD_MS)
    {
        return;
    }

    s_poll_started = true;

    s_last_poll_ms = now_ms;


    /*------------------------------------------------------
     * 第二步：获取真实 GNSS 快照
     *
     * GnssService 内部保证跨任务读取一致性。
     *-----------------------------------------------------*/

    if (!GnssService_GetSnapshot(&gnss))
    {
        return;                               // GNSS 服务尚未初始化
    }


    /*------------------------------------------------------
     * 第三步：判断定位数据是否仍然有效
     *
     * 必须同时满足：
     *
     * 1. 解析器报告有效定位；
     * 2. 距离最近一次有效 RMC 不超过 3 秒。
     *
     * 防止使用已经过期的速度、经纬度。
     *-----------------------------------------------------*/

    fix_ok =
        gnss.fix_valid &&
        (uint32_t)(now_ms - gnss.updated_ms) <=
            UI_GNSS_QUALITY_TIMEOUT_MS;


    /*------------------------------------------------------
     * 第四步：单独判断 GNSS 高度
     *
     * GGA 高度与 RMC 定位不一定同时更新。
     * 因此需要单独检查高度新鲜度。
     *-----------------------------------------------------*/

    altitude_ok =
        fix_ok &&
        gnss.altitude_valid &&
        (uint32_t)(now_ms - gnss.altitude_updated_ms) <=
            UI_GNSS_QUALITY_TIMEOUT_MS;


    /*------------------------------------------------------
     * 第五步：检查 UTC 日期和时间
     *-----------------------------------------------------*/

    utc_ok =
        fix_ok &&
        gnss.time_valid &&
        UI_GnssBridge_ToUnix(&gnss, &unix_seconds);


    /* 没有有效定位时不继续显示旧卫星数量 */

    visible_satellites =
        fix_ok ? gnss.satellites : 0U;


    /*------------------------------------------------------
     * 第六步：检查数据是否真正发生变化
     *
     * 需要考虑：
     * RMC 更新时间、GGA 高度更新时间、
     * 卫星数量、定位有效性、高度有效性和 UTC 状态。
     *
     * 如果全部不变，不重复提交。
     *-----------------------------------------------------*/

    if (s_published &&
        gnss.updated_ms == s_last_rmc_ms &&
        gnss.altitude_updated_ms == s_last_gga_ms &&
        visible_satellites == s_last_satellites &&
        fix_ok == s_last_fix &&
        altitude_ok == s_last_altitude &&
        utc_ok == s_last_time)
    {
        return;
    }


    /*------------------------------------------------------
     * 第七步：读取已有骑行数据
     *
     * 为什么必须读取 existing？
     *
     * 因为现有 UI 的 submit_patch() 不仅更新字段，
     * 还会复制骑行计时和会话身份。
     *
     * 不能随意清零：
     *
     * session_id
     * timer_ms
     * ride_seconds
     * timestamp_utc
     *
     * 否则可能影响后续骑行记录。
     *-----------------------------------------------------*/

    if (!ui_ride_data_model_get(&existing, NULL))
    {
        return;
    }

    patch.quality_known = true;               // 使用正式质量标记

    patch.sample_time_ms = now_ms;             // 本次提交时间

    patch.session_id = existing.session_id;    // 保留当前会话 ID

    patch.timer_ms = existing.timer_ms;        // 保留骑行计时

    patch.ride_seconds = existing.ride_seconds;// 保留骑行秒数

    patch.timestamp_utc =
        utc_ok ? unix_seconds :
                 existing.timestamp_utc;       // 有效时才更新 UTC


    /*------------------------------------------------------
     * 第八步：转换实时速度
     *
     * GNSS Service：
     * speed_centi_kmh 单位为 0.01 km/h。
     *
     * LVGL UI：
     * speed 单位为 km/h，float。
     *-----------------------------------------------------*/

    patch.speed =
        fix_ok ?
        ((float)gnss.speed_centi_kmh / 100.0f) :
        0.0f;


    /*------------------------------------------------------
     * 第九步：转换经纬度
     *
     * GNSS：
     * latitude_e7 / longitude_e7 为 1e-7 度。
     *
     * UI：
     * latitude / longitude 为 double，单位度。
     *-----------------------------------------------------*/

    patch.latitude =
        fix_ok ?
        ((double)gnss.latitude_e7 / 10000000.0) :
        0.0;

    patch.longitude =
        fix_ok ?
        ((double)gnss.longitude_e7 / 10000000.0) :
        0.0;


    /*------------------------------------------------------
     * 第十步：更新卫星数量
     *-----------------------------------------------------*/

    patch.satellites = visible_satellites;


    /*------------------------------------------------------
     * 第十一步：转换海拔
     *
     * GNSS：
     * altitude_cm 单位为厘米。
     *
     * UI：
     * altitude_m 单位为米。
     *
     * 后续 SPL06 接入后，
     * 可由 SensorFusion 统一决定正式海拔来源。
     *-----------------------------------------------------*/

    // patch.altitude_m =
    //     altitude_ok ? gnss.altitude_cm / 100 : 0;


    /*------------------------------------------------------
     * 第十二步：转换时钟
     *
     * GNSS 使用 UTC。
     *
     * 当前 UI 默认使用 UTC+08:00。
     *
     * 注意：
     * 未来需要把时区偏移接入用户设置，
     * 不应长期写死为固定值。
     *-----------------------------------------------------*/

    patch.clock_hour = existing.clock_hour;

    patch.clock_minute = existing.clock_minute;

    if (utc_ok)
    {
        int32_t minute_of_day =
            (int32_t)gnss.hour * 60 +
            (int32_t)gnss.minute +
            UI_GNSS_UTC_OFFSET_MINUTES;

        minute_of_day =
            (minute_of_day % 1440 + 1440) % 1440;

        patch.clock_hour =
            (uint8_t)(minute_of_day / 60);

        patch.clock_minute =
            (uint8_t)(minute_of_day % 60);
    }


    /*------------------------------------------------------
     * 第十三步：为速度设置质量信息
     *
     * supported=true 表示设备具有 GNSS 速度能力。
     *
     * valid=fix_ok 表示本次速度是否有效。
     *
     * 即使 supported=true，
     * 只要 valid=false，UI 也不能将其视为有效测量。
     *-----------------------------------------------------*/

    ui_ride_data_set_quality(
        &patch,
        UI_RIDE_FIELD_SPEED,
        true,
        fix_ok,
        gnss.updated_ms,
        UI_GNSS_QUALITY_TIMEOUT_MS,
        UI_DATA_SOURCE_GNSS
    );


    /*------------------------------------------------------
     * 第十四步：设置经纬度质量信息
     *-----------------------------------------------------*/

    ui_ride_data_set_quality(
        &patch,
        UI_RIDE_FIELD_POSITION,
        true,
        fix_ok,
        gnss.updated_ms,
        UI_GNSS_QUALITY_TIMEOUT_MS,
        UI_DATA_SOURCE_GNSS
    );


    /*------------------------------------------------------
     * 第十五步：设置 GNSS 状态质量信息
     *-----------------------------------------------------*/

    ui_ride_data_set_quality(
        &patch,
        UI_RIDE_FIELD_GNSS,
        true,
        fix_ok,
        gnss.updated_ms,
        UI_GNSS_QUALITY_TIMEOUT_MS,
        UI_DATA_SOURCE_GNSS
    );


    /*------------------------------------------------------
     * 第十六步：设置海拔质量信息
     *-----------------------------------------------------*/

    // ui_ride_data_set_quality(
    //     &patch,
    //     UI_RIDE_FIELD_ALTITUDE,
    //     true,
    //     altitude_ok,
    //     gnss.altitude_updated_ms,
    //     UI_GNSS_QUALITY_TIMEOUT_MS,
    //     UI_DATA_SOURCE_GNSS
    // );


    /*------------------------------------------------------
     * 第十七步：设置 GNSS 时钟质量信息
     *-----------------------------------------------------*/

    ui_ride_data_set_quality(
        &patch,
        UI_RIDE_FIELD_TIME,
        true,
        utc_ok,
        gnss.updated_ms,
        UI_GNSS_QUALITY_TIMEOUT_MS,
        UI_DATA_SOURCE_GNSS
    );


    /*------------------------------------------------------
     * 第十八步：构造需要更新的字段集合
     *
     * 只包含 GNSS 当前负责的五类数据。
     *
     * 不更新：
     * 里程、平均速度、功率、心率、踏频、累计爬升等。
     *-----------------------------------------------------*/

    fields =
        UI_RIDE_FIELD_BIT(UI_RIDE_FIELD_SPEED) |
        UI_RIDE_FIELD_BIT(UI_RIDE_FIELD_POSITION) |
        UI_RIDE_FIELD_BIT(UI_RIDE_FIELD_GNSS) |
        // UI_RIDE_FIELD_BIT(UI_RIDE_FIELD_ALTITUDE) |
        UI_RIDE_FIELD_BIT(UI_RIDE_FIELD_TIME);


    /*------------------------------------------------------
     * 第十九步：通过正式 UI Service 提交数据
     *
     * UI 服务会将补丁合并到骑行数据模型。
     *
     * 本函数不直接操作任何 LVGL 标签或对象。
     *-----------------------------------------------------*/

    if (!ui_stm32_services_submit_ride_patch(
            &patch,
            fields
        ))
    {
        return;                               // 提交失败，本轮不记录成功状态
    }


    /*------------------------------------------------------
     * 第二十步：同步 UI 状态栏
     *-----------------------------------------------------*/

    if (ui_app_state_get(&app_state))
    {
        /*
         * 卫星数量改变时才更新状态栏。
         *
         * 不改变 gps_enabled：
         * 它表示 GNSS 功能开关，不等同于是否定位成功。
         */
        if (app_state.satellites != visible_satellites)
        {
            state_patch.gps_enabled =
                app_state.gps_enabled;

            state_patch.satellites =
                visible_satellites;

            (void)ui_stm32_services_submit_state(
                &state_patch,
                UI_STATE_ADAPTER_SATELLITES
            );
        }


        /*--------------------------------------------------
         * 第二十一步：更新时间
         *
         * ui_app_state_set_clock() 是已有的 UI API。
         * 当前函数运行在 UITask，因此允许调用。
         *-------------------------------------------------*/

        if (utc_ok)
        {
            if (!app_state.clock_valid ||
                app_state.clock_hour != patch.clock_hour ||
                app_state.clock_minute != patch.clock_minute)
            {
                ui_app_state_set_clock(
                    patch.clock_hour,
                    patch.clock_minute
                );
            }

            s_clock_owned = true;
        }
        else if (s_clock_owned &&
                 app_state.clock_valid)
        {
            /*
             * 当前 GNSS 校时源已经失效。
             *
             * 保留电池状态，
             * 只将时钟有效性标记清除。
             *
             * 后续 RTC 服务接入后，
             * 可以继续使用已经校准的 RTC 时间。
             */
            ui_app_state_set_status_validity(
                false,
                app_state.battery_valid
            );
        }
    }


    /*------------------------------------------------------
     * 第二十二步：记录本次已经提交的 GNSS 状态
     *
     * 作为下一次轮询时的变化判断基准。
     *-----------------------------------------------------*/

    s_last_rmc_ms = gnss.updated_ms;

    s_last_gga_ms = gnss.altitude_updated_ms;

    s_last_satellites = visible_satellites;

    s_last_fix = fix_ok;

    s_last_altitude = altitude_ok;

    s_last_time = utc_ok;

    s_published = true;
}
