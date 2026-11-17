
/*============================================================
 * 文件名称：bsp_sdram.c
 * 功能：W9825G6KH-6 SDRAM 正式板级驱动
 *
 * 硬件平台：STM32H743IIT6
 *
 * SDRAM 参数：
 * 容量：32 MiB
 * 数据宽度：16 bit
 * SDRAM 工作时钟：100 MHz
 * CAS Latency（列地址选通延迟）：3
 *
 * 软件架构：
 * 1. main.c 负责 MPU（内存保护单元）配置。
 * 2. 本文件负责 FMC 和 SDRAM 初始化。
 * 3. App_Main() 负责统一调用本模块。
 *
 * 必须在 FreeRTOS（实时操作系统）启动前初始化。
 *============================================================*/

#include "bsp_sdram.h"        /* SDRAM 驱动接口 */
#include "fmc.h"              /* CubeMX 生成的 FMC 初始化接口 */
#include "stm32h7xx_hal.h"    /* STM32 HAL 硬件抽象层 */

/*============================================================
 * 内部配置参数
 *============================================================*/

/* SDRAM 初始化命令超时时间：1000 ms */
#define SDRAM_COMMAND_TIMEOUT      (1000U)

/*
 * SDRAM 自动刷新计数器。
 *
 * SDRAM 工作时钟：100 MHz
 * 64 ms 内需要刷新 8192 次。
 *
 * 计算公式：
 * RefreshCount = 100000000 × 0.064 / 8192 - 20
 *
 * 计算结果约为 761。
 *
 * 注意：
 * 如果后续修改 FMC 时钟，需要重新计算。
 */
#define SDRAM_REFRESH_COUNT        (761U)

/* 突发长度：1 个数据单位 */
#define SDRAM_MODE_BURST_LENGTH_1  (0x0000U)

/* 突发类型：顺序访问 */
#define SDRAM_MODE_BURST_SEQ       (0x0000U)

/* CAS 延迟：3 个时钟周期 */
#define SDRAM_MODE_CAS_3           (0x0030U)

/* 写突发模式：单地址写入 */
#define SDRAM_MODE_WRITE_SINGLE    (0x0200U)

/*============================================================
 * 内部驱动状态
 *
 * 初始化前：NOT_READY
 * 初始化成功：OK
 * 初始化失败：保存相应错误状态
 *
 * 不允许运行过程中重复初始化 SDRAM。
 *============================================================*/
static BSP_SDRAM_Status_t s_sdramStatus =
    BSP_SDRAM_ERROR_NOT_READY;

/*============================================================
 * 内部函数声明
 *============================================================*/

/*
 * SDRAM_SendCommand
 *
 * 功能：
 * 通过 HAL（硬件抽象层）向 SDRAM 发送控制命令。
 */
static HAL_StatusTypeDef SDRAM_SendCommand(
    uint32_t mode,
    uint32_t refreshNumber,
    uint32_t modeRegister
);

/*============================================================
 * BSP_SDRAM_Init
 *
 * 功能：完成外部 SDRAM 正式初始化。
 *
 * 初始化步骤：
 * 1. FMC 控制器初始化
 * 2. SDRAM 时钟使能
 * 3. 等待时钟稳定
 * 4. 所有存储体预充电
 * 5. 执行自动刷新
 * 6. 加载模式寄存器
 * 7. 设置自动刷新周期
 *
 * MPU 已经由 main.c 提前完成配置。
 * 此处不得再次操作 MPU 或 D-Cache。
 *============================================================*/
BSP_SDRAM_Status_t BSP_SDRAM_Init(void)
{
    uint32_t modeRegister;

    /*
     * 已经完成初始化则直接返回。
     *
     * 避免重复初始化导致 SDRAM
     * 内部状态或已有数据受到影响。
     */
    if (s_sdramStatus == BSP_SDRAM_OK)
    {
        return BSP_SDRAM_OK;
    }

    /*
     * 如果此前初始化失败，
     * 则保持故障状态，不自动尝试重新初始化。
     */
    if (s_sdramStatus != BSP_SDRAM_ERROR_NOT_READY)
    {
        return s_sdramStatus;
    }

    /*--------------------------------------------------------
     * 步骤 1：初始化 FMC（灵活存储器控制器）。
     *
     * MX_FMC_Init() 由 STM32CubeMX 生成。
     *
     * 负责配置：
     * - FMC 外设时钟
     * - SDRAM 地址线
     * - SDRAM 数据线
     * - SDRAM 控制信号
     * - 存储器访问时序
     *
     * 原有 fmc.c 还负责 PC2_C / PC3_C
     * 模拟开关配置。
     *
     * 如果 HAL_SDRAM_Init() 失败，
     * 原有 CubeMX 代码会进入 Error_Handler()。
     *--------------------------------------------------------*/
    MX_FMC_Init();

    /*--------------------------------------------------------
     * 步骤 2：使能 SDRAM 工作时钟。
     *
     * 此时 FMC 开始向 SDRAM 提供时钟信号。
     *--------------------------------------------------------*/
    if (SDRAM_SendCommand(
            FMC_SDRAM_CMD_CLK_ENABLE,
            1U,
            0U) != HAL_OK)
    {
        s_sdramStatus = BSP_SDRAM_ERROR_COMMAND;
        return s_sdramStatus;
    }

    /*--------------------------------------------------------
     * 步骤 3：等待 SDRAM 时钟稳定。
     *
     * 使用 HAL_Delay(1) 等待 1 ms。
     * 满足 SDRAM 初始化的上电稳定等待要求。
     *--------------------------------------------------------*/
    HAL_Delay(1U);

    /*--------------------------------------------------------
     * 步骤 4：PRECHARGE ALL（全部存储体预充电）。
     *
     * 使 SDRAM 内部所有 Bank（存储体）
     * 进入统一的预充电状态。
     *--------------------------------------------------------*/
    if (SDRAM_SendCommand(
            FMC_SDRAM_CMD_PALL,
            1U,
            0U) != HAL_OK)
    {
        s_sdramStatus = BSP_SDRAM_ERROR_COMMAND;
        return s_sdramStatus;
    }

    /*--------------------------------------------------------
     * 步骤 5：AUTO REFRESH（自动刷新）。
     *
     * 初始化阶段执行 8 次自动刷新。
     *--------------------------------------------------------*/
    if (SDRAM_SendCommand(
            FMC_SDRAM_CMD_AUTOREFRESH_MODE,
            8U,
            0U) != HAL_OK)
    {
        s_sdramStatus = BSP_SDRAM_ERROR_COMMAND;
        return s_sdramStatus;
    }

    /*--------------------------------------------------------
     * 步骤 6：配置 SDRAM 模式寄存器。
     *
     * Burst Length（突发长度）：1
     * Burst Type（突发类型）：顺序
     * CAS Latency（列选通延迟）：3
     * Write Burst（写突发）：单次写入
     *
     * 最终模式寄存器值：0x0230。
     *--------------------------------------------------------*/
    modeRegister =
        SDRAM_MODE_BURST_LENGTH_1 |
        SDRAM_MODE_BURST_SEQ |
        SDRAM_MODE_CAS_3 |
        SDRAM_MODE_WRITE_SINGLE;

    if (SDRAM_SendCommand(
            FMC_SDRAM_CMD_LOAD_MODE,
            1U,
            modeRegister) != HAL_OK)
    {
        s_sdramStatus = BSP_SDRAM_ERROR_COMMAND;
        return s_sdramStatus;
    }

    /*--------------------------------------------------------
     * 步骤 7：设置 SDRAM 自动刷新计数器。
     *
     * SDRAM 依靠存储电容保存信息，
     * 必须周期性刷新，否则数据可能丢失。
     *
     * FMC 会自动执行刷新，
     * 不需要 FreeRTOS 任务参与。
     *--------------------------------------------------------*/
    if (HAL_SDRAM_ProgramRefreshRate(
            &hsdram1,
            SDRAM_REFRESH_COUNT) != HAL_OK)
    {
        s_sdramStatus = BSP_SDRAM_ERROR_REFRESH;
        return s_sdramStatus;
    }

    /*
     * 数据同步屏障：
     * 确保之前的存储器配置操作完成。
     */
    __DSB();

    /*
     * 指令同步屏障：
     * 确保后续指令在当前处理器状态下执行。
     */
    __ISB();

    /*
     * 初始化命令全部执行成功。
     *
     * 注意：
     * 此处仅表示初始化流程成功，
     * 不代表完成物理存储单元的读写验收。
     */
    s_sdramStatus = BSP_SDRAM_OK;

    return BSP_SDRAM_OK;
}

/*============================================================
 * SDRAM_SendCommand
 *
 * 功能：
 * 统一封装 SDRAM 控制命令。
 *
 * 参数：
 * mode          命令类型
 * refreshNumber 自动刷新次数
 * modeRegister  模式寄存器内容
 *
 * 返回：
 * HAL_OK        命令成功
 * 其他状态      命令失败
 *============================================================*/
static HAL_StatusTypeDef SDRAM_SendCommand(
    uint32_t mode,
    uint32_t refreshNumber,
    uint32_t modeRegister
)
{
    /* 创建并清零 FMC 命令结构体 */
    FMC_SDRAM_CommandTypeDef command = {0};

    /* 指定 SDRAM 命令类型 */
    command.CommandMode = mode;

    /* 当前硬件采用 FMC SDRAM Bank1 */
    command.CommandTarget =
        FMC_SDRAM_CMD_TARGET_BANK1;

    /* 设置本次自动刷新命令的刷新次数 */
    command.AutoRefreshNumber = refreshNumber;

    /* 设置模式寄存器数据 */
    command.ModeRegisterDefinition = modeRegister;

    /* 向 FMC 提交命令并返回执行结果 */
    return HAL_SDRAM_SendCommand(
        &hsdram1,
        &command,
        SDRAM_COMMAND_TIMEOUT
    );
}

/*============================================================
 * BSP_SDRAM_GetStatus
 *
 * 功能：获取最近一次初始化状态。
 *============================================================*/
BSP_SDRAM_Status_t BSP_SDRAM_GetStatus(void)
{
    return s_sdramStatus;
}

/*============================================================
 * BSP_SDRAM_IsReady
 *
 * 功能：查询初始化流程是否成功。
 *
 * 返回：
 * 1：初始化成功
 * 0：尚未初始化或初始化失败
 *============================================================*/
uint8_t BSP_SDRAM_IsReady(void)
{
    return (s_sdramStatus == BSP_SDRAM_OK)
        ? 1U
        : 0U;
}

/*============================================================
 * BSP_SDRAM_GetSize
 *
 * 功能：获取 SDRAM 总容量。
 *
 * 返回：总字节数。
 *============================================================*/
uint32_t BSP_SDRAM_GetSize(void)
{
    return BSP_SDRAM_SIZE_BYTES;
}




/*============================================================
 * UI 专用 SDRAM 零初始化
 *
 * 内存区域：
 * 0xC0200000 开始，最大 4 MiB。
 *
 * 注意：
 * 1. 不清空 LTDC 的双帧缓冲。
 * 2. 不清空整个 4 MiB 预留空间。
 * 3. 只清空链接器实际分配的 ZI 数据段。
 *============================================================*/

/* UI 专用 SDRAM 预留区域起始地址 */
#define BSP_UI_SDRAM_START  (0xC0200000UL)

/* UI 专用 SDRAM 最大预留容量：4 MiB */
#define BSP_UI_SDRAM_SIZE   (4UL * 1024UL * 1024UL)

/*
 * ARM 链接器自动生成的段边界符号。
 *
 * Base：实际零初始化区域起始地址。
 * Limit：实际零初始化区域结束地址的下一字节。
 *
 * 这两个符号不是普通变量。
 * 它们的地址由 Keil 链接器确定。
 */
extern uint8_t Image$$RW_UI_SDRAM$$ZI$$Base[];
extern uint8_t Image$$RW_UI_SDRAM$$ZI$$Limit[];

/*
 * UI SDRAM 区域锚点。
 *
 * 由于目前还没有实际调用完整 LVGL UI，
 * 静态库中的 UI 数据段可能未被提取。
 *
 * 这个锚点确保当前固件具有真实的
 * .bss.ui_sdram 数据段，
 * 使初始化功能可以独立完成链接验收。
 *
 * 后续接入完整 UI 时可以继续保留。
 */
static volatile uint32_t s_uiSdramRegionMarker
    __attribute__((section(".bss.ui_sdram"),
                   aligned(32),
                   used));

/*============================================================
 * BSP_SDRAM_ClearUILargeRam
 *============================================================*/
uint8_t BSP_SDRAM_ClearUILargeRam(void)
{
    uintptr_t startAddress;
    uintptr_t endAddress;
    uintptr_t markerAddress;
    uintptr_t allowedEnd;
    volatile uint32_t *writePointer;

    /* 第一步：确认 SDRAM 已经初始化成功 */
    if (BSP_SDRAM_IsReady() == 0U)
    {
        return 0U;
    }

    /* 第二步：读取链接器提供的实际段边界 */
    startAddress =
        (uintptr_t)Image$$RW_UI_SDRAM$$ZI$$Base;

    endAddress =
        (uintptr_t)Image$$RW_UI_SDRAM$$ZI$$Limit;

    /* 计算允许使用的区域结束地址 */
    allowedEnd =
        BSP_UI_SDRAM_START + BSP_UI_SDRAM_SIZE;

    /* 第三步：检查地址是否处于合法区域 */
    if ((startAddress < BSP_UI_SDRAM_START) ||
        (endAddress <= startAddress) ||
        (endAddress > allowedEnd))
    {
        return 0U;
    }

    /* 确认起止地址均按照 4 字节对齐 */
    if (((startAddress | endAddress) & 0x03U) != 0U)
    {
        return 0U;
    }

    /*
     * 检查区域锚点位于实际链接的数据段中。
     * 同时确保该变量被正式引用。
     */
    markerAddress = (uintptr_t)&s_uiSdramRegionMarker;

    if ((markerAddress < startAddress) ||
        (markerAddress > (endAddress - sizeof(s_uiSdramRegionMarker))))
    {
        return 0U;
    }

    /* 第四步：从实际起始地址开始清零 */
    writePointer = (volatile uint32_t *)startAddress;

    /* 每次写入 4 字节，直到整个实际段清零 */
    while ((uintptr_t)writePointer < endAddress)
    {
        *writePointer = 0U;

        writePointer++;
    }

    /* 确保前面的内存写入完成 */
    __DSB();

    /* 第五步：返回成功 */
    return 1U;
}
