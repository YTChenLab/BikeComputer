#include "resource_storage.h"         /* 当前资源存储模块的接口 */
#include "resource_manager.h"         /* 资源校验管理器 */
#include "resource_package.h"         /* 资源包和 A/B 槽定义 */
#include "bsp_qspi.h"                 /* W25Q128 底层驱动 */
#include "qspi_partition.h"          /* 外部 Flash 分区定义 */
#include "resource_package_format.h" /* 资源包二进制格式 */

#include <stdint.h>                   /* 固定宽度整数 */
#include <stdbool.h>                  /* bool 类型 */
#include <stddef.h>                  /* NULL */
#include <string.h>                  /* memset、memcmp */

/*============================================================
 * 一、活动资源槽元数据
 *
 * 使用两个独立的 4 KiB 扇区。
 *
 * Metadata A：0x00000000
 * Metadata B：0x00001000
 *
 * 其中一份作为当前有效记录，
 * 更新时先写另一份。
 *
 * 提交标记最后写入，避免掉电时误认为更新成功。
 *============================================================*/

#define RS_META_SECTOR_A      (QSPI_METADATA_OFFSET)

#define RS_META_SECTOR_B      \
    (QSPI_METADATA_OFFSET + QSPI_FLASH_SECTOR_SIZE)

#define RS_META_BYTES          (36UL)       /* 记录总长度 */

#define RS_META_CRC_END        (28UL)       /* CRC 计算范围 */

#define RS_META_COMMIT_OFFSET  (32UL)       /* 提交标记偏移 */

#define RS_META_MAGIC          (0x4D435242UL) /* BRCM */

#define RS_META_VERSION        (1UL)        /* 元数据格式版本 */

#define RS_META_COMMITTED      (0x00000000UL)

#define RS_META_NONE           (0xFFU)

/*============================================================
 * 二、运行时状态
 *
 * 全部采用静态内存，不使用 malloc。
 *
 * 约束：
 * 资源更新接口只允许一个资源维护任务调用。
 * 不能从多个任务同时进行 Begin/Write/Commit。
 *============================================================*/

/* 当前有效资源的管理器 */
static resource_manager_t s_activeManager;

/* 新资源包验证期间的临时管理器 */
static resource_manager_t s_candidateManager;

/* SHA-256、CRC32 分块校验缓冲区 */
static uint8_t s_scratch[1024];

/* 重传数据比较缓冲区 */
static uint8_t s_compare[RESOURCE_STORAGE_CHUNK_SIZE];

/* 是否完成 ResourceStorage_Init */
static uint8_t s_initialized = 0U;

/* 是否存在已经提交并验证通过的资源 */
static uint8_t s_hasActive = 0U;

/* 当前是否正在接收新资源包 */
static uint8_t s_updating = 0U;

/* 当前元数据所在扇区：0、1 或无 */
static uint8_t s_metaSector = RS_META_NONE;

/* 当前元数据更新序号 */
static uint32_t s_metaSequence = 0U;

/* 当前 STM32 应用固件版本 */
static uint32_t s_firmwareVersion = 0U;
/* Q4-3：区分完整性失败与总线读取失败，不能把未知可读性当成首次空槽。 */
static resource_manager_status_t s_lastVerifyStatus;

/* 当前有效资源槽 */
static resource_package_slot_t s_activeSlot =
    RESOURCE_PACKAGE_SLOT_A;

/* 本次更新的目标资源槽 */
static resource_package_slot_t s_targetSlot =
    RESOURCE_PACKAGE_SLOT_A;

/* 本次资源包应接收的总字节数 */
static uint32_t s_expected = 0U;

/* 本次已经成功写入的字节数 */
static uint32_t s_received = 0U;

/* 整包增量 CRC32 中间状态 */
static uint32_t s_streamCrc = 0xFFFFFFFFUL;

/*============================================================
 * 三、Metadata 运行时记录
 *
 * 注意：
 * 这是 RAM 中解析后的记录。
 * Flash 中仍使用明确的小端字节格式。
 *============================================================*/

typedef struct
{
    uint32_t sequence;           /* 元数据更新序号 */

    uint32_t slot;               /* 0=A，1=B */

    uint32_t size;               /* 完整资源包长度 */

    uint32_t package_crc;        /* 整包诊断 CRC32 */

    uint8_t sector;              /* 所在元数据扇区 */

    uint8_t valid;               /* 当前记录是否有效 */

} rs_meta_t;

/*============================================================
 * 四、将 32 位整数写入小端字节序列
 *
 * 小端：
 * 低位字节放在前面。
 *
 * 不直接 memcpy C 结构体，避免对齐与填充问题。
 *============================================================*/

static void RS_WriteU32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;             /* bit 0～7 */

    p[1] = (uint8_t)(v >> 8U);     /* bit 8～15 */

    p[2] = (uint8_t)(v >> 16U);    /* bit 16～23 */

    p[3] = (uint8_t)(v >> 24U);    /* bit 24～31 */
}

/*============================================================
 * 五、从小端字节序列读取 32 位整数
 *============================================================*/

static uint32_t RS_ReadU32(const uint8_t *p)
{
    return
        (uint32_t)p[0] |
        ((uint32_t)p[1] << 8U) |
        ((uint32_t)p[2] << 16U) |
        ((uint32_t)p[3] << 24U);
}

/*============================================================
 * 六、CRC32 增量计算
 *
 * CRC-32/ISO-HDLC
 *
 * 反射多项式：0xEDB88320
 * 初始状态：0xFFFFFFFF
 * 最终异或：0xFFFFFFFF
 *
 * 可对多个数据块连续计算。
 *============================================================*/



/*============================================================
 * 七、获取 Resource A/B 起始偏移
 *
 * 地址均为 W25Q128 内部地址。
 *============================================================*/

static uint32_t RS_SlotOffset(
    resource_package_slot_t slot
)
{
    return (slot == RESOURCE_PACKAGE_SLOT_A) ?
           QSPI_RESOURCE_A_OFFSET :
           QSPI_RESOURCE_B_OFFSET;
}

/*============================================================
 * 八、资源管理器的 QSPI 读取回调
 *
 * 完全复用已有 ResourceManager。
 *
 * resource_manager.c 不需要知道 STM32 HAL。
 *============================================================*/

static bool RS_Reader(
    uint32_t address,
    uint8_t *destination,
    uint32_t length,
    void *context
)
{
    /* 当前只有一颗 QSPI Flash，不需要附加上下文 */
    (void)context;

    /* 调用 BSP 的 0x6B 四线快速读取功能 */
    return BSP_QSPI_ReadQuad(
        address,
        destination,
        length
    ) == BSP_QSPI_OK;
}

/*============================================================
 * 九、读取并验证一份 Metadata 记录
 *
 * 执行：
 * 1. 读取 36 字节。
 * 2. 检查魔数。
 * 3. 检查格式版本。
 * 4. 检查 Commit 标记。
 * 5. 计算 CRC32。
 * 6. 检查资源槽和大小。
 *
 * 注意：
 * 无效记录与 QSPI 通信错误必须区别处理。
 *============================================================*/

static resource_storage_status_t RS_LoadMeta(
    uint8_t sector,
    rs_meta_t *meta
)
{
    /* 保存 Flash 中的原始元数据 */
    uint8_t raw[RS_META_BYTES];

    /* 根据编号选择实际扇区起始地址 */
    uint32_t address =
        (sector == 0U) ?
        RS_META_SECTOR_A :
        RS_META_SECTOR_B;

    /* 默认整份记录无效 */
    memset(meta, 0, sizeof(*meta));

    /* 从外部 Flash 读取记录 */
    if (BSP_QSPI_Read(
            address,
            raw,
            sizeof(raw)) != BSP_QSPI_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 检查魔数、版本及最终提交标记 */
    if ((RS_ReadU32(raw + 0U) != RS_META_MAGIC) ||
        (RS_ReadU32(raw + 4U) != RS_META_VERSION) ||
        (RS_ReadU32(
             raw + RS_META_COMMIT_OFFSET
         ) != RS_META_COMMITTED))
    {
        /* 空白或未提交记录不属于硬件故障 */
        return RESOURCE_STORAGE_OK;
    }

    /* 重新计算前 28 字节的 CRC32 */
    uint32_t crc = ResourcePackage_Crc32Update(
        0xFFFFFFFFUL,
        raw,
        RS_META_CRC_END
    ) ^ 0xFFFFFFFFUL;

    /* 比较记录自带的 CRC32 */
    if (crc != RS_ReadU32(raw + 28U))
    {
        /* CRC 不正确时不采用该记录 */
        return RESOURCE_STORAGE_OK;
    }

    /* 读取活动槽编号 */
    uint32_t slot = RS_ReadU32(raw + 12U);

    /* 读取资源包总长度 */
    uint32_t size = RS_ReadU32(raw + 16U);

    /* 校验槽号、资源长度和保留字段 */
    if ((slot > 1U) ||
        (size < RESOURCE_PACKAGE_HEADER_SIZE) ||
        (size > QSPI_RESOURCE_A_SIZE) ||
        (RS_ReadU32(raw + 24U) != 0U))
    {
        return RESOURCE_STORAGE_OK;
    }

    /* 解析更新序列号 */
    meta->sequence = RS_ReadU32(raw + 8U);

    /* 解析资源槽 */
    meta->slot = slot;

    /* 解析包长度 */
    meta->size = size;

    /* 解析整包诊断 CRC */
    meta->package_crc = RS_ReadU32(raw + 20U);

    /* 记录当前元数据来自哪个扇区 */
    meta->sector = sector;

    /* 标记这份记录通过结构检查 */
    meta->valid = 1U;

    return RESOURCE_STORAGE_OK;
}

/*============================================================
 * 十、验证一个完整资源槽
 *
 * 复用已有的 ResourceManager_Open。
 *
 * 已有实现包含：
 * Header CRC32
 * Index 合法性
 * 固件兼容性
 * Payload SHA-256
 * 全部资源 CRC32
 *
 * 不能使用“只检查包头”代替完整验证。
 *============================================================*/

static uint8_t RS_OpenVerified(
    resource_package_slot_t slot,
    uint32_t size,
    resource_manager_t *manager
)
{
    /* 使用已有资源管理器打开指定槽 */
    resource_manager_status_t result =
        ResourceManager_Open(
            manager,
            slot,
            s_firmwareVersion,
            RS_Reader,
            NULL,
            s_scratch,
            sizeof(s_scratch)
        );

    /* 任意完整性检查失败都不允许采用此槽 */
    s_lastVerifyStatus = result;
    if (result != RESOURCE_MANAGER_OK)
    {
        return 0U;
    }

    /* Metadata 中记录的长度必须匹配真实包头 */
    if (manager->package.header.package_size != size)
    {
        return 0U;
    }

    return 1U;
}

/*============================================================
 * 十一、初始化资源存储管理器
 *
 * 只加载已经 Commit 的资源。
 *
 * 先尝试较新的 Metadata。
 * 如果它指向的包已损坏，则尝试旧记录。
 *
 * 不扫描并自动启用未经 Commit 的新包。
 *============================================================*/

resource_storage_status_t ResourceStorage_Init(
    uint32_t firmware_version
)
{
    /* Q4-2：在任何 RAM 状态修改前拒绝，保持 UI 活动槽与映射生命周期。
     * 映射固定后不可更新、重置或提交另一槽，资源升级需重启。 */
    if (BSP_QSPI_IsMemoryMappedPinned() != 0U)
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 两份元数据记录 */
    rs_meta_t a;
    bool verification_io_failed = false;
    rs_meta_t b;

    /* 按更新序号决定检查顺序 */
    rs_meta_t *first;
    rs_meta_t *second;

    /* 更新过程中不允许重新初始化 */
    if (s_updating != 0U)
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 系统启动时首先撤销全部写入授权 */
    if (BSP_QSPI_LockWrite() != BSP_QSPI_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 清理运行时状态 */
    s_initialized = 0U;
    s_hasActive = 0U;
    s_metaSector = RS_META_NONE;
    s_metaSequence = 0U;

    /* 保存当前应用固件版本 */
    s_firmwareVersion = firmware_version;

    /* 清理旧活动管理器 */
    memset(
        &s_activeManager,
        0,
        sizeof(s_activeManager)
    );

    /* 读取第一份 Metadata */
    if (RS_LoadMeta(0U, &a) != RESOURCE_STORAGE_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 读取第二份 Metadata */
    if (RS_LoadMeta(1U, &b) != RESOURCE_STORAGE_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 默认先检查 A，再检查 B */
    first = &a;
    second = &b;

    /* B 有效且比 A 更新时优先检查 B */
    if ((!a.valid && b.valid) ||
        (a.valid && b.valid &&
         (int32_t)(b.sequence - a.sequence) > 0))
    {
        first = &b;
        second = &a;
    }

    /* 将两个候选按优先级排列 */
    rs_meta_t *candidates[2] = {
        first,
        second
    };

    /* 最多尝试两份已经提交的记录 */
    for (uint32_t i = 0U; i < 2U; i++)
    {
        /* 取出当前候选 */
        rs_meta_t *record = candidates[i];

        /* 不采用无效或未提交元数据 */
        if (record->valid == 0U)
        {
            continue;
        }

        /* 对记录指定的整个资源包进行验证 */
        if (RS_OpenVerified(
                (resource_package_slot_t)record->slot,
                record->size,
                &s_candidateManager) == 0U)
        {
            /* 新槽失败时继续尝试旧槽 */
            if (s_lastVerifyStatus == RESOURCE_MANAGER_ERR_IO) verification_io_failed = true;
            continue;
        }

        /* 验证通过后复制完整管理器状态 */
        s_activeManager = s_candidateManager;

        /* 正式记录当前活动资源槽 */
        s_activeSlot =
            (resource_package_slot_t)record->slot;

        /* 保存当前有效元数据扇区 */
        s_metaSector = record->sector;

        /* 保存当前更新序列号 */
        s_metaSequence = record->sequence;

        /* 标记当前存在有效资源包 */
        s_hasActive = 1U;

        /* 已找到有效资源，不必继续扫描 */
        break;
    }

    /* 元数据加载流程结束 */
    /* 没有已验证回退槽且存在 I/O 失败时禁止安装，保留诊断/复位通信。
     * CRC/SHA 损坏仍走原有备用槽回退规则，不扫描或擅自激活未提交槽。 */
    if (s_hasActive == 0U && verification_io_failed) return RESOURCE_STORAGE_ERR_IO;
    s_initialized = 1U;

    /* 空白 Flash 是正常的首次安装状态 */
    return (s_hasActive != 0U) ?
           RESOURCE_STORAGE_OK :
           RESOURCE_STORAGE_EMPTY;
}

/*============================================================
 * 十二、查询活动资源槽
 *============================================================*/

resource_storage_status_t ResourceStorage_GetActiveSlot(
    resource_package_slot_t *slot
)
{
    /* 输出参数不能为空 */
    if (slot == NULL)
    {
        return RESOURCE_STORAGE_ERR_ARGUMENT;
    }

    /* 存储服务必须已完成初始化 */
    if (s_initialized == 0U)
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 当前没有已提交资源 */
    if (s_hasActive == 0U)
    {
        return RESOURCE_STORAGE_EMPTY;
    }

    /* 返回有效资源槽 */
    *slot = s_activeSlot;

    return RESOURCE_STORAGE_OK;
}

/*============================================================
 * 十三、开始资源包更新
 *
 * 始终写入非活动槽。
 *
 * 当前 A 有效 -> 写 B
 * 当前 B 有效 -> 写 A
 * 没有资源     -> 首次写 A
 *
 * 这里只建立更新会话，还不擦除整个槽。
 *============================================================*/

resource_storage_status_t ResourceStorage_Begin(
    uint32_t package_size,
    resource_package_slot_t *target
)
{
    /* Q4-2：在任何 RAM 状态修改前拒绝，保持 UI 活动槽与映射生命周期。
     * 映射固定后不可更新、重置或提交另一槽，资源升级需重启。 */
    if (BSP_QSPI_IsMemoryMappedPinned() != 0U)
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 将资源槽转换为 BSP 的分区权限 */
    BSP_QSPI_Partition_t partition;

    /* 输出参数必须有效 */
    if (target == NULL)
    {
        return RESOURCE_STORAGE_ERR_ARGUMENT;
    }

    /* 必须已经初始化且当前没有其他更新 */
    if ((s_initialized == 0U) || (s_updating != 0U))
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 检查资源包总长度 */
    if ((package_size < RESOURCE_PACKAGE_HEADER_SIZE) ||
        (package_size > QSPI_RESOURCE_A_SIZE))
    {
        return RESOURCE_STORAGE_ERR_RANGE;
    }

    /* 自动选择非活动资源槽 */
    s_targetSlot =
        ((s_hasActive == 0U) ||
         (s_activeSlot == RESOURCE_PACKAGE_SLOT_B)) ?
        RESOURCE_PACKAGE_SLOT_A :
        RESOURCE_PACKAGE_SLOT_B;

    /* 转换为 BSP 写入授权分区 */
    partition =
        (s_targetSlot == RESOURCE_PACKAGE_SLOT_A) ?
        BSP_QSPI_PART_RESOURCE_A :
        BSP_QSPI_PART_RESOURCE_B;

    /* 告知 BSP 当前只允许写入该资源槽 */
    if (BSP_QSPI_AllowWritePartition(
            partition) != BSP_QSPI_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 记录完整包的目标长度 */
    s_expected = package_size;

    /* 接收位置从零开始 */
    s_received = 0U;

    /* 初始化整包增量 CRC32 */
    s_streamCrc = 0xFFFFFFFFUL;

    /* 正式进入更新状态 */
    s_updating = 1U;

    /* 返回本次选择的目标槽 */
    *target = s_targetSlot;

    return RESOURCE_STORAGE_OK;
}

/*============================================================
 * 十四、接收并写入一个资源分包
 *
 * 要求：
 * 1. 每包不超过 256 字节。
 * 2. 偏移按 256 字节对齐。
 * 3. 除最后一包外，长度固定为 256。
 * 4. 必须按顺序接收。
 * 5. 允许旧包重传，但内容必须一致。
 *
 * 每进入一个新 4 KiB 扇区，自动擦除一次。
 *============================================================*/

resource_storage_status_t ResourceStorage_Write(
    uint32_t offset,
    const uint8_t *data,
    uint32_t length
)
{
    /* Q4-2：在任何 RAM 状态修改前拒绝，保持 UI 活动槽与映射生命周期。
     * 映射固定后不可更新、重置或提交另一槽，资源升级需重启。 */
    if (BSP_QSPI_IsMemoryMappedPinned() != 0U)
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 本次数据在外部 Flash 中的实际位置 */
    uint32_t address;

    /* 检查输入数据指针 */
    if (data == NULL)
    {
        return RESOURCE_STORAGE_ERR_ARGUMENT;
    }

    /* 当前必须处于更新会话 */
    if (s_updating == 0U)
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 检查分包长度、起点和总范围 */
    if ((length == 0U) ||
        (length > RESOURCE_STORAGE_CHUNK_SIZE) ||
        ((offset % RESOURCE_STORAGE_CHUNK_SIZE) != 0U) ||
        (offset >= s_expected) ||
        (length > s_expected - offset))
    {
        return RESOURCE_STORAGE_ERR_RANGE;
    }

    /* 只有最后一个资源分包可以不足 256 字节 */
    if ((length != RESOURCE_STORAGE_CHUNK_SIZE) &&
        (offset + length != s_expected))
    {
        return RESOURCE_STORAGE_ERR_RANGE;
    }

    /* 转换为 Flash 芯片内部偏移 */
    address = RS_SlotOffset(s_targetSlot) + offset;

    /* 判断本次是否为已经写过的数据包 */
    if (offset < s_received)
    {
        /* 禁止旧数据块与尚未写入的区域交叠 */
        if (length > s_received - offset)
        {
            return RESOURCE_STORAGE_ERR_STATE;
        }

        /* 重新读取 Flash 已有数据 */
        if (BSP_QSPI_Read(
                address,
                s_compare,
                length) != BSP_QSPI_OK)
        {
            return RESOURCE_STORAGE_ERR_IO;
        }

        /* 相同内容重传视为成功，不再次执行编程 */
        return (memcmp(
                    s_compare,
                    data,
                    length) == 0) ?
               RESOURCE_STORAGE_OK :
               RESOURCE_STORAGE_ERR_CONFLICT;
    }

    /* 必须紧接上一个已确认的数据块 */
    if (offset != s_received)
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 每次第一次进入 4 KiB 扇区时才擦除 */
    if ((offset % QSPI_FLASH_SECTOR_SIZE) == 0U)
    {
        if (BSP_QSPI_EraseSector4K(
                address) != BSP_QSPI_OK)
        {
            return RESOURCE_STORAGE_ERR_IO;
        }
    }

    /* 调用正式 BSP 执行分页编程和写后回读 */
    if (BSP_QSPI_Program(
            address,
            data,
            length) != BSP_QSPI_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 只有写入成功，才累计整包 CRC32 */
    s_streamCrc = ResourcePackage_Crc32Update(
        s_streamCrc,
        data,
        length
    );

    /* 只有实际写入成功，才增加已接收字节数 */
    s_received += length;

    return RESOURCE_STORAGE_OK;
}

/*============================================================
 * 十五、提交新的活动资源槽元数据
 *
 * 元数据写入顺序：
 *
 * 1. 选择非活动元数据扇区。
 * 2. 擦除该 4 KiB 扇区。
 * 3. 写入魔数、版本、槽号、长度和 CRC32。
 * 4. 最后独立写入 Commit 标记。
 * 5. 回读检查。
 *
 * 整个过程中不擦除旧的有效元数据扇区。
 *============================================================*/

static resource_storage_status_t RS_CommitMeta(
    uint32_t package_crc
)
{
    /* 元数据原始字节缓冲区 */
    uint8_t record[RS_META_BYTES];

    /* 选择另一份元数据记录 */
    uint8_t targetSector =
        (s_metaSector == 0U) ? 1U : 0U;

    /* 计算目标元数据扇区地址 */
    uint32_t address =
        (targetSector == 0U) ?
        RS_META_SECTOR_A :
        RS_META_SECTOR_B;

    /* 生成新的更新序号 */
    uint32_t nextSequence = s_metaSequence + 1U;

    /* 元数据 CRC32 */
    uint32_t crc;

    /* 初始化为 Flash 擦除状态 */
    memset(record, 0xFF, sizeof(record));

    /* 写入元数据魔数 */
    RS_WriteU32(record + 0U, RS_META_MAGIC);

    /* 写入元数据格式版本 */
    RS_WriteU32(record + 4U, RS_META_VERSION);

    /* 写入新的更新序号 */
    RS_WriteU32(record + 8U, nextSequence);

    /* 写入更新完成后应该启用的资源槽 */
    RS_WriteU32(
        record + 12U,
        (uint32_t)s_targetSlot
    );

    /* 写入本次资源包总长度 */
    RS_WriteU32(record + 16U, s_expected);

    /* 保存整包 CRC32，供诊断和版本管理使用 */
    RS_WriteU32(record + 20U, package_crc);

    /* V1 保留字段，固定为 0 */
    RS_WriteU32(record + 24U, 0U);

    /* 计算前 28 字节 CRC32 */
    crc = ResourcePackage_Crc32Update(
        0xFFFFFFFFUL,
        record,
        RS_META_CRC_END
    ) ^ 0xFFFFFFFFUL;

    /* 写入元数据 CRC32 */
    RS_WriteU32(record + 28U, crc);

    /* 将 BSP 写入授权切换到 Metadata 分区 */
    if (BSP_QSPI_AllowWritePartition(
            BSP_QSPI_PART_METADATA) != BSP_QSPI_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 擦除目标元数据扇区，保留另一扇区 */
    if (BSP_QSPI_EraseSector4K(
            address) != BSP_QSPI_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 第一次写入：不包含最后的提交标记 */
    if (BSP_QSPI_Program(
            address,
            record,
            RS_META_COMMIT_OFFSET) != BSP_QSPI_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 最后写入的 Commit 标记 */
    uint8_t commit[4] = {
        0U, 0U, 0U, 0U
    };

    /* 第二次写入：标记新资源槽已提交 */
    if (BSP_QSPI_Program(
            address + RS_META_COMMIT_OFFSET,
            commit,
            sizeof(commit)) != BSP_QSPI_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 从 Flash 回读刚刚写入的记录 */
    rs_meta_t confirmed;

    /* 只有记录全部正确才认为元数据提交成功 */
    if ((RS_LoadMeta(
             targetSector,
             &confirmed) != RESOURCE_STORAGE_OK) ||
        (confirmed.valid == 0U) ||
        (confirmed.sequence != nextSequence) ||
        (confirmed.slot != (uint32_t)s_targetSlot) ||
        (confirmed.size != s_expected))
    {
        return RESOURCE_STORAGE_ERR_VERIFY;
    }

    /* 保存新的活动元数据扇区 */
    s_metaSector = targetSector;

    /* 保存最新的更新序号 */
    s_metaSequence = nextSequence;

    return RESOURCE_STORAGE_OK;
}

/*============================================================
 * 十六、正式提交完整资源更新
 *
 * 注意：
 * 此函数不能仅因为已经接收全部字节就直接切槽。
 *
 * 必须先完成 ResourceManager_Open 的全部校验。
 *============================================================*/

resource_storage_status_t ResourceStorage_Commit(void)
{
    /* Q4-2：在任何 RAM 状态修改前拒绝，保持 UI 活动槽与映射生命周期。
     * 映射固定后不可更新、重置或提交另一槽，资源升级需重启。 */
    if (BSP_QSPI_IsMemoryMappedPinned() != 0U)
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 记录 Metadata 提交结果 */
    resource_storage_status_t status;

    /* 保存整包诊断 CRC32 */
    uint32_t package_crc;

    /* 必须正在更新，且已经收满全部数据 */
    if ((s_updating == 0U) ||
        (s_received != s_expected))
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 完整校验之前先撤销 Flash 写入权限 */
    if (BSP_QSPI_LockWrite() != BSP_QSPI_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 使用已有 Resource Manager 完整校验新包 */
    if (RS_OpenVerified(
            s_targetSlot,
            s_expected,
            &s_candidateManager) == 0U)
    {
        /* 校验失败，不改变原来的活动资源槽 */
        return RESOURCE_STORAGE_ERR_VERIFY;
    }

    /* 完成整包 CRC32 计算 */
    package_crc = s_streamCrc ^ 0xFFFFFFFFUL;

    /* 将新活动槽写入双备份 Metadata */
    status = RS_CommitMeta(package_crc);

    /* Metadata 提交失败时不修改当前 RAM 活动槽 */
    if (status != RESOURCE_STORAGE_OK)
    {
        (void)BSP_QSPI_LockWrite();

        return status;
    }

    /* 只有整包与元数据都成功，才切换资源管理器 */
    s_activeManager = s_candidateManager;

    /* 更新当前活动槽编号 */
    s_activeSlot = s_targetSlot;

    /* 标记已经存在有效资源 */
    s_hasActive = 1U;

    /* 当前更新会话结束 */
    s_updating = 0U;

    /* 再次撤销写入授权 */
    (void)BSP_QSPI_LockWrite();

    return RESOURCE_STORAGE_OK;
}

/*============================================================
 * 十七、主动中止资源更新
 *
 * 未提交的目标槽允许保留部分数据。
 * 下一次 Begin + Write 会重新擦除对应扇区。
 *
 * 原有效资源槽和元数据均不受到影响。
 *============================================================*/

resource_storage_status_t ResourceStorage_Abort(void)
{
    /* Q4-2：在任何 RAM 状态修改前拒绝，保持 UI 活动槽与映射生命周期。
     * 映射固定后不可更新、重置或提交另一槽，资源升级需重启。 */
    if (BSP_QSPI_IsMemoryMappedPinned() != 0U)
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 只有完成初始化后才能中止会话 */
    if (s_initialized == 0U)
    {
        return RESOURCE_STORAGE_ERR_STATE;
    }

    /* 首先撤销所有 Flash 写入权限 */
    if (BSP_QSPI_LockWrite() != BSP_QSPI_OK)
    {
        return RESOURCE_STORAGE_ERR_IO;
    }

    /* 清除正在更新的状态 */
    s_updating = 0U;

    /* 清除本次期望长度 */
    s_expected = 0U;

    /* 清除本次接收进度 */
    s_received = 0U;

    /* 复位 CRC32 状态 */
    s_streamCrc = 0xFFFFFFFFUL;

    return RESOURCE_STORAGE_OK;
}

/*============================================================
 * 十八、获取已经成功写入的字节数
 *
 * 可以直接用于 UART/ESP32 通信层的进度应答。
 *============================================================*/

uint32_t ResourceStorage_GetReceived(void)
{
    return s_received;
}

/*============================================================
 * 十九、从当前有效资源槽读取指定资源
 *
 * 当前版本使用间接 QUADSPI 读取。
 *
 * 后续 LVGL 接入时，可以进一步增加
 * Memory-Mapped 资源指针访问接口。
 *============================================================*/

resource_package_status_t ResourceStorage_ReadById(
    uint32_t resource_id,
    uint32_t offset,
    uint8_t *destination,
    uint32_t length
)
{
    /* 当前必须存在有效资源 */
    if (s_hasActive == 0U)
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;
    }

    /* 资源更新过程中禁止业务侧并发读取 */
    if (s_updating != 0U)
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;
    }

    /* 使用现有资源管理器查询并读取 Resource ID */
    return ResourceManager_ReadById(
        &s_activeManager,
        resource_id,
        offset,
        destination,
        length
    );
}

/* 只读快照不触发间接读，映射固定后仍可查询，供协议状态回报使用。
 * 所有可修改的 storage 操作仅由 CommTask 安装服务串行调用。 */
bool ResourceStorage_GetInfo(resource_storage_info_t *info)
{
    if (info == NULL) return false;
    memset(info, 0, sizeof(*info));
    info->active_slot = UINT32_MAX;
    if (s_initialized == 0U || s_hasActive == 0U) return false;
    info->active_slot = (uint32_t)s_activeSlot;
    info->package_version = s_activeManager.package.header.package_version;
    info->package_size = s_activeManager.package.header.package_size;
    memcpy(info->payload_sha256, s_activeManager.package.header.payload_sha256, 32U);
    return true;
}
