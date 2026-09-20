#include "platform/pc/ui_pc_ride_history_storage.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define TEST_RMDIR(path) _rmdir(path)
#else
#include <unistd.h>
#define TEST_RMDIR(path) rmdir(path)
#endif

#define TEST_DIR  "test-runtime"
#define TEST_PATH TEST_DIR "/ride_history_test.dat"

static void put_u16(unsigned char *data, uint16_t value)
{
    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8U);
}

static void put_u32(unsigned char *data, uint32_t value)
{
    for(uint8_t index = 0U; index < 4U; index++)
        data[index] = (unsigned char)(value >> (index * 8U));
}

static void put_u64(unsigned char *data, uint64_t value)
{
    for(uint8_t index = 0U; index < 8U; index++)
        data[index] = (unsigned char)(value >> (index * 8U));
}

static void put_float(unsigned char *data, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    put_u32(data, bits);
}

static uint32_t crc32_update(uint32_t crc, const unsigned char *data,
                             size_t size)
{
    crc = ~crc;
    for(size_t index = 0U; index < size; index++) {
        crc ^= data[index];
        for(uint8_t bit = 0U; bit < 8U; bit++)
            crc = (crc >> 1U) ^
                  (UINT32_C(0xEDB88320) &
                   (uint32_t)-(int32_t)(crc & 1U));
    }
    return ~crc;
}

static void write_v1_file(const ui_ride_record_t *value)
{
    unsigned char header[24] = {0};
    unsigned char payload[50] = {0};
    memcpy(header, "BKRIDE1\0", 8U);
    put_u16(header + 8U, 1U);
    put_u16(header + 10U, 24U);
    put_u32(header + 12U, 1U);
    put_u32(header + 16U, sizeof(payload));
    put_u64(payload + 0U, value->id);
    put_u64(payload + 8U, (uint64_t)value->started_at_utc);
    put_u64(payload + 16U, (uint64_t)value->finished_at_utc);
    put_float(payload + 24U, value->distance_km);
    put_u32(payload + 28U, value->duration_seconds);
    put_float(payload + 32U, value->average_speed_kmh);
    put_float(payload + 36U, value->maximum_speed_kmh);
    put_u32(payload + 40U, value->ascent_m);
    put_u16(payload + 44U, value->average_heart_rate);
    put_u16(payload + 46U, value->average_power);
    put_u16(payload + 48U, value->average_cadence);
    put_u32(header + 20U, crc32_update(0U, payload, sizeof(payload)));
    FILE *file = fopen(TEST_PATH, "wb");
    assert(file != NULL);
    assert(fwrite(header, 1U, sizeof(header), file) == sizeof(header));
    assert(fwrite(payload, 1U, sizeof(payload), file) == sizeof(payload));
    assert(fclose(file) == 0);
}

static ui_ride_record_t record(uint64_t id, float distance)
{
    ui_ride_record_t value = {
        .id = id,
        .started_at_utc = 1750000000,
        .finished_at_utc = 1750003600,
        .distance_km = distance,
        .duration_seconds = 3600U,
        .moving_time_seconds = 3500U,
        .average_speed_kmh = distance,
        .maximum_speed_kmh = distance + 10.0f,
        .ascent_m = 321U,
        .descent_m = 300U,
        .maximum_elevation_m = 880,
        .calories_kcal = 720U,
        .has_heart_rate = true,
        .average_heart_rate = 143U,
        .maximum_heart_rate = 181U,
        .minimum_heart_rate = 96U,
        .has_power = true,
        .average_power = 205U,
        .maximum_power = 640U,
        .has_cadence = true,
        .average_cadence = 87U,
        .maximum_cadence = 113U,
        .has_temperature = true,
        .average_temperature_tenths_c = 218
    };
    value.trace.point_count = 3U;
    value.trace.speed_tenths[0] = 200U;
    value.trace.speed_tenths[1] = 310U;
    value.trace.speed_tenths[2] = 280U;
    value.trace.heart_rate_zone_seconds[2] = 1800U;
    return value;
}

static void cleanup(void)
{
    (void)remove(TEST_PATH);
    (void)remove(TEST_PATH ".tmp");
    (void)remove(TEST_PATH ".bak");
    (void)TEST_RMDIR(TEST_DIR);
}

static void write_byte(long offset, unsigned char value)
{
    FILE *file = fopen(TEST_PATH, "r+b");
    assert(file != NULL);
    assert(fseek(file, offset, SEEK_SET) == 0);
    assert(fwrite(&value, 1U, 1U, file) == 1U);
    assert(fclose(file) == 0);
}

static void copy_file(const char *source, const char *destination)
{
    FILE *input = fopen(source, "rb");
    FILE *output = fopen(destination, "wb");
    assert(input != NULL && output != NULL);
    unsigned char buffer[256];
    size_t size;
    while((size = fread(buffer, 1U, sizeof(buffer), input)) > 0U)
        assert(fwrite(buffer, 1U, size, output) == size);
    assert(!ferror(input));
    assert(fclose(input) == 0);
    assert(fclose(output) == 0);
}

int main(void)
{
    cleanup();
    ui_pc_ride_history_storage_context_t context;
    assert(ui_pc_ride_history_storage_init(&context, TEST_PATH));
    ui_ride_history_storage_t backend =
        ui_pc_ride_history_storage_backend(&context);
    ui_ride_record_t source[2] = {
        record(11U, 31.25f), record(12U, 44.75f)
    };
    assert(backend.store(source, 2U, backend.context) ==
           UI_RIDE_HISTORY_OK);

    ui_ride_record_t loaded[UI_RIDE_HISTORY_MAX_RECORDS];
    size_t count = 0U;
    assert(backend.load(loaded, UI_RIDE_HISTORY_MAX_RECORDS, &count,
                        backend.context) == UI_RIDE_HISTORY_OK);
    assert(count == 2U);
    assert(loaded[0].id == 11U && loaded[1].id == 12U);
    assert(loaded[1].distance_km == source[1].distance_km);
    assert(loaded[1].maximum_power == 640U);
    assert(loaded[1].trace.point_count == 3U);
    assert(loaded[1].trace.speed_tenths[1] == 310U);

    write_v1_file(&source[0]);
    assert(backend.load(loaded, UI_RIDE_HISTORY_MAX_RECORDS, &count,
                        backend.context) == UI_RIDE_HISTORY_OK);
    assert(count == 1U && loaded[0].id == source[0].id);
    assert(loaded[0].moving_time_seconds == source[0].duration_seconds);
    assert(loaded[0].has_heart_rate && loaded[0].has_power &&
           loaded[0].has_cadence);
    assert(loaded[0].trace.point_count == 0U);

    ui_ride_record_t replacement = record(13U, 18.5f);
    assert(backend.store(&replacement, 1U, backend.context) ==
           UI_RIDE_HISTORY_OK);
    assert(fopen(TEST_PATH ".tmp", "rb") == NULL);
    assert(fopen(TEST_PATH ".bak", "rb") == NULL);
    assert(backend.load(loaded, UI_RIDE_HISTORY_MAX_RECORDS, &count,
                        backend.context) == UI_RIDE_HISTORY_OK);
    assert(count == 1U && loaded[0].id == 13U);

    assert(rename(TEST_PATH, TEST_PATH ".bak") == 0);
    assert(backend.load(loaded, UI_RIDE_HISTORY_MAX_RECORDS, &count,
                        backend.context) == UI_RIDE_HISTORY_OK);
    assert(count == 1U && loaded[0].id == 13U);
    assert(fopen(TEST_PATH ".bak", "rb") == NULL);

    copy_file(TEST_PATH, TEST_PATH ".bak");
    write_byte(24L, 0xFFU);
    assert(backend.load(loaded, UI_RIDE_HISTORY_MAX_RECORDS, &count,
                        backend.context) == UI_RIDE_HISTORY_OK);
    assert(count == 1U && loaded[0].id == 13U);
    assert(fopen(TEST_PATH ".bak", "rb") == NULL);

    write_byte(8L, 0x7FU);
    assert(backend.load(loaded, UI_RIDE_HISTORY_MAX_RECORDS, &count,
                        backend.context) ==
           UI_RIDE_HISTORY_UNSUPPORTED_VERSION);

    assert(backend.store(source, 2U, backend.context) ==
           UI_RIDE_HISTORY_OK);
    write_byte(24L, 0xFFU);
    assert(backend.load(loaded, UI_RIDE_HISTORY_MAX_RECORDS, &count,
                        backend.context) == UI_RIDE_HISTORY_CORRUPT);

    FILE *file = fopen(TEST_PATH, "wb");
    assert(file != NULL);
    assert(fwrite("short", 1U, 5U, file) == 5U);
    assert(fclose(file) == 0);
    assert(backend.load(loaded, UI_RIDE_HISTORY_MAX_RECORDS, &count,
                        backend.context) == UI_RIDE_HISTORY_CORRUPT);

    cleanup();
    puts("ui_pc_ride_history_storage_test: PASS");
    return 0;
}
