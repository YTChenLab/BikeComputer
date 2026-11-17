#ifndef BSP_ADC_H                              // 防止头文件被重复包含
#define BSP_ADC_H                              // 定义头文件保护宏

#include <stdint.h>                            // 提供 uint8_t、uint16_t、uint32_t 等标准整数类型
#include <stdbool.h>                           // 提供 bool、true 和 false 类型

#ifdef __cplusplus                             // 如果工程使用 C++ 编译器
extern "C" {                                   // 告诉 C++ 编译器按照 C 语言规则链接这些函数
#endif

/**
 * @brief ADC 驱动执行状态
 *
 * @details
 * BSP 不应该在发生错误时直接让整个系统死机。
 * 例如电池 ADC 采样失败，并不意味着 LVGL、GNSS 等功能必须停止。
 *
 * 因此使用枚举类型区分不同错误，让上层决定如何处理。
 */
typedef enum
{
    BSP_ADC_STATUS_OK = 0,                     // 操作成功，采样数据有效

    BSP_ADC_STATUS_NOT_READY,                  // ADC 尚未完成初始化或校准

    BSP_ADC_STATUS_BAD_ARGUMENT,               // 调用函数时传入了无效参数

    BSP_ADC_STATUS_CALIBRATION_FAILED,         // ADC 硬件自校准失败

    BSP_ADC_STATUS_START_FAILED,               // ADC 启动转换失败

    BSP_ADC_STATUS_TIMEOUT,                    // ADC 等待转换完成时超时

    BSP_ADC_STATUS_CONVERSION_FAILED,          // ADC 转换过程中发生其他错误

    BSP_ADC_STATUS_STOP_FAILED                 // ADC 停止转换失败

} bsp_adc_status_t;


/**
 * @brief 电池 ADC 采样结果
 *
 * @details
 * 本结构体只保存 ADC 原始值及换算后的物理电压。
 *
 * 不在这里计算电量百分比，原因是：
 * 1. ADC 属于硬件采样层。
 * 2. 电量百分比属于电池业务管理层。
 * 3. 未来可以增加其他 ADC 通道，而无需改变电池业务逻辑。
 */
typedef struct
{
    uint16_t raw_average;                      // 多次 ADC 采样后的平均原始值，范围 0～65535

    uint32_t adc_input_mv;                     // STM32 PC4 引脚实际测量电压，单位 mV

    uint32_t battery_mv;                       // 根据电阻分压比计算的电池电压，单位 mV

    uint8_t sample_count;                      // 本次测量实际参与平均的 ADC 样本数量

    uint32_t timestamp_ms;                     // 本次测量完成时的 HAL 系统时间，单位 ms

} bsp_adc_battery_data_t;


/**
 * @brief 初始化 BSP ADC 驱动
 *
 * @details
 * 调用前必须已经执行 MX_ADC1_Init()。
 *
 * 本函数主要执行 STM32H743 的 ADC 自校准。
 *
 * 只能在 ADC 没有运行转换时执行。
 * 同一 ADC 不允许多个任务同时操作。
 *
 * @return BSP_ADC_STATUS_OK 初始化成功
 * @return 其他状态值表示初始化失败
 */
bsp_adc_status_t BSP_ADC_Init(void);


/**
 * @brief 采集当前电池电压
 *
 * @details
 * 内部连续进行 8 次独立 ADC 转换。
 * 计算平均原始值，再根据参考电压和分压电阻计算电池电压。
 *
 * 此函数属于同步阻塞式采样。
 * 只能在普通任务上下文调用，不允许在中断中调用。
 *
 * @param data 用于保存采样结果的结构体指针
 *
 * @return BSP_ADC_STATUS_OK 数据有效
 * @return 其他状态值表示读取失败
 */
bsp_adc_status_t BSP_ADC_ReadBattery(bsp_adc_battery_data_t *data);


/**
 * @brief 查询 ADC BSP 是否完成初始化
 *
 * @return true  ADC 已完成校准，可以读取
 * @return false ADC 尚未准备好
 */
bool BSP_ADC_IsReady(void);


#ifdef __cplusplus                             // 如果使用 C++ 编译器
}                                              // 结束 C 语言链接声明
#endif

#endif                                         // BSP_ADC_H