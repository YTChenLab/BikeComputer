
/*============================================================
 * 文件：bsp_lcd.c
 *
 * 功能：
 * STM32H743 + ST7701S LCD 正式板级显示驱动。
 *
 * 硬件：
 * LCD：D300T9307V1
 * 分辨率：480 × 854
 * RGB 数据格式：RGB565
 * SDRAM：32 MiB
 *
 * 已确认 PCB 引脚：
 * PA4  -> LCD_SPI_CS  串行片选
 * PB13 -> LCD_SPI_SCL 串行时钟
 * PB5  -> LCD_SPI_SDA 串行数据
 * PI10 -> LCD_RST     屏幕复位
 * PA1  -> TIM2_CH2    背光 PWM
 *
 * 软件模块职责：
 * 1. 完成 ST7701S 控制器初始化。
 * 2. 检查显示帧缓冲内存。
 * 3. 管理双帧缓冲。
 * 4. 启动 LTDC 显示控制器。
 * 5. 安全管理背光 PWM。
 *
 * 注意：
 * 这里的 ST7701S 参数来自厂家 V0 示例。
 * V1 面板参数仍需要实机确认。
 *============================================================*/

#include "bsp_lcd.h"          /* LCD 驱动公共接口 */
#include "bsp_sdram.h"        /* 外部 SDRAM 驱动 */

#include "bsp_dma2d.h"        /* DMA2D（二??维图形加速器）正式驱动 */

#include "ltdc.h"             /* CubeMX LTDC 配置 */
#include "tim.h"              /* CubeMX TIM2 配置 */
#include "stm32h7xx_hal.h"    /* STM32 HAL 硬件抽象层 */



#include <stdint.h>           /* 固定宽度整数定义 */

/*============================================================
 * 第一部分：编译期内存布局检查
 *
 * 确认双帧缓冲完全位于 SDRAM
 * 前 2 MiB 的非缓存区域内。
 *============================================================*/

#if ((BSP_LCD_FB1_ADDR + BSP_LCD_FRAME_BYTES) > \
     (BSP_SDRAM_FB_REGION_BASE + BSP_SDRAM_FB_REGION_SIZE))
#error "LCD framebuffer exceeds non-cacheable SDRAM region"
#endif

/*============================================================
 * 第二部分：GPIO（通用输入输出）控制
 *
 * 注意：
 * GPIO 的模式由 CubeMX 统一配置。
 * 本文件只控制引脚电平，不重复配置复用功能。
 *============================================================*/

/* SPI 片选：高电平表示不选中 */
#define LCD_CS_HIGH() \
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)

/* SPI 片选：低电平表示选中 */
#define LCD_CS_LOW() \
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)

/* SPI 时钟输出高电平 */
#define LCD_SCL_HIGH() \
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET)

/* SPI 时钟输出低电平 */
#define LCD_SCL_LOW() \
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET)

/* SPI 数据输出高电平 */
#define LCD_SDA_HIGH() \
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET)

/* SPI 数据输出低电平 */
#define LCD_SDA_LOW() \
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET)

/* LCD 复位信号拉高，退出复位 */
#define LCD_RST_HIGH() \
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_10, GPIO_PIN_SET)

/* LCD 复位信号拉低，执行复位 */
#define LCD_RST_LOW() \
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_10, GPIO_PIN_RESET)

/*============================================================
 * 第三部分：ST7701S 初始化命令结构
 *
 * command：
 * 控制器寄存器命令。
 *
 * length：
 * 当前命令的参数数量。
 *
 * delayMs：
 * 当前命令完成之后的等待时间。
 *
 * data：
 * 当前命令的参数，最多 16 字节。
 *
 * 使用固定大小数组的原因：
 * 1. 不需要运行时动态分配内存。
 * 2. 初始化表可以存放在内部 Flash。
 * 3. 结构简单，适合嵌入式正式固件。
 *============================================================*/

typedef struct
{
    uint8_t command;       /* 寄存器命令 */
    uint8_t length;        /* 参数字节数量 */
    uint16_t delayMs;      /* 命令后的延时 */
    uint8_t data[16];      /* 寄存器参数 */

} LCD_InitStep_t;

/*============================================================
 * 第四部分：厂家 ST7701S 初始化命令表
 *
 * 来源：
 * template_H743_D300T9307V0
 * HARDWARE/LCD/ltdc.c
 * RGB_SPI_Init()
 *
 * 参数为厂家 V0 示例中实际启用的命令。
 *
 * 说明：
 * 0xFF：选择扩展寄存器组。
 * 0xC0：配置显示行数。
 * 0xC1：配置垂直时序相关参数。
 * 0xC2：配置反转及刷新相关参数。
 * 0xB0/B1：配置 Gamma 曲线或电源寄存器，
 *         具体作用取决于当前扩展寄存器组。
 * 0x3A：选择 RGB565 颜色格式。
 * 0x11：退出睡眠。
 * 0x29：开启显示。
 *
 * 注意：
 * 本表不应随意修改 Gamma 和电源参数。
 * V1 参数需要以后按厂家版本资料确认。
 *============================================================*/

static const LCD_InitStep_t s_panelInitTable[] =
{
    /*--------------------------------------------------------
     * 厂家扩展寄存器设置
     *--------------------------------------------------------*/
    {0xFF, 5, 0, {0x77, 0x01, 0x00, 0x00, 0x13}},
    {0xEF, 1, 0, {0x08}},

    /*--------------------------------------------------------
     * 扩展寄存器 Bank 0（寄存器组 0）
     *--------------------------------------------------------*/
    {0xFF, 5, 0, {0x77, 0x01, 0x00, 0x00, 0x10}},

    /* 854 行显示参数 */
    {0xC0, 2, 0, {0xE9, 0x03}},

    /* 垂直时序 */
    {0xC1, 2, 0, {0x12, 0x0C}},

    /* 反转模式及帧率相关配置 */
    {0xC2, 2, 0, {0x27, 0x0A}},

    /* 面板颜色驱动相关配置 */
    {0xCC, 1, 0, {0x10}},

    /* Positive Gamma（正极性伽马校正） */
    {0xB0, 16, 0,
     {0x00, 0x23, 0x2A, 0x0A,
      0x0E, 0x03, 0x12, 0x06,
      0x06, 0x2A, 0x00, 0x10,
      0x0F, 0x2D, 0x34, 0x1F}},

    /* Negative Gamma（负极性伽马校正） */
    {0xB1, 16, 0,
     {0x00, 0x24, 0x2B, 0x0F,
      0x12, 0x07, 0x15, 0x0A,
      0x0A, 0x2B, 0x08, 0x13,
      0x10, 0x2D, 0x33, 0x1F}},

    /*--------------------------------------------------------
     * 扩展寄存器 Bank 1（寄存器组 1）
     *
     * 主要包含面板内部电源、驱动及扫描参数。
     * 不同面板版本可能需要不同取值。
     *--------------------------------------------------------*/
    {0xFF, 5, 0, {0x77, 0x01, 0x00, 0x00, 0x11}},

    {0xB0, 1, 0, {0x5D}},
    {0xB1, 1, 0, {0x74}},
    {0xB2, 1, 0, {0x84}},
    {0xB3, 1, 0, {0x80}},
    {0xB5, 1, 0, {0x45}},
    {0xB7, 1, 0, {0x85}},
    {0xB8, 1, 0, {0x33}},
    {0xC1, 1, 0, {0x78}},
    {0xC2, 1, 0, {0x78}},
    {0xD0, 1, 0, {0x88}},

    /*--------------------------------------------------------
     * 面板内部驱动时序及扫描配置
     *--------------------------------------------------------*/
    {0xE0, 3, 0, {0x00, 0x00, 0x02}},

    {0xE1, 11, 0,
     {0x06, 0xA0, 0x08, 0xA0,
      0x05, 0xA0, 0x07, 0xA0,
      0x00, 0x44, 0x44}},

    {0xE2, 12, 0,
     {0x30, 0x30, 0x44, 0x44,
      0x6E, 0xA0, 0x00, 0x00,
      0x6E, 0xA0, 0x00, 0x00}},

    {0xE3, 4, 0, {0x00, 0x00, 0x33, 0x33}},

    {0xE4, 2, 0, {0x44, 0x44}},

    {0xE5, 16, 0,
     {0x0D, 0x69, 0x0A, 0xA0,
      0x0F, 0x6B, 0x0A, 0xA0,
      0x09, 0x65, 0x0A, 0xA0,
      0x0B, 0x67, 0x0A, 0xA0}},

    {0xE6, 4, 0, {0x00, 0x00, 0x33, 0x33}},

    {0xE7, 2, 0, {0x44, 0x44}},

    {0xE8, 16, 0,
     {0x0C, 0x68, 0x0A, 0xA0,
      0x0E, 0x6A, 0x0A, 0xA0,
      0x08, 0x64, 0x0A, 0xA0,
      0x0A, 0x66, 0x0A, 0xA0}},

    {0xE9, 2, 0, {0x36, 0x00}},

    {0xEB, 7, 0,
     {0x00, 0x01, 0xE4, 0xE4,
      0x44, 0x88, 0x40}},

    {0xED, 16, 0,
     {0xFF, 0x45, 0x67, 0xFA,
      0x01, 0x2B, 0xCF, 0xFF,
      0xFF, 0xFC, 0xB2, 0x10,
      0xAF, 0x76, 0x54, 0xFF}},

    {0xEF, 6, 0,
     {0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F}},

    /*--------------------------------------------------------
     * 返回普通命令寄存器组
     *--------------------------------------------------------*/
    {0xFF, 5, 0, {0x77, 0x01, 0x00, 0x00, 0x00}},

    /* MADCTL（显示内存访问控制） */
    {0x36, 1, 0, {0x00}},

    /* COLMOD（颜色格式设置）：RGB565 */
    {0x3A, 1, 0, {0x55}},

    /* SLPOUT（退出睡眠），等待 120 ms */
    {0x11, 0, 120, {0}},

    /* DISPON（开启显示），额外等待 20 ms */
    {0x29, 0, 20, {0}},

    /* TEON（撕裂效应信号功能设置） */
    {0x35, 1, 0, {0x00}}
};

/* 初始化命令条目数量 */
#define LCD_INIT_TABLE_COUNT \
    ((uint32_t)(sizeof(s_panelInitTable) / \
                sizeof(s_panelInitTable[0])))

/*============================================================
 * 第五部分：驱动状态管理
 *============================================================*/

typedef enum
{
    LCD_STATE_RESET = 0,       /* 尚未初始化 */
    LCD_STATE_INITIALIZING,    /* 正在初始化 */
    LCD_STATE_READY,           /* 初始化流程成功 */
    LCD_STATE_FAILED           /* 初始化流程失败 */

} LCD_InternalState_t;

/* 记录当前驱动状态 */
static LCD_InternalState_t s_lcdState = LCD_STATE_RESET;

/* 记录最近一次错误 */
static BSP_LCD_Status_t s_lastError =
    BSP_LCD_ERROR_NOT_READY;

/* 记录发现内存错误的地址 */
static volatile uint32_t s_memoryErrorAddress = 0U;

/* 当前完成显示切换的帧缓冲编号 */
static volatile uint8_t s_frontBuffer = 0U;

/* 当前等待切换的帧缓冲编号 */
static volatile uint8_t s_pendingBuffer = 0U;

/* 帧缓冲是否正在等待垂直同步切换 */
static volatile uint8_t s_presentPending = 0U;

/* 背光 PWM 是否已经启动 */
static uint8_t s_backlightStarted = 0U;

/*============================================================
 * 第六部分：内部函数声明
 *============================================================*/

/* 发送一个完整 9-bit 串行数据单位 */
static void LCD_SPI_Send9Bit(uint8_t isData, uint8_t value);

/* 发送一条屏幕命令及参数 */
static void LCD_WriteRegister(
    uint8_t command,
    const uint8_t *data,
    uint8_t length
);

/* 屏幕硬件复位 */
static void LCD_HardwareReset(void);

/* 执行完整厂家初始化命令表 */
static void LCD_ExecuteInitTable(void);

/* 对帧缓冲区域执行基本实际读写验证 */
static BSP_LCD_Status_t LCD_CheckFrameMemory(void);

/*============================================================
 * 第七部分：软件 SPI 发送
 *
 * SPI（Serial Peripheral Interface，串行外设接口）
 *
 * ST7701S 的三线通信每次传输 9 位：
 *
 * 第 1 位：D/CX（命令/数据标志）
 *          0 = Command（命令）
 *          1 = Data（参数）
 *
 * 后 8 位：寄存器命令或参数字节。
 *
 * 数据从 MSB（最高有效位）开始发送。
 * 屏幕在 SCL（时钟）的上升沿采样数据。
 *
 * 本函数在每个 9-bit 数据单位前后控制 CS。
 * 与厂家参考工程的事务边界一致。
 *============================================================*/
static void LCD_SPI_Send9Bit(uint8_t isData, uint8_t value)
{
    uint16_t word;
    uint8_t bitIndex;

    /* 最高位为命令/数据标志 */
    word = (uint16_t)(
        ((uint16_t)(isData & 0x01U) << 8U) |
        (uint16_t)value
    );

    /* 时钟首先进入低电平空闲状态 */
    LCD_SCL_LOW();

    /* 选中 LCD 串行接口 */
    LCD_CS_LOW();

    /* 保证片选建立时间 */
    __NOP();
    __NOP();

    /* 发送 9 位数据 */
    for (bitIndex = 0U; bitIndex < 9U; bitIndex++)
    {
        /* 时钟先拉低，准备当前数据位 */
        LCD_SCL_LOW();

        /* 从最高位依次输出 */
        if ((word & 0x0100U) != 0U)
        {
            LCD_SDA_HIGH();
        }
        else
        {
            LCD_SDA_LOW();
        }

        /* 保证数据建立时间 */
        __NOP();
        __NOP();

        /* 上升沿：屏幕锁存当前数据位 */
        LCD_SCL_HIGH();

        /* 保证时钟高电平保持时间 */
        __NOP();
        __NOP();

        /* 左移，准备下一位 */
        word <<= 1U;
    }

    /* 完成最后一个高电平周期 */
    LCD_SCL_LOW();

    /* 保证片选保持时间 */
    __NOP();
    __NOP();

    /* 结束一个 9-bit 数据单位 */
    LCD_CS_HIGH();

    /* 保证两次事务之间有片选高电平间隔 */
    __NOP();
    __NOP();
}

/*============================================================
 * 第八部分：寄存器写入
 *
 * 一条命令的发送方式：
 *
 * 命令标志 + 8 位命令
 * 参数标志 + 8 位参数 0
 * 参数标志 + 8 位参数 1
 * ...
 *
 * 本接口仅在驱动内部使用。
 * 不允许多个 FreeRTOS 任务并发调用。
 *============================================================*/
static void LCD_WriteRegister(
    uint8_t command,
    const uint8_t *data,
    uint8_t length
)
{
    uint8_t i;

    /* 首先发送命令 */
    LCD_SPI_Send9Bit(0U, command);

    /* 发送后续参数 */
    for (i = 0U; i < length; i++)
    {
        LCD_SPI_Send9Bit(1U, data[i]);
    }
}

/*============================================================
 * 第九部分：LCD 硬件复位
 *
 * RESET（复位）为低电平有效。
 *
 * 此处使用保守的启动等待时间。
 * 只在屏幕初始化阶段调用。
 *============================================================*/
static void LCD_HardwareReset(void)
{
    /* 串行接口进入空闲状态 */
    LCD_CS_HIGH();
    LCD_SCL_LOW();
    LCD_SDA_HIGH();

    /* 先释放复位 */
    LCD_RST_HIGH();
    HAL_Delay(5U);

    /* 拉低复位信号 */
    LCD_RST_LOW();
    HAL_Delay(20U);

    /* 重新释放复位 */
    LCD_RST_HIGH();

    /*
     * 等待显示控制器内部上电稳定。
     *
     * 这里采用 220 ms，
     * 保留厂家参考工程的上电等待裕量。
     */
    HAL_Delay(220U);
}

/*============================================================
 * 第十部分：执行 ST7701S 初始化表
 *
 * 本函数仅负责发送软件配置命令。
 *
 * 三线 SPI 为单向写入模式，
 * 无法仅凭发送完成确认屏幕实际执行成功。
 *
 * 因此本函数完成不等于屏幕显示验收通过。
 *============================================================*/
static void LCD_ExecuteInitTable(void)
{
    uint32_t index;

    /* 按照厂家命令表顺序逐条发送 */
    for (index = 0U; index < LCD_INIT_TABLE_COUNT; index++)
    {
        /* 发送当前寄存器及参数 */
        LCD_WriteRegister(
            s_panelInitTable[index].command,
            s_panelInitTable[index].data,
            s_panelInitTable[index].length
        );

        /* 对需要等待的命令执行延时 */
        if (s_panelInitTable[index].delayMs != 0U)
        {
            HAL_Delay(s_panelInitTable[index].delayMs);
        }
    }
}

/*============================================================
 * 第十一部分：帧缓冲基本读写检查
 *
 * 设计目的：
 * 检查用于显示的 SDRAM 实际可读写。
 *
 * 工作方式：
 * 选择帧缓冲中的若干代表性地址。
 * 先写入不同的 32 位数据，再逐一读回。
 *
 * 该函数仅在 LCD 首次初始化前运行。
 * 会覆盖这些地址中的原始数据。
 *
 * 注意：
 * 这是基本读写检查，不是完整内存压力测试。
 *============================================================*/
static BSP_LCD_Status_t LCD_CheckFrameMemory(void)
{
    /*
     * 所有偏移均为 4 字节对齐，
     * 可使用 32 位内存访问。
     */
    static const uint32_t offsets[] =
    {
        0U,
        4U,
        32U,
        256U,
        4096U,

        BSP_LCD_FRAME_BYTES - 4U,

        BSP_LCD_FRAME_BYTES,
        BSP_LCD_FRAME_BYTES + 4U,

        BSP_LCD_FRAME_BYTES * 2U - 4U,

        BSP_SDRAM_FB_REGION_SIZE - 4U
    };

    uint32_t index;
    uint32_t address;
    uint32_t expected;
    volatile uint32_t *memory;

    /* 清除上一次的失败地址 */
    s_memoryErrorAddress = 0U;

    /*--------------------------------------------------------
     * 第一轮：写入所有采样地址。
     *
     * 使用不同数据，能够检查部分地址混叠问题。
     *--------------------------------------------------------*/
    for (index = 0U;
         index < (sizeof(offsets) / sizeof(offsets[0]));
         index++)
    {
        address = BSP_SDRAM_FB_REGION_BASE + offsets[index];

        expected = 0xA55A0000UL ^
                   (index * 0x01010101UL);

        memory = (volatile uint32_t *)(uintptr_t)address;

        *memory = expected;
    }

    /* 确保写入访问完成 */
    __DSB();

    /*--------------------------------------------------------
     * 第二轮：逐一读取并比较。
     *--------------------------------------------------------*/
    for (index = 0U;
         index < (sizeof(offsets) / sizeof(offsets[0]));
         index++)
    {
        address = BSP_SDRAM_FB_REGION_BASE + offsets[index];

        expected = 0xA55A0000UL ^
                   (index * 0x01010101UL);

        memory = (volatile uint32_t *)(uintptr_t)address;

        /* 比较实际读取的数据 */
        if (*memory != expected)
        {
            /* 保存首次发现错误的物理地址 */
            s_memoryErrorAddress = address;

            return BSP_LCD_ERROR_MEMORY;
        }
    }

    /* 所有抽样地址读写一致 */
    return BSP_LCD_OK;
}

/*============================================================
 * 第十二部分：获取帧缓冲地址
 *
 * 双帧缓冲采用连续物理内存。
 *
 * Framebuffer 0：0xC0000000
 * Framebuffer 1：0xC00C8280
 *============================================================*/
uint32_t BSP_LCD_GetFrameBuffer(uint8_t index)
{
    if (index == 0U)
    {
        return BSP_LCD_FB0_ADDR;
    }

    if (index == 1U)
    {
        return BSP_LCD_FB1_ADDR;
    }

    /* 无效编号 */
    return 0U;
}


/*============================================================
 * BSP_LCD_FillBuffer
 *
 * 功能：
 * 使用 DMA2D（二维图形加速器）填充整块帧缓冲。
 *
 * 相比之前的 CPU 逐像素写入：
 * 1. CPU 不再执行 409920 次像素赋值。
 * 2. 专用 DMA2D 硬件负责生成和写入颜色。
 * 3. 通过传输完成检查确定写入已经结束。
 *
 * 注意：
 * 不允许直接填充正在显示的前台缓冲，
 * 否则可能发生画面撕裂。
 * 本函数在 LCD 启动前填充双缓冲是安全的。
 *============================================================*/
BSP_LCD_Status_t BSP_LCD_FillBuffer(
    uint8_t index,
    uint16_t color
)
{
    uint32_t address;

    BSP_DMA2D_Status_t dmaStatus;

    /* 确认 SDRAM 已经初始化 */
    if (BSP_SDRAM_IsReady() == 0U)
    {
        return BSP_LCD_ERROR_SDRAM;
    }

    /* 确认 DMA2D 驱动已经准备好 */
    if (BSP_DMA2D_IsReady() == 0U)
    {
        return BSP_LCD_ERROR_DMA2D;
    }

    /* 获取指定帧缓冲地址 */
    address = BSP_LCD_GetFrameBuffer(index);

    /* 拒绝非法缓冲编号 */
    if (address == 0U)
    {
        return BSP_LCD_ERROR_PARAM;
    }

    /*
     * 运行期间不得直接改写当前正在扫描的帧。
     *
     * 在 LCD 初始化阶段，状态尚未 READY，
     * 两块缓冲均可安全清空。
     */
    if ((s_lcdState == LCD_STATE_READY) &&
        ((index == s_frontBuffer) ||
         (s_presentPending != 0U)))
    {
        return BSP_LCD_ERROR_BUSY;
    }

    /*
     * 使用 DMA2D 填充完整屏幕：
     *
     * 起始地址：address
     * 每行像素：480
     * 起点坐标：(0, 0)
     * 矩形尺寸：480 × 854
     * 颜色格式：RGB565
     */
    dmaStatus = BSP_DMA2D_FillRGB565(
        address,
        BSP_LCD_WIDTH,
        0U,
        0U,
        BSP_LCD_WIDTH,
        BSP_LCD_HEIGHT,
        color
    );

    /* 传输失败，不允许继续使用错误的帧缓冲 */
    if (dmaStatus != BSP_DMA2D_OK)
    {
        return BSP_LCD_ERROR_DMA2D;
    }

    /* 确认硬件写入完成 */
    __DSB();

    return BSP_LCD_OK;
}


/*============================================================
 * 第十四部分：正式 LCD 初始化
 *
 * 初始化步骤：
 *
 * 1. 检查 SDRAM 状态
 * 2. 帧缓冲基本读写验收
 * 3. 清除双帧缓冲
 * 4. 执行屏幕硬件复位
 * 5. 发送 ST7701S 初始化命令
 * 6. 启动 LTDC
 * 7. 标记 LCD 初始化流程完成
 *
 * 注意：
 * 此函数不会开启背光。
 *============================================================*/
BSP_LCD_Status_t BSP_LCD_Init(uint16_t initialColor)
{
    BSP_LCD_Status_t result;

    /* 已经完成初始化，无须重复执行 */
    if (s_lcdState == LCD_STATE_READY)
    {
        return BSP_LCD_OK;
    }

    /* 初始化过程中禁止再次进入 */
    if (s_lcdState == LCD_STATE_INITIALIZING)
    {
        return BSP_LCD_ERROR_BUSY;
    }

    /* 初始化失败后不自动重复操作面板 */
    if (s_lcdState == LCD_STATE_FAILED)
    {
        return s_lastError;
    }

    /* 标记开始初始化 */
    s_lcdState = LCD_STATE_INITIALIZING;

    /*--------------------------------------------------------
     * 步骤 1：检查 SDRAM 初始化状态。
     *--------------------------------------------------------*/
    if (BSP_SDRAM_IsReady() == 0U)
    {
        result = BSP_LCD_ERROR_SDRAM;
        goto init_failed;
    }

    /*--------------------------------------------------------
     * 步骤 2：确认显示帧缓冲可读写。
     *--------------------------------------------------------*/
    result = LCD_CheckFrameMemory();

    if (result != BSP_LCD_OK)
    {
        goto init_failed;
    }

    /*--------------------------------------------------------
     * 步骤 3：初始化双帧缓冲。
     *
     * 先准备完整图像，
     * 然后再允许 LTDC 开始读取。
     *--------------------------------------------------------*/
    result = BSP_LCD_FillBuffer(0U, initialColor);

    if (result != BSP_LCD_OK)
    {
        goto init_failed;
    }

    result = BSP_LCD_FillBuffer(1U, initialColor);

    if (result != BSP_LCD_OK)
    {
        goto init_failed;
    }

    /* 初始化帧缓冲管理状态 */
    s_frontBuffer = 0U;
    s_pendingBuffer = 0U;
    s_presentPending = 0U;

    /*--------------------------------------------------------
     * 步骤 4：屏幕硬件复位。
     *--------------------------------------------------------*/
    LCD_HardwareReset();

    /*--------------------------------------------------------
     * 步骤 5：执行厂家初始化命令。
     *
     * 这里使用已确认来源的 V0 参数。
     * V1 面板仍需要实际显示验收。
     *--------------------------------------------------------*/
    LCD_ExecuteInitTable();

    /*--------------------------------------------------------
     * 步骤 6：启动 LTDC。
     *
     * MX_LTDC_Init() 由 CubeMX 生成。
     *
     * 其中已经配置：
     * 480×854 分辨率
     * RGB565 像素格式
     * 0xC0000000 帧缓冲起始地址
     * LCD 同步时序和像素时钟
     *
     * 注意：
     * CubeMX 生成的函数在 HAL 初始化失败时
     * 可能直接调用 Error_Handler()。
     *--------------------------------------------------------*/
    MX_LTDC_Init();

    /*--------------------------------------------------------
     * 步骤 7：确认 LTDC 已完成初始化。
     *--------------------------------------------------------*/
    if (hltdc.State != HAL_LTDC_STATE_READY)
    {
        result = BSP_LCD_ERROR_LTDC;
        goto init_failed;
    }

    /* 确保 LTDC 配置及图像写入操作已完成 */
    __DSB();
    __ISB();

    /* 标记驱动初始化流程成功 */
    s_lcdState = LCD_STATE_READY;
    s_lastError = BSP_LCD_OK;

    return BSP_LCD_OK;

/*------------------------------------------------------------
 * 初始化失败统一处理
 *------------------------------------------------------------*/
init_failed:

    /* 保存失败原因 */
    s_lastError = result;

    /* 标记初始化失败 */
    s_lcdState = LCD_STATE_FAILED;

    /* 返回具体错误 */
    return result;
}

/*============================================================
 * 第十五部分：请求切换显示帧缓冲
 *
 * Present（提交显示）：
 * 告诉 LTDC 下一帧应该显示哪块缓冲。
 *
 * 采用 Vertical Blanking（垂直消隐期）
 * 重新加载方式，降低画面撕裂风险。
 *
 * 返回 OK 仅表示切换请求已成功提交，
 * 不表示切换已经在屏幕上完成。
 *============================================================*/
BSP_LCD_Status_t BSP_LCD_Present(uint8_t index)
{
    uint32_t address;

    /* 必须完成 LCD 初始化 */
    if (s_lcdState != LCD_STATE_READY)
    {
        return BSP_LCD_ERROR_NOT_READY;
    }

    /* 检查目标帧缓冲编号 */
    address = BSP_LCD_GetFrameBuffer(index);

    if (address == 0U)
    {
        return BSP_LCD_ERROR_PARAM;
    }

    /* 前一个切换尚未完成，拒绝新的请求 */
    if (s_presentPending != 0U)
    {
        return BSP_LCD_ERROR_BUSY;
    }

    /* 已经是当前显示缓冲，无须重复切换 */
    if (index == s_frontBuffer)
    {
        return BSP_LCD_OK;
    }

    /* 标记正在等待的缓冲编号 */
    s_pendingBuffer = index;

    /* 在提交硬件请求前设置等待标志 */
    s_presentPending = 1U;

    /* 先修改图层帧缓冲地址，不立即生效 */
    if (HAL_LTDC_SetAddress_NoReload(
            &hltdc,
            address,
            0U) != HAL_OK)
    {
        s_presentPending = 0U;
        s_lastError = BSP_LCD_ERROR_LTDC;

        return BSP_LCD_ERROR_LTDC;
    }

    /*
     * 请求在下一次垂直消隐期间
     * 重新加载图层配置。
     */
    if (HAL_LTDC_Reload(
            &hltdc,
            LTDC_RELOAD_VERTICAL_BLANKING) != HAL_OK)
    {
        s_presentPending = 0U;
        s_lastError = BSP_LCD_ERROR_LTDC;

        return BSP_LCD_ERROR_LTDC;
    }

    return BSP_LCD_OK;
}

/*============================================================
 * 第十六部分：LTDC 重新加载完成回调
 *
 * HAL（硬件抽象层）在 LTDC Reload Event
 * （显示配置重新加载事件）发生时调用。
 *
 * 该函数运行在中断上下文中。
 *
 * 不允许：
 * 1. 阻塞等待
 * 2. 执行复杂 UI 绘制
 * 3. 调用不适合中断环境的 FreeRTOS API
 *
 * 这里只更新必要状态变量。
 *============================================================*/
void HAL_LTDC_ReloadEventCallback(
    LTDC_HandleTypeDef *ltdcHandle
)
{
    /* 只处理本系统的 LTDC */
    if (ltdcHandle->Instance != LTDC)
    {
        return;
    }

    /* 没有等待的帧切换则直接返回 */
    if (s_presentPending == 0U)
    {
        return;
    }

    /* 更新当前已经切换的帧缓冲编号 */
    s_frontBuffer = s_pendingBuffer;

    /* 清除等待标志 */
    s_presentPending = 0U;
}

/*============================================================
 * 第十七部分：背光 PWM 亮度控制
 *
 * 背光电路：
 * TIM2_CH2 -> PA1 -> MP3302 EN
 *
 * PWM1（脉宽调制模式 1）：
 * 0% 占空比 -> 输出低电平
 * 100% 占空比 -> 输出高电平
 *
 * 本函数在首次启用背光时，
 * 显式将 TIM2_CH2 配置为 PWM1。
 *
 * 这样不会依赖当前 CubeMX 中 PWM2 的方向。
 *
 * 注意：
 * 只有硬件安全授权后才能开启背光。
 *============================================================*/
BSP_LCD_Status_t BSP_LCD_SetBacklight(
    uint8_t percent
)
{
    TIM_OC_InitTypeDef pwmConfig = {0};
    uint32_t period;
    uint32_t compareValue;

    /* 亮度百分比不能大于 100 */
    if (percent > 100U)
    {
        return BSP_LCD_ERROR_PARAM;
    }

    /*--------------------------------------------------------
     * 关闭背光始终允许。
     *
     * 背光未启动时，硬件 EN 下拉电阻负责
     * 将升压芯片维持在关闭状态。
     *--------------------------------------------------------*/
    if (percent == 0U)
    {
        if (s_backlightStarted != 0U)
        {
            __HAL_TIM_SET_COMPARE(
                &htim2,
                TIM_CHANNEL_2,
                0U
            );
        }

        return BSP_LCD_OK;
    }

    /*--------------------------------------------------------
     * 非零背光亮度必须满足两个条件：
     *
     * 1. LCD 已完成初始化。
     * 2. 背光硬件安全条件已确认。
     *--------------------------------------------------------*/
    if (s_lcdState != LCD_STATE_READY)
    {
        return BSP_LCD_ERROR_NOT_READY;
    }

    if (BSP_LCD_BACKLIGHT_APPROVED == 0U)
    {
        return BSP_LCD_ERROR_BACKLIGHT_LOCK;
    }

    /*--------------------------------------------------------
     * 首次启用 PWM。
     *--------------------------------------------------------*/
    if (s_backlightStarted == 0U)
    {
        /*
         * 使用 PWM1 模式。
         *
         * CNT（计数值）小于 CCR（比较值）
         * 时输出有效高电平。
         */
        pwmConfig.OCMode = TIM_OCMODE_PWM1;

        /* 初始化为 0% 占空比 */
        pwmConfig.Pulse = 0U;

        /* 高电平为有效输出 */
        pwmConfig.OCPolarity = TIM_OCPOLARITY_HIGH;

        /* 关闭快速模式 */
        pwmConfig.OCFastMode = TIM_OCFAST_DISABLE;

        /* 配置 TIM2 通道 2 */
        if (HAL_TIM_PWM_ConfigChannel(
                &htim2,
                &pwmConfig,
                TIM_CHANNEL_2) != HAL_OK)
        {
            s_lastError = BSP_LCD_ERROR_PWM;
            return BSP_LCD_ERROR_PWM;
        }

        /* 启动 PWM，此时比较值仍然为 0 */
        if (HAL_TIM_PWM_Start(
                &htim2,
                TIM_CHANNEL_2) != HAL_OK)
        {
            s_lastError = BSP_LCD_ERROR_PWM;
            return BSP_LCD_ERROR_PWM;
        }

        /* 标记 PWM 已启动 */
        s_backlightStarted = 1U;
    }

    /*--------------------------------------------------------
     * 根据 TIM2 自动重装载值计算占空比。
     *
     * 当前 Period = 999。
     * 一个完整计数周期为 1000 个计数。
     *--------------------------------------------------------*/
    period = htim2.Instance->ARR + 1U;

    compareValue = (uint32_t)(
        ((uint64_t)period * (uint64_t)percent) / 100ULL
    );

    /* 更新 TIM2 通道 2 的比较值 */
    __HAL_TIM_SET_COMPARE(
        &htim2,
        TIM_CHANNEL_2,
        compareValue
    );

    return BSP_LCD_OK;
}

/*============================================================
 * 第十八部分：状态查询接口
 *============================================================*/

/* 查询 LCD 初始化状态 */
uint8_t BSP_LCD_IsReady(void)
{
    return (s_lcdState == LCD_STATE_READY) ? 1U : 0U;
}

/* 获取最近一次错误 */
BSP_LCD_Status_t BSP_LCD_GetLastError(void)
{
    return s_lastError;
}

/* 获取内存错误地址 */
uint32_t BSP_LCD_GetMemoryErrorAddress(void)
{
    return s_memoryErrorAddress;
}

/* 查询是否正在等待画面切换 */
uint8_t BSP_LCD_IsPresentPending(void)
{
    return s_presentPending;
}

/* 获取当前显示缓冲编号 */
uint8_t BSP_LCD_GetFrontBuffer(void)
{
    return s_frontBuffer;
}
