#include "ui_pc_ride_history_storage.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#define UI_MKDIR(path) _mkdir(path)
#define UI_FILE_COMMIT(file) _commit(_fileno(file))
#else
#include <sys/stat.h>
#include <unistd.h>
#define UI_MKDIR(path) mkdir(path, 0777)
#define UI_FILE_COMMIT(file) fsync(fileno(file))
#endif

#define HISTORY_HEADER_SIZE 24U
#define HISTORY_RECORD_V1_SIZE 50U
#define HISTORY_RECORD_V2_SIZE 1320U
#define HISTORY_MAGIC_SIZE    8U

static const uint8_t s_magic[HISTORY_MAGIC_SIZE] = {
    'B', 'K', 'R', 'I', 'D', 'E', '1', '\0'
};

static void put_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *data, uint32_t value)
{
    for(uint8_t index = 0U; index < 4U; index++)
        data[index] = (uint8_t)(value >> (index * 8U));
}

static void put_u64(uint8_t *data, uint64_t value)
{
    for(uint8_t index = 0U; index < 8U; index++)
        data[index] = (uint8_t)(value >> (index * 8U));
}

static uint16_t get_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static int16_t get_i16(const uint8_t *data)
{
    return (int16_t)get_u16(data);
}

static uint32_t get_u32(const uint8_t *data)
{
    uint32_t value = 0U;
    for(uint8_t index = 0U; index < 4U; index++)
        value |= (uint32_t)data[index] << (index * 8U);
    return value;
}

static uint64_t get_u64(const uint8_t *data)
{
    uint64_t value = 0U;
    for(uint8_t index = 0U; index < 8U; index++)
        value |= (uint64_t)data[index] << (index * 8U);
    return value;
}

static void put_float(uint8_t *data, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    put_u32(data, bits);
}

static float get_float(const uint8_t *data)
{
    uint32_t bits = get_u32(data);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size)
{
    crc = ~crc;
    for(size_t index = 0U; index < size; index++) {
        crc ^= data[index];
        for(uint8_t bit = 0U; bit < 8U; bit++)
            crc = (crc >> 1U) ^
                  (UINT32_C(0xEDB88320) & (uint32_t)-(int32_t)(crc & 1U));
    }
    return ~crc;
}

static void encode_record_v2(uint8_t *data,
                             const ui_ride_record_t *record)
{
    memset(data, 0, HISTORY_RECORD_V2_SIZE);
    put_u64(data + 0U, record->id);
    put_u64(data + 8U, (uint64_t)record->started_at_utc);
    put_u64(data + 16U, (uint64_t)record->finished_at_utc);
    put_float(data + 24U, record->distance_km);
    put_u32(data + 28U, record->duration_seconds);
    put_float(data + 32U, record->average_speed_kmh);
    put_float(data + 36U, record->maximum_speed_kmh);
    put_u32(data + 40U, record->ascent_m);
    put_u16(data + 44U, record->average_heart_rate);
    put_u16(data + 46U, record->average_power);
    put_u16(data + 48U, record->average_cadence);
    put_u32(data + 50U, record->moving_time_seconds);
    put_u32(data + 54U, record->descent_m);
    put_u32(data + 58U, (uint32_t)record->maximum_elevation_m);
    put_u32(data + 62U, record->calories_kcal);
    put_u16(data + 66U,
            (uint16_t)record->average_temperature_tenths_c);
    put_u16(data + 68U, record->maximum_heart_rate);
    put_u16(data + 70U, record->minimum_heart_rate);
    put_u16(data + 72U, record->maximum_power);
    put_u16(data + 74U, record->maximum_cadence);
    data[76U] = (uint8_t)((record->has_heart_rate ? 1U : 0U) |
                          (record->has_power ? 2U : 0U) |
                          (record->has_cadence ? 4U : 0U) |
                          (record->has_temperature ? 8U : 0U));
    put_u16(data + 78U, record->trace.point_count);
    size_t offset = 80U;
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        put_u16(data + offset, record->trace.speed_tenths[index]);
        offset += 2U;
    }
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        put_u16(data + offset, record->trace.heart_rate[index]);
        offset += 2U;
    }
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        put_u16(data + offset, record->trace.power[index]);
        offset += 2U;
    }
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        put_u16(data + offset, record->trace.cadence[index]);
        offset += 2U;
    }
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        put_u16(data + offset,
                (uint16_t)record->trace.elevation_m[index]);
        offset += 2U;
    }
    for(uint8_t index = 0U; index < UI_RIDE_REPORT_ZONE_COUNT; index++) {
        put_u32(data + offset,
                record->trace.heart_rate_zone_seconds[index]);
        offset += 4U;
    }
    for(uint8_t index = 0U; index < UI_RIDE_REPORT_ZONE_COUNT; index++) {
        put_u32(data + offset, record->trace.power_zone_seconds[index]);
        offset += 4U;
    }
}

static void decode_record_v1(ui_ride_record_t *record,
                             const uint8_t *data)
{
    *record = (ui_ride_record_t) {
        .id = get_u64(data + 0U),
        .started_at_utc = (int64_t)get_u64(data + 8U),
        .finished_at_utc = (int64_t)get_u64(data + 16U),
        .distance_km = get_float(data + 24U),
        .duration_seconds = get_u32(data + 28U),
        .average_speed_kmh = get_float(data + 32U),
        .maximum_speed_kmh = get_float(data + 36U),
        .ascent_m = get_u32(data + 40U),
        .average_heart_rate = get_u16(data + 44U),
        .average_power = get_u16(data + 46U),
        .average_cadence = get_u16(data + 48U)
    };
    record->moving_time_seconds = record->duration_seconds;
    record->has_heart_rate = record->average_heart_rate > 0U;
    record->has_power = record->average_power > 0U;
    record->has_cadence = record->average_cadence > 0U;
}

static void decode_record_v2(ui_ride_record_t *record,
                             const uint8_t *data)
{
    decode_record_v1(record, data);
    record->moving_time_seconds = get_u32(data + 50U);
    record->descent_m = get_u32(data + 54U);
    record->maximum_elevation_m = (int32_t)get_u32(data + 58U);
    record->calories_kcal = get_u32(data + 62U);
    record->average_temperature_tenths_c = get_i16(data + 66U);
    record->maximum_heart_rate = get_u16(data + 68U);
    record->minimum_heart_rate = get_u16(data + 70U);
    record->maximum_power = get_u16(data + 72U);
    record->maximum_cadence = get_u16(data + 74U);
    uint8_t flags = data[76U];
    record->has_heart_rate = (flags & 1U) != 0U;
    record->has_power = (flags & 2U) != 0U;
    record->has_cadence = (flags & 4U) != 0U;
    record->has_temperature = (flags & 8U) != 0U;
    record->trace.point_count = get_u16(data + 78U);
    size_t offset = 80U;
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        record->trace.speed_tenths[index] = get_u16(data + offset);
        offset += 2U;
    }
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        record->trace.heart_rate[index] = get_u16(data + offset);
        offset += 2U;
    }
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        record->trace.power[index] = get_u16(data + offset);
        offset += 2U;
    }
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        record->trace.cadence[index] = get_u16(data + offset);
        offset += 2U;
    }
    for(uint16_t index = 0U; index < UI_RIDE_REPORT_POINT_MAX; index++) {
        record->trace.elevation_m[index] = get_i16(data + offset);
        offset += 2U;
    }
    for(uint8_t index = 0U; index < UI_RIDE_REPORT_ZONE_COUNT; index++) {
        record->trace.heart_rate_zone_seconds[index] =
            get_u32(data + offset);
        offset += 4U;
    }
    for(uint8_t index = 0U; index < UI_RIDE_REPORT_ZONE_COUNT; index++) {
        record->trace.power_zone_seconds[index] = get_u32(data + offset);
        offset += 4U;
    }
}

static bool ensure_parent_directory(const char *path)
{
    char parent[UI_PC_RIDE_HISTORY_PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", path);
    char *slash = strrchr(parent, '/');
    char *backslash = strrchr(parent, '\\');
    char *separator = slash > backslash ? slash : backslash;
    if(separator == NULL) return true;
    *separator = '\0';
    if(parent[0] == '\0') return true;
    return UI_MKDIR(parent) == 0 || errno == EEXIST;
}

static ui_ride_history_result_t load_file(
    const char *path, ui_ride_record_t *records, size_t capacity,
    size_t *count)
{
    if(path == NULL || records == NULL || count == NULL)
        return UI_RIDE_HISTORY_INVALID_DATA;
    *count = 0U;
    FILE *file = fopen(path, "rb");
    if(file == NULL)
        return errno == ENOENT ? UI_RIDE_HISTORY_EMPTY :
                                 UI_RIDE_HISTORY_IO_ERROR;
    uint8_t header[HISTORY_HEADER_SIZE];
    if(fread(header, 1U, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return UI_RIDE_HISTORY_CORRUPT;
    }
    if(memcmp(header, s_magic, sizeof(s_magic)) != 0) {
        fclose(file);
        return UI_RIDE_HISTORY_CORRUPT;
    }
    uint16_t version = get_u16(header + 8U);
    if(version != 1U && version != UI_RIDE_RECORD_FORMAT_VERSION) {
        fclose(file);
        return UI_RIDE_HISTORY_UNSUPPORTED_VERSION;
    }
    uint16_t header_size = get_u16(header + 10U);
    uint32_t record_count = get_u32(header + 12U);
    uint32_t payload_size = get_u32(header + 16U);
    uint32_t expected_crc = get_u32(header + 20U);
    uint32_t record_size = version == 1U ? HISTORY_RECORD_V1_SIZE :
                                           HISTORY_RECORD_V2_SIZE;
    if(header_size != HISTORY_HEADER_SIZE || record_count > capacity ||
       record_count > UI_RIDE_HISTORY_MAX_RECORDS ||
       payload_size != record_count * record_size) {
        fclose(file);
        return UI_RIDE_HISTORY_CORRUPT;
    }
    uint32_t crc = 0U;
    uint8_t encoded[HISTORY_RECORD_V2_SIZE];
    for(uint32_t index = 0U; index < record_count; index++) {
        if(fread(encoded, 1U, record_size, file) != record_size) {
            fclose(file);
            return UI_RIDE_HISTORY_CORRUPT;
        }
        crc = crc32_update(crc, encoded, record_size);
        if(version == 1U) decode_record_v1(&records[index], encoded);
        else decode_record_v2(&records[index], encoded);
    }
    if(fgetc(file) != EOF || ferror(file) || crc != expected_crc) {
        fclose(file);
        return UI_RIDE_HISTORY_CORRUPT;
    }
    fclose(file);
    *count = record_count;
    return UI_RIDE_HISTORY_OK;
}

static ui_ride_history_result_t load_records(
    ui_ride_record_t *records, size_t capacity, size_t *count,
    void *context)
{
    ui_pc_ride_history_storage_context_t *storage = context;
    if(storage == NULL || records == NULL || count == NULL)
        return UI_RIDE_HISTORY_INVALID_DATA;

    ui_ride_history_result_t primary = load_file(
        storage->path, records, capacity, count);
    if(primary == UI_RIDE_HISTORY_OK ||
       primary == UI_RIDE_HISTORY_UNSUPPORTED_VERSION)
        return primary;

    char backup[UI_PC_RIDE_HISTORY_PATH_MAX + 8U];
    snprintf(backup, sizeof(backup), "%s.bak", storage->path);
    ui_ride_history_result_t recovered = load_file(
        backup, records, capacity, count);
    if(recovered != UI_RIDE_HISTORY_OK) return primary;

    /* A validated backup is authoritative after an interrupted replacement.
     * Restoration is best-effort: the in-memory recovery still succeeds if
     * the filesystem refuses the rename, and the next save can repair it. */
    (void)remove(storage->path);
    (void)rename(backup, storage->path);
    return UI_RIDE_HISTORY_OK;
}

static bool replace_file(const char *temporary, const char *destination)
{
    char backup[UI_PC_RIDE_HISTORY_PATH_MAX + 8U];
    snprintf(backup, sizeof(backup), "%s.bak", destination);
    (void)remove(backup);
    bool had_original = rename(destination, backup) == 0;
    if(rename(temporary, destination) != 0) {
        if(had_original) (void)rename(backup, destination);
        return false;
    }
    if(had_original) (void)remove(backup);
    return true;
}

static ui_ride_history_result_t store_records(
    const ui_ride_record_t *records, size_t count, void *context)
{
    ui_pc_ride_history_storage_context_t *storage = context;
    if(storage == NULL || count > UI_RIDE_HISTORY_MAX_RECORDS ||
       (count > 0U && records == NULL))
        return UI_RIDE_HISTORY_INVALID_DATA;
    if(!ensure_parent_directory(storage->path))
        return UI_RIDE_HISTORY_IO_ERROR;

    char temporary[UI_PC_RIDE_HISTORY_PATH_MAX + 8U];
    snprintf(temporary, sizeof(temporary), "%s.tmp", storage->path);
    FILE *file = fopen(temporary, "wb");
    if(file == NULL) return UI_RIDE_HISTORY_IO_ERROR;

    uint8_t header[HISTORY_HEADER_SIZE] = {0};
    memcpy(header, s_magic, sizeof(s_magic));
    put_u16(header + 8U, UI_RIDE_RECORD_FORMAT_VERSION);
    put_u16(header + 10U, HISTORY_HEADER_SIZE);
    put_u32(header + 12U, (uint32_t)count);
    put_u32(header + 16U, (uint32_t)(count * HISTORY_RECORD_V2_SIZE));
    uint32_t crc = 0U;
    uint8_t encoded[HISTORY_RECORD_V2_SIZE];
    for(size_t index = 0U; index < count; index++) {
        encode_record_v2(encoded, &records[index]);
        crc = crc32_update(crc, encoded, sizeof(encoded));
    }
    put_u32(header + 20U, crc);

    bool success = fwrite(header, 1U, sizeof(header), file) == sizeof(header);
    for(size_t index = 0U; success && index < count; index++) {
        encode_record_v2(encoded, &records[index]);
        success = fwrite(encoded, 1U, sizeof(encoded), file) ==
                  sizeof(encoded);
    }
    success = success && fflush(file) == 0;
    success = success && UI_FILE_COMMIT(file) == 0;
    success = fclose(file) == 0 && success;
    if(!success) {
        (void)remove(temporary);
        return UI_RIDE_HISTORY_IO_ERROR;
    }
    if(!replace_file(temporary, storage->path)) {
        (void)remove(temporary);
        return UI_RIDE_HISTORY_IO_ERROR;
    }
    return UI_RIDE_HISTORY_OK;
}

static int64_t now_utc(void *context)
{
    (void)context;
    return (int64_t)time(NULL);
}

bool ui_pc_ride_history_storage_init(
    ui_pc_ride_history_storage_context_t *context, const char *path)
{
    if(context == NULL || path == NULL || path[0] == '\0' ||
       strlen(path) >= sizeof(context->path)) return false;
    snprintf(context->path, sizeof(context->path), "%s", path);
    return true;
}

ui_ride_history_storage_t ui_pc_ride_history_storage_backend(
    ui_pc_ride_history_storage_context_t *context)
{
    return (ui_ride_history_storage_t) {
        .load = load_records,
        .store = store_records,
        .now_utc = now_utc,
        .context = context
    };
}
