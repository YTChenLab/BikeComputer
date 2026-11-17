
#ifndef BSP_QSPI_H
#define BSP_QSPI_H

#include <stdint.h>                 /* 固定宽度整数类型 */

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================
 * W25Q128JV 外部 Flash 驱动接口
 *
 * 硬件：
 * STM32H743IIT6 + W25Q128
 *
 * 容量：16 MiB
 * 通信：QUADSPI Bank1
 * 时钟：25 MHz
 *
 * 注意：
 * 所有 address 参数都是 Flash 内部偏移。
 * 不是 STM32 的 0x90000000 映射地址。
 *============================================================*/

/* Flash 总容量：16 MiB */
#define BSP_QSPI_FLASH_SIZE   (16UL * 1024UL * 1024UL)

/* 每个可编程页：256 字节 */
#define BSP_QSPI_PAGE_SIZE    (256UL)

/* 最小擦除扇区：4096 字节 */
#define BSP_QSPI_SECTOR_SIZE  (4096UL)

/* STM32 QUADSPI 内存映射地址 */
#define BSP_QSPI_MAPPED_BASE  (0x90000000UL)

/*============================================================
 * 驱动状态
 *
 * 保留之前已经使用的返回值编号，
 * 新增错误类型追加在枚举末尾。
 *============================================================*/
typedef enum
{
    BSP_QSPI_OK = 0,            /* 操作成功 */
    BSP_QSPI_ERROR_PARAM,       /* 参数错误 */
    BSP_QSPI_ERROR_STATE,       /* 初始化状态或模式错误 */
    BSP_QSPI_ERROR_HAL,         /* HAL 外设操作错误 */
    BSP_QSPI_ERROR_TIMEOUT,     /* 操作超时 */
    BSP_QSPI_ERROR_ID,          /* JEDEC ID 不匹配 */
    BSP_QSPI_ERROR_RANGE,       /* 地址越界 */
    BSP_QSPI_ERROR_QUAD,        /* QE 四线功能启用失败 */
    BSP_QSPI_ERROR_VERIFY,      /* 写后校验失败 */
    BSP_QSPI_ERROR_PROTECTED,   /* 未获得分区写入授权 */
    BSP_QSPI_ERROR_NOT_ERASED,  /* 目标区域尚未擦除 */
    BSP_QSPI_ERROR_CONTEXT,     /* 非法任务或中断上下文 */
    BSP_QSPI_ERROR_BUSY         /* 互斥锁或外设忙 */
} BSP_QSPI_Status_t;

/* JEDEC ID：三个字节 */
typedef struct
{
    uint8_t manufacturer;      /* 制造商编号 */
    uint8_t memoryType;        /* 存储器类型 */
    uint8_t capacity;          /* 容量编号 */
} BSP_QSPI_JedecID_t;

/*============================================================
 * 写入分区类型
 *
 * 与 qspi_partition.h 的固定分区保持一致。
 *
 * NONE 为默认状态：禁止擦除和写入。
 *============================================================*/
typedef enum
{
    BSP_QSPI_PART_NONE = 0,     /* 未授权任何分区 */
    BSP_QSPI_PART_METADATA,     /* OTA 元数据 */
    BSP_QSPI_PART_STAGING,      /* 新固件暂存 */
    BSP_QSPI_PART_BACKUP,       /* 旧固件备份 */
    BSP_QSPI_PART_RESOURCE_A,   /* UI 资源 A */
    BSP_QSPI_PART_RESOURCE_B,   /* UI 资源 B */
    BSP_QSPI_PART_RESERVED      /* 预留区 */
} BSP_QSPI_Partition_t;

/* 初始化芯片，检查 JEDEC ID 并确保 QE 启用 */
BSP_QSPI_Status_t BSP_QSPI_Init(BSP_QSPI_JedecID_t *id);

/* 外设异常恢复；调用前须保证没有活动的映射读取 */
BSP_QSPI_Status_t BSP_QSPI_Recover(void);

/* 读取 JEDEC ID */
BSP_QSPI_Status_t BSP_QSPI_ReadJEDEC(BSP_QSPI_JedecID_t *id);

/* 读取状态寄存器 1 */
BSP_QSPI_Status_t BSP_QSPI_ReadStatus1(uint8_t *status);

/* 读取状态寄存器 2 */
BSP_QSPI_Status_t BSP_QSPI_ReadStatus2(uint8_t *status);

/* 使用 0x03 单线指令读取数据 */
BSP_QSPI_Status_t BSP_QSPI_Read(
    uint32_t address,
    uint8_t *buffer,
    uint32_t size
);

/* 使用 0x6B 四线快速读取数据 */
BSP_QSPI_Status_t BSP_QSPI_ReadQuad(
    uint32_t address,
    uint8_t *buffer,
    uint32_t size
);

/* 授权指定分区可写 */
BSP_QSPI_Status_t BSP_QSPI_AllowWritePartition(
    BSP_QSPI_Partition_t partition
);

/* 撤销写入授权，恢复默认只读状态 */
BSP_QSPI_Status_t BSP_QSPI_LockWrite(void);

/* 自动分页写入，要求目标字节已擦除为 0xFF */
BSP_QSPI_Status_t BSP_QSPI_Program(
    uint32_t address,
    const uint8_t *buffer,
    uint32_t size
);

/* 擦除一个 4 KiB 对齐扇区 */
BSP_QSPI_Status_t BSP_QSPI_EraseSector4K(
    uint32_t address
);

/* 进入只读 Memory-Mapped 模式 */
BSP_QSPI_Status_t BSP_QSPI_EnterMemoryMapped(void);

/* 退出 Memory-Mapped 模式 */
BSP_QSPI_Status_t BSP_QSPI_ExitMemoryMapped(void);

/* 取得 Flash 的内存映射指针 */
/* Q4-2：固定映射至 MCU 复位；不提供解锁，升级需停止 UI 后重启。 */
/* Q4-3：显式触发。result 固定 13 个 uint32_t，见交付协议。
 * 没有已证明可擦写的 scratch 分区，flags bit0 请求破坏测试时必须拒绝。 */
void BSP_QSPI_Diagnose(uint32_t flags, uint32_t result[13]);
BSP_QSPI_Status_t BSP_QSPI_PinMemoryMapped(void);
uint8_t BSP_QSPI_IsMemoryMappedPinned(void);

const uint8_t *BSP_QSPI_GetMappedPointer(
    uint32_t address,
    uint32_t size
);

#ifdef __cplusplus
}
#endif

#endif /* BSP_QSPI_H */
