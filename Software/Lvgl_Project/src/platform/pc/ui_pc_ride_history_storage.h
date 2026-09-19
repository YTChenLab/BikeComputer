#ifndef UI_PC_RIDE_HISTORY_STORAGE_H
#define UI_PC_RIDE_HISTORY_STORAGE_H

#include "services/ui_ride_history_repository.h"

#include <stdbool.h>

#define UI_PC_RIDE_HISTORY_PATH_MAX 512U

typedef struct
{
    char path[UI_PC_RIDE_HISTORY_PATH_MAX];
} ui_pc_ride_history_storage_context_t;

bool ui_pc_ride_history_storage_init(
    ui_pc_ride_history_storage_context_t *context, const char *path);
ui_ride_history_storage_t ui_pc_ride_history_storage_backend(
    ui_pc_ride_history_storage_context_t *context);

#endif /* UI_PC_RIDE_HISTORY_STORAGE_H */
