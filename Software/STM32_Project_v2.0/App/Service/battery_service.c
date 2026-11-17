/**
 * @file    battery_service.c
 * @brief   公路车码表电池电量与充电状态管理服务
 *
 * @details
 *
 * 硬件平台：STM32H743IIT6
 *
 * 电池电压：
 * BAT+ → R34/R36 电阻分压 → PC4 → ADC1
 *
 * 充电状态：
 * BQ24075 STAT_N  → PH4
 * BQ24075 PGOOD_N → PH5
 *
 * 软件架构：
 * SensorTask → BatteryService → UI Task
 *
 * 主要功能：
 * 1. 定时读取真实电池电压
 * 2. 一阶数字滤波
 * 3. 电压查表与电量百分比估算
 * 4. 充电状态 GPIO 消抖
 * 5. 低电量阈值及滞回
 * 6. ADC 失败和数据过期处理
 * 7. FreeRTOS 任务安全的状态快照
 *
 * 使用约束：
 * BatteryService_Init() 和 Process() 只能由 SensorTask 调用。
 * GetSnapshot() 可供正常任务读取。
 * 不允许在中断中调用这些接口。
 * 本模块不直接调用 LVGL API。
 */

#include "battery_service.h"                    // 引入电池管理服务公开接口
#include "bsp_adc.h"                            // 引入已完成的 ADC 底层驱动
#include "stm32h7xx_hal.h"                      // 提供 HAL GPIO 和时间相关接口
#include "FreeRTOS.h"                           // 引入 FreeRTOS 基础定义
#include "task.h"                               // 引入 FreeRTOS 任务和临界区 API


/*=============================================================
 * 一、基本参数
 *=============================================================*/

/* 5 秒没有成功采样，就认为旧电压已经过期 */
#define BATTERY_STALE_TIMEOUT_MS       (5000U)

/* 充电状态连续稳定 5 个 SensorTask 周期才更新 */
#define BATTERY_GPIO_DEBOUNCE_SAMPLES  (5U)

/* 电量百分比连续出现 3 次一致的新目标才更新 */
#define BATTERY_SOC_DEBOUNCE_SAMPLES   (3U)

/* ADC 电池电压的粗略合理范围，用于排除明显异常值 */
#define BATTERY_VALID_MIN_MV           (2300U)
#define BATTERY_VALID_MAX_MV           (4400U)

/* 低电量进入和退出阈值 */
#define BATTERY_LOW_ENTER_MV           (3500U)
#define BATTERY_LOW_EXIT_MV            (3600U)

/* 严重低电量进入和退出阈值 */
#define BATTERY_CRIT_ENTER_MV          (3300U)
#define BATTERY_CRIT_EXIT_MV           (3400U)

/* 使用 Q8 定点数保存滤波电压 */
#define BATTERY_FILTER_SCALE           (256U)

/* 一阶滤波系数 = 1/4 */
#define BATTERY_FILTER_DIVISOR         (4)


/*=============================================================
 * 二、单节锂电池电压与电量估算表
 *
 * 注意：
 * 这是一条用于普通 4.20V 满充锂离子电池的初始估算曲线。
 *
 * 并非针对你的实际电芯测得的标定表。
 * 电池型号、充放电电流、温度和老化都会影响结果。
 *
 * 后续只需替换本表，就能更新估算模型。
 * 不需要修改 BSP、FreeRTOS 或 UI 代码。
 *=============================================================*/

typedef struct
{
    uint16_t mv;                                 // 电池电压，单位 mV

    uint8_t percent;                             // 此电压对应的估算百分比

} battery_soc_point_t;


static const battery_soc_point_t s_soc_table[] =
{
    {3000U,   0U},                               // 3.00V 对应约 0%

    {3300U,   5U},                               // 3.30V 对应约 5%

    {3450U,  10U},                               // 3.45V 对应约 10%

    {3600U,  20U},                               // 3.60V 对应约 20%

    {3700U,  35U},                               // 3.70V 对应约 35%

    {3750U,  45U},                               // 3.75V 对应约 45%

    {3800U,  55U},                               // 3.80V 对应约 55%

    {3850U,  65U},                               // 3.85V 对应约 65%

    {3900U,  75U},                               // 3.90V 对应约 75%

    {4000U,  88U},                               // 4.00V 对应约 88%

    {4100U,  96U},                               // 4.10V 对应约 96%

    {4200U, 100U}                                // 4.20V 对应约 100%
};


/*=============================================================
 * 三、服务内部状态
 *
 * static 表示这些变量只属于当前 C 文件。
 *
 * SensorTask：
 * 唯一执行采样和状态计算的任务。
 *
 * UITask：
 * 只允许通过 GetSnapshot() 读取快照。
 *
 * 通过 FreeRTOS 临界区保护跨任务读取。
 *=============================================================*/

static battery_service_snapshot_t s_snapshot;   // 当前对外发布的完整电池状态

static bool s_initialized = false;              // 服务是否已初始化

static bool s_filter_initialized = false;       // 数字滤波器是否已有初始电压

static bool s_soc_initialized = false;          // 电量百分比是否已完成首次估算

static uint32_t s_filtered_q8 = 0U;             // 采用 Q8 格式保存滤波结果

static uint32_t s_last_attempt_ms = 0U;         // 上一次执行 ADC 测量的时间

static battery_power_state_t s_power_candidate =
    BATTERY_POWER_UNKNOWN;                     // 尚未经过消抖的候选供电状态

static uint8_t s_power_count = 0U;              // 候选供电状态连续出现的次数

static uint8_t s_soc_candidate = 0U;            // 候选电量百分比

static uint8_t s_soc_count = 0U;                // 候选百分比连续出现的次数


/*=============================================================
 * 四、电压查表及线性插值
 *=============================================================*/

/**
 * @brief 将电池电压转换成估算电量百分比
 *
 * @param mv 电池电压，单位 mV
 *
 * @return 0～100 的估算百分比
 */
static uint8_t BatteryService_VoltageToPercent(uint32_t mv)
{
    const uint32_t count =                       // 计算电压查表的总项数
        (uint32_t)(sizeof(s_soc_table) /
                   sizeof(s_soc_table[0]));

    uint32_t i;                                  // 定义查表循环变量


    /* 电压低于查表下限，返回最小值 */

    if (mv <= s_soc_table[0].mv)
    {
        return s_soc_table[0].percent;
    }


    /* 电压高于查表上限，返回最大值 */

    if (mv >= s_soc_table[count - 1U].mv)
    {
        return s_soc_table[count - 1U].percent;
    }


    /* 在表中查找当前电压所在区间 */

    for (i = 1U; i < count; ++i)
    {
        if (mv <= s_soc_table[i].mv)              // 找到当前电压对应的插值区间
        {
            const uint32_t dv =                   // 计算区间电压跨度
                s_soc_table[i].mv -
                s_soc_table[i - 1U].mv;

            const uint32_t dp =                   // 计算区间百分比跨度
                s_soc_table[i].percent -
                s_soc_table[i - 1U].percent;

            const uint32_t offset =               // 计算当前电压距离区间下限的偏移
                mv - s_soc_table[i - 1U].mv;

            return (uint8_t)(                     // 使用整数线性插值并四舍五入
                s_soc_table[i - 1U].percent +
                (offset * dp + dv / 2U) / dv
            );
        }
    }

    return 100U;                                 // 防御性返回，正常情况不会到达
}


/*=============================================================
 * 五、读取 BQ24075 供电与充电引脚
 *=============================================================*/

/**
 * @brief 读取当前瞬时供电状态
 *
 * PH5：PGOOD_N
 * 低电平表示有效外部输入。
 *
 * PH4：STAT_N
 * 低电平表示充电芯片正在输出充电指示。
 *
 * 注意：
 * 函数返回的是瞬时状态。
 * 后续 Process() 还会进行消抖。
 */
static battery_power_state_t BatteryService_ReadPowerPins(void)
{
    const bool input_good =                       // 判断外部输入是否有效
        (HAL_GPIO_ReadPin(GPIOH, GPIO_PIN_5)
         == GPIO_PIN_RESET);

    const bool charge_indicated =                 // 判断充电指示是否被拉低
        (HAL_GPIO_ReadPin(GPIOH, GPIO_PIN_4)
         == GPIO_PIN_RESET);


    if (input_good && charge_indicated)          // 外部电源有效且有充电指示
    {
        return BATTERY_POWER_CHARGING;           // 报告充电指示有效
    }

    if (input_good)                              // 外部电源有效，充电指示未拉低
    {
        return BATTERY_POWER_EXTERNAL;           // 不能直接认定电池已充满
    }

    if (!charge_indicated)                       // 无有效外部输入且没有充电指示
    {
        return BATTERY_POWER_BATTERY;            // 当前未检测到外部输入电源
    }

    return BATTERY_POWER_UNKNOWN;                // 不一致状态，暂时报告未知
}


/*=============================================================
 * 六、电池服务初始化
 *=============================================================*/

/**
 * @brief 初始化电池管理服务
 *
 * @note
 * 只在 SensorTask 启动时调用一次。
 */
void BatteryService_Init(void)
{
    battery_service_snapshot_t initial = {0};    // 创建初始电池状态

    bsp_adc_status_t result;                     // 保存 BSP ADC 初始化结果


    if (s_initialized)                           // 检查服务是否已经初始化
    {
        return;                                  // 已初始化，不重复清除状态
    }


    s_filter_initialized = false;               // 滤波器等待第一笔有效数据

    s_soc_initialized = false;                  // 电量估算等待第一笔有效数据

    s_filtered_q8 = 0U;                         // 清除滤波累积值

    s_last_attempt_ms =                          // 让第一次 Process 可以立即执行采样
        HAL_GetTick() -
        BATTERY_SERVICE_SAMPLE_PERIOD_MS;

    s_power_candidate = BATTERY_POWER_UNKNOWN;   // 初始化供电消抖候选状态

    s_power_count = 0U;                          // 清除供电状态消抖计数

    s_soc_candidate = 0U;                        // 清除电量百分比候选值

    s_soc_count = 0U;                            // 清除百分比消抖计数

    initial.power_state = BATTERY_POWER_UNKNOWN; // 初始化为未知供电状态

    initial.revision = 1U;                       // 版本号 0 保留为尚未初始化状态


    /* 执行 ADC BSP 初始化与校准 */

    result = BSP_ADC_Init();                     // 调用之前已经编写的 ADC 驱动

    if (result != BSP_ADC_STATUS_OK)             // 如果硬件初始化或校准失败
    {
        initial.error_count = 1U;               // 记录一次初始化异常
    }


    /* 任务安全地发布初始状态 */

    taskENTER_CRITICAL();                        // 进入 FreeRTOS 临界区

    s_snapshot = initial;                        // 将初始状态写入共享快照

    s_initialized = true;                        // 标记服务软件初始化完成

    taskEXIT_CRITICAL();                         // 退出 FreeRTOS 临界区
}


/*=============================================================
 * 七、电池管理周期处理
 *=============================================================*/

/**
 * @brief 执行一次电池服务周期处理
 *
 * @param now_ms 当前 HAL 毫秒时间戳
 *
 * @details
 * 建议由 SensorTask 每 100ms 调用一次。
 *
 * 具体任务：
 * 1. 检查供电 GPIO 并消抖
 * 2. 每 1000ms 读取一次电池 ADC
 * 3. 电压滤波
 * 4. 电量百分比估算
 * 5. 低电压判断
 * 6. 处理数据过期
 * 7. 发布最新电池状态
 */
void BatteryService_Process(uint32_t now_ms)
{
    battery_service_snapshot_t next;            // 保存本轮准备发布的新状态

    battery_power_state_t instant;              // 保存供电 GPIO 的瞬时状态

    bool changed = false;                       // 标记本轮是否需要发布快照


    /*=========================================================
     * 7.1 读取当前状态
     *=========================================================*/

    if (!BatteryService_GetSnapshot(&next))     // 获取上一轮完整电池状态
    {
        return;                                 // 尚未初始化，退出本次处理
    }


    /*=========================================================
     * 7.2 读取充电状态 GPIO
     *=========================================================*/

    instant = BatteryService_ReadPowerPins();    // 读取 PH4、PH5 瞬时状态


    if (instant == s_power_candidate)           // 判断当前状态是否与上一轮一致
    {
        if (s_power_count <
            BATTERY_GPIO_DEBOUNCE_SAMPLES)      // 判断稳定次数是否达到上限
        {
            ++s_power_count;                    // 连续状态一致，计数加一
        }
    }
    else                                        // 如果 GPIO 状态发生变化
    {
        s_power_candidate = instant;            // 更新候选供电状态

        s_power_count = 1U;                     // 从第一次观察开始重新计数
    }


    /* 只有稳定达到设定次数后，才更新正式状态 */

    if (s_power_count >=
            BATTERY_GPIO_DEBOUNCE_SAMPLES &&
        next.power_state != instant)
    {
        next.power_state = instant;             // 保存经过消抖的供电状态

        next.external_power =                   // 判断是否存在有效外部电源
            (instant == BATTERY_POWER_EXTERNAL ||
             instant == BATTERY_POWER_CHARGING);

        next.charging =                         // 只有 CHG# 拉低才报告充电指示
            (instant == BATTERY_POWER_CHARGING);

        changed = true;                        // 供电状态改变，需要发布更新
    }


    /*=========================================================
     * 7.3 判断 ADC 采样时间
     *
     * 使用无符号减法计算时间间隔，
     * 正常调度下能够适应 HAL Tick 回绕。
     *=========================================================*/

    if ((uint32_t)(now_ms - s_last_attempt_ms) >=
        BATTERY_SERVICE_SAMPLE_PERIOD_MS)
    {
        bsp_adc_battery_data_t reading = {0};   // 创建本次 ADC 测量结果

        bsp_adc_status_t status;                // 保存底层 ADC 返回状态

        bool sample_ok = false;                 // 标记本次电池测量是否有效


        s_last_attempt_ms = now_ms;             // 更新最近一次采样尝试的时间


        /*=====================================================
         * 7.4 确保 ADC 已经完成初始化
         *=====================================================*/

        if (!BSP_ADC_IsReady())                 // 判断底层 ADC 是否已校准
        {
            (void)BSP_ADC_Init();               // 未准备好则重新尝试校准
        }


        /*=====================================================
         * 7.5 获取真实电池电压
         *=====================================================*/

        if (BSP_ADC_IsReady())                  // 只有 ADC 就绪才读取电压
        {
            status = BSP_ADC_ReadBattery(       // 调用底层 ADC 电池采样函数
                &reading
            );

            sample_ok =                         // 同时检查转换状态和电压合理范围
                (status == BSP_ADC_STATUS_OK &&
                 reading.battery_mv >= BATTERY_VALID_MIN_MV &&
                 reading.battery_mv <= BATTERY_VALID_MAX_MV);
        }


        /*=====================================================
         * 7.6 有效 ADC 数据处理
         *=====================================================*/

        if (sample_ok)
        {
            const uint32_t raw_q8 =             // 将电池毫伏值转成 Q8 定点值
                reading.battery_mv *
                BATTERY_FILTER_SCALE;


            /* 第一笔有效采样直接初始化滤波器 */

            if (!s_filter_initialized)
            {
                s_filtered_q8 = raw_q8;         // 避免滤波从 0mV 缓慢上升

                s_filter_initialized = true;    // 标记滤波器已经初始化
            }
            else                                // 后续数据执行一阶滤波
            {
                const int32_t error =           // 计算当前电压与滤波电压的有符号误差
                    (int32_t)raw_q8 -
                    (int32_t)s_filtered_q8;

                s_filtered_q8 =                // 执行 α=1/4 的一阶指数滤波
                    (uint32_t)(
                        (int32_t)s_filtered_q8 +
                        error / BATTERY_FILTER_DIVISOR
                    );
            }


            /* 保存原始电压和滤波电压 */

            next.battery_raw_mv =               // 保存原始电池电压
                reading.battery_mv;

            next.battery_filtered_mv =          // 将 Q8 定点值转换回毫伏整数
                (s_filtered_q8 +
                 BATTERY_FILTER_SCALE / 2U) /
                BATTERY_FILTER_SCALE;

            next.last_valid_ms = now_ms;        // 记录最近一次成功采样时间

            next.voltage_valid = true;          // 当前电池电压有效


            /*=================================================
             * 7.7 计算剩余电量百分比
             *=================================================*/

            {
                const uint8_t estimate =        // 根据滤波电压查表估算百分比
                    BatteryService_VoltageToPercent(
                        next.battery_filtered_mv
                    );


                if (!s_soc_initialized)        // 如果是第一次得到有效电量估算
                {
                    next.percent = estimate;    // 首次直接采用当前估算值

                    s_soc_initialized = true;   // 标记电量估算已初始化

                    s_soc_candidate = estimate; // 保存当前候选百分比

                    s_soc_count = 0U;           // 清除消抖计数
                }
                else if (estimate == next.percent) // 估算值与当前显示值相同
                {
                    s_soc_candidate = estimate; // 更新候选值

                    s_soc_count = 0U;           // 不需要切换电量显示
                }
                else                            // 估算电量发生变化
                {
                    if (estimate != s_soc_candidate)
                    {
                        s_soc_candidate = estimate; // 记录新的候选百分比

                        s_soc_count = 1U;       // 从第一次稳定观察开始
                    }
                    else if (
                        s_soc_count <
                        BATTERY_SOC_DEBOUNCE_SAMPLES
                    )
                    {
                        ++s_soc_count;          // 候选值连续出现，增加计数
                    }


                    if (s_soc_count >=
                        BATTERY_SOC_DEBOUNCE_SAMPLES)
                    {
                        next.percent = estimate; // 采用经过消抖的新百分比

                        s_soc_count = 0U;      // 清除计数，准备下一次变化
                    }
                }
            }


            /*=================================================
             * 7.8 低电池电压判断
             *
             * 使用两个不同阈值形成滞回区间。
             *=================================================*/

            if (!next.low_battery &&
                next.battery_filtered_mv <=
                BATTERY_LOW_ENTER_MV)
            {
                next.low_battery = true;        // 电压下降到进入阈值，置位低电量
            }
            else if (next.low_battery &&
                     next.battery_filtered_mv >=
                     BATTERY_LOW_EXIT_MV)
            {
                next.low_battery = false;       // 电压恢复超过退出阈值，解除低电量
            }


            /*=================================================
             * 7.9 严重低电池电压判断
             *=================================================*/

            if (!next.critical_battery &&
                next.battery_filtered_mv <=
                BATTERY_CRIT_ENTER_MV)
            {
                next.critical_battery = true;   // 进入严重低电压区间
            }
            else if (next.critical_battery &&
                     next.battery_filtered_mv >=
                     BATTERY_CRIT_EXIT_MV)
            {
                next.critical_battery = false;  // 电压充分恢复，解除严重低电压
            }
        }
        else                                    // ADC 失败或电压明显不合理
        {
            ++next.error_count;                 // 增加错误计数

            /* 不把错误值写成 0mV */
            /* 不把电量百分比直接改成 0% */
            /* 保留之前的有效数值，等待数据过期判断 */
        }

        changed = true;                         // 每次 ADC 采样尝试均发布诊断状态
    }


    /*=========================================================
     * 7.10 检查电池数据是否过期
     *
     * 如果超过 5 秒没有成功采样：
     * 电池电量标记为未知，不再将旧值作为当前有效值。
     *=========================================================*/

    if (next.voltage_valid &&
        (uint32_t)(now_ms - next.last_valid_ms) >=
        BATTERY_STALE_TIMEOUT_MS)
    {
        next.voltage_valid = false;             // 将电池数据标记为过期

        next.low_battery = false;               // 不使用过期电压输出新的低电量结论

        next.critical_battery = false;          // 不使用过期电压输出新的严重低电量结论

        changed = true;                         // 通知消费者状态有效性发生变化
    }


    /*=========================================================
     * 7.11 发布最新完整电池快照
     *
     * 临界区内只进行结构体复制。
     * 不在临界区中执行 ADC、延时或 LVGL 操作。
     *=========================================================*/

    if (changed)
    {
        taskENTER_CRITICAL();                   // 进入 FreeRTOS 临界区

        next.revision =                         // 在当前快照版本号基础上加 1
            s_snapshot.revision + 1U;

        if (next.revision == 0U)                // 避免回绕到未初始化标记 0
        {
            next.revision = 1U;                 // 将版本号恢复为有效范围
        }

        s_snapshot = next;                     // 一次性发布所有电池状态字段

        taskEXIT_CRITICAL();                    // 退出 FreeRTOS 临界区
    }
}


/*=============================================================
 * 八、获取电池状态快照
 *=============================================================*/

/**
 * @brief 任务安全地读取最新电池状态
 *
 * @param out 输出结构体指针
 *
 * @return true 成功
 * @return false 失败
 */
bool BatteryService_GetSnapshot(
    battery_service_snapshot_t *out
)
{
    bool ready;                                  // 保存服务初始化状态


    if (out == NULL)                            // 判断输出指针是否合法
    {
        return false;                           // 无效地址，直接返回
    }


    taskENTER_CRITICAL();                       // 进入临界区，防止复制过程中被任务切换打断

    ready = s_initialized;                      // 读取当前服务初始化状态

    if (ready)                                  // 只有初始化完成后才能读取状态
    {
        *out = s_snapshot;                      // 将完整结构体复制给调用者
    }

    taskEXIT_CRITICAL();                        // 退出临界区


    return ready;                               // 返回本次读取结果
}
