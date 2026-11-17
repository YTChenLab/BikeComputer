/**
 * @file    resource_system.c
 * @brief   STM32H743 QSPI 资源安装与维护通信系统
 *
 * @details
 * 本模块负责：
 *
 * 1. 初始化 ResourceInstall 资源安装状态机。
 * 2. 获取 STM32 芯片 UID。
 * 3. 管理资源协议的 USART1 收发。
 * 4. 判断 QSPI MemoryMapped 是否已被锁定。
 * 5. 调用 QSPI 资源诊断接口。
 * 6. 处理资源安装中的串口接收异常。
 * 7. 管理资源安装模式与正常 UI 启动流程。
 *
 * 数据流：
 *
 * PC 资源安装工具
 *       |
 *       v
 *     USART1
 *       |
 *       v
 *    BSP_UART
 *       |
 *       v
 * ResourceSystem
 *       |
 *       v
 * ResourceInstall
 *       |
 *       v
 *    BSP_QSPI
 *
 * 本模块不包含网页遥测功能。
 *
 * FreeRTOS：
 * ResourceSystem_Start() 由 CommTask 调用一次。
 * ResourceSystem_Poll() 由 CommTask 周期调用。
 *
 * UI 启动状态由 UITask 查询与反馈。
 */

#include "resource_system.h"       // 资源系统公开接口
#include "resource_install.h"      // 资源安装协议状态机
#include "bsp_uart.h"              // USART1 底层收发
#include "bsp_qspi.h"              // QSPI MemoryMapped 管理
#include "bsp_resource_boot.h"     // 维护模式与安全复位
#include "stm32h7xx_hal.h"         // HAL 时间和芯片 UID

#include <stdbool.h>               // bool 类型
#include <stdint.h>                // 固定宽度整数
#include <stddef.h>                // size_t 类型


/*==========================================================
 * 一、模块状态
 *=========================================================*/

static uint32_t s_initialized = 0U; // 资源系统初始化完成标志

static uint32_t s_errors = 0U;      // 上一次 UART 接收错误累计值


/*==========================================================
 * 二、资源系统复位回调
 *
 * ResourceInstall 需要切换到维护模式或者重新启动时，
 * 通过此回调请求系统安全复位。
 *=========================================================*/

static void ResourceSystem_Reset(
    bool maintenance,
    void *context
)
{
    (void)context;                 // 当前无扩展上下文

    BSP_ResourceBoot_Reset(
        maintenance                 // 是否进入维护模式
    );
}


/*==========================================================
 * 三、资源协议发送回调
 *
 * 所有资源协议回复均通过 USART1 发送。
 *
 * 不直接在 ResourceInstall 中访问 HAL。
 *=========================================================*/

static bool ResourceSystem_Send(
    const uint8_t *data,
    uint32_t bytes,
    void *context
)
{
    (void)context;                 // 当前不使用上下文

    return BSP_UART_Write(
        data,                      // 发送数据地址
        bytes                      // 数据长度
    ) == 0;                        // BSP 返回 0 表示成功
}


/*==========================================================
 * 四、QSPI 映射锁定状态查询
 *
 * 原理：
 *
 * 当 LVGL 使用外部 Flash 资源时，
 * QSPI 可能处于 MemoryMapped 模式。
 *
 * 此时不能随意擦除或改写正在使用的资源区域。
 *
 * 因此 ResourceInstall 必须先查询锁定状态。
 *=========================================================*/

static bool ResourceSystem_Locked(void *context)
{
    (void)context;                  // 不需要扩展上下文

    return
        BSP_QSPI_IsMemoryMappedPinned() != 0U;
}


/*==========================================================
 * 五、QSPI 资源诊断接口
 *
 * 由资源安装协议请求底层执行诊断。
 *
 * 保留原有 13 个 uint32_t 诊断结果字段。
 *=========================================================*/

static void ResourceSystem_Diagnose(
    uint32_t flags,
    uint32_t result[13],
    void *context
)
{
    (void)context;                  // 当前不需要上下文

    BSP_QSPI_Diagnose(
        flags,                      // 诊断请求标志
        result                      // 诊断结果缓冲区
    );
}


/*==========================================================
 * 六、ResourceSystem_Start()
 *
 * 由 CommTask 启动时调用一次。
 *
 * 初始化过程：
 *
 * 1. 创建资源安装端口配置。
 * 2. 获取 STM32 96 位 UID。
 * 3. 注册资源协议相关回调。
 * 4. 获取维护模式启动标志。
 * 5. 初始化 ResourceInstall 状态机。
 * 6. 启动 USART1 接收中断。
 * 7. 发布资源系统初始化完成状态。
 *=========================================================*/

void ResourceSystem_Start(void)
{
    resource_install_port_t port = {0}; // 清零资源安装端口配置

    uint32_t uid[3] = {
        HAL_GetUIDw0(),                 // UID 低 32 位
        HAL_GetUIDw1(),                 // UID 中间 32 位
        HAL_GetUIDw2()                  // UID 高 32 位
    };

    bool maintenance;                  // 当前维护模式标志

    unsigned i;                        // UID 字节转换计数器

    /* 一、注册资源协议发送回调 */

    port.send = ResourceSystem_Send;

    /* 二、注册资源安装复位回调 */

    port.reset = ResourceSystem_Reset;

    /* 三、注册 QSPI 映射保护回调 */

    port.locked = ResourceSystem_Locked;

    /* 四、注册 QSPI 诊断接口 */

    port.diagnose = ResourceSystem_Diagnose;

    /* 五、将 STM32 96 位 UID 拆分为 12 个字节 */

    for (i = 0U; i < 12U; ++i)
    {
        port.uid[i] = (uint8_t)(
            uid[i / 4U] >> (8U * (i % 4U))
        );
    }

    /*
     * UID 用于资源协议中的设备标识。
     *
     * 保持原有小端字节排列规则，
     * 不改变 PC 资源安装工具使用的格式。
     */

    /* 六、读取维护模式启动标志 */

    maintenance =
        BSP_ResourceBoot_ConsumeMaintenance();

    /* 七、初始化资源安装状态机 */

    ResourceInstall_Init(
        &port,                          // 资源通信和回调配置
        maintenance,                    // 是否处于维护模式
        HAL_GetTick()                   // 当前系统毫秒时间
    );

    /* 八、启动 USART1 接收 */

    (void)BSP_UART_ReceiveStart();

    /* 九、记录 UART 接收异常基准 */

    s_errors = BSP_UART_ReceiveErrors();

    /* 十、发布初始化完成状态 */

    __atomic_store_n(
        &s_initialized,
        1U,
        __ATOMIC_RELEASE
    );
}


/*==========================================================
 * 七、ResourceSystem_Poll()
 *
 * CommTask 周期性调用。
 *
 * 本函数负责：
 *
 * 1. 获取当前 HAL 时间。
 * 2. 检查 UART 接收错误。
 * 3. 消费 UART 接收缓冲区。
 * 4. 将原始字节交给资源协议状态机。
 * 5. 执行资源安装状态机周期处理。
 *
 * 不进行任何 JSON 或网页握手解析。
 *=========================================================*/

void ResourceSystem_Poll(void)
{
    uint8_t bytes[64];                    // 单次资源接收临时缓冲区

    uint32_t now_ms = HAL_GetTick();      // 获取当前毫秒时间

    uint32_t errors =
        BSP_UART_ReceiveErrors();         // 当前 UART 接收错误计数

    size_t n;                             // 本次实际读取的字节数量

    /* 一、检测 USART1 接收异常 */

    if (errors != s_errors)
    {
        s_errors = errors;                // 更新错误计数快照

        /*
         * 发生 UART 数据丢失时，
         * 当前资源安装事务可能已被破坏。
         *
         * 需要通知 ResourceInstall，
         * 防止继续使用不完整的数据包。
         */
        ResourceInstall_TransportLost();

        /* 清空当前已经损坏的接收数据 */

        while (BSP_UART_Read(
            bytes,
            sizeof(bytes)
        ) != 0U)
        {
            /*
             * 丢弃本轮接收数据。
             *
             * 等待 PC 端重新发起资源安装操作。
             */
        }
    }

    /* 二、消费 USART1 接收缓冲区 */

    while ((n = BSP_UART_Read(
        bytes,
        sizeof(bytes)
    )) != 0U)
    {
        /*
         * 将 UART 原始字节直接交给资源安装状态机。
         *
         * ResourceInstall 负责解析数据帧、
         * 处理协议命令、校验以及状态转换。
         */
        ResourceInstall_Feed(
            bytes,                        // 原始数据字节
            (uint32_t)n,                  // 有效字节数量
            now_ms                        // 本轮时间戳
        );
    }

    /* 三、执行资源协议周期处理 */

    ResourceInstall_Poll(now_ms);
}


/*==========================================================
 * 八、ResourceSystem_UiRequested()
 *
 * 由 UITask 查询是否允许启动正式 LVGL UI。
 *
 * 只有资源安装系统初始化完成，
 * 且状态机进入 RI_UI_PENDING，
 * 才允许执行正式 UI 启动。
 *=========================================================*/

bool ResourceSystem_UiRequested(void)
{
    return
        (__atomic_load_n(
            &s_initialized,
            __ATOMIC_ACQUIRE
        ) != 0U)
        &&
        (ResourceInstall_Mode() == RI_UI_PENDING);
}


/*==========================================================
 * 九、ResourceSystem_UiResult()
 *
 * UITask 启动 UI 后，
 * 将启动成功或失败结果报告给资源安装状态机。
 *
 * 保留原有资源管理与 UI 启动协同机制。
 *=========================================================*/

void ResourceSystem_UiResult(bool success)
{
    ResourceInstall_UiResult(success);
}
