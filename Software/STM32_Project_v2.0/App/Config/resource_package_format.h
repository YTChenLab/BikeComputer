#ifndef RESOURCE_PACKAGE_FORMAT_H
#define RESOURCE_PACKAGE_FORMAT_H

/* ============================================================
 * 文件名称：resource_package_format.h
 *
 * 项目名称：BikeComputer STM32H743 公路车码表
 *
 * 模块名称：QSPI Resource Package 数据格式
 *
 * 功能说明：
 * 1. 定义 LVGL 静态资源包的二进制存储格式。
 * 2. 定义资源包头部、索引项和资源类型。
 * 3. 为后续 Resource Manager 提供统一的数据结构。
 * 4. 为 PC 资源打包工具和 OTA 升级提供格式依据。
 *
 * 重要说明：
 * - 本文件只定义存储格式，不操作 QSPI 硬件。
 * - 所有地址均采用相对于资源包起点的偏移。
 * - 所有多字节整数采用小端字节序。
 * - Flash 文件格式中禁止使用 C 指针或枚举类型。
 * - Resource A/B 使用完全相同的资源包格式。
 *
 * 格式版本：V1
 * ============================================================ */

#include <stdint.h>                     /* 固定宽度整数类型 */
#include <stddef.h>                     /* offsetof() */
#include "qspi_partition.h"             /* QSPI 分区配置 */


/* ============================================================
 * 一、资源包基本常量
 * ============================================================ */

/* 魔数：文件开头四字节依次为 'B' 'C' 'R' 'P' */
#define RESOURCE_PACKAGE_MAGIC          0x50524342UL

/* 当前资源包文件格式版本 */
#define RESOURCE_PACKAGE_FORMAT_VERSION 1UL

/* 当前资源包与 UI 代码的接口版本，后续独立维护 */
#define RESOURCE_PACKAGE_UI_ABI_VERSION 1UL

/* 固定资源包头部长度：128 Byte */
#define RESOURCE_PACKAGE_HEADER_SIZE    128UL

/* 每条资源索引长度：48 Byte */
#define RESOURCE_PACKAGE_INDEX_SIZE     48UL

/* 单个资源包允许的最大索引数量 */
#define RESOURCE_PACKAGE_MAX_ENTRIES    256UL

/* 资源数据推荐按 32 Byte 边界对齐 */
#define RESOURCE_PACKAGE_ALIGNMENT      32UL

/* 单个 Resource 槽的最大存储容量 */
#define RESOURCE_PACKAGE_MAX_SIZE       QSPI_RESOURCE_A_SIZE

/* 当前格式不使用任何标志位 */
#define RESOURCE_PACKAGE_FLAGS_NONE     0UL


/* ============================================================
 * 二、资源类型
 *
 * 注意：
 * 这里使用固定数值宏，而不是 C enum。
 *
 * 原因：
 * 编译器可能采用不同的 enum 存储宽度，
 * 不适合直接作为跨平台二进制文件字段。
 * ============================================================ */

/* LVGL 字体字形位图 */
#define RESOURCE_TYPE_FONT_BITMAP       1UL

/* 普通图片资源，例如首页背景、水印 */
#define RESOURCE_TYPE_IMAGE             2UL

/* 开机动画使用的图片资源 */
#define RESOURCE_TYPE_BOOT_IMAGE        3UL

/* 其他二进制资源 */
#define RESOURCE_TYPE_BINARY            4UL


/* ============================================================
 * 三、资源编码类型
 *
 * 编码类型表示资源数据的实际存储方式。
 *
 * 注意：定义编码编号并不代表驱动已经支持解码。
 * 后续 Resource Manager 必须显式检查支持情况。
 * ============================================================ */

/* 原始字节数据，例如字体 glyph_bitmap */
#define RESOURCE_ENCODING_RAW           0UL

/* 16 bit RGB565 图片 */
#define RESOURCE_ENCODING_RGB565        1UL

/* 8 bit Alpha 灰度图片 */
#define RESOURCE_ENCODING_A8            2UL

/* JPEG 图片，预留给后续支持的解码器 */
#define RESOURCE_ENCODING_JPEG          3UL


/* ============================================================
 * 四、Resource Package Header
 *
 * 固定长度：128 Byte
 *
 * 保存位置：
 * Resource A/B 分区起始地址。
 *
 * 主要作用：
 * - 识别资源包是否符合 BikeComputer 格式。
 * - 判断资源包与当前 STM32 固件是否兼容。
 * - 定位资源索引和实际资源数据。
 * - 提供完整性校验信息。
 * ============================================================ */

typedef struct
{
    uint32_t magic;
    /* 资源包魔数，必须为 RESOURCE_PACKAGE_MAGIC */

    uint32_t format_version;
    /* 二进制格式版本，当前固定为 1 */

    uint32_t header_size;
    /* 头部字节数，当前固定为 128 */

    uint32_t package_version;
    /* 资源包发布版本，采用递增整数 */

    uint32_t ui_abi_version;
    /* UI 资源接口版本，须与固件要求一致 */

    uint32_t min_firmware_version;
    /* 允许使用本资源包的最低固件版本 */

    uint32_t max_firmware_version;
    /* 允许使用本资源包的最高固件版本 */

    uint32_t package_size;
    /* 整个资源包长度，包含头部、索引和数据 */

    uint32_t index_offset;
    /* 资源索引区相对于资源包起点的偏移 */

    uint32_t index_count;
    /* 资源索引条目数量 */

    uint32_t index_entry_size;
    /* 单个索引条目大小，当前为 48 Byte */

    uint32_t data_offset;
    /* 实际资源数据区起始偏移 */

    uint32_t flags;
    /* 当前版本必须为 0，保留扩展 */

    uint8_t payload_sha256[32];
    /*
     * SHA-256 摘要。
     *
     * 校验范围：
     * 从 index_offset 开始，
     * 到 package_size 结束。
     *
     * 也就是资源索引区、对齐填充及资源数据。
     *
     * SHA-256 用于完整性验证。
     * 真实性验证仍需后续 OTA 数字签名机制。
     */

    uint32_t header_crc32;
    /*
     * 头部 CRC32。
     *
     * 计算范围：完整 128 Byte Header。
     * 计算时将 header_crc32 字段暂时置为 0。
     *
     * 算法：CRC-32/ISO-HDLC
     * Polynomial：0x04C11DB7
     * Init：0xFFFFFFFF
     * RefIn/RefOut：true
     * XorOut：0xFFFFFFFF
     */

    uint32_t reserved[10];
    /* 保留字段。当前全部填 0 */

} resource_package_header_t;


/* ============================================================
 * 五、Resource Index Entry
 *
 * 每条索引固定长度：48 Byte
 *
 * 一个资源对应一条索引。
 *
 * 例如：
 * ID 1001：MiSans UI20 字形位图
 * ID 1002：MiSans UI24 字形位图
 * ID 2001：开机动画图片
 *
 * 上述 ID 只是说明示例，实际编号将在资源清单中统一定义。
 * ============================================================ */

typedef struct
{
    uint32_t resource_id;
    /* 全局唯一资源 ID */

    uint32_t resource_type;
    /* 资源类型，RESOURCE_TYPE_xxx */

    uint32_t encoding;
    /* 数据编码，RESOURCE_ENCODING_xxx */

    uint32_t flags;
    /* 当前固定为 0 */

    uint32_t data_offset;
    /*
     * 此资源数据相对于整个资源包起点的偏移。
     *
     * 注意：
     * 不是 QSPI 绝对地址。
     */

    uint32_t stored_size;
    /* 实际在 QSPI 中保存的字节长度 */

    uint32_t original_size;
    /* 解码后的字节长度；无压缩时等于 stored_size */

    uint32_t data_crc32;
    /* 对当前资源实际存储字节计算的 CRC32 */

    uint32_t metadata0;
    /* 图片可用作宽度；字体暂时为 0 */

    uint32_t metadata1;
    /* 图片可用作高度；字体暂时为 0 */

    uint32_t metadata2;
    /* 图片可用作行跨度；字体暂时为 0 */

    uint32_t reserved;
    /* 保留字段，当前为 0 */

} resource_package_index_t;


/* ============================================================
 * 六、编译期结构体尺寸检查
 *
 * 固件、PC 打包工具和 Bootloader 必须使用一致的布局。
 *
 * 检查失败时直接产生编译错误，禁止生成不兼容数据。
 * ============================================================ */

typedef char resource_header_size_check_t[
    (sizeof(resource_package_header_t) ==
     RESOURCE_PACKAGE_HEADER_SIZE) ? 1 : -1
];

typedef char resource_index_size_check_t[
    (sizeof(resource_package_index_t) ==
     RESOURCE_PACKAGE_INDEX_SIZE) ? 1 : -1
];

typedef char resource_header_crc_offset_check_t[
    (offsetof(resource_package_header_t, header_crc32) ==
     84UL) ? 1 : -1
];


/* ============================================================
 * 七、资源分区一致性检查
 *
 * Resource A 和 Resource B 必须大小一致。
 * 资源包最大容量不能超过单个分区。
 * ============================================================ */

#if QSPI_RESOURCE_A_SIZE != QSPI_RESOURCE_B_SIZE
#error "Resource A/B partition sizes must match"
#endif

#if RESOURCE_PACKAGE_HEADER_SIZE >= QSPI_RESOURCE_A_SIZE
#error "Resource header exceeds partition size"
#endif

#endif /* RESOURCE_PACKAGE_FORMAT_H */