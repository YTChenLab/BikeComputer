
#ifndef BSP_SDRAM_H
#define BSP_SDRAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================
 * 文件名称：bsp_sdram.h
 * 功能：外部 SDRAM（同步动态随机存取存储器）驱动接口
 *
 * 硬件平台：
 * MCU（微控制器）：STM32H743IIT6
 * SDRAM 芯片：W9825G6KH-6
 * FMC（灵活存储器控制器）：SDRAM Bank1
 * 数据总线宽度：16 bit
 * 总容量：32 MiB
 *
 * 模块职责：
 * 1. 定义外部 SDRAM 地址空间。
 * 2. 提供 SDRAM 初始化和状态查询接口。
 * 3. 统一声明显示帧缓冲的预留内存。
 *
 * 注意：
 * MPU（内存保护单元）由 main.c 统一配置。
 * BSP 驱动不得再次修改 MPU。
 *============================================================*/

/* SDRAM Bank1 的起始物理地址 */
#define BSP_SDRAM_BASE_ADDR       (0xC0000000UL)

/* 外部 SDRAM 总容量：32 MiB */
#define BSP_SDRAM_SIZE_BYTES      (32UL * 1024UL * 1024UL)

/* SDRAM 最后一个有效字节的物理地址 */
#define BSP_SDRAM_END_ADDR        \
    (BSP_SDRAM_BASE_ADDR + BSP_SDRAM_SIZE_BYTES - 1UL)

/*============================================================
 * LTDC（液晶显示控制器）专用帧缓冲区域
 *
 * 前 2 MiB 为非缓存内存。
 * 由 MPU Region 2（内存保护区域 2）进行配置。
 *
 * 主要用途：
 * 存放两帧 480×854 RGB565 显示画面。
 *
 * 普通内存分配器不能使用这块区域。
 *============================================================*/

/* 帧缓冲专用区域起始地址 */
#define BSP_SDRAM_FB_REGION_BASE  (BSP_SDRAM_BASE_ADDR)

/* 帧缓冲专用区域容量：2 MiB */
#define BSP_SDRAM_FB_REGION_SIZE  (2UL * 1024UL * 1024UL)

/*============================================================
 * 普通可缓存 SDRAM 区域
 *
 * 起始地址：0xC0200000
 * 大小：30 MiB
 *
 * 主要用途：
 * LVGL（轻量级图形库）工作内存、
 * 地图数据、导航缓存及其他应用数据。
 *============================================================*/

/* 普通可缓存区域起始地址 */
#define BSP_SDRAM_CACHE_BASE      \
    (BSP_SDRAM_BASE_ADDR + BSP_SDRAM_FB_REGION_SIZE)

/* 普通可缓存区域容量 */
#define BSP_SDRAM_CACHE_SIZE      \
    (BSP_SDRAM_SIZE_BYTES - BSP_SDRAM_FB_REGION_SIZE)

/*============================================================
 * SDRAM 驱动状态
 *============================================================*/
typedef enum
{
    BSP_SDRAM_OK = 0,            /* 初始化流程成功 */

    BSP_SDRAM_ERROR_NOT_READY,   /* 尚未执行初始化 */

    BSP_SDRAM_ERROR_COMMAND,     /* 初始化指令执行失败 */

    BSP_SDRAM_ERROR_REFRESH     /* 自动刷新配置失败 */

} BSP_SDRAM_Status_t;

/*============================================================
 * 对外函数接口
 *============================================================*/

/* 初始化 FMC 和外部 SDRAM */
BSP_SDRAM_Status_t BSP_SDRAM_Init(void);

/* 获取最近一次初始化结果 */
BSP_SDRAM_Status_t BSP_SDRAM_GetStatus(void);

/* 判断初始化流程是否成功 */
uint8_t BSP_SDRAM_IsReady(void);

/* 获取 SDRAM 总容量，单位：字节 */
uint32_t BSP_SDRAM_GetSize(void);


/*============================================================
 * BSP_SDRAM_ClearUILargeRam
 *
 * 功能：清零 UI 专用外部 SDRAM 数据段。
 *
 * 必须在 BSP_SDRAM_Init() 成功后调用。
 *
 * 返回：
 * 1U：清零成功。
 * 0U：SDRAM 未就绪或内存范围不正确。
 *============================================================*/
uint8_t BSP_SDRAM_ClearUILargeRam(void);


#ifdef __cplusplus
}
#endif

#endif /* BSP_SDRAM_H */
