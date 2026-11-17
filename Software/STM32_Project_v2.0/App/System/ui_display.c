#include "ui_display.h"

#include "bsp_lcd.h"
#include "bsp_dma2d.h"
#include "stm32h7xx_hal.h"

#include <stddef.h>
#include <stdint.h>

/*============================================================
 * LVGL 局部绘制缓冲地址
 *
 * 两块完整帧缓冲：
 * FB0 = 0xC0000000
 * FB1 = 0xC00C8280
 *
 * 两块完整帧缓冲结束地址：
 * 0xC0190500
 *
 * 因此前 2 MiB 显示区域剩余空间
 * 可以继续放置 LVGL 局部绘制缓冲。
 *
 * 本方案：
 * A = 0xC0191000
 * B = 0xC01A4000
 *
 * 每块 480 × 80 × 2 = 76800 字节。
 *
 * 两块均位于 MPU 非缓存 SDRAM 区域内，
 * 不需要额外执行 D-Cache 清理。
 *============================================================*/

#define APP_UI_DRAW_A_ADDR    (0xC0191000UL)

#define APP_UI_DRAW_B_ADDR    (0xC01A4000UL)

/* 帧切换等待超时：200ms */
#define APP_UI_PRESENT_TIMEOUT_MS   (200U)

/* 当前是否正在绘制一帧 */
static uint8_t s_frameActive = 0U;

/* 当前后台帧缓冲编号 */
static uint8_t s_backBuffer = 0U;

/*============================================================
 * 等待 LTDC 在垂直消隐期完成切换
 *
 * 注意：
 * 这里只查询 BSP 的切换状态。
 * LTDC 中断负责更新实际前台缓冲编号。
 *============================================================*/
static uint8_t App_UI_WaitPresent(uint8_t index)
{
    uint32_t startTick;

    /* 获取开始等待的系统时间 */
    startTick = HAL_GetTick();

    /* 等待 LTDC 完成帧缓冲地址重新加载 */
    while (BSP_LCD_IsPresentPending() != 0U)
    {
        /* 超过允许时间则返回失败 */
        if ((uint32_t)(HAL_GetTick() - startTick) >=
            APP_UI_PRESENT_TIMEOUT_MS)
        {
            return 0U;
        }
    }

    /* 检查实际显示的缓冲是否正确 */
    if (BSP_LCD_GetFrontBuffer() != index)
    {
        return 0U;
    }

    /* 帧切换完成 */
    return 1U;
}

/*============================================================
 * 开始绘制新一帧
 *
 * 为什么需要整帧复制？
 *
 * LVGL 当前使用局部渲染模式，
 * 并不是每次都重新绘制整个屏幕。
 *
 * 因此必须先将当前完整画面复制到
 * 后台帧缓冲，然后覆盖变化的区域。
 *
 * 否则直接交换双缓冲，可能丢失
 * 上一帧中没有重新绘制的 UI 内容。
 *============================================================*/
static uint8_t App_UI_BeginFrame(void)
{
    uint8_t frontBuffer;
    uint32_t frontAddress;
    uint32_t backAddress;

    /* 如果已经开始当前帧，无需重复复制 */
    if (s_frameActive != 0U)
    {
        return 1U;
    }

    /* 不允许在上次缓冲切换期间开始绘制 */
    if (BSP_LCD_IsPresentPending() != 0U)
    {
        return 0U;
    }

    /* 读取 LTDC 当前正在显示的缓冲编号 */
    frontBuffer = BSP_LCD_GetFrontBuffer();

    /* 选择另一块缓冲作为后台缓冲 */
    s_backBuffer = (frontBuffer == 0U) ? 1U : 0U;

    /* 获取实际的 SDRAM 地址 */
    frontAddress = BSP_LCD_GetFrameBuffer(frontBuffer);

    backAddress = BSP_LCD_GetFrameBuffer(s_backBuffer);

    /* 检查缓冲地址 */
    if ((frontAddress == 0U) || (backAddress == 0U))
    {
        return 0U;
    }

    /*
     * DMA2D 复制当前整帧到后台。
     *
     * 这一步保持未修改区域的画面连续性，
     * 后续再把 LVGL 的局部更新覆盖进去。
     */
    if (BSP_DMA2D_CopyRGB565(
            frontAddress,
            backAddress,
            BSP_LCD_WIDTH,
            BSP_LCD_WIDTH,
            0U,
            0U,
            0U,
            0U,
            BSP_LCD_WIDTH,
            BSP_LCD_HEIGHT
        ) != BSP_DMA2D_OK)
    {
        return 0U;
    }

    /* 标记当前帧已经开始 */
    s_frameActive = 1U;

    return 1U;
}

/*============================================================
 * LVGL 刷新回调
 *
 * area：本次需要更新的矩形坐标。
 * pixels：LVGL 已绘制好的 RGB565 图像数据。
 * stride_bytes：每行图像实际占用的字节数。
 *
 * 整个函数在 UI Task 中同步执行。
 *
 * 当前采用 DMA2D 阻塞传输，
 * 返回 DONE 时已经释放源缓冲所有权。
 *
 * 这样与现有 ui_stm32_display.c
 * 的刷新完成协议保持一致。
 *============================================================*/
static ui_stm32_flush_status_t App_UI_StartFlush(
    const lv_area_t *area,
    const uint8_t *pixels,
    uint32_t stride_bytes,
    void *context
)
{
    uint32_t backAddress;
    uint32_t width;
    uint32_t height;
    lv_display_t *display;
    uint8_t isLast;

    /* 当前不需要额外的上下文对象 */
    (void)context;

    /* 检查输入参数 */
    if ((area == NULL) || (pixels == NULL))
    {
        return UI_STM32_FLUSH_FAILED;
    }

    /* 检查更新区域坐标是否合法 */
    if ((area->x1 < 0) ||
        (area->y1 < 0) ||
        (area->x2 >= (int32_t)BSP_LCD_WIDTH) ||
        (area->y2 >= (int32_t)BSP_LCD_HEIGHT) ||
        (area->x2 < area->x1) ||
        (area->y2 < area->y1))
    {
        return UI_STM32_FLUSH_FAILED;
    }

    /* 计算本次更新矩形的宽度和高度 */
    width = (uint32_t)(area->x2 - area->x1 + 1);

    height = (uint32_t)(area->y2 - area->y1 + 1);

    /*
     * stride_bytes 必须满足：
     * 1. RGB565 的 2 字节像素对齐。
     * 2. 行跨度不能小于实际矩形宽度。
     */
    if (((stride_bytes & 1U) != 0U) ||
        (stride_bytes < width * 2U) ||
        ((stride_bytes / 2U) > UINT16_MAX))
    {
        return UI_STM32_FLUSH_FAILED;
    }

    /*
     * 取得当前 LVGL 显示对象。
     *
     * 本产品当前只创建一个 480×854 显示器，
     * 因此使用默认显示对象。
     */
    display = lv_display_get_default();

    if (display == NULL)
    {
        return UI_STM32_FLUSH_FAILED;
    }

    /* 查询这是不是本轮 LVGL 刷新的最后一块区域 */
    isLast = lv_display_flush_is_last(display) ? 1U : 0U;

    /* 如果当前是新帧，先复制上一帧内容 */
    if (App_UI_BeginFrame() == 0U)
    {
        return UI_STM32_FLUSH_FAILED;
    }

    /* 获取当前后台完整帧缓冲地址 */
    backAddress = BSP_LCD_GetFrameBuffer(s_backBuffer);

    if (backAddress == 0U)
    {
        return UI_STM32_FLUSH_FAILED;
    }

    /*
     * 将 LVGL 局部绘制结果通过 DMA2D
     * 复制到后台完整帧缓冲对应位置。
     *
     * 源地址：LVGL 的局部绘制缓冲。
     * 目标地址：LTDC 的后台帧缓冲。
     */
    if (BSP_DMA2D_CopyRGB565(
            (uint32_t)(uintptr_t)pixels,
            backAddress,
            (uint16_t)(stride_bytes / 2U),
            BSP_LCD_WIDTH,
            0U,
            0U,
            (uint16_t)area->x1,
            (uint16_t)area->y1,
            (uint16_t)width,
            (uint16_t)height
        ) != BSP_DMA2D_OK)
    {
        s_frameActive = 0U;
        return UI_STM32_FLUSH_FAILED;
    }

    /*
     * 如果还有其他更新区域，
     * 只需要等待 LVGL 继续调用本回调。
     */
    if (isLast == 0U)
    {
        return UI_STM32_FLUSH_DONE;
    }

    /*
     * 所有局部更新完成。
     *
     * 现在才允许 LTDC 切换到新画面。
     */
    if (BSP_LCD_Present(s_backBuffer) != BSP_LCD_OK)
    {
        s_frameActive = 0U;
        return UI_STM32_FLUSH_FAILED;
    }

    /* 等待真正完成垂直同步帧缓冲切换 */
    if (App_UI_WaitPresent(s_backBuffer) == 0U)
    {
        s_frameActive = 0U;
        return UI_STM32_FLUSH_FAILED;
    }

    /* 一帧已经完整提交 */
    s_frameActive = 0U;

    /* DMA2D 传输和 LTDC 切换均已完成 */
    return UI_STM32_FLUSH_DONE;
}

/*============================================================
 * App_UI_DisplayBuildPort
 *
 * 构造现有 UI 库要求的显示接口结构体。
 *
 * 此函数不调用 lv_init()，
 * 也不主动创建 LVGL 显示对象。
 *
 * UI 启动入口会使用返回的接口
 * 调用 ui_stm32_display_create()。
 *============================================================*/
uint8_t App_UI_DisplayBuildPort(
    ui_stm32_display_port_t *port
)
{
    /* 检查输出指针 */
    if (port == NULL)
    {
        return 0U;
    }

    /* 确认 LCD 和 DMA2D 已经初始化完成 */
    if ((BSP_LCD_IsReady() == 0U) ||
        (BSP_DMA2D_IsReady() == 0U))
    {
        return 0U;
    }

    /* 检查两块 LVGL 绘制缓冲没有重叠 */
    if ((APP_UI_DRAW_A_ADDR + APP_UI_DRAW_BUFFER_BYTES >
         APP_UI_DRAW_B_ADDR) ||
        (APP_UI_DRAW_B_ADDR + APP_UI_DRAW_BUFFER_BYTES >
         0xC0200000UL))
    {
        return 0U;
    }

    /* 设置真实 LCD 分辨率 */
    port->width = BSP_LCD_WIDTH;
    port->height = BSP_LCD_HEIGHT;

    /* 设置 LVGL 两块局部绘制缓冲的实际地址 */
    port->draw_buffer_a =
        (void *)(uintptr_t)APP_UI_DRAW_A_ADDR;

    port->draw_buffer_b =
        (void *)(uintptr_t)APP_UI_DRAW_B_ADDR;

    /* 告诉 LVGL 每块绘制缓冲的字节容量 */
    port->draw_buffer_bytes = APP_UI_DRAW_BUFFER_BYTES;

    /* 注册正式刷新回调 */
    port->start_flush = App_UI_StartFlush;

    /*
     * 当前 DMA2D 使用同步阻塞传输。
     * 不会返回 UI_STM32_FLUSH_ASYNC，
     * 因此无需异步完成等待回调。
     */
    port->wait_flush_complete = NULL;

    /* 当前不需要额外上下文 */
    port->context = NULL;

    /* 重置内部帧绘制状态 */
    s_frameActive = 0U;
    s_backBuffer = 0U;

    /* 显示接口构建成功 */
    return 1U;
}
