#ifndef UI_DEVICE_INFO_H
#define UI_DEVICE_INFO_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    char device_model[48], hardware_target[48], firmware_version[32], health[48];
    uint64_t storage_free_bytes;
    bool storage_ready;
} ui_device_info_t;
/* Task-safe mailbox. Text copied; display update occurs on the UI task. */
bool ui_device_info_submit(const ui_device_info_t *info);
bool ui_device_info_process(ui_device_info_t *info);
#endif
