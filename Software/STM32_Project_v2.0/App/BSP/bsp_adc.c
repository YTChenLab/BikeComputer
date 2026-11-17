/**
 * @file    bsp_adc.c
 * @brief   STM32H743 ADC 底层采集驱动
 *
 * @details
 * 硬件平台：STM32H743IIT6
 *
 * ADC 外设：ADC1
 * ADC 输入：PC4 / ADC1_INP4
 * ADC 分辨率：16 bit
 * ADC 时钟：16 MHz
 * ADC 采样时间：387.5 Cycles
 *
 * 电池采样电路：
 * BAT+ → R34(100kΩ) → BAT_ADC → R37(1kΩ) → PC4
 *                          |
 *                       R36(100kΩ)
 *                          |
 *                         GND
 *
 * BAT_ADC 节点另有 C66(100nF) 对地滤波。
 *
 * 驱动功能：
 * 1. ADC 硬件自校准
 * 2. 单次转换
 * 3. 转换超时处理
 * 4. 多次采样平均
 * 5. ADC 原始值转毫伏
 * 6. 电阻分压补偿
 *
 * 使用约束：
 * ADC1 由 Sensor Task 独占访问。
 * 禁止在中断中调用采样函数。
 * 禁止多个 FreeRTOS Task 同时操作 ADC1。
 */

#include "bsp_adc.h"                           // 引入 ADC BSP 对外接口
#include "adc.h"                               // 引入 CubeMX 生成的 ADC HAL 配置及 hadc1
#include <string.h>                            // 使用 memset() 清空结构体
#include <stdbool.h>       										 // bool、true、false


/* ============================================================
 * 一、ADC 硬件参数
 * ============================================================
 */

/* 每次测量执行 8 次 ADC 转换 */
#define BSP_ADC_SAMPLE_COUNT       (8U)

/* 单次 ADC 转换的最大等待时间，单位 ms */
#define BSP_ADC_TIMEOUT_MS         (10U)

/*
 * ADC 参考电压，单位 mV。
 *
 * 当前先按照板载 3.3V 模拟参考电压计算。
 * 该值是标称值，不是经过实测校准的精确值。
 *
 * 后续可以使用 VREFINT 或实际测得的 VREF+
 * 完善电压精度校准。
 */
#define BSP_ADC_VREF_MV            (3300U)

/* R34：电池正极到采样节点的上拉分压电阻 */
#define BSP_ADC_R_TOP_OHM          (100000U)

/* R36：采样节点到 GND 的下拉分压电阻 */
#define BSP_ADC_R_BOTTOM_OHM       (100000U)

/* 16-bit ADC 最大输出值 = 2^16 - 1 */
#define BSP_ADC_MAX_RAW            (65535U)


/* ============================================================
 * 二、驱动内部状态
 * ============================================================
 */

/*
 * ADC 初始化完成标志。
 *
 * static：
 * 限制变量只能被当前 bsp_adc.c 文件访问。
 *
 * false：
 * 系统刚启动时，ADC 尚未完成 BSP 校准。
 *
 * 这个变量在校准完成后置为 true。
 */
static bool s_adc_ready = false;


/* ============================================================
 * 三、ADC 单次转换函数
 * ============================================================
 */

/**
 * @brief 读取一次 ADC 原始转换结果
 *
 * @param raw 用于保存 16-bit ADC 原始值的指针
 *
 * @return ADC 执行状态
 *
 * @note
 * 本函数使用 static 修饰，因此不对其他 C 文件开放。
 * 外部模块必须通过 BSP_ADC_ReadBattery() 读取。
 */
static bsp_adc_status_t BSP_ADC_ReadOne(uint16_t *raw)
{
    HAL_StatusTypeDef hal_result;              // 保存 HAL 函数返回状态

    uint32_t conversion;                      // 保存 HAL_ADC_GetValue 返回的原始转换结果


    /* ---------- 1. 检查函数参数 ---------- */

    if (raw == NULL)                           // 检查调用者是否传入有效存储地址
    {
        return BSP_ADC_STATUS_BAD_ARGUMENT;    // 空指针不能保存 ADC 结果，直接返回错误
    }


    /* ---------- 2. 启动 ADC 转换 ---------- */

    if (HAL_ADC_Start(&hadc1) != HAL_OK)       // 启动 ADC1 的规则通道转换
    {
        return BSP_ADC_STATUS_START_FAILED;    // 启动失败，返回错误
    }


    /* ---------- 3. 等待 ADC 转换完成 ---------- */

    hal_result = HAL_ADC_PollForConversion(     // 等待 ADC 硬件置位转换完成标志
        &hadc1,                               // 指定 ADC1 句柄
        BSP_ADC_TIMEOUT_MS                    // 设置最大等待时间为 10ms
    );

    if (hal_result != HAL_OK)                  // 判断等待转换的过程是否发生错误
    {
        (void)HAL_ADC_Stop(&hadc1);            // 尝试停止 ADC，清理当前失败的转换

        if (hal_result == HAL_TIMEOUT)         // 如果 HAL 返回的是超时错误
        {
            return BSP_ADC_STATUS_TIMEOUT;     // 向上层明确报告转换超时
        }

        return BSP_ADC_STATUS_CONVERSION_FAILED; // 其他硬件转换错误
    }


    /* ---------- 4. 读取 ADC 转换结果 ---------- */

    conversion = HAL_ADC_GetValue(&hadc1);     // 读取 ADC1 数据寄存器中的数字结果


    /* ---------- 5. 停止 ADC 转换 ---------- */

    if (HAL_ADC_Stop(&hadc1) != HAL_OK)        // 停止 ADC，确保本次转换正常结束
    {
        return BSP_ADC_STATUS_STOP_FAILED;     // 停止失败时返回错误
    }


    /* ---------- 6. 返回 ADC 原始值 ---------- */

    *raw = (uint16_t)conversion;               // 将原始数字值写入调用者提供的内存

    return BSP_ADC_STATUS_OK;                  // 本次 ADC 读取成功
}


/* ============================================================
 * 四、ADC BSP 初始化与校准
 * ============================================================
 */

/**
 * @brief 初始化 ADC BSP
 *
 * @note
 * 必须在 MX_ADC1_Init() 后调用。
 * 必须确保 ADC1 当前没有进行任何转换。
 */
bsp_adc_status_t BSP_ADC_Init(void)
{
    /* ---------- 1. 检查是否已经初始化 ---------- */

    if (s_adc_ready)                           // 判断 ADC 是否已经完成校准
    {
        return BSP_ADC_STATUS_OK;              // 已完成初始化，不重复校准
    }


    /* ---------- 2. 检查 HAL ADC 句柄 ---------- */

    if (hadc1.Instance != ADC1)                // 判断 CubeMX 是否已经绑定 ADC1
    {
        return BSP_ADC_STATUS_NOT_READY;       // ADC1 尚未准备好
    }


    /* ---------- 3. 执行 ADC 硬件校准 ---------- */

    if (HAL_ADCEx_Calibration_Start(            // 调用 STM32H7 HAL ADC 自校准接口
            &hadc1,                            // 传入 ADC1 句柄
            ADC_CALIB_OFFSET_LINEARITY,        // 执行偏移及线性校准
            ADC_SINGLE_ENDED                   // 当前 PC4 采用单端输入
        ) != HAL_OK)                           // 判断硬件校准是否成功
    {
        return BSP_ADC_STATUS_CALIBRATION_FAILED; // 校准失败，不允许继续正常采样
    }


    /* ---------- 4. 更新 ADC 初始化状态 ---------- */

    s_adc_ready = true;                        // 标记 ADC 已经完成校准

    return BSP_ADC_STATUS_OK;                  // 初始化成功
}


/* ============================================================
 * 五、ADC 状态查询
 * ============================================================
 */

/**
 * @brief 查询 ADC 是否完成初始化
 */
bool BSP_ADC_IsReady(void)
{
    return s_adc_ready;                        // 返回当前 ADC 初始化状态
}


/* ============================================================
 * 六、电池电压采集
 * ============================================================
 */

/**
 * @brief 读取电池电压
 *
 * @details
 * 1. 连续采集 8 个 ADC 样本
 * 2. 计算平均 ADC 原始值
 * 3. 根据参考电压计算 PC4 的模拟输入电压
 * 4. 根据 R34/R36 的分压比计算电池真实电压
 * 5. 保存采样时间
 *
 * @param data 保存电池测量结果的结构体指针
 *
 * @return BSP_ADC_STATUS_OK 表示本次测量有效
 */
bsp_adc_status_t BSP_ADC_ReadBattery(bsp_adc_battery_data_t *data)
{
    uint32_t sum = 0U;                         // 保存 8 次 ADC 原始值的累加结果

    uint32_t average;                          // 保存平均后的 ADC 原始值

    uint64_t numerator;                        // 使用 64-bit 整数保存公式分子，防止乘法溢出

    uint64_t denominator;                      // 使用 64-bit 整数保存公式分母

    bsp_adc_status_t status;                   // 保存单次 ADC 转换的执行结果

    uint16_t raw;                              // 保存每一次读取到的 ADC 原始值


    /* ---------- 1. 检查输出参数 ---------- */

    if (data == NULL)                          // 判断输出数据指针是否有效
    {
        return BSP_ADC_STATUS_BAD_ARGUMENT;    // 空指针无法写入测量结果
    }


    /* ---------- 2. 清空上一次测量数据 ---------- */

    memset(data, 0, sizeof(*data));            // 将数据结构体初始化为 0，避免保留过期数据


    /* ---------- 3. 检查 ADC 是否已经校准 ---------- */

    if (!s_adc_ready)                          // ADC 未完成 BSP 初始化时禁止读取
    {
        return BSP_ADC_STATUS_NOT_READY;       // 返回 ADC 未就绪状态
    }


    /* ---------- 4. 连续采集 8 次 ADC 数据 ---------- */

    for (uint32_t i = 0U;                      // 从第 0 次采样开始
         i < BSP_ADC_SAMPLE_COUNT;            // 总共执行 8 次转换
         i++)                                  // 每完成一次采样，计数器加 1
    {
        status = BSP_ADC_ReadOne(&raw);        // 调用内部函数获取一次 ADC 原始值

        if (status != BSP_ADC_STATUS_OK)       // 判断本次转换是否成功
        {
            return status;                     // 任意一次采样失败，本组测量即判定无效
        }

        sum += (uint32_t)raw;                   // 将本次原始值累加到 sum
    }


    /* ---------- 5. 计算平均 ADC 原始值 ---------- */

    average = (sum + BSP_ADC_SAMPLE_COUNT / 2U) // 加入半个除数，用于整数四舍五入
              / BSP_ADC_SAMPLE_COUNT;          // 将累计结果除以 8，计算平均 ADC 值


    /* ---------- 6. 计算 ADC 引脚实际电压 ---------- */

    numerator = (uint64_t)average              // 将原始值转换为 64-bit 后参与计算
                * (uint64_t)BSP_ADC_VREF_MV;   // ADC 原始值乘以参考电压，单位 mV

    data->adc_input_mv = (uint32_t)(           // 将 PC4 输入电压保存为 32-bit 毫伏值
        (numerator + BSP_ADC_MAX_RAW / 2U)     // 对整数除法结果进行四舍五入
        / (uint64_t)BSP_ADC_MAX_RAW            // 除以 16-bit ADC 最大数字值 65535
    );


    /* ---------- 7. 计算电池实际电压 ---------- */

    numerator = (uint64_t)average              // ADC 平均原始值
                * (uint64_t)BSP_ADC_VREF_MV   // 乘以 ADC 参考电压
                * (                                  // 乘以分压补偿系数的分子
                    (uint64_t)BSP_ADC_R_TOP_OHM
                    + (uint64_t)BSP_ADC_R_BOTTOM_OHM
                  );

    denominator = (uint64_t)BSP_ADC_MAX_RAW    // ADC 满量程数字值
                  * (uint64_t)BSP_ADC_R_BOTTOM_OHM; // 乘以下拉分压电阻阻值

    data->battery_mv = (uint32_t)(             // 保存最终计算得到的电池电压
        (numerator + denominator / 2U)         // 对整数除法进行四舍五入
        / denominator                          // 分子除以分母得到电池毫伏值
    );


    /* ---------- 8. 保存本次测量的辅助信息 ---------- */

    data->raw_average = (uint16_t)average;     // 保存 8 次平均后的 ADC 原始结果

    data->sample_count = (uint8_t)BSP_ADC_SAMPLE_COUNT; // 保存参与计算的样本数量

    data->timestamp_ms = HAL_GetTick();        // 记录本次测量完成时的系统毫秒计数


    /* ---------- 9. 返回成功状态 ---------- */

    return BSP_ADC_STATUS_OK;                  // 全部采样和计算成功
}