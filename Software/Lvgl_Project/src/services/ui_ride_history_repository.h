#ifndef UI_RIDE_HISTORY_REPOSITORY_H
#define UI_RIDE_HISTORY_REPOSITORY_H

#include "ui/models/ui_ride_history_model.h"
#include "ui/models/ui_ride_summary_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    UI_RIDE_HISTORY_OK = 0,
    UI_RIDE_HISTORY_EMPTY,
    UI_RIDE_HISTORY_NOT_CONFIGURED,
    UI_RIDE_HISTORY_INVALID_DATA,
    UI_RIDE_HISTORY_IO_ERROR,
    UI_RIDE_HISTORY_UNSUPPORTED_VERSION,
    UI_RIDE_HISTORY_CORRUPT
} ui_ride_history_result_t;

typedef ui_ride_history_result_t (*ui_ride_history_load_cb_t)(
    ui_ride_record_t *records, size_t capacity, size_t *count,
    void *context);
typedef ui_ride_history_result_t (*ui_ride_history_store_cb_t)(
    const ui_ride_record_t *records, size_t count, void *context);
typedef int64_t (*ui_ride_history_now_cb_t)(void *context);

typedef struct
{
    ui_ride_history_load_cb_t load;
    ui_ride_history_store_cb_t store;
    ui_ride_history_now_cb_t now_utc;
    void *context;
} ui_ride_history_storage_t;

void ui_ride_history_repository_init(void);
bool ui_ride_history_repository_register_storage(
    const ui_ride_history_storage_t *storage);
ui_ride_history_result_t ui_ride_history_repository_load(void);
ui_ride_history_result_t ui_ride_history_repository_prepare(
    const ui_ride_summary_t *summary);
ui_ride_history_result_t ui_ride_history_repository_commit_pending(void);
bool ui_ride_history_repository_has_pending(void);
void ui_ride_history_repository_clear_pending(void);
ui_ride_history_result_t ui_ride_history_repository_last_result(void);

#endif /* UI_RIDE_HISTORY_REPOSITORY_H */
