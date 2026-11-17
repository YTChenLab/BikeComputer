
#include "resource_manager.h"            /* Resource Manager 公共接口 */
#include <string.h>                       /* memcpy、memset、memcmp */


/* ============================================================
 * 一、SHA-256 内部运行状态
 *
 * 本实现不依赖第三方密码库。
 *
 * 使用固定长度工作区，不动态分配内存。
 * ============================================================ */

typedef struct
{
    uint32_t state[8];                    /* SHA-256 内部状态 */
    uint64_t total_bytes;                 /* 累计处理的字节数 */
    uint8_t block[64];                    /* 512 bit 数据块 */
    uint32_t used;                        /* 当前块已经填充的字节数 */

} rm_sha256_ctx_t;


/* ============================================================
 * 二、SHA-256 基础位操作
 * ============================================================ */

static uint32_t rm_rotr(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32U - n));    /* 32 bit 循环右移 */
}


/* ============================================================
 * 三、读取大端格式的 32 bit 整数
 *
 * SHA-256 标准内部使用大端字节顺序。
 * STM32 资源包本身仍采用小端格式。
 * ============================================================ */

static uint32_t rm_load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |
           (uint32_t)p[3];
}


/* ============================================================
 * 四、将 32 bit 整数写成大端字节序列
 * ============================================================ */

static void rm_store_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}


/* ============================================================
 * 五、SHA-256 核心压缩函数
 *
 * 每次处理 64 字节输入块。
 *
 * 此处使用 SHA-256 标准的 64 个轮常量。
 * ============================================================ */

static void rm_sha256_compress(
    rm_sha256_ctx_t *ctx,
    const uint8_t block[64])
{
    static const uint32_t k[64] =
    {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
        0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
        0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
        0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
    };

    uint32_t w[64];                      /* 64 个消息调度字 */

    uint32_t a, b, c, d, e, f, g, h;     /* 八个工作状态 */

    /* 读取前 16 个消息字 */
    for (uint32_t i = 0; i < 16U; ++i)
    {
        w[i] = rm_load_be32(block + 4U * i);
    }

    /* 扩展为 64 个消息字 */
    for (uint32_t i = 16U; i < 64U; ++i)
    {
        uint32_t s0 =
            rm_rotr(w[i - 15U], 7U) ^
            rm_rotr(w[i - 15U], 18U) ^
            (w[i - 15U] >> 3U);

        uint32_t s1 =
            rm_rotr(w[i - 2U], 17U) ^
            rm_rotr(w[i - 2U], 19U) ^
            (w[i - 2U] >> 10U);

        w[i] =
            w[i - 16U] +
            s0 +
            w[i - 7U] +
            s1;
    }

    /* 初始化当前数据块的工作状态 */
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    /* 执行 64 轮 SHA-256 压缩计算 */
    for (uint32_t i = 0; i < 64U; ++i)
    {
        uint32_t s1 =
            rm_rotr(e, 6U) ^
            rm_rotr(e, 11U) ^
            rm_rotr(e, 25U);

        uint32_t choose =
            (e & f) ^
            ((~e) & g);

        uint32_t t1 =
            h + s1 + choose + k[i] + w[i];

        uint32_t s0 =
            rm_rotr(a, 2U) ^
            rm_rotr(a, 13U) ^
            rm_rotr(a, 22U);

        uint32_t majority =
            (a & b) ^
            (a & c) ^
            (b & c);

        uint32_t t2 = s0 + majority;

        /* 更新八个工作状态 */
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* 将本轮结果累加到 SHA-256 状态 */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}


/* ============================================================
 * 六、初始化 SHA-256 上下文
 * ============================================================ */

static void rm_sha256_init(rm_sha256_ctx_t *ctx)
{
    static const uint32_t iv[8] =
    {
        0x6a09e667U, 0xbb67ae85U,
        0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU,
        0x1f83d9abU, 0x5be0cd19U
    };

    memset(ctx, 0, sizeof(*ctx));

    memcpy(ctx->state, iv, sizeof(iv));
}


/* ============================================================
 * 七、向 SHA-256 累积输入数据
 *
 * 可以多次调用，适合 QSPI 分块读取。
 * ============================================================ */

static void rm_sha256_update(
    rm_sha256_ctx_t *ctx,
    const uint8_t *data,
    uint32_t length)
{
    ctx->total_bytes += length;

    while (length != 0U)
    {
        uint32_t space = 64U - ctx->used;

        uint32_t count =
            (length < space) ? length : space;

        memcpy(
            ctx->block + ctx->used,
            data,
            count
        );

        ctx->used += count;
        data += count;
        length -= count;

        /* 收满一个 64 字节数据块后执行压缩 */
        if (ctx->used == 64U)
        {
            rm_sha256_compress(ctx, ctx->block);
            ctx->used = 0U;
        }
    }
}


/* ============================================================
 * 八、结束 SHA-256 并生成 32 字节摘要
 *
 * 按 SHA-256 标准完成填充与长度编码。
 * ============================================================ */

static void rm_sha256_finish(
    rm_sha256_ctx_t *ctx,
    uint8_t digest[32])
{
    uint64_t bits = ctx->total_bytes * 8ULL;

    /* 追加标准填充起始位 */
    ctx->block[ctx->used++] = 0x80U;

    /* 剩余空间不足以容纳长度字段时增加一块 */
    if (ctx->used > 56U)
    {
        memset(
            ctx->block + ctx->used,
            0,
            64U - ctx->used
        );

        rm_sha256_compress(ctx, ctx->block);

        ctx->used = 0U;
    }

    /* 填充零直到偏移 56 */
    memset(
        ctx->block + ctx->used,
        0,
        56U - ctx->used
    );

    /* 末尾八字节保存消息长度，采用大端表示 */
    for (uint32_t i = 0; i < 8U; ++i)
    {
        ctx->block[56U + i] =
            (uint8_t)(bits >> (56U - 8U * i));
    }

    /* 压缩最后一个数据块 */
    rm_sha256_compress(ctx, ctx->block);

    /* 输出 32 字节摘要 */
    for (uint32_t i = 0; i < 8U; ++i)
    {
        rm_store_be32(
            digest + 4U * i,
            ctx->state[i]
        );
    }

    /* 清理工作状态 */
    memset(ctx, 0, sizeof(*ctx));
}


/* ============================================================
 * 九、验证整个 Resource Package 的 SHA-256
 *
 * 校验范围：
 * index_offset 到 package_size，不包含 Header。
 *
 * 与 Python 打包工具保持一致。
 * ============================================================ */

static resource_manager_status_t rm_verify_sha256(
    const resource_package_t *package,
    uint8_t *scratch,
    uint32_t scratch_size)
{
    rm_sha256_ctx_t sha;                  /* SHA-256 上下文 */

    uint8_t digest[32];                   /* 实际计算得到的摘要 */

    uint32_t position =
        package->header.index_offset;    /* 从 Index 起点开始 */

    rm_sha256_init(&sha);

    while (position < package->header.package_size)
    {
        uint32_t bytes =
            package->header.package_size - position;

        if (bytes > scratch_size)
        {
            bytes = scratch_size;         /* 限制单次读取容量 */
        }

        /* 从 QSPI 读取当前数据块 */
        if (!package->reader(
                package->slot_offset + position,
                scratch,
                bytes,
                package->reader_context))
        {
            return RESOURCE_MANAGER_ERR_IO;
        }

        /* 将数据块加入 SHA-256 */
        rm_sha256_update(
            &sha,
            scratch,
            bytes
        );

        position += bytes;
    }

    /* 生成整个 Payload 的 SHA-256 */
    rm_sha256_finish(&sha, digest);

    /* 对比 Header 中记录的摘要 */
    if (memcmp(
            digest,
            package->header.payload_sha256,
            sizeof(digest)) != 0)
    {
        return RESOURCE_MANAGER_ERR_SHA256;
    }

    return RESOURCE_MANAGER_OK;
}


/* ============================================================
 * 十、验证所有资源的 CRC32
 *
 * 逐条读取 Index，并复用已有 ResourcePackage
 * 的 CRC32 校验实现。
 *
 * 当前资源包的索引和数据已经经过 SHA-256 检查。
 * ============================================================ */

static resource_manager_status_t rm_verify_all_crc(
    resource_manager_t *manager,
    uint8_t *scratch,
    uint32_t scratch_size)
{
    const resource_package_t *package =
        &manager->package;

    /* 遍历所有 Resource Index */
    for (uint32_t i = 0;
         i < package->header.index_count;
         ++i)
    {
        resource_package_index_t entry;

        /* 当前索引条目的实际 QSPI 地址偏移 */
        uint32_t index_address =
            package->slot_offset +
            package->header.index_offset +
            i * RESOURCE_PACKAGE_INDEX_SIZE;

        /* 读取 48 字节索引 */
        if (!package->reader(
                index_address,
                (uint8_t *)&entry,
                RESOURCE_PACKAGE_INDEX_SIZE,
                package->reader_context))
        {
            return RESOURCE_MANAGER_ERR_IO;
        }

        /* 计算并比较当前资源的 CRC32 */
        resource_package_status_t status =
            ResourcePackage_VerifyResourceCRC32(
                package,
                &entry,
                scratch,
                scratch_size
            );

        if (status != RESOURCE_PACKAGE_OK)
        {
            manager->last_package_status = status;

            if (status == RESOURCE_PACKAGE_ERR_IO)
            {
                return RESOURCE_MANAGER_ERR_IO;
            }

            if (status == RESOURCE_PACKAGE_ERR_CRC)
            {
                return RESOURCE_MANAGER_ERR_RESOURCE_CRC;
            }

            return RESOURCE_MANAGER_ERR_PACKAGE;
        }
    }

    return RESOURCE_MANAGER_OK;
}


/* ============================================================
 * 十一、打开并验证指定资源分区
 *
 * 这是 Resource Manager 的正式初始化入口。
 * ============================================================ */

resource_manager_status_t ResourceManager_Open(
    resource_manager_t *manager,
    resource_package_slot_t slot,
    uint32_t firmware_version,
    resource_package_read_fn_t reader,
    void *reader_context,
    uint8_t *scratch,
    uint32_t scratch_size)
{
    /* 管理器对象不能为空 */
    if (manager == NULL)
    {
        return RESOURCE_MANAGER_ERR_ARGUMENT;
    }

    /* 初始化之前清除旧状态 */
    memset(manager, 0, sizeof(*manager));

    /* Reader 和工作缓冲区检查 */
    if (reader == NULL ||
        scratch == NULL ||
        scratch_size < 64U)
    {
        return RESOURCE_MANAGER_ERR_ARGUMENT;
    }

    /* 第一步：调用已有资源包解析器 */
    manager->last_package_status =
        ResourcePackage_Open(
            &manager->package,
            slot,
            firmware_version,
            reader,
            reader_context
        );

    if (manager->last_package_status != RESOURCE_PACKAGE_OK)
    {
        if (manager->last_package_status == RESOURCE_PACKAGE_ERR_IO)
        {
            return RESOURCE_MANAGER_ERR_IO;
        }

        return RESOURCE_MANAGER_ERR_PACKAGE;
    }

    /* 第二步：验证整个 Payload SHA-256 */
    resource_manager_status_t status =
        rm_verify_sha256(
            &manager->package,
            scratch,
            scratch_size
        );

    if (status != RESOURCE_MANAGER_OK)
    {
        return status;
    }

    /* 第三步：逐项验证资源 CRC32 */
    status = rm_verify_all_crc(
        manager,
        scratch,
        scratch_size
    );

    if (status != RESOURCE_MANAGER_OK)
    {
        return status;
    }

    /* 全部验证通过后，正式允许资源读取 */
    manager->ready = true;

    return RESOURCE_MANAGER_OK;
}


/* ============================================================
 * 十二、查询 Resource Manager 就绪状态
 * ============================================================ */

bool ResourceManager_IsReady(
    const resource_manager_t *manager)
{
    return manager != NULL && manager->ready;
}


/* ============================================================
 * 十三、通过资源 ID 查找资源索引
 * ============================================================ */

resource_package_status_t ResourceManager_Find(
    const resource_manager_t *manager,
    uint32_t resource_id,
    resource_package_index_t *entry)
{
    if (!ResourceManager_IsReady(manager))
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;
    }

    return ResourcePackage_FindById(
        &manager->package,
        resource_id,
        entry
    );
}


/* ============================================================
 * 十四、根据资源索引读取数据
 *
 * 调用者应传入从当前管理器查询得到的 entry。
 * ============================================================ */

resource_package_status_t ResourceManager_Read(
    const resource_manager_t *manager,
    const resource_package_index_t *entry,
    uint32_t offset,
    uint8_t *destination,
    uint32_t length)
{
    if (!ResourceManager_IsReady(manager))
    {
        return RESOURCE_PACKAGE_ERR_ARGUMENT;
    }

    return ResourcePackage_Read(
        &manager->package,
        entry,
        offset,
        destination,
        length
    );
}


/* ============================================================
 * 十五、通过资源 ID 直接读取数据
 *
 * 先查询索引，再读取资源指定范围。
 * ============================================================ */

resource_package_status_t ResourceManager_ReadById(
    const resource_manager_t *manager,
    uint32_t resource_id,
    uint32_t offset,
    uint8_t *destination,
    uint32_t length)
{
    resource_package_index_t entry;

    /* 查找指定 ID 对应的索引 */
    resource_package_status_t status =
        ResourceManager_Find(
            manager,
            resource_id,
            &entry
        );

    if (status != RESOURCE_PACKAGE_OK)
    {
        return status;
    }

    /* 读取当前资源指定数据范围 */
    return ResourceManager_Read(
        manager,
        &entry,
        offset,
        destination,
        length
    );
}
