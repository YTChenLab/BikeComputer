/**
 * @file    gnss.c
 * @brief   PU-252 GNSS 标准 NMEA 0183 RMC/GGA 解析
 *
 * 数据通路：bsp_gnss 的 UART DMA -> GNSSTask -> 本解析器 -> GnssService。
 * 仅处理真实校验通过的数据；不调用 malloc、HAL、FreeRTOS 和 LVGL。
 */
#include "gnss.h"
#include <stddef.h>             // NULL
#include <string.h>             // memset

/* 将十六进制 ASCII 字符转换为 0..15；非法返回 -1。 */
static int HexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* 安全读取十进制无符号整数；空字段及非数字一律拒绝。 */
static bool ParseUint(const char *s, uint32_t limit, uint32_t *out)
{
    uint32_t v = 0U;
    if (s == NULL || *s == '\0' || out == NULL) return false;
    while (*s != '\0')
    {
        uint32_t digit;
        if (*s < '0' || *s > '9') return false;
        digit = (uint32_t)(*s++ - '0');
        if (v > (limit - digit) / 10U || digit > limit) return false;
        v = v * 10U + digit;
    }
    *out = v;
    return true;
}

/*
 * 将十进制数字转换为指定小数位数的定点整数。
 * 例如："12.345"、decimals=2 -> 1234（多出的尾数直接截断）。
 * 支持负数；采用 int64_t 中间值避免计算溢出。
 */
static bool ParseFixed(const char *s, uint8_t decimals, int32_t *out)
{
    int64_t whole = 0, fraction = 0, scale = 1;
    bool negative = false, any = false, dot = false;
    uint8_t digits = 0U;
    if (s == NULL || out == NULL || decimals > 6U) return false;
    if (*s == '-' || *s == '+') negative = (*s++ == '-');
    for (uint8_t i = 0U; i < decimals; ++i) scale *= 10;
    while (*s != '\0')
    {
        if (*s == '.' && !dot) { dot = true; ++s; continue; }
        if (*s < '0' || *s > '9') return false;
        any = true;
        if (!dot)
        {
            whole = whole * 10 + (int64_t)(*s - '0');
            if (whole > 2147483647LL) return false;
        }
        else if (digits < decimals)
        {
            fraction = fraction * 10 + (int64_t)(*s - '0');
            ++digits;
        }
        ++s;
    }
    if (!any) return false;
    while (digits++ < decimals) fraction *= 10;
    whole = whole * scale + fraction;
    if (negative) whole = -whole;
    if (whole < -2147483647LL - 1LL || whole > 2147483647LL) return false;
    *out = (int32_t)whole;
    return true;
}

/*
 * 将 ddmm.mmmmm / dddmm.mmmmm 转成 1e-7 度。
 * 小数部分按 7 位补零，之后把“分”除以 60 换算为“度”。
 */
static bool ParseCoordinate(const char *s, char hemisphere, bool latitude,
                            int32_t *out)
{
    uint32_t deg = 0U, minute = 0U, fraction = 0U, scale = 1000000U;
    uint8_t deg_digits = latitude ? 2U : 3U;
    int64_t result;
    if (s == NULL || out == NULL) return false;
    if (strlen(s) < (size_t)deg_digits + 2U) return false;
    for (uint8_t i = 0U; i < deg_digits; ++i)
    {
        if (*s < '0' || *s > '9') return false;
        deg = deg * 10U + (uint32_t)(*s++ - '0');
    }
    for (uint8_t i = 0U; i < 2U; ++i)
    {
        if (*s < '0' || *s > '9') return false;
        minute = minute * 10U + (uint32_t)(*s++ - '0');
    }
    if (*s == '.')
    {
        ++s;
        while (*s != '\0')
        {
            if (*s < '0' || *s > '9') return false;
            if (scale > 0U) { fraction += (uint32_t)(*s - '0') * scale; scale /= 10U; }
            ++s;
        }
    }
    else if (*s != '\0') return false;
    if (minute >= 60U || deg > (latitude ? 90U : 180U)) return false;
    if (deg == (latitude ? 90U : 180U) && (minute != 0U || fraction != 0U)) return false;
    if ((latitude && hemisphere != 'N' && hemisphere != 'S') ||
        (!latitude && hemisphere != 'E' && hemisphere != 'W')) return false;
    result = (int64_t)deg * 10000000LL +
             ((int64_t)minute * 10000000LL + (int64_t)fraction + 30LL) / 60LL;
    if (hemisphere == 'S' || hemisphere == 'W') result = -result;
    *out = (int32_t)result;
    return true;
}

/* 验证 hhmmss[.sss]；日期另由 RMC 的 ddmmyy 字段提供。 */
static bool ParseUtc(const char *s, uint8_t *h, uint8_t *m, uint8_t *sec)
{
    uint32_t hh, mm, ss;
    if (s == NULL || h == NULL || m == NULL || sec == NULL) return false;
    if (strlen(s) < 6U) return false;
    for (uint8_t i = 0U; i < 6U; ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    if (s[6] != '\0' && s[6] != '.') return false;
    hh = (uint32_t)(s[0]-'0')*10U + (uint32_t)(s[1]-'0');
    mm = (uint32_t)(s[2]-'0')*10U + (uint32_t)(s[3]-'0');
    ss = (uint32_t)(s[4]-'0')*10U + (uint32_t)(s[5]-'0');
    if (hh >= 24U || mm >= 60U || ss > 60U) return false;
    for (const char *p = s + 6; *p != '\0'; ++p)
        if (*p != '.' && (*p < '0' || *p > '9')) return false;
    *h = (uint8_t)hh; *m = (uint8_t)mm; *sec = (uint8_t)ss;
    return true;
}

/* 用 RMC 更新位置、速度、航向和 UTC；A 才表示有效定位。 */
static bool ParseRmc(char *const fields[], uint8_t count,
                     gnss_snapshot_t *fix, uint32_t now_ms)
{
    gnss_snapshot_t tmp = *fix;
    int32_t knots_milli = 0, heading_centi = 0;
    uint32_t day, month, year;
    const char *date;
    if (count < 10U || fields[2][0] == '\0') return false;
    if (fields[2][0] == 'V')
    {
        fix->fix_valid = false;   // 接收到了明确的无效定位，不继续使用旧坐标
        fix->time_valid = false;
        return true;
    }
    if (fields[2][0] != 'A' || fields[2][1] != '\0') return false;
    if (!ParseUtc(fields[1], &tmp.hour, &tmp.minute, &tmp.second) ||
        !ParseCoordinate(fields[3], fields[4][0], true, &tmp.latitude_e7) ||
        !ParseCoordinate(fields[5], fields[6][0], false, &tmp.longitude_e7)) return false;
    if (fields[7][0] != '\0' && !ParseFixed(fields[7], 3U, &knots_milli)) return false;
    if (fields[8][0] != '\0' && !ParseFixed(fields[8], 2U, &heading_centi)) return false;
    if (knots_milli < 0 || knots_milli > 200000 ||
        heading_centi < 0 || heading_centi > 36000) return false;
    tmp.speed_centi_kmh = ((uint32_t)knots_milli * 1852U + 5000U) / 10000U;
    if (tmp.speed_centi_kmh > 30000U) return false; // 大于 300 km/h：不符合骑行目标场景
    tmp.course_centi_deg = (uint16_t)heading_centi;
    date = fields[9];
    if (strlen(date) != 6U) return false;
    for (uint8_t i = 0U; i < 6U; ++i)
        if (date[i] < '0' || date[i] > '9') return false;
    day = (uint32_t)(date[0]-'0')*10U + (uint32_t)(date[1]-'0');
    month = (uint32_t)(date[2]-'0')*10U + (uint32_t)(date[3]-'0');
    year = (uint32_t)(date[4]-'0')*10U + (uint32_t)(date[5]-'0');
    if (day == 0U || day > 31U || month == 0U || month > 12U) return false;
    tmp.day = (uint8_t)day; tmp.month = (uint8_t)month;
    tmp.year = (uint16_t)(2000U + year);
    tmp.updated_ms = now_ms;
    tmp.time_valid = true;
    tmp.fix_valid = true;
    *fix = tmp;                  // 全部字段验证成功后才提交完整结果
    return true;
}

/* 用 GGA 补充卫星数量、定位质量、HDOP 和海拔。 */
static bool ParseGga(char *const fields[], uint8_t count,
                     gnss_snapshot_t *fix, uint32_t now_ms)
{
    gnss_snapshot_t tmp = *fix;
    uint32_t quality, sats;
    int32_t hdop, altitude;
    if (count < 11U || !ParseUint(fields[6], 8U, &quality) ||
        !ParseUint(fields[7], 99U, &sats)) return false;
    tmp.fix_quality = (uint8_t)quality;
    tmp.satellites = (uint8_t)sats;
    if (quality == 0U)
    {
        tmp.altitude_valid = false;  // 无有效 GGA 高度时立刻失效
        *fix = tmp;
        return true;
    }
    if (!ParseFixed(fields[8], 2U, &hdop) || hdop < 0 || hdop > 65535 ||
        !ParseFixed(fields[9], 2U, &altitude) || fields[10][0] != 'M') return false;
    tmp.hdop_centi = (uint16_t)hdop;
    tmp.altitude_cm = altitude;
    tmp.altitude_updated_ms = now_ms;
    tmp.altitude_valid = true;
    *fix = tmp;
    return true;
}

/* 验证 NMEA XOR 校验和，拆分字段，识别 RMC / GGA。 */
static void ProcessLine(gnss_decoder_t *decoder, uint32_t now_ms)
{
    char *fields[20];
    char *star, *p;
    uint8_t xor_value = 0U, count = 0U;
    bool parsed = false;
    int hi, lo;
    if (decoder->used < 7U || decoder->line[0] != '$') return;
    decoder->line[decoder->used] = '\0';
    star = NULL;
    for (p = decoder->line + 1; *p != '\0'; ++p)
    {
        if (*p == '*') { star = p; break; }
        xor_value ^= (uint8_t)*p;
    }
    if (star == NULL || star[1] == '\0' || star[2] == '\0' || star[3] != '\0' ||
        (hi = HexDigit(star[1])) < 0 || (lo = HexDigit(star[2])) < 0 ||
        xor_value != (uint8_t)((hi << 4) | lo))
    {
        decoder->checksum_errors++;
        return;
    }
    *star = '\0';
    fields[count++] = decoder->line + 1;
    for (p = fields[0]; *p != '\0'; ++p)
    {
        if (*p == ',')
        {
            *p = '\0';
            if (count >= 20U) { decoder->format_errors++; return; }
            fields[count++] = p + 1;
        }
    }
    if (strlen(fields[0]) != 5U) return;  // 只识别标准两字符 talker + 三字符类型
    if (strcmp(fields[0] + 2, "RMC") == 0)
        parsed = ParseRmc(fields, count, &decoder->snapshot, now_ms);
    else if (strcmp(fields[0] + 2, "GGA") == 0)
        parsed = ParseGga(fields, count, &decoder->snapshot, now_ms);
    else return;                           // GSA/GSV 等后续需要时再扩展
    if (parsed) decoder->sentences_ok++;
    else decoder->format_errors++;
}

void GNSS_Decoder_Init(gnss_decoder_t *decoder)
{
    if (decoder != NULL) memset(decoder, 0, sizeof(*decoder));
}

void GNSS_Decoder_Feed(gnss_decoder_t *decoder, const uint8_t *data,
                       uint32_t length, uint32_t now_ms)
{
    if (decoder == NULL || (data == NULL && length != 0U)) return;
    for (uint32_t i = 0U; i < length; ++i)
    {
        char c = (char)data[i];
        if (c == '$') { decoder->collecting = true; decoder->used = 0U; }
        if (!decoder->collecting) continue;
        if (c == '\n')
        {
            ProcessLine(decoder, now_ms);
            decoder->collecting = false;
            decoder->used = 0U;
        }
        else if (c != '\r')
        {
            if (decoder->used + 1U >= GNSS_NMEA_LINE_CAPACITY)
            {
                decoder->line_overflows++;
                decoder->collecting = false;
                decoder->used = 0U;
            }
            else decoder->line[decoder->used++] = c;
        }
    }
}

void GNSS_Decoder_Tick(gnss_decoder_t *decoder, uint32_t now_ms)
{
    if (decoder == NULL) return;
    if (decoder->snapshot.fix_valid &&
        (uint32_t)(now_ms - decoder->snapshot.updated_ms) > GNSS_FIX_TIMEOUT_MS)
        decoder->snapshot.fix_valid = false;
    if (decoder->snapshot.altitude_valid &&
        (uint32_t)(now_ms - decoder->snapshot.altitude_updated_ms) > GNSS_FIX_TIMEOUT_MS)
        decoder->snapshot.altitude_valid = false;
}

void GNSS_Decoder_Invalidate(gnss_decoder_t *decoder)
{
    if (decoder == NULL) return;
    decoder->snapshot.fix_valid = false;      // UART DMA 故障后不保留旧定位有效性
    decoder->snapshot.altitude_valid = false;
    decoder->collecting = false;             // 丢弃可能被截断的 NMEA 语句
    decoder->used = 0U;
}

void GNSS_Decoder_GetSnapshot(const gnss_decoder_t *decoder,
                              gnss_snapshot_t *out)
{
    if (decoder != NULL && out != NULL) *out = decoder->snapshot;
}
