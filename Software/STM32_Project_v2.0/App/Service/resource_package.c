
#include "resource_package.h"                           /* 当前模块公共接口 */
#include <stddef.h>                                      /* offsetof */
#include <string.h>                                      /* memcpy / memset */


/* ============================================================
 * 一、CRC32 增量计算
 *
 * 算法：CRC-32/ISO-HDLC
 * 反射多项式：0xEDB88320
 *
 * 支持多次调用，适合分块校验大型资源。
 * ============================================================ */

uint32_t ResourcePackage_Crc32Update(
    uint32_t crc,
    const uint8_t *buffer,
    uint32_t length)
{
    for (uint32_t i = 0; i < length; ++i)                 /* 逐字节处理输入数据 */
    {
        crc ^= (uint32_t)buffer[i];                       /* 当前字节参与 CRC 计算 */

        for (uint32_t bit = 0; bit < 8U; ++bit)           /* 每字节处理八位 */
        {
            if ((crc & 1U) != 0U)                         /* 最低位为 1 */
            {
                crc = (crc >> 1U) ^ 0xEDB88320UL;        /* 右移并异或多项式 */
            }
            else                                        /* 最低位为 0 */
            {
                crc >>= 1U;                              /* 直接右移 */
            }
        }
    }

    return crc;                                          /* 返回可继续累积的 CRC 状态 */
}


/* ============================================================
 * 二、统一底层 Flash 读取
 *
 * 本函数不直接调用 HAL。
 * 实际数据访问由 reader 回调完成。
 * ============================================================ */

static resource_package_status_t rp_read_flash(
    const resource_package_t *package,
    uint32_t flash_offset,
    uint8_t *destination,
    uint32_t length)
{
    if (package == NULL ||
        package->reader == NULL ||
        destination == NULL ||
        length == 0U)
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;           /* 参数检查失败 */
    }

    if (flash_offset >= QSPI_FLASH_TOTAL_SIZE ||
        length > QSPI_FLASH_TOTAL_SIZE - flash_offset)
    {
        return RESOURCE_PACKAGE_ERR_RANGE;              /* 防止跨越 Flash 末尾 */
    }

    if (!package->reader(
            flash_offset,
            destination,
            length,
            package->reader_context))
    {
        return RESOURCE_PACKAGE_ERR_IO;                 /* 底层 QSPI 读取失败 */
    }

    return RESOURCE_PACKAGE_OK;                         /* 读取成功 */
}


/* ============================================================
 * 三、读取指定资源索引条目
 *
 * Index 每条固定为 48 字节。
 * ============================================================ */

static resource_package_status_t rp_read_index(
    const resource_package_t *package,
    uint32_t index,
    resource_package_index_t *entry)
{
    if (package == NULL ||
        entry == NULL ||
        index >= package->header.index_count)
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;           /* 索引位置非法 */
    }

    uint32_t local_offset =
        package->header.index_offset +
        index * RESOURCE_PACKAGE_INDEX_SIZE;            /* 计算索引在包内的偏移 */

    return rp_read_flash(
        package,
        package->slot_offset + local_offset,             /* 转换为 QSPI 芯片偏移 */
        (uint8_t *)entry,                                /* 索引输出缓冲区 */
        RESOURCE_PACKAGE_INDEX_SIZE                      /* 固定读取 48 字节 */
    );
}


/* ============================================================
 * 四、单项资源索引有效性检查
 * ============================================================ */

static bool rp_entry_is_valid(
    const resource_package_t *package,
    const resource_package_index_t *entry)
{
    if (entry->resource_id == 0U ||                      /* 资源 ID 不能为 0 */
        entry->resource_type < RESOURCE_TYPE_FONT_BITMAP ||
        entry->resource_type > RESOURCE_TYPE_BINARY ||
        entry->encoding > RESOURCE_ENCODING_JPEG ||
        entry->flags != 0U ||
        entry->reserved != 0U ||
        entry->stored_size == 0U ||
        entry->original_size == 0U)
    {
        return false;                                    /* 索引基本字段非法 */
    }

    if ((entry->data_offset % RESOURCE_PACKAGE_ALIGNMENT) != 0U ||
        entry->data_offset < package->header.data_offset ||
        entry->data_offset > package->header.package_size)
    {
        return false;                                    /* 数据地址或对齐不正确 */
    }

    if (entry->stored_size >
        package->header.package_size - entry->data_offset)
    {
        return false;                                    /* 资源数据超出包边界 */
    }

    return true;                                         /* 索引结构有效 */
}


/* ============================================================
 * 五、资源包 Header 结构检查
 *
 * 注意：
 * 本函数仅校验字段约束。
 * Header CRC32 在 ResourcePackage_Open 中检查。
 * ============================================================ */

static resource_package_status_t rp_validate_header(
    const resource_package_header_t *header,
    uint32_t slot_size,
    uint32_t firmware_version)
{
    if (header->magic != RESOURCE_PACKAGE_MAGIC ||
        header->format_version != RESOURCE_PACKAGE_FORMAT_VERSION ||
        header->header_size != RESOURCE_PACKAGE_HEADER_SIZE ||
        header->index_entry_size != RESOURCE_PACKAGE_INDEX_SIZE ||
        header->flags != RESOURCE_PACKAGE_FLAGS_NONE ||
        header->package_version == 0U)
    {
        return RESOURCE_PACKAGE_ERR_FORMAT;             /* 文件格式不符合 V1 定义 */
    }

    for (uint32_t i = 0; i < 10U; ++i)                   /* 检查 Header 保留字段 */
    {
        if (header->reserved[i] != 0U)
        {
            return RESOURCE_PACKAGE_ERR_FORMAT;         /* V1 保留字段必须为 0 */
        }
    }

    if (header->ui_abi_version != RESOURCE_PACKAGE_UI_ABI_VERSION ||
        header->min_firmware_version > header->max_firmware_version ||
        firmware_version < header->min_firmware_version ||
        firmware_version > header->max_firmware_version)
    {
        return RESOURCE_PACKAGE_ERR_COMPATIBILITY;      /* 当前 Application 不兼容 */
    }

    if (header->package_size > slot_size ||             /* 包不能超过资源槽 */
        header->package_size < RESOURCE_PACKAGE_HEADER_SIZE ||
        header->index_count == 0U ||
        header->index_count > RESOURCE_PACKAGE_MAX_ENTRIES ||
        header->index_offset < RESOURCE_PACKAGE_HEADER_SIZE ||
        header->index_offset >= header->package_size ||
        (header->index_offset % RESOURCE_PACKAGE_ALIGNMENT) != 0U ||
        header->data_offset > header->package_size ||
        (header->data_offset % RESOURCE_PACKAGE_ALIGNMENT) != 0U)
    {
        return RESOURCE_PACKAGE_ERR_FORMAT;             /* 包内地址范围错误 */
    }

    if (header->index_count >
        (header->package_size - header->index_offset) /
        RESOURCE_PACKAGE_INDEX_SIZE)
    {
        return RESOURCE_PACKAGE_ERR_FORMAT;             /* 索引超出包的实际长度 */
    }

    uint32_t index_end =
        header->index_offset +
        header->index_count * RESOURCE_PACKAGE_INDEX_SIZE;

    if (index_end > header->data_offset)
    {
        return RESOURCE_PACKAGE_ERR_FORMAT;             /* Index 和 Data 不得重叠 */
    }

    return RESOURCE_PACKAGE_OK;                         /* Header 结构检查通过 */
}


/* ============================================================
 * 六、打开 Resource Package
 *
 * 开始前不要求已经建立 LVGL 对象。
 * 只需要底层 QSPI Reader 可以正常读数据。
 * ============================================================ */

resource_package_status_t ResourcePackage_Open(
    resource_package_t *package,
    resource_package_slot_t slot,
    uint32_t firmware_version,
    resource_package_read_fn_t reader,
    void *reader_context)
{
    if (package == NULL)
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;           /* 输出句柄不能为空 */
    }

    memset(package, 0, sizeof(*package));                /* 避免失败时残留旧有效状态 */

    if (reader == NULL ||
        (slot != RESOURCE_PACKAGE_SLOT_A &&
         slot != RESOURCE_PACKAGE_SLOT_B))
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;           /* Reader 或槽号非法 */
    }

    resource_package_t candidate = {0};                  /* 临时构造资源包句柄 */

    candidate.reader = reader;                           /* 保存读取回调 */
    candidate.reader_context = reader_context;           /* 保存驱动上下文 */

    candidate.slot_offset =
        (slot == RESOURCE_PACKAGE_SLOT_A) ?
        QSPI_RESOURCE_A_OFFSET :
        QSPI_RESOURCE_B_OFFSET;                          /* 自动选择资源分区起点 */

    candidate.slot_size =
        (slot == RESOURCE_PACKAGE_SLOT_A) ?
        QSPI_RESOURCE_A_SIZE :
        QSPI_RESOURCE_B_SIZE;                            /* 自动选择资源分区大小 */

    uint8_t raw_header[RESOURCE_PACKAGE_HEADER_SIZE];    /* 128 字节临时缓冲区 */

    resource_package_status_t status = rp_read_flash(
        &candidate,
        candidate.slot_offset,
        raw_header,
        (uint32_t)sizeof(raw_header)
    );

    if (status != RESOURCE_PACKAGE_OK)
    {
        return status;                                   /* 读取 Header 失败 */
    }

    memcpy(
        &candidate.header,
        raw_header,
        sizeof(candidate.header)
    );                                                   /* STM32H743 按小端解析定长字段 */

    status = rp_validate_header(
        &candidate.header,
        candidate.slot_size,
        firmware_version
    );

    if (status != RESOURCE_PACKAGE_OK)
    {
        return status;                                   /* 资源包字段非法 */
    }

    memset(
        raw_header + offsetof(resource_package_header_t, header_crc32),
        0,
        sizeof(candidate.header.header_crc32)
    );                                                   /* 计算 CRC 前清零 Header 的 CRC 字段 */

    uint32_t crc =
        ResourcePackage_Crc32Update(
            0xFFFFFFFFUL,
            raw_header,
            (uint32_t)sizeof(raw_header)
        ) ^ 0xFFFFFFFFUL;                               /* CRC32 结束异或 */

    if (crc != candidate.header.header_crc32)
    {
        return RESOURCE_PACKAGE_ERR_CRC;                /* Header CRC32 不一致 */
    }

    uint32_t last_id = 0U;                               /* 上一个资源 ID */
    uint32_t last_end = candidate.header.data_offset;    /* 上一个资源的数据结束位置 */

    for (uint32_t i = 0; i < candidate.header.index_count; ++i)
    {
        resource_package_index_t entry;                  /* 单条资源索引 */

        status = rp_read_index(
            &candidate,
            i,
            &entry
        );                                               /* 读取当前索引 */

        if (status != RESOURCE_PACKAGE_OK)
        {
            return status;                               /* 索引读取失败 */
        }

        if (!rp_entry_is_valid(&candidate, &entry) ||
            entry.resource_id <= last_id ||
            entry.data_offset < last_end)
        {
            return RESOURCE_PACKAGE_ERR_INDEX;          /* ID 重复、顺序错误或资源重叠 */
        }

        last_id = entry.resource_id;                     /* 更新最后一个资源 ID */

        last_end =
            entry.data_offset +
            entry.stored_size;                           /* 更新最后资源结束地址 */
    }

    candidate.parsed = true;                             /* 仅表示结构解析通过 */

    *package = candidate;                                /* 全部成功后发布句柄 */

    return RESOURCE_PACKAGE_OK;
}


/* ============================================================
 * 七、按照 Resource ID 二分查找
 *
 * 因为 Resource Package 的索引按 ID 递增，
 * 所以不用逐条遍历全部资源。
 * ============================================================ */

resource_package_status_t ResourcePackage_FindById(
    const resource_package_t *package,
    uint32_t resource_id,
    resource_package_index_t *entry)
{
    if (package == NULL ||
        !package->parsed ||
        entry == NULL ||
        resource_id == 0U)
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;           /* 参数或句柄状态错误 */
    }

    uint32_t left = 0U;                                  /* 左侧查找边界 */

    uint32_t right = package->header.index_count;        /* 右侧查找边界（不包含） */

    while (left < right)
    {
        uint32_t middle =
            left + (right - left) / 2U;                   /* 计算中间索引，避免加法溢出 */

        resource_package_index_t current;

        resource_package_status_t status =
            rp_read_index(
                package,
                middle,
                &current
            );                                           /* 读取中间索引条目 */

        if (status != RESOURCE_PACKAGE_OK)
        {
            return status;                               /* QSPI 读取失败 */
        }

        if (current.resource_id == resource_id)
        {
            *entry = current;                            /* 找到目标资源 */

            return RESOURCE_PACKAGE_OK;
        }

        if (current.resource_id < resource_id)
        {
            left = middle + 1U;                          /* 目标在右半区 */
        }
        else
        {
            right = middle;                              /* 目标在左半区 */
        }
    }

    return RESOURCE_PACKAGE_ERR_NOT_FOUND;              /* 目标资源不存在 */
}


/* ============================================================
 * 八、读取资源数据片段
 *
 * 例如某字体资源 400 KiB，
 * 可以按 512 Byte 或 4 KiB 分块读取。
 * ============================================================ */

resource_package_status_t ResourcePackage_Read(
    const resource_package_t *package,
    const resource_package_index_t *entry,
    uint32_t offset_in_resource,
    uint8_t *destination,
    uint32_t length)
{
    if (package == NULL ||
        !package->parsed ||
        entry == NULL ||
        destination == NULL ||
        length == 0U)
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;           /* 参数检查 */
    }

    if (!rp_entry_is_valid(package, entry))
    {
        return RESOURCE_PACKAGE_ERR_INDEX;              /* 索引无效 */
    }

    if (offset_in_resource > entry->stored_size ||
        length > entry->stored_size - offset_in_resource)
    {
        return RESOURCE_PACKAGE_ERR_RANGE;              /* 读取不能超出资源大小 */
    }

    uint32_t flash_offset =
        package->slot_offset +
        entry->data_offset +
        offset_in_resource;                              /* 计算真实 QSPI 读取偏移 */

    return rp_read_flash(
        package,
        flash_offset,
        destination,
        length
    );                                                   /* 调用底层读取回调 */
}


/* ============================================================
 * 九、验证单项资源 CRC32
 *
 * 使用外部 scratch 缓冲区。
 *
 * 本函数计算的是完整资源数据的 CRC32，
 * 不等同于整个资源包的 SHA-256 验证。
 * ============================================================ */

resource_package_status_t ResourcePackage_VerifyResourceCRC32(
    const resource_package_t *package,
    const resource_package_index_t *entry,
    uint8_t *scratch,
    uint32_t scratch_size)
{
    if (package == NULL ||
        !package->parsed ||
        entry == NULL ||
        scratch == NULL ||
        scratch_size == 0U)
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;           /* 参数检查 */
    }

    if (!rp_entry_is_valid(package, entry))
    {
        return RESOURCE_PACKAGE_ERR_INDEX;              /* 不允许读取无效索引 */
    }

    uint32_t crc = 0xFFFFFFFFUL;                         /* CRC32 初始值 */

    uint32_t position = 0U;                              /* 当前已验证的字节数 */

    while (position < entry->stored_size)
    {
        uint32_t bytes =
            entry->stored_size - position;               /* 计算剩余字节数 */

        if (bytes > scratch_size)
        {
            bytes = scratch_size;                        /* 限制单次读取大小 */
        }

        resource_package_status_t status =
            ResourcePackage_Read(
                package,
                entry,
                position,
                scratch,
                bytes
            );                                           /* 读取当前数据块 */

        if (status != RESOURCE_PACKAGE_OK)
        {
            return status;                               /* 读取失败则终止 */
        }

        crc = ResourcePackage_Crc32Update(
            crc,
            scratch,
            bytes
        );                                               /* 累积当前数据块的 CRC32 */

        position += bytes;                               /* 更新读取位置 */
    }

    crc ^= 0xFFFFFFFFUL;                                 /* 全部数据处理完毕后结束异或 */

    if (crc != entry->data_crc32)
    {
        return RESOURCE_PACKAGE_ERR_CRC;                /* 资源内容校验失败 */
    }

    return RESOURCE_PACKAGE_OK;                         /* 当前资源 CRC32 校验成功 */
}
