
#ifndef BIKE_RESOURCE_STORAGE_H_
#define BIKE_RESOURCE_STORAGE_H_

#include <stdint.h>               /* uint8_t、uint32_t 等整数类型 */
#include <stdbool.h>              /* bool、true、false 类型 */
#include "resource_package.h"    /* 资源槽类型、资源包状态码 */
#include "resource_manager.h"    /* resource_manager_t、完整资源校验接口 */

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================
 * QSPI Resource A/B 资源存储管理器
 *
 * 本模块位于 App/Service。
 *
 * 职责：
 * 1. 管理 Resource A/B 当前有效槽。
 * 2. 接收新的完整资源包。
 * 3. 分包写入 W25Q128。
 * 4. 调用 Resource Manager 校验完整性。
 * 5. 成功后原子提交活动槽元数据。
 *============================================================*/

/* 单次传输最大长度：256 字节 */
#define RESOURCE_STORAGE_CHUNK_SIZE (256UL)

/* 资源存储服务统一返回码 */
typedef enum
{
    RESOURCE_STORAGE_OK = 0,        /* 操作成功 */

    RESOURCE_STORAGE_EMPTY,         /* 当前还没有有效资源包 */

    RESOURCE_STORAGE_ERR_ARGUMENT,  /* 输入参数错误 */

    RESOURCE_STORAGE_ERR_STATE,     /* 当前状态不允许操作 */

    RESOURCE_STORAGE_ERR_RANGE,     /* 资源地址或长度越界 */

    RESOURCE_STORAGE_ERR_IO,        /* QSPI 底层读写失败 */

    RESOURCE_STORAGE_ERR_VERIFY,    /* 资源包完整性验证失败 */

    RESOURCE_STORAGE_ERR_CONFLICT   /* 重复分包内容不一致 */

} resource_storage_status_t;

/* 初始化 Resource A/B 存储管理器 */
resource_storage_status_t ResourceStorage_Init(
    uint32_t firmware_version
);

/* 获取当前已提交的有效资源槽 */
resource_storage_status_t ResourceStorage_GetActiveSlot(
    resource_package_slot_t *slot
);

/* 开始资源更新，自动选择非活动槽 */
resource_storage_status_t ResourceStorage_Begin(
    uint32_t package_size,
    resource_package_slot_t *target
);

/* 接收并写入资源包的一个数据块 */
resource_storage_status_t ResourceStorage_Write(
    uint32_t offset,
    const uint8_t *data,
    uint32_t length
);

/* 校验完整资源包并正式提交活动槽 */
resource_storage_status_t ResourceStorage_Commit(void);

/* 中止当前资源更新 */
resource_storage_status_t ResourceStorage_Abort(void);

/* 获取当前更新已接收的字节数 */
uint32_t ResourceStorage_GetReceived(void);

/* 按照 Resource ID 读取当前有效资源 */
resource_package_status_t ResourceStorage_ReadById(
    uint32_t resource_id,
    uint32_t offset,
    uint8_t *destination,
    uint32_t length
);

/* Q4-3：已提交活动包的只读快照；无活动包时返回 false，slot=UINT32_MAX。 */
typedef struct {
    uint32_t active_slot, package_version, package_size;
    uint8_t payload_sha256[32];
} resource_storage_info_t;
bool ResourceStorage_GetInfo(resource_storage_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* BIKE_RESOURCE_STORAGE_H_ */
