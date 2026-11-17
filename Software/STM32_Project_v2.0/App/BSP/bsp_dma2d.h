
#ifndef BSP_DMA2D_H
#define BSP_DMA2D_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================
 * 文件名称：bsp_dma2d.h
 * 功能：STM32H743 DMA2D（二??维图形加速器）驱动接口
 *
 * 设计目标：
 * 1. 为 LCD 提供 RGB565 硬件图形加速。
 * 2. 支持矩形颜色填充。
 * 3. 支持两块图像缓冲之间的矩形复制。
 * 4. 提供完整参数检查和错误状态。
 * 5. 后续可扩展 LVGL 非阻塞图形处理。
 *
 * 当前阶段约束：
 * 仅操作 SDRAM 前 2 MiB 非缓存显示区域。
 * 不依赖 FreeRTOS。
 * 仅由启动协调器或单一 UI Task 顺序调用。
 * 不允许在中断服务函数中调用阻塞接口。
 *============================================================*/

/* DMA2D（二维图形加速器）传输超时，单位毫秒 */
#define BSP_DMA2D_TIMEOUT_MS        (1000U)

/* RGB565（16 位颜色）每像素字节数 */
#define BSP_DMA2D_PIXEL_BYTES       (2U)

/*============================================================
 * 驱动返回状态
 *============================================================*/
typedef enum
{
    BSP_DMA2D_OK = 0,             /* 操作成功 */

    BSP_DMA2D_ERROR_PARAM,        /* 输入参数错误 */

    BSP_DMA2D_ERROR_NOT_READY,    /* 外设尚未初始化 */

    BSP_DMA2D_ERROR_BUSY,         /* 图形加速器正在使用 */

    BSP_DMA2D_ERROR_ADDRESS,      /* 内存地址超出允许区域 */

    BSP_DMA2D_ERROR_OVERLAP,      /* 源和目标区域发生重叠 */

    BSP_DMA2D_ERROR_CONFIG,       /* 外设配置失败 */

    BSP_DMA2D_ERROR_TRANSFER,     /* 硬件传输失败 */

    BSP_DMA2D_ERROR_TIMEOUT      /* 等待传输超时 */

} BSP_DMA2D_Status_t;

/*============================================================
 * 对外接口
 *============================================================*/

/* 检查并启用正式 DMA2D 驱动 */
BSP_DMA2D_Status_t BSP_DMA2D_Init(void);

/* 查询 DMA2D 驱动是否准备好 */
uint8_t BSP_DMA2D_IsReady(void);

/* 查询最近一次操作结果 */
BSP_DMA2D_Status_t BSP_DMA2D_GetLastStatus(void);

/*
 * RGB565 矩形填充。
 *
 * bufferAddress：图像缓冲起始地址
 * stridePixels ：整幅图像每行的像素数量
 * x、y         ：矩形左上角坐标
 * width        ：矩形宽度
 * height       ：矩形高度
 * color        ：RGB565 填充颜色
 */
BSP_DMA2D_Status_t BSP_DMA2D_FillRGB565(
    uint32_t bufferAddress,
    uint16_t stridePixels,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color
);

/*
 * RGB565 矩形复制。
 *
 * srcAddress：源图像缓冲起始地址
 * dstAddress：目标图像缓冲起始地址
 *
 * srcStride、dstStride：
 * 两幅图像分别占用的每行像素数量。
 *
 * srcX、srcY：源矩形位置
 * dstX、dstY：目标矩形位置
 *
 * width、height：需要复制的矩形大小
 *
 * 注意：
 * 源与目标的实际存储范围不得重叠。
 */
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
);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DMA2D_H */
