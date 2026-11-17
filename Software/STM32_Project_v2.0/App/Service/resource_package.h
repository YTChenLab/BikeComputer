
#ifndef RESOURCE_PACKAGE_H
#define RESOURCE_PACKAGE_H

/* ============================================================
 * 文件名称：resource_package.h
 * 项目名称：BikeComputer STM32H743 公路车码表
 * 模块名称：QSPI 资源包解析与校验
 *
 * 功能：
 * 1. 从 QSPI Resource A/B 读取资源包头部。
 * 2. 检查格式版本、固件兼容性及索引边界。
 * 3. 根据 Resource ID 查找资源。
 * 4. 分块读取字体、图片和动画数据。
 * 5. 对单项资源进行 CRC32 完整性检查。
 *
 * 约束：
 * - 不直接依赖 HAL、FreeRTOS、LVGL。
 * - 不执行 Flash 擦除或写入。
 * - 不进行动态内存分配。
 * - 所有地址采用 QSPI 芯片内部偏移。
 * - 本阶段尚不执行 Payload SHA-256 或签名验收。
 * ============================================================ */

#include <stdbool.h>                                      /* bool 类型 */
#include <stdint.h>                                       /* 固定宽度整数类型 */
#include "resource_package_format.h"                     /* 资源包二进制格式 */


/* ============================================================
 * 一、资源包操作状态码
 *
 * enum 仅用于程序运行时的函数返回值，
 * 不会直接存储到 QSPI 资源包中。
 * ============================================================ */

typedef enum
{
    RESOURCE_PACKAGE_OK = 0,                              /* 操作成功 */

    RESOURCE_PACKAGE_ERR_ARGUMENT,                        /* 函数参数错误 */

    RESOURCE_PACKAGE_ERR_IO,                              /* QSPI 底层读取失败 */

    RESOURCE_PACKAGE_ERR_FORMAT,                          /* 资源包格式错误 */

    RESOURCE_PACKAGE_ERR_COMPATIBILITY,                   /* 与当前固件不兼容 */

    RESOURCE_PACKAGE_ERR_INDEX,                           /* 索引错误或数据越界 */

    RESOURCE_PACKAGE_ERR_NOT_FOUND,                       /* 没有找到资源 */

    RESOURCE_PACKAGE_ERR_RANGE,                           /* 读取范围越界 */

    RESOURCE_PACKAGE_ERR_CRC                              /* CRC32 校验失败 */

} resource_package_status_t;


/* ============================================================
 * 二、资源槽选择
 * ============================================================ */

typedef enum
{
    RESOURCE_PACKAGE_SLOT_A = 0,                          /* Resource A */

    RESOURCE_PACKAGE_SLOT_B = 1                           /* Resource B */

} resource_package_slot_t;


/* ============================================================
 * 三、底层读取回调类型
 *
 * 未来由 W25Q128 QSPI 驱动提供实际实现。
 *
 * flash_offset：芯片内部偏移地址。
 * destination：  目标 RAM 缓冲区。
 * length：       本次读取字节数。
 * context：      驱动上下文指针。
 *
 * 返回 true 表示读取成功。
 * 返回 false 表示读取失败。
 * ============================================================ */

typedef bool (*resource_package_read_fn_t)(
    uint32_t flash_offset,
    uint8_t *destination,
    uint32_t length,
    void *context
);


/* ============================================================
 * 四、资源包运行时句柄
 *
 * 由上层模块创建并持有。
 *
 * 此结构体保存在 RAM 中，不直接写入 QSPI。
 * ============================================================ */

typedef struct
{
    resource_package_read_fn_t reader;                    /* QSPI 读取接口 */

    void *reader_context;                                 /* 读取驱动上下文 */

    resource_package_header_t header;                     /* 解析后的资源包头部 */

    uint32_t slot_offset;                                 /* Resource A/B 起始偏移 */

    uint32_t slot_size;                                   /* 当前资源槽总容量 */

    bool parsed;                                          /* 仅表示结构解析成功 */

} resource_package_t;


/* ============================================================
 * 五、资源包打开与基础校验
 *
 * 检查：
 * 1. Header 格式。
 * 2. Header CRC32。
 * 3. 固件兼容版本。
 * 4. Index 排序及边界。
 * 5. 资源数据是否重叠。
 *
 * 不检查完整 Payload SHA-256。
 * ============================================================ */

resource_package_status_t ResourcePackage_Open(
    resource_package_t *package,
    resource_package_slot_t slot,
    uint32_t firmware_version,
    resource_package_read_fn_t reader,
    void *reader_context
);


/* ============================================================
 * 六、通过 Resource ID 查找资源
 *
 * 采用二分查找。
 * 要求资源索引按照 resource_id 递增排列。
 * ============================================================ */

resource_package_status_t ResourcePackage_FindById(
    const resource_package_t *package,
    uint32_t resource_id,
    resource_package_index_t *entry
);


/* ============================================================
 * 七、读取资源数据
 *
 * 可以只读取某项资源的一部分。
 * 因此无须一次将大型字体全部加载到 RAM。
 * ============================================================ */

resource_package_status_t ResourcePackage_Read(
    const resource_package_t *package,
    const resource_package_index_t *entry,
    uint32_t offset_in_resource,
    uint8_t *destination,
    uint32_t length
);


/* ============================================================
 * 八、验证单项资源 CRC32
 *
 * 使用调用者提供的 scratch 缓冲区分块读取。
 * 无须申请与资源同等大小的 RAM。
 * ============================================================ */

resource_package_status_t ResourcePackage_VerifyResourceCRC32(
    const resource_package_t *package,
    const resource_package_index_t *entry,
    uint8_t *scratch,
    uint32_t scratch_size
);

/* Q4-3：共享原有 CRC-32/ISO-HDLC 累加器。初值/最终异或均为 0xFFFFFFFF。 */
uint32_t ResourcePackage_Crc32Update(uint32_t crc, const uint8_t *data, uint32_t length);
#endif /* RESOURCE_PACKAGE_H */
