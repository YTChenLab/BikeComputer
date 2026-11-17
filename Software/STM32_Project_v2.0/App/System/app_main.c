
/*============================================================
 * 文件名称：app_main.c
 * 功能：STM32H743 应用系统启动协调器
 *
 * Application Boot Coordinator（应用启动协调器）
 *
 * 当前职责：
 * 1. 记录系统复位原因。
 * 2. 输出启动阶段日志。
 * 3. 初始化外部 SDRAM。
 * 4. 管理启动阶段状态及故障。
 * 5. 将控制权交还 main.c，启动 FreeRTOS。
 *
 * 设计原则：
 * 1. 不在此文件中直接操作外设寄存器。
 * 2. 使用 BSP 层接口初始化硬件。
 * 3. 不直接使用 FreeRTOS API。
 * 4. 不重复初始化 FMC 或 MPU。
 *============================================================*/

#include "app_main.h"       /* 应用启动接口 */
#include "main.h"           /* 系统错误处理接口 */
#include "usart.h"          /* USART1 串口接口 */
#include "bsp_sdram.h"      /* SDRAM 正式板级驱动 */

#include "bsp_lcd.h"        /* LCD（液晶显示屏）正式板级驱动 */

#include "bsp_dma2d.h"    /* DMA2D 图形加速驱动 */

#include "bsp_qspi.h"          /* W25Q128 正式驱动接口 */
#include "quadspi.h"           /* CubeMX QUADSPI 初始化 */
#include <stdio.h>             /* snprintf 格式化日志 */

/*============================================================
 * LCD 显示系统生产自检开关
 *
 * 1U：执行颜色、复制、双缓冲切换验收。
 * 0U：不执行，正常启动产品 UI。
 *
 * 正式 LVGL 集成时设置为 0U，
 * 自检功能本身可以继续保留。
 *============================================================*/
#define APP_LCD_DISPLAY_SELF_CHECK   (0U)



#include <stdint.h>         /* 标准整数类型 */
#include <string.h>         /* 字符串长度计算 */

/*============================================================
 * 系统启动状态
 *
 * volatile（易变限定符）：
 * 告诉编译器这些变量可能被调试器或其他
 * 执行上下文观察，避免不适当的访问优化。
 *============================================================*/

/* 保存硬件复位标志 */
volatile uint32_t g_bootResetFlags = 0U;

/* 保存当前启动阶段 */
volatile uint32_t g_bootStage = 0U;

/* 保存最近一次启动失败原因 */
volatile uint32_t g_bootFailure = 0U;

/* 保存启动日志串口发送失败次数 */
volatile uint32_t g_bootUartFailures = 0U;


/**
 * @brief  USART1 启动文本日志允许标志
 *
 * @details
 * 1U：允许 Boot_Log() 输出文本日志。
 * 0U：禁止 Boot_Log() 输出文本日志。
 *
 * 为什么需要这个变量？
 *
 * USART1 在 FreeRTOS 启动之前用于打印启动日志；
 * FreeRTOS 启动之后，USART1 用于 Q4-3 二进制资源协议。
 *
 * 如果 UI Task 初始化显示硬件时继续打印文本，
 * 就可能与 CommTask 发送的 ACK 数据混在一起，
 * 导致电脑端无法正确解析协议。
 *
 * 因此，在基础启动结束后必须关闭文本日志。
 */
static volatile uint8_t s_bootLogAllowed = 1U;


/*============================================================
 * 启动阶段定义
 *
 * 0：尚未执行启动协调器
 * 1：开始执行初始化
 * 2：SDRAM 初始化完成
 * 3：应用启动准备完成
 *============================================================*/

#define BOOT_STAGE_RESET           (0U)
#define BOOT_STAGE_START           (1U)
#define BOOT_STAGE_SDRAM_READY     (2U)
#define BOOT_STAGE_COMPLETE        (3U)

/*============================================================
 * Boot_Log
 *
 * 功能：
 * 通过 USART1 输出启动阶段日志。
 *
 * 工作方式：
 * HAL_UART_Transmit（串口阻塞发送）。
 *
 * 注意：
 * 只允许在 FreeRTOS 启动之前使用。
 *
 * 正式多任务运行期应使用独立日志服务，
 * 不能直接复用本函数进行并发输出。
 *============================================================*/

/**
 * @brief  启动阶段串口日志输出函数
 *
 * @param  message 需要输出的字符串
 *
 * @details
 * 本函数使用 USART1 发送启动信息。
 *
 * HAL_UART_Transmit() 属于阻塞式发送接口。
 *
 * 由于本函数只在 FreeRTOS 调度器启动之前使用，
 * 因此不会与正式的资源协议任务并发发送数据。
 *
 * FreeRTOS 启动后：
 * s_bootLogAllowed 被置为 0；
 * 此时调用本函数将直接返回，不再操作 USART1。
 */
static void Boot_Log(const char *message)
{
    size_t length;  /* 保存待发送字符串的实际字节长度 */

    /* 检查是否允许通过 USART1 输出文本日志 */
    if (s_bootLogAllowed == 0U)
    {
        return;  /* 已进入正式通信阶段，禁止文本输出 */
    }

    /* 检查传入的字符串指针是否有效 */
    if (message == NULL)
    {
        return;  /* 指针无效，不执行发送 */
    }

    /* 计算字符串的实际长度，不包含末尾的 '\0' */
    length = strlen(message);

    /* 检查字符串是否为空，以及长度是否超过 HAL 参数范围 */
    if ((length == 0U) || (length > UINT16_MAX))
    {
        return;  /* 非法长度，直接退出 */
    }

    /*
     * 使用 USART1 发送启动日志。
     *
     * 参数说明：
     *
     * &huart1：
     * CubeMX 创建的 USART1 句柄。
     *
     * (uint8_t *)message：
     * 需要发送的数据缓冲区。
     *
     * (uint16_t)length：
     * 实际需要发送的字节数。
     *
     * 1000U：
     * HAL 发送超时参数，单位 ms。
     */
    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)message,
            (uint16_t)length,
            1000U
        ) != HAL_OK)
    {
        /* 发送失败时，记录累计失败次数 */
        g_bootUartFailures++;
    }
}


/*============================================================
 * LCD 显示系统生产自检
 *
 * 验证内容：
 * 1. DMA2D 矩形颜色填充。
 * 2. RGB565 像素读回。
 * 3. LTDC 第一次双缓冲切换。
 * 4. DMA2D 完整帧复制。
 * 5. LTDC 第二次双缓冲切换。
 *
 * 返回值：
 * 0：全部通过
 * 1：驱动未准备好
 * 2：颜色填充失败
 * 3：颜色读回失败
 * 4：第一次切换失败
 * 5：图像复制失败
 * 6：复制内容检查失败
 * 7：白色矩形填充失败
 * 8：第二次切换失败
 * 9：最终画面检查失败
 *
 * 注意：
 * 本函数只在 FreeRTOS 启动之前执行。
 *============================================================*/

#if (APP_LCD_DISPLAY_SELF_CHECK == 1U)

/*============================================================
 * 等待帧缓冲切换真正完成
 *
 * index：期望显示的缓冲编号
 *
 * 使用 HAL_GetTick() 计时，最多等待 200ms。
 * 不允许无限等待 VSYNC。
 *============================================================*/
static uint8_t App_LCD_WaitPresent(uint8_t index)
{
    uint32_t startTick = HAL_GetTick();

    /*
     * 只要 LTDC 仍在等待重新加载，
     * 就继续查询状态。
     *
     * 真正完成切换时，LTDC 中断回调
     * 会清除 PresentPending 标志。
     */
    while (BSP_LCD_IsPresentPending() != 0U)
    {
        if ((uint32_t)(HAL_GetTick() - startTick) >= 200U)
        {
            return 0U;
        }
    }

    /* 确认前台缓冲已经切换为目标编号 */
    return (BSP_LCD_GetFrontBuffer() == index) ? 1U : 0U;
}

/*============================================================
 * 从指定帧缓冲读取一个 RGB565 像素
 *
 * 用途：
 * 验证 DMA2D 实际写入的颜色是否正确。
 *
 * 当前帧缓冲属于 MPU 非缓存内存，
 * 因此可以直接读取 SDRAM 中的实际数据。
 *============================================================*/
static uint16_t App_LCD_ReadPixel(
    uint32_t address,
    uint16_t x,
    uint16_t y
)
{
    volatile const uint16_t *pixel;

    /* 计算指定像素的实际物理地址 */
    uint32_t pixelAddress =
        address +
        (((uint32_t)y * BSP_LCD_WIDTH + x) * 2U);

    /* 转换为 16 位 RGB565 像素指针 */
    pixel = (volatile const uint16_t *)
            (uintptr_t)pixelAddress;

    return *pixel;
}

/*============================================================
 * App_LCD_DisplaySelfCheck
 *
 * 执行完整显示链路验收。
 *
 * 不修改显示底层架构，
 * 只调用已经建立的正式 BSP 接口。
 *============================================================*/
static uint32_t App_LCD_DisplaySelfCheck(void)
{
    BSP_DMA2D_Status_t dmaStatus;
    BSP_LCD_Status_t lcdStatus;

    /* 两块正式帧缓冲的物理地址 */
    const uint32_t fb0 = BSP_LCD_FB0_ADDR;
    const uint32_t fb1 = BSP_LCD_FB1_ADDR;

    /*--------------------------------------------------------
     * 第一步：检查驱动状态。
     *--------------------------------------------------------*/
    if ((BSP_LCD_IsReady() == 0U) ||
        (BSP_DMA2D_IsReady() == 0U))
    {
        return 1U;
    }

    /*--------------------------------------------------------
     * 第二步：准备后台缓冲 Buffer 1。
     *
     * 当前 LTDC 正在显示 Buffer 0，
     * 因此可以安全修改 Buffer 1。
     *--------------------------------------------------------*/

    /* 先清空后台缓冲为黑色 */
    dmaStatus = BSP_DMA2D_FillRGB565(
        fb1, 480U, 0U, 0U,
        480U, 854U, BSP_LCD_COLOR_BLACK
    );

    if (dmaStatus != BSP_DMA2D_OK)
    {
        return 2U;
    }

    /* 顶部 284 行填充红色 */
    dmaStatus = BSP_DMA2D_FillRGB565(
        fb1, 480U, 0U, 0U,
        480U, 284U, BSP_LCD_COLOR_RED
    );

    if (dmaStatus != BSP_DMA2D_OK)
    {
        return 2U;
    }

    /* 中间 285 行填充绿色 */
    dmaStatus = BSP_DMA2D_FillRGB565(
        fb1, 480U, 0U, 284U,
        480U, 285U, BSP_LCD_COLOR_GREEN
    );

    if (dmaStatus != BSP_DMA2D_OK)
    {
        return 2U;
    }

    /* 底部 285 行填充蓝色 */
    dmaStatus = BSP_DMA2D_FillRGB565(
        fb1, 480U, 0U, 569U,
        480U, 285U, BSP_LCD_COLOR_BLUE
    );

    if (dmaStatus != BSP_DMA2D_OK)
    {
        return 2U;
    }

    /*--------------------------------------------------------
     * 第三步：读回代表性像素。
     *
     * 通过实际 SDRAM 数据确认填充结果。
     *--------------------------------------------------------*/
    if ((App_LCD_ReadPixel(fb1, 10U, 10U)
            != BSP_LCD_COLOR_RED) ||
        (App_LCD_ReadPixel(fb1, 10U, 300U)
            != BSP_LCD_COLOR_GREEN) ||
        (App_LCD_ReadPixel(fb1, 10U, 800U)
            != BSP_LCD_COLOR_BLUE))
    {
        return 3U;
    }

    /*--------------------------------------------------------
     * 第四步：切换到 Buffer 1。
     *
     * BSP_LCD_Present() 只提交切换请求。
     * App_LCD_WaitPresent() 负责确认真正完成。
     *--------------------------------------------------------*/
    lcdStatus = BSP_LCD_Present(1U);

    if (lcdStatus != BSP_LCD_OK)
    {
        return 4U;
    }

    if (App_LCD_WaitPresent(1U) == 0U)
    {
        return 4U;
    }

    /*--------------------------------------------------------
     * 第五步：执行完整帧硬件复制。
     *
     * 当前 Buffer 1 正在显示。
     * Buffer 0 已成为后台缓冲。
     *
     * DMA2D 将 Buffer 1 的 480×854 图像
     * 复制到 Buffer 0。
     *--------------------------------------------------------*/
    dmaStatus = BSP_DMA2D_CopyRGB565(
        fb1,                 /* 源缓冲地址 */
        fb0,                 /* 目标缓冲地址 */
        480U,                /* 源每行像素数 */
        480U,                /* 目标每行像素数 */
        0U, 0U,              /* 源矩形起点 */
        0U, 0U,              /* 目标矩形起点 */
        480U, 854U           /* 复制整个画面 */
    );

    if (dmaStatus != BSP_DMA2D_OK)
    {
        return 5U;
    }

    /* 检查图像复制结果 */
    if ((App_LCD_ReadPixel(fb0, 10U, 10U)
            != BSP_LCD_COLOR_RED) ||
        (App_LCD_ReadPixel(fb0, 10U, 300U)
            != BSP_LCD_COLOR_GREEN) ||
        (App_LCD_ReadPixel(fb0, 10U, 800U)
            != BSP_LCD_COLOR_BLUE))
    {
        return 6U;
    }

    /*--------------------------------------------------------
     * 第六步：在后台缓冲增加白色矩形。
     *
     * 白色矩形位于绿色区域中间。
     *
     * 用途：
     * 区分原始缓冲与复制后修改的缓冲。
     *--------------------------------------------------------*/
    dmaStatus = BSP_DMA2D_FillRGB565(
        fb0,
        480U,
        160U, 350U,
        160U, 154U,
        BSP_LCD_COLOR_WHITE
    );

    if (dmaStatus != BSP_DMA2D_OK)
    {
        return 7U;
    }

    /*--------------------------------------------------------
     * 第七步：切换回 Buffer 0。
     *--------------------------------------------------------*/
    lcdStatus = BSP_LCD_Present(0U);

    if (lcdStatus != BSP_LCD_OK)
    {
        return 8U;
    }

    if (App_LCD_WaitPresent(0U) == 0U)
    {
        return 8U;
    }

    /*--------------------------------------------------------
     * 第八步：检查最终画面像素。
     *
     * 检查三个颜色区域和白色矩形。
     *--------------------------------------------------------*/
    if ((App_LCD_ReadPixel(fb0, 10U, 10U)
            != BSP_LCD_COLOR_RED) ||
        (App_LCD_ReadPixel(fb0, 10U, 300U)
            != BSP_LCD_COLOR_GREEN) ||
        (App_LCD_ReadPixel(fb0, 10U, 800U)
            != BSP_LCD_COLOR_BLUE) ||
        (App_LCD_ReadPixel(fb0, 200U, 400U)
            != BSP_LCD_COLOR_WHITE))
    {
        return 9U;
    }

    /* 全部检查通过 */
    return 0U;
}

#endif /* APP_LCD_DISPLAY_SELF_CHECK */


/*============================================================
 * App_Main
 *
 * 功能：应用系统启动协调。
 *
 * 调用关系：
 *
 * main()
 *   └─ App_Main()
 *       ├─ 记录复位原因
 *       ├─ 输出日志
 *       ├─ BSP_SDRAM_Init()
 *       └─ 返回 main()
 *
 * 返回后 main.c 继续初始化 FreeRTOS。
 *============================================================*/

/**
 * @brief  STM32 应用基础启动协调函数
 *
 * @details
 * 调用位置：
 *
 * Core/Src/main.c
 *     |
 *     +-- HAL_Init()
 *     +-- SystemClock_Config()
 *     +-- CubeMX 外设初始化
 *     |
 *     +-- App_Main()
 *             |
 *             +-- 记录复位状态
 *             +-- 输出启动日志
 *             +-- BSP_QSPI_Init()
 *             +-- 交还 main()
 *
 * 注意：
 * 不在本函数中执行 SDRAM、DMA2D、LCD 初始化。
 *
 * 这些显示外设将在 UITask 中按需启动。
 */
void App_Main(void)
{
    /*========================================================
     * 第一步：记录 STM32 复位原因
     *========================================================*/

    /* 读取 RCC 复位状态寄存器 */
    g_bootResetFlags = RCC->RSR;

    /* 清除已经记录的复位标志，避免影响下一次判断 */
    __HAL_RCC_CLEAR_RESET_FLAGS();

    /* 标记应用已经进入启动阶段 */
    g_bootStage = BOOT_STAGE_START;

    /* 清除之前记录的启动故障 */
    g_bootFailure = 0U;

    /* 确保基础启动期间允许发送 USART1 文本日志 */
    s_bootLogAllowed = 1U;

    /*========================================================
     * 第二步：输出基础启动日志
     *========================================================*/

    /* 输出空行，便于区分不同复位周期 */
    Boot_Log("\r\n");

    /* 输出 STM32 启动标志 */
    Boot_Log("[BOOT] STM32H743 START\r\n");

    /*========================================================
     * 第三步：初始化 W25Q128 QSPI Flash
     *
     * 此时 main.c 已经完成：
     *
     * 1. MPU 配置。
     * 2. CPU Cache 配置。
     * 3. HAL 初始化。
     * 4. 系统时钟配置。
     * 5. MX_QUADSPI_Init()。
     *
     * 因此可以调用正式的 QSPI BSP 驱动。
     *
     * 本阶段不擦除、不写入 Flash。
     *========================================================*/

    {
        /* 保存 W25Q128 返回的三个 JEDEC ID 字节 */
        BSP_QSPI_JedecID_t qspiID = {0};

        /* 保存 BSP_QSPI_Init() 的执行结果 */
        BSP_QSPI_Status_t qspiResult;

        /* 保存格式化后的 QSPI 状态日志 */
        char qspiLog[96];

        /* 通知串口开始进行 QSPI 探测 */
        Boot_Log("[BOOT] QSPI PROBE START\r\n");

        /*
         * 调用正式 QSPI BSP 初始化接口。
         *
         * 成功后：
         * qspiID 中保存实际读取的 JEDEC ID。
         *
         * 返回值：
         * BSP_QSPI_OK 表示驱动识别成功。
         */
        qspiResult = BSP_QSPI_Init(&qspiID);

        /* 将 JEDEC ID 和初始化结果转换成可读字符串 */
        (void)snprintf(
            qspiLog,
            sizeof(qspiLog),
            "[BOOT] QSPI JEDEC=%02X %02X %02X RESULT=%u\r\n",
            (unsigned int)qspiID.manufacturer,
            (unsigned int)qspiID.memoryType,
            (unsigned int)qspiID.capacity,
            (unsigned int)qspiResult
        );

        /* 通过 USART1 输出实际探测结果 */
        Boot_Log(qspiLog);

        /* 判断 W25Q128 是否成功识别 */
        if (qspiResult != BSP_QSPI_OK)
        {
            /*
             * QSPI 探测失败时只记录错误。
             *
             * 这里不调用 Error_Handler()。
             *
             * 原因：
             * 即使外部资源 Flash 不可用，
             * 也应该允许 FreeRTOS 尝试启动，
             * 由资源维护服务报告设备状态。
             */
            Boot_Log("[BOOT][ERROR] QSPI PROBE FAILED\r\n");
        }
        else
        {
            /* QSPI 初始化成功 */
            Boot_Log("[BOOT] QSPI PROBE PASS\r\n");
        }
    }

    /*========================================================
     * 第四步：结束基础启动
     *========================================================*/

    /*
     * 这条日志的含义：
     *
     * App_Main() 已经执行到最后阶段，
     * 接下来将返回 main()，
     * 由 main() 初始化并启动 FreeRTOS。
     *
     * 注意：
     * 该日志不代表 FreeRTOS 已经成功开始调度。
     */
    Boot_Log("[BOOT] RTOS HANDOFF PENDING\r\n");

    /*
     * 关闭 USART1 文本启动日志。
     *
     * 之后 USART1 交由 CommTask 的资源协议使用。
     * 不允许 UI Task 再通过 Boot_Log() 发送文本。
     */
    s_bootLogAllowed = 0U;

    /*
     * 函数执行结束。
     *
     * 返回 main.c 后将继续执行：
     *
     * osKernelInitialize();
     * MX_FREERTOS_Init();
     * osKernelStart();
     */
}


/**
 * @brief  初始化 UI 所需要的显示硬件
 *
 * @details
 * 本函数由 UI_BoardStart() 调用。
 *
 * 它运行在 FreeRTOS 的 UITask 上下文中，
 * 不再由 main() 在启动调度器之前调用。
 *
 * 初始化顺序：
 *
 * 1. SDRAM 初始化
 * 2. UI 专用 SDRAM 数据区清零
 * 3. DMA2D 初始化
 * 4. LCD / LTDC 初始化
 * 5. 可选显示自检
 * 6. 背光初始化
 *
 * 注意：
 * 本函数仍使用原有的正式 BSP 驱动。
 * 不改变 FMC、LTDC、DMA2D 的底层时序。
 *
 * @retval 1U 全部初始化成功
 * @retval 0U 任一初始化阶段失败
 */
uint8_t App_DisplayHardwareInit(void)
{
    /* 保存 SDRAM BSP 初始化函数的返回状态 */
    BSP_SDRAM_Status_t sdramStatus;

    /* 保存 LCD BSP 初始化函数的返回状态 */
    BSP_LCD_Status_t lcdStatus;

    /* 保存 DMA2D BSP 初始化函数的返回状态 */
    BSP_DMA2D_Status_t dma2dStatus;

    /*
     * 记录是否曾经尝试初始化显示硬件。
     *
     * static 局部变量的特点：
     * 函数执行结束之后，变量的值仍然保留。
     *
     * 第一次执行时等于 0；
     * 完成首次判断后设置为 1。
     *
     * 这样可以避免多次重复执行：
     * FMC 初始化、SDRAM 初始化命令、
     * ST7701S 硬件复位以及 LCD 初始化表。
     */
    static uint8_t s_displayInitAttempted = 0U;

    /* 判断是否已经执行过显示硬件初始化 */
    if (s_displayInitAttempted != 0U)
    {
        /*
         * 如果此前已经初始化成功：
         * g_bootStage 应当等于 BOOT_STAGE_COMPLETE。
         *
         * 如果此前初始化失败：
         * 返回 0，不再重复初始化。
         */
        if (g_bootStage == BOOT_STAGE_COMPLETE)
        {
            return 1U;  /* 已经成功初始化 */
        }

        return 0U;      /* 此前初始化失败 */
    }

    /* 标记已经开始首次初始化 */
    s_displayInitAttempted = 1U;

    /*========================================================
     * 第一阶段：初始化 SDRAM
     *========================================================*/

    /*
     * 调用正式 SDRAM BSP 驱动。
     *
     * BSP_SDRAM_Init() 内部负责：
     *
     * 1. FMC 初始化。
     * 2. SDRAM 时钟使能命令。
     * 3. 时钟稳定等待。
     * 4. PRECHARGE ALL 命令。
     * 5. AUTO REFRESH 命令。
     * 6. LOAD MODE REGISTER 命令。
     * 7. SDRAM 自动刷新周期配置。
     *
     * 注意：
     * 此处不需要再次手动调用 MX_FMC_Init()。
     */
    sdramStatus = BSP_SDRAM_Init();

    /* 判断 SDRAM 初始化是否成功 */
    if (sdramStatus != BSP_SDRAM_OK)
    {
        /*
         * 保存 SDRAM 初始化错误码。
         *
         * g_bootFailure 是全局可观察变量，
         * 可以通过 Keil Watch 窗口读取。
         */
        g_bootFailure = (uint32_t)sdramStatus;

        /*
         * 因为此时已经进入 FreeRTOS 运行阶段，
         * Boot_Log() 不再通过 USART1 输出文本。
         *
         * 这样避免破坏 CommTask 的二进制资源协议。
         *
         * 以下日志调用保留原有错误分支含义，
         * 实际运行时会被 Boot_Log() 的开关拦截。
         */
        if (sdramStatus == BSP_SDRAM_ERROR_REFRESH)
        {
            Boot_Log("[BOOT][ERROR] SDRAM REFRESH FAILED\r\n");
        }
        else
        {
            Boot_Log("[BOOT][ERROR] SDRAM INIT FAILED\r\n");
        }

        /*
         * SDRAM 没有准备好时：
         *
         * 不允许继续初始化 DMA2D、LCD 或 LVGL。
         *
         * 当前只退出 UI 显示初始化函数，
         * 不主动停止 FreeRTOS 调度器。
         */
        return 0U;
    }

    /* SDRAM BSP 初始化成功，更新启动阶段 */
    g_bootStage = BOOT_STAGE_SDRAM_READY;

    /* 保留原有初始化日志接口 */
    Boot_Log("[BOOT] SDRAM INIT PASS\r\n");

    /*========================================================
     * 第二阶段：清零 UI 专用 SDRAM 数据段
     *========================================================*/

    /*
     * 为什么需要清零？
     *
     * UI 工程可能包含大量全局变量、
     * 图形缓存和其他零初始化数据。
     *
     * 这些变量被链接到外部 SDRAM。
     *
     * MCU 上电之后，外部 SDRAM 的内容不能保证为 0。
     *
     * 因此必须在 LVGL 启动之前，
     * 将 UI 专用的零初始化区域清零。
     */

    Boot_Log("[BOOT] UI SDRAM CLEAR START\r\n");

    /*
     * 执行 UI SDRAM 数据区清零。
     *
     * 返回 0 表示清零失败；
     * 非 0 表示清零成功。
     */
    if (BSP_SDRAM_ClearUILargeRam() == 0U)
    {
        /* 保留原来的 UI SDRAM 清零错误编码 */
        g_bootFailure = 0x500U;

        /* 保留原有错误日志接口 */
        Boot_Log("[BOOT][ERROR] UI SDRAM CLEAR FAILED\r\n");

        /*
         * 清零失败时，不允许继续启动 UI。
         *
         * 注意：
         * 本函数只返回失败，不调用 Error_Handler()。
         */
        return 0U;
    }

    /* UI SDRAM 数据区已完成清零 */
    Boot_Log("[BOOT] UI SDRAM CLEAR PASS\r\n");

    /*========================================================
     * 第三阶段：初始化 DMA2D
     *========================================================*/

    /*
     * DMA2D 是 STM32H743 的二维图形加速器。
     *
     * 它可以执行：
     * 1. 图形矩形填充。
     * 2. RGB565 图像复制。
     * 3. 图像像素格式转换。
     *
     * LCD 初始化流程中需要使用 DMA2D，
     * 因此必须先初始化 DMA2D，再初始化 LCD。
     */

    Boot_Log("[BOOT] DMA2D INIT START\r\n");

    /* 调用正式 DMA2D BSP 初始化函数 */
    dma2dStatus = BSP_DMA2D_Init();

    /* 检查 DMA2D 初始化是否成功 */
    if (dma2dStatus != BSP_DMA2D_OK)
    {
        /*
         * 保留原有错误码：
         *
         * 0x300 + DMA2D BSP 返回状态。
         */
        g_bootFailure = 0x300U + (uint32_t)dma2dStatus;

        /* 记录 DMA2D 初始化失败 */
        Boot_Log("[BOOT][ERROR] DMA2D INIT FAILED\r\n");

        /*
         * DMA2D 没有就绪时，
         * 禁止启动依赖 DMA2D 的显示系统。
         */
        return 0U;
    }

    /* DMA2D 初始化成功 */
    Boot_Log("[BOOT] DMA2D INIT PASS\r\n");

    /*========================================================
     * 第四阶段：初始化 LCD 和 LTDC
     *========================================================*/

    /*
     * 这一步仍然使用原来的 BSP_LCD_Init()。
     *
     * BSP_LCD_Init() 内部负责：
     *
     * 1. 检查 SDRAM 是否就绪。
     * 2. 验证 LCD 帧缓冲地址。
     * 3. 使用 DMA2D 初始化双帧缓冲。
     * 4. 复位 ST7701S 显示驱动。
     * 5. 发送 LCD 厂家初始化命令。
     * 6. 配置并启动 LTDC。
     *
     * 初始化颜色仍然使用原来的蓝色。
     *
     * 这里只改变调用位置，
     * 不修改 LCD 驱动的初始化表。
     */

    Boot_Log("[BOOT] LCD INIT START\r\n");

    /* 初始化 LCD，第一帧背景颜色为蓝色 */
    lcdStatus = BSP_LCD_Init(BSP_LCD_COLOR_BLUE);

    /* 判断 LCD 初始化是否成功 */
    if (lcdStatus != BSP_LCD_OK)
    {
        /*
         * 保留原有 LCD 故障编码方式。
         *
         * 0x100 + LCD BSP 返回状态。
         */
        g_bootFailure = 0x100U + (uint32_t)lcdStatus;

        /* 根据 LCD 返回的具体错误类型分类处理 */
        if (lcdStatus == BSP_LCD_ERROR_MEMORY)
        {
            /* LCD 帧缓冲内存验证失败 */
            Boot_Log("[BOOT][ERROR] LCD FRAME MEMORY FAILED\r\n");
        }
        else if (lcdStatus == BSP_LCD_ERROR_SDRAM)
        {
            /* LCD BSP 检测到 SDRAM 尚未就绪 */
            Boot_Log("[BOOT][ERROR] LCD SDRAM NOT READY\r\n");
        }
        else if (lcdStatus == BSP_LCD_ERROR_LTDC)
        {
            /* LTDC 初始化失败 */
            Boot_Log("[BOOT][ERROR] LTDC INIT FAILED\r\n");
        }
        else
        {
            /* 其他 LCD 初始化错误 */
            Boot_Log("[BOOT][ERROR] LCD INIT FAILED\r\n");
        }

        /*
         * LCD 初始化失败时：
         *
         * 1. 不允许创建 LVGL 显示对象。
         * 2. 不允许读取未准备好的帧缓冲。
         * 3. 不主动关闭 FreeRTOS 中断。
         *
         * 返回失败，由 UI_BoardStart() 处理。
         */
        return 0U;
    }

    /*
     * 走到这里说明：
     *
     * BSP_LCD_Init() 已经返回 BSP_LCD_OK。
     *
     * 但软件初始化成功不等于屏幕实际画面
     * 已经通过实机验证。
     */
    Boot_Log("[BOOT] LCD INIT SEQUENCE SENT\r\n");
    Boot_Log("[BOOT] LTDC CONFIGURED\r\n");

    /*========================================================
     * 第五阶段：可选的 LCD 显示自检
     *========================================================*/

    /*
     * APP_LCD_DISPLAY_SELF_CHECK：
     *
     * 0U：不执行显示自检。
     * 1U：执行显示自检。
     *
     * 当前工程原有配置为 0U。
     *
     * 因此正常运行时，
     * 这里的自检代码不会被编译进去。
     *
     * 保留原来的 App_LCD_DisplaySelfCheck()，
     * 不需要修改该函数。
     */

#if (APP_LCD_DISPLAY_SELF_CHECK == 1U)

    {
        /* 保存 LCD 生产自检的返回结果 */
        uint32_t checkResult;

        /* 记录显示自检开始 */
        Boot_Log("[BOOT] DISPLAY CHECK START\r\n");

        /*
         * 执行原有完整 LCD 自检。
         *
         * 包含：
         * RGB565 颜色填充、像素读回、
         * DMA2D 图像复制、双缓冲切换等。
         */
        checkResult = App_LCD_DisplaySelfCheck();

        /* 检查自检是否全部通过 */
        if (checkResult != 0U)
        {
            /*
             * 保留原有显示自检错误码。
             *
             * 0x400 + 具体自检失败步骤。
             */
            g_bootFailure = 0x400U + checkResult;

            /* 显示自检失败 */
            Boot_Log("[BOOT][ERROR] DISPLAY CHECK FAILED\r\n");

            /* 禁止继续启动正式 UI */
            return 0U;
        }

        /* LCD 生产自检成功 */
        Boot_Log("[BOOT] DISPLAY CHECK PASS\r\n");
    }

#endif /* APP_LCD_DISPLAY_SELF_CHECK */

    /*========================================================
     * 第六阶段：安全开启 LCD 背光
     *========================================================*/

    /*
     * BSP_LCD_BACKLIGHT_APPROVED：
     *
     * 1U：允许开启背光。
     * 0U：禁止开启背光。
     *
     * 这个宏定义位于 bsp_lcd.h。
     *
     * 当前上传工程中，该宏设置为 1U。
     *
     * 因此保留原来的 15% 初始亮度设置。
     */

#if (BSP_LCD_BACKLIGHT_APPROVED == 1U)

    /* 将 LCD 背光设置为 15% */
    lcdStatus = BSP_LCD_SetBacklight(15U);

    /* 判断背光驱动是否成功接受亮度设置 */
    if (lcdStatus != BSP_LCD_OK)
    {
        /*
         * 保留原有背光错误码。
         *
         * 0x200 + LCD BSP 返回状态。
         */
        g_bootFailure = 0x200U + (uint32_t)lcdStatus;

        /* 记录背光控制失败 */
        Boot_Log("[BOOT][ERROR] LCD BACKLIGHT FAILED\r\n");

        /* 背光初始化失败，本次 UI 启动失败 */
        return 0U;
    }

    /* 背光设置成功 */
    Boot_Log("[BOOT] LCD BACKLIGHT ENABLED\r\n");

#else

    /*
     * 如果背光尚未获得允许：
     * 只初始化 LCD 控制器，不主动开启背光。
     */
    Boot_Log("[BOOT] LCD BACKLIGHT LOCKED\r\n");

#endif /* BSP_LCD_BACKLIGHT_APPROVED */

    /*========================================================
     * 第七阶段：显示硬件初始化全部完成
     *========================================================*/

    /*
     * 更新启动状态。
     *
     * BOOT_STAGE_COMPLETE 表示：
     *
     * SDRAM、UI SDRAM、DMA2D、LCD
     * 已经完成本函数规定的初始化流程。
     *
     * 注意：
     * 这并不表示 LVGL 已经创建成功。
     */
    g_bootStage = BOOT_STAGE_COMPLETE;

    /* 保留显示硬件初始化成功的日志接口 */
    Boot_Log("[BOOT] DISPLAY INIT PASS\r\n");

    /*
     * 返回 1：
     *
     * 通知 UI_BoardStart()：
     * 显示硬件已经准备完成，
     * 可以继续执行 UI 服务和资源绑定。
     */
    return 1U;
}
