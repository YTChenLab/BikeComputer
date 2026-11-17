#ifndef QSPI_PARTITION_H
#define QSPI_PARTITION_H

/* ============================================================
 * 文件名称：qspi_partition.h
 *
 * 所属项目：BikeComputer STM32H743 公路车码表
 *
 * 功能说明：
 * 1. 统一定义外部 W25Q128 QSPI Flash 分区布局。
 * 2. 为 LVGL Resource Manager 提供资源分区地址。
 * 3. 为 STM32 OTA 提供固件暂存与备份分区地址。
 * 4. 防止各模块使用不一致的地址导致数据覆盖。
 *
 * 设计原则：
 * - 分区地址全部采用 QSPI 芯片内部偏移地址。
 * - 不直接使用 STM32 Memory-Mapped 绝对地址。
 * - 所有分区按照 Flash 擦除粒度对齐。
 * - 当前仅定义布局，不执行任何擦除或写入。
 *
 * 版本：V1.0
 * ============================================================ */

#include <stdint.h>

/* ============================================================
 * 1. QSPI Flash 基本参数
 * ============================================================ */

/* W25Q128 总容量：16 MiB */
#define QSPI_FLASH_TOTAL_SIZE       (16UL * 1024UL * 1024UL)

/* 4 KiB 擦除扇区 */
#define QSPI_FLASH_SECTOR_SIZE      (4UL * 1024UL)

/* 256 Byte 页面编程粒度 */
#define QSPI_FLASH_PAGE_SIZE        256UL


/* ============================================================
 * 2. OTA 元数据分区
 * ============================================================ */

/* 保存资源分区状态、版本、升级状态等元数据 */
#define QSPI_METADATA_OFFSET        0x00000000UL

#define QSPI_METADATA_SIZE          0x00020000UL


/* ============================================================
 * 3. STM32 新固件暂存分区
 * ============================================================ */

/* ESP32 下载后，经通信协议传入 STM32 的新固件 */
#define QSPI_STAGING_OFFSET         0x00020000UL

#define QSPI_STAGING_SIZE           0x00200000UL


/* ============================================================
 * 4. STM32 旧固件备份分区
 * ============================================================ */

/* OTA 安装前保存已验证的旧 Application 镜像 */
#define QSPI_BACKUP_OFFSET          0x00220000UL

#define QSPI_BACKUP_SIZE            0x00200000UL


/* ============================================================
 * 5. LVGL Resource A 分区
 * ============================================================ */

/* 字体、背景、开机动画等资源的 A 槽 */
#define QSPI_RESOURCE_A_OFFSET      0x00420000UL

#define QSPI_RESOURCE_A_SIZE        0x00500000UL


/* ============================================================
 * 6. LVGL Resource B 分区
 * ============================================================ */

/* 资源 OTA 更新及回滚使用的 B 槽 */
#define QSPI_RESOURCE_B_OFFSET      0x00920000UL

#define QSPI_RESOURCE_B_SIZE        0x00500000UL


/* ============================================================
 * 7. 保留分区
 * ============================================================ */

/* 为未来的功能和存储扩展预留 */
#define QSPI_RESERVED_OFFSET        0x00E20000UL

#define QSPI_RESERVED_SIZE          0x001E0000UL


/* ============================================================
 * 8. 分区边界检查
 *
 * 在编译阶段检查分区是否连续、是否超出芯片容量。
 * 当未来修改容量时，可提前发现分区重叠问题。
 * ============================================================ */

#if (QSPI_METADATA_OFFSET + QSPI_METADATA_SIZE) != QSPI_STAGING_OFFSET
#error "QSPI partition error: Metadata and Staging"
#endif

#if (QSPI_STAGING_OFFSET + QSPI_STAGING_SIZE) != QSPI_BACKUP_OFFSET
#error "QSPI partition error: Staging and Backup"
#endif

#if (QSPI_BACKUP_OFFSET + QSPI_BACKUP_SIZE) != QSPI_RESOURCE_A_OFFSET
#error "QSPI partition error: Backup and Resource A"
#endif

#if (QSPI_RESOURCE_A_OFFSET + QSPI_RESOURCE_A_SIZE) != QSPI_RESOURCE_B_OFFSET
#error "QSPI partition error: Resource A and Resource B"
#endif

#if (QSPI_RESOURCE_B_OFFSET + QSPI_RESOURCE_B_SIZE) != QSPI_RESERVED_OFFSET
#error "QSPI partition error: Resource B and Reserved"
#endif

#if (QSPI_RESERVED_OFFSET + QSPI_RESERVED_SIZE) != QSPI_FLASH_TOTAL_SIZE
#error "QSPI partition error: Total size mismatch"
#endif

#if (QSPI_METADATA_OFFSET % QSPI_FLASH_SECTOR_SIZE) != 0 || \
    (QSPI_STAGING_OFFSET % QSPI_FLASH_SECTOR_SIZE) != 0 || \
    (QSPI_BACKUP_OFFSET % QSPI_FLASH_SECTOR_SIZE) != 0 || \
    (QSPI_RESOURCE_A_OFFSET % QSPI_FLASH_SECTOR_SIZE) != 0 || \
    (QSPI_RESOURCE_B_OFFSET % QSPI_FLASH_SECTOR_SIZE) != 0 || \
    (QSPI_RESERVED_OFFSET % QSPI_FLASH_SECTOR_SIZE) != 0
#error "QSPI partition error: Unaligned partition offset"
#endif

#endif /* QSPI_PARTITION_H */