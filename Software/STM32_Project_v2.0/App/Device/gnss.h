/**
 * @file    gnss.h
 * @brief   PU-252 GNSS NMEA 0183 解析接口（纯 C，不依赖 HAL/RTOS）
 * @note    只由 GNSSTask 写入解析器，其他任务通过 GnssService 取得快照。
 */
#ifndef GNSS_H
#define GNSS_H
#include <stdbool.h>
#include <stdint.h>

#define GNSS_NMEA_LINE_CAPACITY  128U  // 带校验和的 NMEA 语句最大接收长度
#define GNSS_FIX_TIMEOUT_MS     3000U // 连续 3 秒没有有效 RMC，则定位标记失效

typedef struct
{
    uint32_t updated_ms;             // 最近一次有效 RMC 的接收时刻，单位 ms
    uint32_t altitude_updated_ms;    // 最近一次有效 GGA 高度的接收时刻，单位 ms
    int32_t latitude_e7;             // 纬度：1e-7 度，南纬为负
    int32_t longitude_e7;            // 经度：1e-7 度，西经为负
    uint32_t speed_centi_kmh;        // 速度：0.01 km/h
    uint16_t course_centi_deg;       // 对地航向：0.01 度
    int32_t altitude_cm;             // 海拔：厘米（GGA 的 MSL 高度）
    uint16_t hdop_centi;            // 水平精度因子：0.01
    uint8_t satellites;             // GGA 报告的参与定位卫星数
    uint8_t fix_quality;            // GGA 定位质量（0 = 无效）
    uint8_t hour, minute, second;   // UTC 时分秒
    uint8_t day, month;             // UTC 日期
    uint16_t year;                  // UTC 年份（00..99 按 2000..2099）
    bool fix_valid;                 // 有效且未过期的 RMC 定位
    bool altitude_valid;            // 有效且未过期的 GGA 海拔
    bool time_valid;                // RMC 提供有效 UTC 日期和时刻
} gnss_snapshot_t;

typedef struct
{
    char line[GNSS_NMEA_LINE_CAPACITY]; // 串口数据被切片时，用于跨批次拼接语句
    uint16_t used;                     // 当前语句已保存字符数
    bool collecting;                   // 是否已经遇到起始符 '$'
    gnss_snapshot_t snapshot;          // 当前解析得到的最新状态
    uint32_t sentences_ok;             // 校验正确并识别的 RMC/GGA 数
    uint32_t checksum_errors;          // 校验不通过的 NMEA 语句数
    uint32_t format_errors;            // 校验正确但字段非法的 RMC/GGA 数
    uint32_t line_overflows;           // 语句超出接收容量的次数
} gnss_decoder_t;

void GNSS_Decoder_Init(gnss_decoder_t *decoder);  // 初始化解析器
void GNSS_Decoder_Feed(gnss_decoder_t *decoder, const uint8_t *data,
                       uint32_t length, uint32_t now_ms); // 输入任意长度字节流
void GNSS_Decoder_Tick(gnss_decoder_t *decoder, uint32_t now_ms); // 清除过期定位
void GNSS_Decoder_Invalidate(gnss_decoder_t *decoder); // DMA 故障时清除定位有效性
void GNSS_Decoder_GetSnapshot(const gnss_decoder_t *decoder,
                              gnss_snapshot_t *out);  // 任务本地获取解析结果
#endif
