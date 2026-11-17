/**
 * @file    bsp_spl06.h
 * @brief   SPL06-001 数字气压计底层驱动接口
 *
 * @details
 * 本文件负责：
 * 1. 定义 SPL06 初始化和测量状态码。
 * 2. 定义气压、温度测量结果结构体。
 * 3. 声明芯片初始化、测量和状态查询接口。
 *
 * 软件分层：
 * SensorTask -> SensorService -> BSP_SPL06 -> HAL I2C1
 *
 * 调用约束：
 * 1. SPL06 驱动由 SensorTask 独占调用。
 * 2. 不允许在中断中执行 I2C 寄存器访问。
 * 3. 本驱动不直接操作 LVGL。
 */

#ifndef BSP_SPL06_H                           // 防止头文件重复包含
#define BSP_SPL06_H                           // 定义头文件保护宏

#include "stm32h7xx_hal.h"                    // STM32 HAL 类型及 I2C 接口
#include <stdbool.h>                         // bool、true、false
#include <stdint.h>                          // 固定宽度整数类型

#ifdef __cplusplus                           // 兼容 C++ 编译环境
extern "C" {                                 // 使用 C 语言链接规则
#endif


/* ---------- 1. SPL06 驱动状态定义 ---------- */

typedef enum
{
    BSP_SPL06_STATUS_OK = 0,                 // 操作成功

    BSP_SPL06_STATUS_NOT_READY,              // 设备或校准数据尚未准备好

    BSP_SPL06_STATUS_NO_DEVICE,              // I2C 总线上未找到 SPL06

    BSP_SPL06_STATUS_BAD_ID,                 // 设备应答，但产品 ID 不匹配

    BSP_SPL06_STATUS_I2C_ERROR,              // I2C 读写失败

    BSP_SPL06_STATUS_NO_SAMPLE,              // 当前没有新的完整测量数据

    BSP_SPL06_STATUS_DATA_INVALID,           // 补偿后的测量值超出合理范围

    BSP_SPL06_STATUS_INVALID_ARG             // 函数传入的指针无效

} bsp_spl06_status_t;


/* ---------- 2. SPL06 测量结果结构体 ---------- */

typedef struct
{
    float pressure_pa;                       // 补偿气压，单位 Pa

    float temperature_c;                     // 补偿温度，单位 °C

    uint32_t sampled_at_ms;                  // 完成采集时的系统毫秒时间戳

} bsp_spl06_sample_t;


/* ---------- 3. SPL06 底层驱动接口 ---------- */

/**
 * @brief 初始化 SPL06 气压计
 *
 * @param hi2c CubeMX 创建的 I2C 句柄
 *
 * @return 驱动状态码
 *
 * @note
 * 必须在 MX_I2C1_Init() 完成后调用。
 * 初始化过程不使用长时间延时；
 * 芯片尚未就绪时返回 NOT_READY，由 SensorService 稍后重试。
 */
bsp_spl06_status_t BSP_SPL06_Init(
    I2C_HandleTypeDef *hi2c
);


/**
 * @brief 读取一次最新的补偿气压和温度
 *
 * @param out 输出测量结果的结构体地址
 *
 * @return 驱动状态码
 *
 * @note
 * NO_SAMPLE 表示没有新测量，不属于通信错误。
 */
bsp_spl06_status_t BSP_SPL06_Read(
    bsp_spl06_sample_t *out
);


/**
 * @brief 查询 SPL06 是否已经初始化成功
 */
bool BSP_SPL06_IsReady(void);


/**
 * @brief 获取当前识别到的 7 位 I2C 地址
 *
 * @return 0x76、0x77，或者未识别时的 0
 */
uint8_t BSP_SPL06_GetAddress(void);


/**
 * @brief 获取芯片产品及修订版本 ID
 */
uint8_t BSP_SPL06_GetChipId(void);


#ifdef __cplusplus                           // 结束 C++ 兼容声明
}                                              // 结束 extern "C"
#endif

#endif /* BSP_SPL06_H */                     // 结束头文件保护
