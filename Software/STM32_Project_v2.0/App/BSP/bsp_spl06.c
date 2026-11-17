/**
 * @file    bsp_spl06.c
 * @brief   SPL06-001 数字气压计 I2C 底层驱动
 *
 * @details
 * 主要功能：
 * 1. 自动识别 I2C 地址 0x76 / 0x77。
 * 2. 读取并校验 SPL06 产品 ID。
 * 3. 等待传感器及校准系数准备完成。
 * 4. 读取芯片内部的温度、气压校准系数。
 * 5. 配置气压及温度连续测量模式。
 * 6. 读取原始测量值并计算补偿气压、温度。
 *
 * 本版本配置：
 * 气压测量：4Hz、64 倍过采样。
 * 温度测量：4Hz、单次采样、外部 MEMS 温度源。
 *
 * 调用约束：
 * 由 SensorTask 通过 SensorService 独占调用。
 */

#include "bsp_spl06.h"                       // 本驱动公开接口
#include <math.h>                            // isfinite() 浮点有效性检查


/* ---------- 1. SPL06 寄存器地址定义 ---------- */

#define SPL06_REG_PRS_B2           0x00U       // 气压原始值最高字节
#define SPL06_REG_TMP_B2           0x03U       // 温度原始值最高字节

#define SPL06_REG_PRS_CFG          0x06U       // 气压采样频率及过采样配置
#define SPL06_REG_TMP_CFG          0x07U       // 温度采样频率及过采样配置
#define SPL06_REG_MEAS_CFG         0x08U       // 测量模式及状态
#define SPL06_REG_CFG             0x09U       // 移位、FIFO 和中断配置

#define SPL06_REG_ID              0x0DU       // 产品及修订版本 ID
#define SPL06_REG_COEF            0x10U       // 校准系数起始寄存器


/* ---------- 2. SPL06 I2C 配置 ---------- */

#define SPL06_I2C_ADDR_LOW        0x76U       // SDO 接地时的 7 位地址
#define SPL06_I2C_ADDR_HIGH       0x77U       // SDO 拉高时的 7 位地址

#define SPL06_I2C_TIMEOUT_MS      10U         // 单次 I2C 事务最长等待时间

#define SPL06_PRODUCT_ID_MASK     0xF0U       // 产品 ID 位于寄存器高 4 位
#define SPL06_PRODUCT_ID_VALUE    0x10U       // SPL06 产品 ID 高 4 位


/* ---------- 3. 测量状态位 ---------- */

#define SPL06_COEF_READY          0x80U       // 校准系数已准备好
#define SPL06_SENSOR_READY        0x40U       // 传感器内部初始化已完成
#define SPL06_TEMP_READY          0x20U       // 有新的温度测量
#define SPL06_PRESS_READY         0x10U       // 有新的气压测量


/* ---------- 4. 正式测量配置 ---------- */

#define SPL06_PRESS_CONFIG        0x26U       // 气压 4Hz、64 倍过采样
#define SPL06_TEMP_CONFIG         0xA0U       // 温度 4Hz、单次采样、外部温度源

#define SPL06_GENERAL_CONFIG      0x04U       // 开启气压结果移位
#define SPL06_CONTINUOUS_MODE     0x07U       // 连续测量气压和温度

#define SPL06_PRESS_SCALE         1040384.0f  // 气压 64 倍过采样的补偿比例
#define SPL06_TEMP_SCALE          524288.0f   // 温度单次采样的补偿比例


/* ---------- 5. SPL06 校准系数数据结构 ---------- */

typedef struct
{
    int16_t c0;                               // 温度补偿常数项
    int16_t c1;                               // 温度补偿一次项

    int32_t c00;                              // 气压补偿常数项
    int32_t c10;                              // 气压补偿一次项

    int16_t c01;                              // 温度对气压的补偿项
    int16_t c11;                              // 气压与温度交叉补偿项
    int16_t c20;                              // 气压补偿二次项
    int16_t c21;                              // 二次交叉补偿项
    int16_t c30;                              // 气压补偿三次项

} spl06_coefficients_t;


/* ---------- 6. SPL06 驱动内部状态 ---------- */

static I2C_HandleTypeDef *s_i2c = NULL;       // 当前绑定的 I2C 外设

static uint8_t s_address = 0U;                // 实际 I2C 设备地址，HAL 格式

static uint8_t s_chip_id = 0U;                // 读取到的产品及修订版本 ID

static bool s_ready = false;                  // SPL06 初始化完成标志

static spl06_coefficients_t s_coeff;          // 从芯片读取的补偿系数


/* ---------- 7. I2C 寄存器读取函数 ---------- */

static bsp_spl06_status_t SPL06_ReadRegisters(
    uint8_t reg,
    uint8_t *data,
    uint16_t length
)
{
    HAL_StatusTypeDef halStatus;               // 保存 HAL 返回状态

    if (s_i2c == NULL ||                     // I2C 尚未绑定
        data == NULL ||                      // 输出缓冲区无效
        length == 0U)                        // 禁止零长度读取
    {
        return BSP_SPL06_STATUS_INVALID_ARG;  // 参数不合法
    }

    halStatus = HAL_I2C_Mem_Read(
        s_i2c,                               // 当前 I2C 外设
        s_address,                           // HAL 使用左移一位的设备地址
        reg,                                 // 寄存器地址
        I2C_MEMADD_SIZE_8BIT,                // SPL06 使用 8 位寄存器地址
        data,                                // 接收缓冲区
        length,                              // 读取字节数
        SPL06_I2C_TIMEOUT_MS                 // 单次事务超时
    );

    if (halStatus != HAL_OK)                 // 判断 I2C 读取是否成功
    {
        return BSP_SPL06_STATUS_I2C_ERROR;   // 读取失败
    }

    return BSP_SPL06_STATUS_OK;              // 读取成功
}


/* ---------- 8. I2C 寄存器写入函数 ---------- */

static bsp_spl06_status_t SPL06_WriteRegister(
    uint8_t reg,
    uint8_t value
)
{
    HAL_StatusTypeDef halStatus;               // 保存 HAL 写入结果

    if (s_i2c == NULL)                       // 未绑定 I2C 不允许访问设备
    {
        return BSP_SPL06_STATUS_NOT_READY;    // 返回未就绪
    }

    halStatus = HAL_I2C_Mem_Write(
        s_i2c,                               // 当前 I2C 外设
        s_address,                           // SPL06 设备地址
        reg,                                 // 目标寄存器
        I2C_MEMADD_SIZE_8BIT,                // 8 位寄存器地址
        &value,                              // 待写入的寄存器值
        1U,                                  // 本次写入 1 字节
        SPL06_I2C_TIMEOUT_MS                 // 通信超时时间
    );

    if (halStatus != HAL_OK)                 // 判断写入是否成功
    {
        return BSP_SPL06_STATUS_I2C_ERROR;   // 返回通信错误
    }

    return BSP_SPL06_STATUS_OK;              // 寄存器写入成功
}


/* ---------- 9. 有符号补码扩展函数 ---------- */

static int32_t SPL06_SignExtend(
    uint32_t value,
    uint8_t bits
)
{
    uint32_t signBit;                         // 当前数据最高有效位

    uint32_t magnitudeMask;                   // 最高符号位以下的数据掩码

    int32_t result;                           // 最终有符号结果

    signBit = 1UL << (bits - 1U);             // 计算符号位掩码

    magnitudeMask = signBit - 1U;             // 得到低位数据掩码

    result = (int32_t)(value & magnitudeMask); // 提取符号位以下的数据

    if ((value & signBit) != 0U)              // 判断原始数值是否为负数
    {
        result -= (int32_t)signBit;           // 转换成正确的有符号值
    }

    return result;                            // 返回符号扩展结果
}


/* ---------- 10. 解析芯片内部校准系数 ---------- */

static void SPL06_ParseCoefficients(
    const uint8_t data[18]
)
{
    uint32_t raw;                             // 校准系数临时拼接变量

    /* ---------- 10.1 解析 12 位 c0 ---------- */

    raw = ((uint32_t)data[0] << 4) |         // c0 的高 8 位
          ((uint32_t)data[1] >> 4);          // c0 的低 4 位

    s_coeff.c0 = (int16_t)SPL06_SignExtend(
        raw, 12U
    );                                       // 转换成有符号 12 位系数

    /* ---------- 10.2 解析 12 位 c1 ---------- */

    raw = (((uint32_t)data[1] & 0x0FU) << 8) |
          (uint32_t)data[2];                 // 拼接 c1 的全部 12 位

    s_coeff.c1 = (int16_t)SPL06_SignExtend(
        raw, 12U
    );                                       // 转换成有符号系数

    /* ---------- 10.3 解析 20 位 c00 ---------- */

    raw = ((uint32_t)data[3] << 12) |
          ((uint32_t)data[4] << 4) |
          ((uint32_t)data[5] >> 4);          // 拼接 20 位 c00

    s_coeff.c00 = SPL06_SignExtend(
        raw, 20U
    );                                       // 转换成有符号 20 位系数

    /* ---------- 10.4 解析 20 位 c10 ---------- */

    raw = (((uint32_t)data[5] & 0x0FU) << 16) |
          ((uint32_t)data[6] << 8) |
          (uint32_t)data[7];                 // 拼接 20 位 c10

    s_coeff.c10 = SPL06_SignExtend(
        raw, 20U
    );                                       // 转换成有符号系数

    /* ---------- 10.5 解析 16 位 c01 ---------- */

    raw = ((uint32_t)data[8] << 8) |
          (uint32_t)data[9];                 // 拼接 16 位数据

    s_coeff.c01 = (int16_t)SPL06_SignExtend(
        raw, 16U
    );                                       // 转换成有符号系数

    /* ---------- 10.6 解析 16 位 c11 ---------- */

    raw = ((uint32_t)data[10] << 8) |
          (uint32_t)data[11];                // 拼接 c11

    s_coeff.c11 = (int16_t)SPL06_SignExtend(
        raw, 16U
    );                                       // 转换成有符号系数

    /* ---------- 10.7 解析 16 位 c20 ---------- */

    raw = ((uint32_t)data[12] << 8) |
          (uint32_t)data[13];                // 拼接 c20

    s_coeff.c20 = (int16_t)SPL06_SignExtend(
        raw, 16U
    );                                       // 转换成有符号系数

    /* ---------- 10.8 解析 16 位 c21 ---------- */

    raw = ((uint32_t)data[14] << 8) |
          (uint32_t)data[15];                // 拼接 c21

    s_coeff.c21 = (int16_t)SPL06_SignExtend(
        raw, 16U
    );                                       // 转换成有符号系数

    /* ---------- 10.9 解析 16 位 c30 ---------- */

    raw = ((uint32_t)data[16] << 8) |
          (uint32_t)data[17];                // 拼接 c30

    s_coeff.c30 = (int16_t)SPL06_SignExtend(
        raw, 16U
    );                                       // 转换成有符号系数
}


/* ---------- 11. 初始化 SPL06 ---------- */

bsp_spl06_status_t BSP_SPL06_Init(
    I2C_HandleTypeDef *hi2c
)
{
    static const uint8_t addresses[2] =
    {
        SPL06_I2C_ADDR_LOW,                  // 首先探测 0x76
        SPL06_I2C_ADDR_HIGH                  // 然后探测 0x77
    };

    bsp_spl06_status_t status;                // 当前操作状态

    uint8_t id = 0U;                         // 芯片 ID 寄存器

    uint8_t measStatus = 0U;                 // 芯片内部准备状态

    uint8_t calibration[18] = {0};            // 芯片内部校准系数

    bool deviceResponded = false;             // 是否有 I2C 设备应答

    /* ---------- 11.1 检查 I2C 句柄 ---------- */

    if (hi2c == NULL)                        // 禁止使用空 I2C 句柄
    {
        return BSP_SPL06_STATUS_INVALID_ARG;  // 返回参数错误
    }

    s_ready = false;                         // 初始化期间设备视为未就绪
    s_i2c = hi2c;                            // 保存 I2C 外设句柄
    s_address = 0U;                          // 清除旧设备地址
    s_chip_id = 0U;                          // 清除旧芯片 ID

    /* ---------- 11.2 自动探测 0x76 和 0x77 ---------- */

    for (uint32_t i = 0U; i < 2U; i++)        // 最多检查两个合法地址
    {
        uint16_t halAddress =
            (uint16_t)(addresses[i] << 1);   // HAL 设备地址需要左移一位

        if (HAL_I2C_IsDeviceReady(
                s_i2c,                       // I2C1 句柄
                halAddress,                  // 当前候选设备地址
                1U,                          // 执行一次探测
                SPL06_I2C_TIMEOUT_MS         // 单次探测超时
            ) != HAL_OK)
        {
            continue;                        // 当前地址无响应，检查下一地址
        }

        deviceResponded = true;              // 当前地址存在设备应答
        s_address = (uint8_t)halAddress;      // 暂时保存地址用于读取 ID

        status = SPL06_ReadRegisters(
            SPL06_REG_ID,                    // 产品 ID 寄存器
            &id,                             // 接收 ID
            1U                               // 读取 1 字节
        );

        if (status != BSP_SPL06_STATUS_OK)    // 判断 ID 是否读取成功
        {
            return status;                   // I2C 读取错误，交给上层重试
        }

        if ((id & SPL06_PRODUCT_ID_MASK) ==
             SPL06_PRODUCT_ID_VALUE)         // 判断是否属于 SPL06 产品
        {
            s_chip_id = id;                  // 保存完整芯片 ID
            break;                          // 找到正确设备，结束探测
        }

        s_address = 0U;                      // 设备不是 SPL06，清除候选地址
    }

    /* ---------- 11.3 检查是否找到正确芯片 ---------- */

    if (s_address == 0U)                     // 两个地址均未匹配
    {
        return deviceResponded ?
            BSP_SPL06_STATUS_BAD_ID :        // 有设备响应但产品 ID 不匹配
            BSP_SPL06_STATUS_NO_DEVICE;      // I2C 上未检测到设备
    }

    /* ---------- 11.4 查询芯片启动准备状态 ---------- */

    status = SPL06_ReadRegisters(
        SPL06_REG_MEAS_CFG,                  // 测量状态寄存器
        &measStatus,                         // 接收状态字节
        1U                                   // 读取 1 字节
    );

    if (status != BSP_SPL06_STATUS_OK)        // 检查状态寄存器读取结果
    {
        return status;                       // 读取失败，交由上层重试
    }

    if ((measStatus &
         (SPL06_COEF_READY |
          SPL06_SENSOR_READY)) !=
         (SPL06_COEF_READY |
          SPL06_SENSOR_READY))               // 两个准备标志都必须置位
    {
        return BSP_SPL06_STATUS_NOT_READY;    // 不阻塞等待，下次再初始化
    }

    /* ---------- 11.5 读取 18 字节校准系数 ---------- */

    status = SPL06_ReadRegisters(
        SPL06_REG_COEF,                      // 校准数据起始地址 0x10
        calibration,                         // 18 字节接收缓冲区
        sizeof(calibration)                  // 读取全部校准系数
    );

    if (status != BSP_SPL06_STATUS_OK)        // 判断是否读取成功
    {
        return status;                       // 校准数据读取失败
    }

    SPL06_ParseCoefficients(calibration);     // 解析全部温度、气压校准系数

    /* ---------- 11.6 配置前停止后台测量 ---------- */

    status = SPL06_WriteRegister(
        SPL06_REG_MEAS_CFG,                  // 工作模式寄存器
        0x00U                                // 暂时进入 Standby 模式
    );

    if (status != BSP_SPL06_STATUS_OK)        // 检查停止测量是否成功
    {
        return status;                       // I2C 写入失败
    }

    /* ---------- 11.7 配置气压测量 ---------- */

    status = SPL06_WriteRegister(
        SPL06_REG_PRS_CFG,                   // 气压配置寄存器
        SPL06_PRESS_CONFIG                   // 4Hz、64 倍过采样
    );

    if (status != BSP_SPL06_STATUS_OK)        // 检查气压配置写入结果
    {
        return status;                       // 配置失败
    }

    /* ---------- 11.8 配置温度测量 ---------- */

    status = SPL06_WriteRegister(
        SPL06_REG_TMP_CFG,                   // 温度配置寄存器
        SPL06_TEMP_CONFIG                    // 4Hz、单次采样、外部温度源
    );

    if (status != BSP_SPL06_STATUS_OK)        // 检查温度配置结果
    {
        return status;                       // 配置失败
    }

    /* ---------- 11.9 配置气压数据移位 ---------- */

    status = SPL06_WriteRegister(
        SPL06_REG_CFG,                       // 综合配置寄存器
        SPL06_GENERAL_CONFIG                 // 开启 P_SHIFT，不启用 FIFO
    );

    if (status != BSP_SPL06_STATUS_OK)        // 检查配置是否写入成功
    {
        return status;                       // 配置失败
    }

    /* ---------- 11.10 启动连续气压与温度测量 ---------- */

    status = SPL06_WriteRegister(
        SPL06_REG_MEAS_CFG,                  // 测量控制寄存器
        SPL06_CONTINUOUS_MODE                // 0x07：连续测量气压和温度
    );

    if (status != BSP_SPL06_STATUS_OK)        // 判断连续测量是否启动成功
    {
        return status;                       // 启动失败
    }

    s_ready = true;                          // 全部初始化步骤完成

    return BSP_SPL06_STATUS_OK;              // 返回初始化成功
}


/* ---------- 12. 读取并计算气压和温度 ---------- */

bsp_spl06_status_t BSP_SPL06_Read(
    bsp_spl06_sample_t *out
)
{
    bsp_spl06_status_t status;                // 本次通信状态

    uint8_t measStatus = 0U;                 // 传感器测量状态

    uint8_t rawData[6] = {0};                 // 气压 3 字节、温度 3 字节

    uint32_t pressureBits;                    // 拼接后的气压原始位数据
    uint32_t temperatureBits;                 // 拼接后的温度原始位数据

    int32_t rawPressure;                      // 有符号气压 ADC 原始值
    int32_t rawTemperature;                   // 有符号温度 ADC 原始值

    float pScaled;                            // 归一化后的气压原始值
    float tScaled;                            // 归一化后的温度原始值

    float pressure;                           // 补偿后的真实气压
    float temperature;                        // 补偿后的真实温度

    /* ---------- 12.1 检查输出缓冲区 ---------- */

    if (out == NULL)                          // 检查调用者提供的指针
    {
        return BSP_SPL06_STATUS_INVALID_ARG;  // 输出缓冲区无效
    }

    /* ---------- 12.2 检查驱动初始化状态 ---------- */

    if (!s_ready)                             // SPL06 未完成初始化
    {
        return BSP_SPL06_STATUS_NOT_READY;    // 禁止直接读取测量寄存器
    }

    /* ---------- 12.3 检查是否有新的气压和温度 ---------- */

    status = SPL06_ReadRegisters(
        SPL06_REG_MEAS_CFG,                  // 测量状态寄存器
        &measStatus,                         // 读取状态位
        1U                                   // 读取 1 字节
    );

    if (status != BSP_SPL06_STATUS_OK)        // 判断状态读取是否成功
    {
        s_ready = false;                     // 通信失败，要求重新初始化
        return status;                       // 将错误返回上层
    }

    if ((measStatus &
         (SPL06_PRESS_READY |
          SPL06_TEMP_READY)) !=
         (SPL06_PRESS_READY |
          SPL06_TEMP_READY))                 // 气压和温度必须都有新数据
    {
        return BSP_SPL06_STATUS_NO_SAMPLE;    // 数据尚未更新，不属于故障
    }

    /* ---------- 12.4 一次读取全部原始数据 ---------- */

    status = SPL06_ReadRegisters(
        SPL06_REG_PRS_B2,                    // 从气压寄存器 0x00 开始
        rawData,                             // 接收连续 6 字节
        sizeof(rawData)                      // 气压 3 字节 + 温度 3 字节
    );

    if (status != BSP_SPL06_STATUS_OK)        // 检查原始数据读取是否成功
    {
        s_ready = false;                     // 读取异常，准备重新初始化
        return status;                       // 返回 I2C 错误
    }

    /* ---------- 12.5 拼接气压 24 位原始数据 ---------- */

    pressureBits =
        ((uint32_t)rawData[0] << 16) |       // 气压最高 8 位
        ((uint32_t)rawData[1] << 8) |        // 气压中间 8 位
        (uint32_t)rawData[2];                // 气压最低 8 位

    rawPressure = SPL06_SignExtend(
        pressureBits, 24U
    );                                       // 转换成有符号 24 位数值

    /* ---------- 12.6 拼接温度 24 位原始数据 ---------- */

    temperatureBits =
        ((uint32_t)rawData[3] << 16) |       // 温度最高 8 位
        ((uint32_t)rawData[4] << 8) |        // 温度中间 8 位
        (uint32_t)rawData[5];                // 温度最低 8 位

    rawTemperature = SPL06_SignExtend(
        temperatureBits, 24U
    );                                       // 转换成有符号 24 位数值

    /* ---------- 12.7 根据过采样倍率进行归一化 ---------- */

    pScaled =
        (float)rawPressure /
        SPL06_PRESS_SCALE;                   // 气压使用 64 倍过采样比例

    tScaled =
        (float)rawTemperature /
        SPL06_TEMP_SCALE;                    // 温度使用单次采样比例

    /* ---------- 12.8 计算补偿后的真实温度 ---------- */

    temperature =
        (float)s_coeff.c0 * 0.5f +           // 温度补偿常数项
        (float)s_coeff.c1 * tScaled;         // 温度补偿一次项

    /* ---------- 12.9 计算补偿后的真实气压 ---------- */

    pressure =
        (float)s_coeff.c00 +                 // 气压补偿常数项

        pScaled *
        (
            (float)s_coeff.c10 +             // 气压一次补偿项

            pScaled *
            (
                (float)s_coeff.c20 +         // 气压二次补偿项
                pScaled *
                (float)s_coeff.c30           // 气压三次补偿项
            )
        )

        + tScaled *
          (float)s_coeff.c01                 // 温度对气压的修正

        + tScaled * pScaled *
          (
              (float)s_coeff.c11 +           // 温度与气压交叉项
              pScaled *
              (float)s_coeff.c21             // 二次交叉补偿项
          );

    /* ---------- 12.10 检查补偿后的测量值 ---------- */

    if (!isfinite(pressure) ||                // 不允许 NaN 或无穷大气压
        !isfinite(temperature) ||             // 不允许 NaN 或无穷大温度
        pressure < 30000.0f ||                // SPL06 有效压力范围下限
        pressure > 110000.0f ||               // SPL06 有效压力范围上限
        temperature < -40.0f ||               // SPL06 有效温度范围下限
        temperature > 85.0f)                  // SPL06 有效温度范围上限
    {
        return BSP_SPL06_STATUS_DATA_INVALID; // 无效数据不交付上层
    }

    /* ---------- 12.11 发布本次测量结果 ---------- */

    out->pressure_pa = pressure;              // 保存真实气压，单位 Pa

    out->temperature_c = temperature;         // 保存真实温度，单位 °C

    out->sampled_at_ms = HAL_GetTick();        // 记录本次读取完成的时间

    return BSP_SPL06_STATUS_OK;               // 返回本次测量成功
}


/* ---------- 13. 查询 SPL06 初始化状态 ---------- */

bool BSP_SPL06_IsReady(void)
{
    return s_ready;                            // 返回驱动内部就绪标志
}


/* ---------- 14. 查询 SPL06 实际设备地址 ---------- */

uint8_t BSP_SPL06_GetAddress(void)
{
    return (uint8_t)(s_address >> 1);          // 将 HAL 地址转换回 7 位地址
}


/* ---------- 15. 查询 SPL06 产品 ID ---------- */

uint8_t BSP_SPL06_GetChipId(void)
{
    return s_chip_id;                          // 返回芯片完整 ID
}