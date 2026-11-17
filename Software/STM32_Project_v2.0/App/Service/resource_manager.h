
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

/* ============================================================
 * 文件名称：resource_manager.h
 * 项目名称：BikeComputer STM32H743 公路车码表
 *
 * 模块名称：LVGL QSPI Resource Manager
 *
 * 功能：
 * 1. 统一管理指定的 Resource A/B 资源槽。
 * 2. 检查资源包基本结构和固件兼容性。
 * 3. 对整个 Payload 执行 SHA-256 完整性检查。
 * 4. 对每项资源执行 CRC32 检查。
 * 5. 为 LVGL 提供资源查找和分块读取。
 *
 * 设计约束：
 * - 不直接依赖 HAL、FreeRTOS 或 LVGL。
 * - 不进行动态内存分配。
 * - 不操作 Flash 擦除和编程。
 * - 不自动切换 A/B 活动分区。
 * - 启动验证成功后才允许读取资源。
 *
 * 注意：
 * SHA-256 不等于数字签名。
 * 本模块不承担 OTA 升级包真实性认证。
 * ============================================================ */

#include <stdint.h>                       /* 固定宽度整数类型 */
#include <stdbool.h>                      /* bool 类型 */
#include "resource_package.h"            /* 已完成的资源包解析接口 */


/* ============================================================
 * 一、Resource Manager 状态码
 * ============================================================ */

typedef enum
{
    RESOURCE_MANAGER_OK = 0,              /* 初始化与完整性验证成功 */

    RESOURCE_MANAGER_ERR_ARGUMENT,        /* 调用参数错误 */

    RESOURCE_MANAGER_ERR_PACKAGE,         /* 包头、索引、版本等解析失败 */

    RESOURCE_MANAGER_ERR_IO,              /* 底层 QSPI 读取失败 */

    RESOURCE_MANAGER_ERR_SHA256,          /* 整包 Payload SHA-256 不匹配 */

    RESOURCE_MANAGER_ERR_RESOURCE_CRC     /* 单项资源 CRC32 不匹配 */

} resource_manager_status_t;


/* ============================================================
 * 二、Resource Manager 运行时对象
 *
 * 由 STM32 上层模块长期持有。
 *
 * 内部保存解析器对象和验证结果。
 * 不允许将此对象直接写入 QSPI Flash。
 * ============================================================ */

typedef struct
{
    resource_package_t package;           /* 已有 Resource Package 解析器 */

    resource_package_status_t last_package_status;
    /* 保存最后一次解析器返回的错误，方便调试 */

    bool ready;
    /* true 表示已经完成初始化和完整性验证 */

} resource_manager_t;


/* ============================================================
 * 三、初始化并验证指定资源槽
 *
 * 参数：
 * manager          Resource Manager 对象
 * slot             RESOURCE_PACKAGE_SLOT_A 或 B
 * firmware_version 当前 STM32 Application 版本
 * reader           QSPI 只读回调函数
 * reader_context   驱动上下文
 * scratch          临时工作缓冲区
 * scratch_size     工作缓冲区大小，至少 64 字节
 *
 * 推荐提供 1024 或 4096 字节 scratch。
 *
 * 执行过程：
 * 1. 解析 Header 和 Index。
 * 2. 检查资源包版本兼容性。
 * 3. 分块计算完整 Payload SHA-256。
 * 4. 检查全部资源 CRC32。
 * 5. 全部成功后将 ready 设置为 true。
 *
 * 注意：
 * 本函数会读取整个资源包，适合在系统初始化阶段运行。
 * 不应在 LVGL 绘制回调中调用。
 * ============================================================ */

resource_manager_status_t ResourceManager_Open(
    resource_manager_t *manager,
    resource_package_slot_t slot,
    uint32_t firmware_version,
    resource_package_read_fn_t reader,
    void *reader_context,
    uint8_t *scratch,
    uint32_t scratch_size
);


/* ============================================================
 * 四、查询管理器是否就绪
 * ============================================================ */

bool ResourceManager_IsReady(
    const resource_manager_t *manager
);


/* ============================================================
 * 五、按照 Resource ID 获取索引
 * ============================================================ */

resource_package_status_t ResourceManager_Find(
    const resource_manager_t *manager,
    uint32_t resource_id,
    resource_package_index_t *entry
);


/* ============================================================
 * 六、根据已经查询到的资源索引读取数据
 *
 * entry 应当来自当前管理器的 ResourceManager_Find。
 *
 * offset 是资源内部偏移，不是 QSPI 绝对地址。
 * ============================================================ */

resource_package_status_t ResourceManager_Read(
    const resource_manager_t *manager,
    const resource_package_index_t *entry,
    uint32_t offset,
    uint8_t *destination,
    uint32_t length
);


/* ============================================================
 * 七、通过 Resource ID 直接读取数据
 *
 * 内部完成索引查询和实际读取。
 * 适合普通图片、小型资源或偶发读取。
 * ============================================================ */

resource_package_status_t ResourceManager_ReadById(
    const resource_manager_t *manager,
    uint32_t resource_id,
    uint32_t offset,
    uint8_t *destination,
    uint32_t length
);

#endif /* RESOURCE_MANAGER_H */
