
#ifndef BSP_LCD_H
#define BSP_LCD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================
 * 文件：bsp_lcd.h
 * 功能：STM32H743 LCD（液晶显示屏）正式板级驱动接口
 *
 * 硬件平台：
 * MCU（微控制器）：STM32H743IIT6
 * LCD（液晶显示屏）：D300T9307V1
 * 显示控制器：ST7701S
 * 分辨率：480 × 854
 * 颜色格式：RGB565（16 位颜色格式）
 *
 * 通信方式：
 * 1. 三线 9-bit SPI（九位串行通信）配置寄存器。
 * 2. LTDC（液晶显示控制器）输出 RGB 图像。
 * 3. TIM2_CH2 PWM（脉宽调制）控制背光。
 *
 * 内存结构：
 * FMC（灵活存储器控制器）连接 32 MiB SDRAM。
 * 前 2 MiB 使用 MPU（内存保护单元）非缓存区域。
 * 内部保存两块完整显示帧缓冲。
 *
 * 注意：
 * 本模块不负责初始化 FMC、MPU、FreeRTOS。
 *============================================================*/

/* 屏幕水平像素数量 */
#define BSP_LCD_WIDTH                (480U)

/* 屏幕垂直像素数量 */
#define BSP_LCD_HEIGHT               (854U)

/* RGB565 每个像素占用 2 字节 */
#define BSP_LCD_PIXEL_BYTES          (2U)

/* 单帧完整图像大小：819840 字节 */
#define BSP_LCD_FRAME_BYTES          \
    (BSP_LCD_WIDTH * BSP_LCD_HEIGHT * BSP_LCD_PIXEL_BYTES)

/* 第一块帧缓冲地址 */
#define BSP_LCD_FB0_ADDR             (0xC0000000UL)

/* 第二块帧缓冲地址 */
#define BSP_LCD_FB1_ADDR             \
    (BSP_LCD_FB0_ADDR + BSP_LCD_FRAME_BYTES)

/* 帧缓冲数量 */
#define BSP_LCD_BUFFER_COUNT         (2U)

/*============================================================
 * RGB565 常用颜色定义
 *
 * R（红色）：5 bit
 * G（绿色）：6 bit
 * B（蓝色）：5 bit
 *
 * 主要用于背景清除与启动画面。
 *============================================================*/

#define BSP_LCD_COLOR_BLACK          (0x0000U)
#define BSP_LCD_COLOR_WHITE          (0xFFFFU)
#define BSP_LCD_COLOR_RED            (0xF800U)
#define BSP_LCD_COLOR_GREEN          (0x07E0U)
#define BSP_LCD_COLOR_BLUE           (0x001FU)

/*============================================================
 * 背光硬件安全授权
 *
 * 0：背光硬件尚未完成确认，禁止开启。
 * 1：已确认背光升压电路满足要求，允许开启。
 *
 * 重要：
 * 在确认 C83 电容耐压、背光升压电路和
 * LCD 排线连接前，必须保持 0。
 *
 * 本配置属于产品硬件安全条件，
 * 与屏幕初始化成功与否相互独立。
 *============================================================*/
#define BSP_LCD_BACKLIGHT_APPROVED    (1U)

/*============================================================
 * LCD 驱动执行状态
 *============================================================*/
typedef enum
{
    BSP_LCD_OK = 0,                /* 操作成功 */

    BSP_LCD_ERROR_PARAM,           /* 输入参数不正确 */

    BSP_LCD_ERROR_NOT_READY,       /* LCD 尚未完成初始化 */

    BSP_LCD_ERROR_SDRAM,           /* SDRAM 尚未初始化 */

    BSP_LCD_ERROR_MEMORY,          /* 帧缓冲内存读写失败 */

    BSP_LCD_ERROR_LTDC,            /* LTDC 操作失败 */

    BSP_LCD_ERROR_PWM,             /* 背光 PWM 配置失败 */

    BSP_LCD_ERROR_BACKLIGHT_LOCK,  /* 背光硬件未确认 */

    BSP_LCD_ERROR_BUSY,             /* 显示缓冲切换未完成 */

    BSP_LCD_ERROR_DMA2D             /* DMA2D 图形加速操作失败 */

} BSP_LCD_Status_t;

/*============================================================
 * 正式对外接口
 *============================================================*/

/*
 * 初始化完整 LCD 显示系统。
 *
 * initialColor：
 * 两块帧缓冲的初始 RGB565 颜色。
 *
 * 初始化过程中背光始终关闭。
 */
BSP_LCD_Status_t BSP_LCD_Init(uint16_t initialColor);

/* 查询屏幕驱动是否完成初始化 */
uint8_t BSP_LCD_IsReady(void);

/* 获取初始化或操作的最近一次错误 */
BSP_LCD_Status_t BSP_LCD_GetLastError(void);

/* 获取基本内存验证失败的物理地址 */
uint32_t BSP_LCD_GetMemoryErrorAddress(void);

/* 获取指定帧缓冲的起始地址 */
uint32_t BSP_LCD_GetFrameBuffer(uint8_t index);

/* 将一块帧缓冲填充为 RGB565 颜色 */
BSP_LCD_Status_t BSP_LCD_FillBuffer(
    uint8_t index,
    uint16_t color
);

/*
 * 请求在垂直消隐期间切换显示缓冲。
 *
 * 调用者必须保证目标缓冲已经绘制完成。
 */
BSP_LCD_Status_t BSP_LCD_Present(uint8_t index);

/* 查询是否仍有显示缓冲切换正在等待完成 */
uint8_t BSP_LCD_IsPresentPending(void);

/* 获取当前实际完成切换的显示缓冲编号 */
uint8_t BSP_LCD_GetFrontBuffer(void);

/*
 * 设置背光亮度。
 *
 * percent：0～100。
 * 0：关闭背光。
 * 100：最大占空比。
 *
 * 此百分比不等于人眼感知亮度百分比。
 */
BSP_LCD_Status_t BSP_LCD_SetBacklight(
    uint8_t percent
);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LCD_H */
