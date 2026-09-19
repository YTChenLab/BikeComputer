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

static ui_ride_record_t record(uint64_t id, float distance)
{
    return (ui_ride_record_t) {
        .id = id,
        .started_at_utc = 1750000000,
        .finished_at_utc = 1750003600,
        .distance_km = distance,
        .duration_seconds = 3600U,
        .average_speed_kmh = distance,
        .maximum_speed_kmh = distance + 10.0f,
        .ascent_m = 321U,
        .average_heart_rate = 143U,
        .average_power = 205U,
        .average_cadence = 87U
    };
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
