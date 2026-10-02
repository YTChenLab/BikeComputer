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
    UI_RIDE_HISTORY_CORRUPT,
    UI_RIDE_HISTORY_PENDING
} ui_ride_history_result_t;

typedef ui_ride_history_result_t (*ui_ride_history_load_cb_t)(
    ui_ride_record_t *records, size_t capacity, size_t *count,
    void *context);
typedef ui_ride_history_result_t (*ui_ride_history_store_cb_t)(
    const ui_ride_record_t *records, size_t count, void *context);
typedef int64_t (*ui_ride_history_now_cb_t)(void *context);

typedef struct {
    ui_ride_history_totals_t totals;
    uint64_t last_committed_id;
    ui_ride_record_t pending;
    bool pending_valid;
    bool pending_is_new;
} ui_ride_history_checkpoint_t;

typedef ui_ride_history_result_t (*ui_ride_history_checkpoint_load_cb_t)(
    ui_ride_history_checkpoint_t *checkpoint, void *context);
typedef ui_ride_history_result_t (*ui_ride_history_checkpoint_store_cb_t)(
    const ui_ride_history_checkpoint_t *checkpoint, void *context);
typedef ui_ride_history_result_t (*ui_ride_history_page_cb_t)(
    size_t newest_offset, ui_ride_record_t *records, size_t capacity,
    size_t *count, void *context);

typedef struct
{
    ui_ride_history_load_cb_t load;
    ui_ride_history_store_cb_t store;
    ui_ride_history_now_cb_t now_utc;
    void *context;
    /* Durable write-ahead checkpoint, including lifetime totals. store must
     * return OK only after a recoverable durable commit. Optional for legacy
     * adapters; real board adapters should implement both callbacks. */
    ui_ride_history_checkpoint_load_cb_t load_checkpoint;
    ui_ride_history_checkpoint_store_cb_t store_checkpoint;
    ui_ride_history_page_cb_t load_page;
    /* Idempotent archive append. With this callback, load returns the newest
     * RAM window and older records remain accessible through load_page. */
    ui_ride_history_result_t (*append_record)(const ui_ride_record_t *record,
                                            void *context);
} ui_ride_history_storage_t;

#define UI_RIDE_HISTORY_PAGE_MAX 10U
/* Setup only. Callback enqueues a page job; it must not perform I/O on UI. */
bool ui_ride_history_repository_register_page_worker(bool (*request)(void *), void *context);
/* UI-thread request/cache read. One shared job serializes archive reads/writes. */
ui_ride_history_result_t ui_ride_history_repository_request_page(size_t newest_offset,
    ui_ride_record_t *records, size_t capacity, size_t *count);
/* Storage task; UI task subsequently publishes the page cache. */
ui_ride_history_result_t ui_ride_history_repository_page_worker(void);
bool ui_ride_history_repository_process_page(void);
void ui_ride_history_repository_init(void);
/* UI-thread clock supplied by the registered platform backend; zero means unknown. */
int64_t ui_ride_history_repository_now_utc(void);
bool ui_ride_history_repository_register_storage(
    const ui_ride_history_storage_t *storage);
ui_ride_history_result_t ui_ride_history_repository_load(void);
ui_ride_history_result_t ui_ride_history_repository_prepare(
    const ui_ride_summary_t *summary);
/* UI-thread: stage without filesystem I/O, then freeze a transaction. */
ui_ride_history_result_t ui_ride_history_repository_stage(const ui_ride_summary_t *summary);
bool ui_ride_history_repository_begin_commit(void);
bool ui_ride_history_repository_abort_commit(void);
uint32_t ui_ride_history_repository_commits(void);
/* One storage worker: performs blocking I/O, never calls LVGL or model observers. */
ui_ride_history_result_t ui_ride_history_repository_commit_worker(void);
/* UI-thread: publishes a completed transaction to the history/pending models. */
bool ui_ride_history_repository_process(ui_ride_history_result_t *result);
bool ui_ride_history_repository_busy(void);
/* Startup/test-only synchronous convenience; not for an animated UI callback. */
ui_ride_history_result_t ui_ride_history_repository_commit_pending(void);
bool ui_ride_history_repository_has_pending(void);
bool ui_ride_history_repository_get_pending(ui_ride_record_t *record);
bool ui_ride_history_repository_recovery_supported(void);
ui_ride_history_result_t ui_ride_history_repository_load_page(
    size_t newest_offset, ui_ride_record_t *records, size_t capacity,
    size_t *count);
void ui_ride_history_repository_clear_pending(void);
ui_ride_history_result_t ui_ride_history_repository_last_result(void);

#endif /* UI_RIDE_HISTORY_REPOSITORY_H */
