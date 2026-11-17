
/*============================================================
 * 文件名称：bsp_dma2d.c
 * 功能：STM32H743 DMA2D 正式图形加速驱动
 *
 * 当前支持：
 * 1. RGB565 矩形颜色填充。
 * 2. RGB565 图像矩形复制。
 * 3. 外部 SDRAM 地址范围检查。
 * 4. HAL 传输完成等待和错误恢复。
 *
 * 内存架构：
 * 0xC0000000 - 0xC01FFFFF
 * 为 MPU（内存保护单元）定义的非缓存区域。
 *
 * 当前接口不支持缓存区内的图像数据。
 * 若后续使用可缓存内存，必须配套处理
 * D-Cache（数据缓存）一致性。
 *
 * 软件使用限制：
 * DMA2D 由单个显示所有者统一调度。
 * 不允许多个任务同时操作 DMA2D。
 *============================================================*/

#include "bsp_dma2d.h"      /* DMA2D 正式驱动接口 */
#include "bsp_sdram.h"      /* SDRAM 地址与容量定义 */
#include "dma2d.h"          /* CubeMX 生成的 DMA2D 接口 */
#include "stm32h7xx_hal.h" /* HAL 硬件抽象层 */

/*============================================================
 * 内部参数
 *============================================================*/

/* DMA2D 输出采用 RGB565 */
#define DMA2D_OUTPUT_FORMAT  DMA2D_OUTPUT_RGB565

/* DMA2D 输入采用 RGB565 */
#define DMA2D_INPUT_FORMAT   DMA2D_INPUT_RGB565

/* DMA2D 前景层编号 */
#define DMA2D_FG_LAYER      (1U)

/* MPU 非缓存显示区域末尾的下一个地址 */
#define DMA2D_FB_LIMIT \
    (BSP_SDRAM_FB_REGION_BASE + BSP_SDRAM_FB_REGION_SIZE)

/*============================================================
 * 内部状态
 *============================================================*/

/* 驱动是否已经初始化 */
static uint8_t s_dma2dReady = 0U;

/* 驱动正在执行一次同步操作 */
static volatile uint8_t s_dma2dBusy = 0U;

/* 保存最近一次操作结果 */
static BSP_DMA2D_Status_t s_lastStatus =
    BSP_DMA2D_ERROR_NOT_READY;

/*============================================================
 * 内部函数声明
 *============================================================*/

/* 检查矩形是否位于允许的内存区域 */
static uint8_t DMA2D_CheckRect(
    uint32_t base,
    uint16_t stride,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint32_t *startAddress,
    uint64_t *endAddress
);

/* 将 RGB565 转换为 RGB888 */
static uint32_t DMA2D_RGB565ToRGB888(uint16_t color);

/* 配置当前 DMA2D 工作模式 */
static BSP_DMA2D_Status_t DMA2D_SetMode(
    uint32_t mode,
    uint32_t outputOffset,
    uint32_t inputOffset
);

/* 启动一次同步 DMA2D 传输 */
static BSP_DMA2D_Status_t DMA2D_Transfer(
    uint32_t sourceOrColor,
    uint32_t destination,
    uint32_t width,
    uint32_t height
);

/*============================================================
 * BSP_DMA2D_Init
 *
 * 功能：
 * 检查 CubeMX 初始化后的 DMA2D 是否可用。
 *
 * 调用顺序：
 * MX_DMA2D_Init()
 *       ↓
 * BSP_SDRAM_Init()
 *       ↓
 * BSP_DMA2D_Init()
 *
 * 本函数不重复调用 MX_DMA2D_Init()。
 *============================================================*/
BSP_DMA2D_Status_t BSP_DMA2D_Init(void)
{
    /* 如果已经初始化，直接返回 */
    if (s_dma2dReady != 0U)
    {
        return BSP_DMA2D_OK;
    }

    /* 外部 SDRAM 必须首先完成初始化 */
    if (BSP_SDRAM_IsReady() == 0U)
    {
        s_lastStatus = BSP_DMA2D_ERROR_NOT_READY;
        return s_lastStatus;
    }

    /* 检查 DMA2D 寄存器地址是否正确 */
    if (hdma2d.Instance != DMA2D)
    {
        s_lastStatus = BSP_DMA2D_ERROR_NOT_READY;
        return s_lastStatus;
    }

    /*
     * 确认 CubeMX 生成的初始化已经执行。
     *
     * HAL_DMA2D_STATE_READY：
     * 表示当前 HAL 驱动处于可操作状态。
     */
    if (hdma2d.State != HAL_DMA2D_STATE_READY)
    {
        s_lastStatus = BSP_DMA2D_ERROR_NOT_READY;
        return s_lastStatus;
    }

    /* 初始化内部驱动状态 */
    s_dma2dBusy = 0U;

    /* 标记驱动准备完成 */
    s_dma2dReady = 1U;

    s_lastStatus = BSP_DMA2D_OK;

    return BSP_DMA2D_OK;
}

/*============================================================
 * DMA2D_CheckRect
 *
 * 功能：
 * 验证矩形的宽度、坐标和物理地址范围。
 *
 * 为什么使用 uint64_t？
 *
 * 因为地址和尺寸相加时可能产生 32 位溢出。
 * 使用 64 位运算能够在访问内存之前识别
 * 溢出或者越界。
 *
 * endAddress 为矩形覆盖范围末尾的
 * 下一个字节地址，不包含在实际访问范围内。
 *============================================================*/
static uint8_t DMA2D_CheckRect(
    uint32_t base,
    uint16_t stride,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint32_t *startAddress,
    uint64_t *endAddress
)
{
    uint64_t first;
    uint64_t last;

    /* 输出指针必须有效 */
    if ((startAddress == NULL) || (endAddress == NULL))
    {
        return 0U;
    }

    /* 图像尺寸和行跨度不能为零 */
    if ((stride == 0U) || (width == 0U) || (height == 0U))
    {
        return 0U;
    }

    /* 矩形不能超出一行的有效像素范围 */
    if ((x >= stride) || (width > (stride - x)))
    {
        return 0U;
    }

    /* RGB565 像素需要 2 字节对齐 */
    if ((base & 0x01U) != 0U)
    {
        return 0U;
    }

    /*
     * 计算第一行第一个像素地址。
     *
     * 一个像素为 2 字节。
     */
    first = (uint64_t)base +
            (((uint64_t)y * stride + x) *
             BSP_DMA2D_PIXEL_BYTES);

    /*
     * 计算最后一行最后一个像素
     * 后面的地址。
     */
    last = (uint64_t)base +
           ((((uint64_t)y + height - 1U) * stride +
             x + width) *
            BSP_DMA2D_PIXEL_BYTES);

    /*
     * 当前只允许访问前 2 MiB 非缓存 SDRAM。
     */
    if ((first < BSP_SDRAM_FB_REGION_BASE) ||
        (last > DMA2D_FB_LIMIT))
    {
        return 0U;
    }

    /* 返回已经确认合法的范围 */
    *startAddress = (uint32_t)first;
    *endAddress = last;

    return 1U;
}

/*============================================================
 * DMA2D_RGB565ToRGB888
 *
 * 功能：
 * 将 16 位 RGB565 颜色转换为 24 位 RGB888。
 *
 * RGB565：
 * RRRRR GGGGGG BBBBB
 *
 * RGB888：
 * RRRRRRRR GGGGGGGG BBBBBBBB
 *
 * 注意：
 * HAL_DMA2D_Start() 在 R2M（寄存器到存储器）
 * 模式下，传入的是颜色寄存器的 RGB 分量。
 *
 * HAL 会按照输出颜色格式重新打包。
 * 因此不能直接把 0xF800 当成 24 位红色传入。
 *============================================================*/
static uint32_t DMA2D_RGB565ToRGB888(uint16_t color)
{
    uint32_t r5;
    uint32_t g6;
    uint32_t b5;

    uint32_t r8;
    uint32_t g8;
    uint32_t b8;

    /* 提取原始红色 5 位 */
    r5 = (color >> 11U) & 0x1FU;

    /* 提取原始绿色 6 位 */
    g6 = (color >> 5U) & 0x3FU;

    /* 提取原始蓝色 5 位 */
    b5 = color & 0x1FU;

    /* 将红色 5 位扩展为 8 位 */
    r8 = (r5 << 3U) | (r5 >> 2U);

    /* 将绿色 6 位扩展为 8 位 */
    g8 = (g6 << 2U) | (g6 >> 4U);

    /* 将蓝色 5 位扩展为 8 位 */
    b8 = (b5 << 3U) | (b5 >> 2U);

    /* 按 0x00RRGGBB 顺序组装 */
    return (r8 << 16U) | (g8 << 8U) | b8;
}

/*============================================================
 * DMA2D_SetMode
 *
 * 功能：
 * 为当前操作配置 DMA2D 的传输模式。
 *
 * DMA2D_R2M：
 * Register To Memory（寄存器到存储器）
 * 用于快速颜色填充。
 *
 * DMA2D_M2M：
 * Memory To Memory（存储器到存储器）
 * 用于图像像素复制。
 *
 * OutputOffset：
 * 输出行末需要跳过的像素数量。
 *
 * InputOffset：
 * 输入行末需要跳过的像素数量。
 *============================================================*/
static BSP_DMA2D_Status_t DMA2D_SetMode(
    uint32_t mode,
    uint32_t outputOffset,
    uint32_t inputOffset
)
{
    /* 设置 DMA2D 工作模式 */
    hdma2d.Init.Mode = mode;

    /* 输出像素格式固定为 RGB565 */
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_FORMAT;

    /* 设置输出行偏移，单位为像素 */
    hdma2d.Init.OutputOffset = outputOffset;

    /* 输出 Alpha（透明度）不反转 */
    hdma2d.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;

    /* 不交换红色和蓝色通道 */
    hdma2d.Init.RedBlueSwap = DMA2D_RB_REGULAR;

    /* 保持字节正常顺序 */
    hdma2d.Init.BytesSwap = DMA2D_BYTES_REGULAR;

    /* 行偏移使用像素数量，不使用字节数量 */
    hdma2d.Init.LineOffsetMode = DMA2D_LOM_PIXELS;

    /* 将当前模式参数写入 DMA2D 寄存器 */
    if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
    {
        return BSP_DMA2D_ERROR_CONFIG;
    }

    /*--------------------------------------------------------
     * M2M 模式还需要配置前景输入层。
     *
     * R2M 不需要源图像，因此无需配置输入层。
     *--------------------------------------------------------*/
    if (mode == DMA2D_M2M)
    {
        /* 前景输入图像颜色格式 */
        hdma2d.LayerCfg[DMA2D_FG_LAYER].InputColorMode =
            DMA2D_INPUT_FORMAT;

        /* 输入图像每行末尾的像素偏移 */
        hdma2d.LayerCfg[DMA2D_FG_LAYER].InputOffset =
            inputOffset;

        /* 不改变源像素 Alpha */
        hdma2d.LayerCfg[DMA2D_FG_LAYER].AlphaMode =
            DMA2D_NO_MODIF_ALPHA;

        /* 默认 Alpha 设为不透明 */
        hdma2d.LayerCfg[DMA2D_FG_LAYER].InputAlpha =
            0xFFU;

        /* 不反转输入 Alpha */
        hdma2d.LayerCfg[DMA2D_FG_LAYER].AlphaInverted =
            DMA2D_REGULAR_ALPHA;

        /* 不交换输入红蓝通道 */
        hdma2d.LayerCfg[DMA2D_FG_LAYER].RedBlueSwap =
            DMA2D_RB_REGULAR;

        /* 配置前景输入层寄存器 */
        if (HAL_DMA2D_ConfigLayer(
                &hdma2d,
                DMA2D_FG_LAYER) != HAL_OK)
        {
            return BSP_DMA2D_ERROR_CONFIG;
        }
    }

    return BSP_DMA2D_OK;
}

/*============================================================
 * DMA2D_Transfer
 *
 * 功能：
 * 执行一次同步图形传输。
 *
 * HAL_DMA2D_Start：
 * 配置源数据、目标地址和矩形尺寸，
 * 启动硬件 DMA2D。
 *
 * HAL_DMA2D_PollForTransfer：
 * 等待传输完成，检查传输结果。
 *
 * 注意：
 * 同步接口会阻塞当前调用者。
 * 后续高频 LVGL 绘制将使用非阻塞方式。
 *============================================================*/
static BSP_DMA2D_Status_t DMA2D_Transfer(
    uint32_t sourceOrColor,
    uint32_t destination,
    uint32_t width,
    uint32_t height
)
{
    HAL_StatusTypeDef halStatus;

    /* 确保 DMA2D 启动前的内存访问已经完成 */
    __DSB();

    /* 启动一次硬件图形传输 */
    halStatus = HAL_DMA2D_Start(
        &hdma2d,
        sourceOrColor,
        destination,
        width,
        height
    );

    if (halStatus != HAL_OK)
    {
        return BSP_DMA2D_ERROR_TRANSFER;
    }

    /* 等待本次传输完成 */
    halStatus = HAL_DMA2D_PollForTransfer(
        &hdma2d,
        BSP_DMA2D_TIMEOUT_MS
    );

    /* 传输完成 */
    if (halStatus == HAL_OK)
    {
        /* 确保 DMA2D 写入结果对后续访问可见 */
        __DSB();

        return BSP_DMA2D_OK;
    }

    /*
     * 出错后停止当前传输。
     *
     * 这里不自动重试，
     * 避免尚未处理的异常持续破坏内存。
     */
    (void)HAL_DMA2D_Abort(&hdma2d);

    /* 区分超时与普通硬件错误 */
    if (halStatus == HAL_TIMEOUT)
    {
        return BSP_DMA2D_ERROR_TIMEOUT;
    }

    return BSP_DMA2D_ERROR_TRANSFER;
}

/*============================================================
 * BSP_DMA2D_FillRGB565
 *
 * 功能：
 * 使用 DMA2D 将指定矩形填充为 RGB565 颜色。
 *
 * 例如：
 * stridePixels = 480
 * x = 0
 * y = 0
 * width = 480
 * height = 854
 *
 * 代表填充一整帧画面。
 *============================================================*/
BSP_DMA2D_Status_t BSP_DMA2D_FillRGB565(
    uint32_t bufferAddress,
    uint16_t stridePixels,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color
)
{
    uint32_t destination;
    uint32_t rgb888;
    uint64_t endAddress;

    BSP_DMA2D_Status_t result;

    /* 驱动尚未初始化 */
    if (s_dma2dReady == 0U)
    {
        return BSP_DMA2D_ERROR_NOT_READY;
    }

    /* 阻塞接口不能从中断中调用 */
    if (__get_IPSR() != 0U)
    {
        return BSP_DMA2D_ERROR_PARAM;
    }

    /* 检查另一个图形操作是否正在执行 */
    if (s_dma2dBusy != 0U)
    {
        return BSP_DMA2D_ERROR_BUSY;
    }

    /* 检查当前矩形的地址与尺寸 */
    if (DMA2D_CheckRect(
            bufferAddress,
            stridePixels,
            x,
            y,
            width,
            height,
            &destination,
            &endAddress) == 0U)
    {
        return BSP_DMA2D_ERROR_ADDRESS;
    }

    /*
     * 当前由启动协调器或单一 UI Task 调用，
     * 后续多任务调用必须通过显示服务串行化。
     */
    s_dma2dBusy = 1U;

    /* 配置寄存器到存储器模式 */
    result = DMA2D_SetMode(
        DMA2D_R2M,
        (uint32_t)(stridePixels - width),
        0U
    );

    if (result == BSP_DMA2D_OK)
    {
        /* 将输入颜色转换为 24 位 RGB */
        rgb888 = DMA2D_RGB565ToRGB888(color);

        /* 执行硬件颜色填充 */
        result = DMA2D_Transfer(
            rgb888,
            destination,
            width,
            height
        );
    }

    /* 保存操作状态 */
    s_lastStatus = result;

    /* 释放 DMA2D 操作状态 */
    s_dma2dBusy = 0U;

    return result;
}

/*============================================================
 * BSP_DMA2D_CopyRGB565
 *
 * 功能：
 * 从源图像复制一块 RGB565 矩形到目标图像。
 *
 * 适用于：
 * 1. 完整帧缓冲复制。
 * 2. 复制图形局部区域。
 * 3. 后续显示缓存同步。
 *
 * 限制：
 * 不支持源和目标实际存储范围重叠。
 *============================================================*/
BSP_DMA2D_Status_t BSP_DMA2D_CopyRGB565(
    uint32_t srcAddress,
    uint32_t dstAddress,
    uint16_t srcStride,
    uint16_t dstStride,
    uint16_t srcX,
    uint16_t srcY,
    uint16_t dstX,
    uint16_t dstY,
    uint16_t width,
    uint16_t height
)
{
    uint32_t source;
    uint32_t destination;

    uint64_t sourceEnd;
    uint64_t destinationEnd;

    BSP_DMA2D_Status_t result;

    /* 确保驱动已经初始化 */
    if (s_dma2dReady == 0U)
    {
        return BSP_DMA2D_ERROR_NOT_READY;
    }

    /* 禁止在中断服务函数中阻塞等待 */
    if (__get_IPSR() != 0U)
    {
        return BSP_DMA2D_ERROR_PARAM;
    }

    /* 当前图形加速器正在使用 */
    if (s_dma2dBusy != 0U)
    {
        return BSP_DMA2D_ERROR_BUSY;
    }

    /* 检查源矩形范围 */
    if (DMA2D_CheckRect(
            srcAddress,
            srcStride,
            srcX,
            srcY,
            width,
            height,
            &source,
            &sourceEnd) == 0U)
    {
        return BSP_DMA2D_ERROR_ADDRESS;
    }

    /* 检查目标矩形范围 */
    if (DMA2D_CheckRect(
            dstAddress,
            dstStride,
            dstX,
            dstY,
            width,
            height,
            &destination,
            &destinationEnd) == 0U)
    {
        return BSP_DMA2D_ERROR_ADDRESS;
    }

    /*
     * 使用保守的连续地址包络检查。
     *
     * 如果源区域和目标区域可能重叠，
     * 直接拒绝复制，避免未定义的覆盖行为。
     *
     * 这种检查可能拒绝部分实际不重叠、
     * 但地址包络交叉的同帧矩形。
     */
    if (!((sourceEnd <= destination) ||
          (destinationEnd <= source)))
    {
        return BSP_DMA2D_ERROR_OVERLAP;
    }

    /* 标记当前正在执行硬件传输 */
    s_dma2dBusy = 1U;

    /* 配置存储器到存储器模式 */
    result = DMA2D_SetMode(
        DMA2D_M2M,
        (uint32_t)(dstStride - width),
        (uint32_t)(srcStride - width)
    );

    if (result == BSP_DMA2D_OK)
    {
        /* 执行图像矩形复制 */
        result = DMA2D_Transfer(
            source,
            destination,
            width,
            height
        );
    }

    /* 保存最近一次操作结果 */
    s_lastStatus = result;

    /* 释放图形加速器 */
    s_dma2dBusy = 0U;

    return result;
}

/*============================================================
 * 驱动状态查询
 *============================================================*/

/* 判断驱动是否完成初始化 */
uint8_t BSP_DMA2D_IsReady(void)
{
    return s_dma2dReady;
}

/* 获取最近一次图形操作状态 */
BSP_DMA2D_Status_t BSP_DMA2D_GetLastStatus(void)
{
    return s_lastStatus;
}
