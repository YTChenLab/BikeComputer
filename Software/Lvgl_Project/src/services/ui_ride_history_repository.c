#include "ui_ride_history_repository.h"
#include "ui/core/ui_memory.h"

#include <math.h>
#include <stdatomic.h>
#include <string.h>

typedef struct
{
    ui_ride_history_storage_t storage;
    ui_ride_record_t pending;
    ui_ride_history_result_t last_result;
    bool configured;
    bool pending_valid;
    ui_ride_history_checkpoint_t checkpoint;
} ui_ride_history_repository_context_t;

static UI_LARGE_RAM ui_ride_history_repository_context_t s_repository;
/* Controlled workspace, never a 100-record task-stack allocation. Boards may
 * place it and the history model in external RAM through the linker. */
static UI_LARGE_RAM ui_ride_record_t s_working[UI_RIDE_HISTORY_MAX_RECORDS];
static UI_LARGE_RAM struct {
    ui_ride_history_storage_t storage;
    ui_ride_history_checkpoint_t checkpoint;
    ui_ride_history_result_t result;
    size_t count;
    bool already_stored, is_page;
    size_t page_offset, page_capacity;
} s_job;
/* 0 idle, 1 frozen, 2 worker owns, 3 completed. */
static atomic_int s_job_state;
static uint32_t s_commits;
static uint32_t s_record_nonce;
static bool (*s_request_page_worker)(void *);
static void *s_page_worker_context;
static UI_LARGE_RAM ui_ride_record_t s_page_records[UI_RIDE_HISTORY_PAGE_MAX];
static struct {
    size_t offset, capacity, count;
    ui_ride_history_result_t result;
    bool valid;
} s_page_cache;

bool ui_ride_history_repository_register_page_worker(bool (*request)(void *), void *context)
{
    if(atomic_load(&s_job_state) != 0) return false;
    s_request_page_worker=request; s_page_worker_context=context;
    s_page_cache.valid=false; return true;
}


static uint64_t mix_id(uint64_t value)
{
    value ^= value >> 30U;
    value *= UINT64_C(0xBF58476D1CE4E5B9);
    value ^= value >> 27U;
    value *= UINT64_C(0x94D049BB133111EB);
    value ^= value >> 31U;
    return value == 0U ? 1U : value;
}

int64_t ui_ride_history_repository_now_utc(void)
{
    if(!s_repository.configured || s_repository.storage.now_utc == NULL) return 0;
    int64_t now = s_repository.storage.now_utc(s_repository.storage.context);
    return now > 0 ? now : 0;
}

void ui_ride_history_repository_init(void)
{
    if(atomic_load(&s_job_state) != 0) return;
    ui_ride_history_storage_t storage = s_repository.storage;
    bool configured = s_repository.configured;
    memset(&s_repository, 0, sizeof(s_repository));
    s_repository.storage = storage;
    s_repository.configured = configured;
    s_repository.last_result = configured ? UI_RIDE_HISTORY_OK :
                                             UI_RIDE_HISTORY_NOT_CONFIGURED;
    ui_ride_history_model_init();
    s_page_cache.valid=false;
}

bool ui_ride_history_repository_register_storage(
    const ui_ride_history_storage_t *storage)
{
    if(storage == NULL || storage->load == NULL || storage->store == NULL ||
       storage->now_utc == NULL || atomic_load(&s_job_state) != 0 ||
       ((storage->load_checkpoint == NULL) != (storage->store_checkpoint == NULL))) return false;
    s_repository.storage = *storage;
    s_page_cache.valid=false;
    s_repository.configured = true;
    s_repository.last_result = UI_RIDE_HISTORY_OK;
    return true;
}

ui_ride_history_result_t ui_ride_history_repository_load(void)
{
    if(!s_repository.configured) {
        s_repository.last_result = UI_RIDE_HISTORY_NOT_CONFIGURED;
        return s_repository.last_result;
    }
    size_t count = 0U;
    ui_ride_history_result_t result = s_repository.storage.load(
        s_working, UI_RIDE_HISTORY_MAX_RECORDS, &count,
        s_repository.storage.context);
    if(result == UI_RIDE_HISTORY_EMPTY) {
        ui_ride_history_model_init();
        result = UI_RIDE_HISTORY_OK;
    }
    else if(result == UI_RIDE_HISTORY_OK &&
            !ui_ride_history_model_replace(s_working, count)) {
        result = UI_RIDE_HISTORY_INVALID_DATA;
    }
    if(result == UI_RIDE_HISTORY_OK) {
        ui_ride_history_model_totals(&s_repository.checkpoint.totals);
        if(s_repository.storage.load_checkpoint != NULL) {
            ui_ride_history_result_t checkpoint_result = s_repository.storage.load_checkpoint(
                &s_repository.checkpoint, s_repository.storage.context);
            if(checkpoint_result != UI_RIDE_HISTORY_EMPTY && checkpoint_result != UI_RIDE_HISTORY_OK)
                result = checkpoint_result;
            else if(checkpoint_result == UI_RIDE_HISTORY_OK) {
                s_repository.pending = s_repository.checkpoint.pending;
                s_repository.pending_valid = s_repository.checkpoint.pending_valid;
                if(s_repository.pending_valid && ui_ride_history_model_contains(s_repository.pending.id))
                    result = ui_ride_history_repository_commit_pending();
                else if(!ui_ride_history_model_set_totals(&s_repository.checkpoint.totals))
                    result = UI_RIDE_HISTORY_CORRUPT;
            }
        }
    }
    s_repository.last_result = result;
    return result;
}

ui_ride_history_result_t ui_ride_history_repository_stage(
    const ui_ride_summary_t *summary)
{
    if(!s_repository.configured) return UI_RIDE_HISTORY_NOT_CONFIGURED;
    if(s_repository.pending_valid || (atomic_load(&s_job_state) != 0 && !s_job.is_page)) return UI_RIDE_HISTORY_INVALID_DATA;
    if(summary == NULL || !isfinite(summary->distance_km) ||
       summary->distance_km < 0.0f ||
       !isfinite(summary->average_speed_kmh) ||
       !isfinite(summary->maximum_speed_kmh))
        return UI_RIDE_HISTORY_INVALID_DATA;

    int64_t finished = s_repository.storage.now_utc(
        s_repository.storage.context);
    bool utc_valid = finished > 0;
    if(!utc_valid) finished = 0;
    int64_t started = !utc_valid ? 0 : summary->started_at_utc > 0 && summary->started_at_utc <= finished ?
        summary->started_at_utc : finished - (int64_t)summary->duration_seconds;
    uint64_t identity = (uint64_t)finished ^
                        ((uint64_t)summary->session_id << 16U) ^
                        ((uint64_t)summary->duration_seconds << 32U) ^
                        ((uint64_t)(summary->distance_km * 1000.0f + 0.5f)
                         << 1U);
    if(!utc_valid) {
        static UI_LARGE_RAM ui_ride_record_t newest;
        uint64_t previous = ui_ride_history_model_get_newest(0, &newest) ? newest.id : 0;
        identity ^= previous ^ ((uint64_t)++s_record_nonce << 8);
    }
    memset(&s_repository.pending,0,sizeof(s_repository.pending));
    s_repository.pending.id = mix_id(identity);
    s_repository.pending.started_at_utc = started;
    s_repository.pending.finished_at_utc = finished;
    s_repository.pending.distance_km = summary->distance_km;
    s_repository.pending.duration_seconds = summary->duration_seconds;
    s_repository.pending.moving_time_seconds = summary->moving_time_seconds;
    s_repository.pending.average_speed_kmh = summary->average_speed_kmh;
    s_repository.pending.maximum_speed_kmh = summary->maximum_speed_kmh;
    s_repository.pending.ascent_m = summary->ascent_m;
    s_repository.pending.descent_m = summary->descent_m;
    s_repository.pending.maximum_elevation_m = summary->maximum_elevation_m;
    s_repository.pending.calories_kcal = summary->calories_kcal;
    s_repository.pending.has_heart_rate = summary->has_heart_rate;
    s_repository.pending.average_heart_rate = summary->average_heart_rate;
    s_repository.pending.maximum_heart_rate = summary->maximum_heart_rate;
    s_repository.pending.minimum_heart_rate = summary->minimum_heart_rate;
    s_repository.pending.has_power = summary->has_power;
    s_repository.pending.average_power = summary->average_power;
    s_repository.pending.maximum_power = summary->maximum_power;
    s_repository.pending.has_cadence = summary->has_cadence;
    s_repository.pending.average_cadence = summary->average_cadence;
    s_repository.pending.maximum_cadence = summary->maximum_cadence;
    s_repository.pending.has_temperature = summary->has_temperature;
    s_repository.pending.average_temperature_tenths_c = summary->average_temperature_tenths_c;
    s_repository.pending.metric_flags = summary->metric_flags;
    s_repository.pending.trace = summary->trace;
    if(!ui_ride_record_is_valid(&s_repository.pending)) return UI_RIDE_HISTORY_INVALID_DATA;
    s_repository.pending_valid = true;
    ui_ride_history_model_totals(&s_repository.checkpoint.totals);
    s_repository.checkpoint.pending = s_repository.pending;
    s_repository.checkpoint.pending_valid = true;
    s_repository.checkpoint.pending_is_new = !ui_ride_history_model_contains(s_repository.pending.id);
    s_repository.last_result = UI_RIDE_HISTORY_OK;
    return UI_RIDE_HISTORY_OK;
}

ui_ride_history_result_t ui_ride_history_repository_prepare(const ui_ride_summary_t *summary)
{
    ui_ride_history_result_t result = ui_ride_history_repository_stage(summary);
    if(result == UI_RIDE_HISTORY_OK && s_repository.storage.store_checkpoint != NULL)
        result = s_repository.storage.store_checkpoint(&s_repository.checkpoint, s_repository.storage.context);
    s_repository.last_result = result;
    return result;
}

bool ui_ride_history_repository_begin_commit(void)
{
    if(!s_repository.configured || !s_repository.pending_valid || atomic_load(&s_job_state) != 0)
        return false;
    s_job.is_page = false;
    s_job.storage = s_repository.storage;
    s_job.checkpoint = s_repository.checkpoint;
    s_job.already_stored = ui_ride_history_model_contains(s_repository.pending.id);
    if(!ui_ride_history_model_copy_oldest_first(s_working, UI_RIDE_HISTORY_MAX_RECORDS, &s_job.count))
        return false;
    if(!s_job.already_stored) {
        if(s_job.count == UI_RIDE_HISTORY_MAX_RECORDS) {
            memmove(s_working, s_working + 1, (--s_job.count) * sizeof(s_working[0]));
        }
        s_working[s_job.count++] = s_repository.pending;
    }
    atomic_store_explicit(&s_job_state, 1, memory_order_release);
    return true;
}

bool ui_ride_history_repository_abort_commit(void)
{
    int expected = 1;
    return atomic_compare_exchange_strong(&s_job_state, &expected, 0);
}
uint32_t ui_ride_history_repository_commits(void) { return s_commits; }

ui_ride_history_result_t ui_ride_history_repository_commit_worker(void)
{
    int expected = 1;
    if(atomic_load_explicit(&s_job_state, memory_order_acquire) != 1 || s_job.is_page ||
       !atomic_compare_exchange_strong(&s_job_state, &expected, 2))
        return UI_RIDE_HISTORY_INVALID_DATA;
    ui_ride_history_storage_t *storage = &s_job.storage;
    ui_ride_history_checkpoint_t *checkpoint = &s_job.checkpoint;
    ui_ride_history_result_t result = UI_RIDE_HISTORY_OK;
    if(storage->store_checkpoint != NULL)
        result = storage->store_checkpoint(checkpoint, storage->context);
    if(result == UI_RIDE_HISTORY_OK && !s_job.already_stored) {
        result = storage->append_record != NULL ?
            storage->append_record(&checkpoint->pending, storage->context) :
            storage->store(s_working, s_job.count, storage->context);
    }
    if(result == UI_RIDE_HISTORY_OK) {
        if(checkpoint->pending_is_new) {
            checkpoint->totals.total_distance_km += checkpoint->pending.distance_km;
            checkpoint->totals.total_duration_seconds += checkpoint->pending.duration_seconds;
            checkpoint->totals.total_ascent_m += checkpoint->pending.ascent_m;
            ++checkpoint->totals.ride_count;
        }
        checkpoint->last_committed_id = checkpoint->pending.id;
        checkpoint->pending_valid = false;
        if(storage->store_checkpoint != NULL)
            result = storage->store_checkpoint(checkpoint, storage->context);
    }
    s_job.result = result;
    atomic_store_explicit(&s_job_state, 3, memory_order_release);
    return result;
}

bool ui_ride_history_repository_process(ui_ride_history_result_t *result)
{
    if(atomic_load_explicit(&s_job_state, memory_order_acquire) != 3) return false;
    ui_ride_history_result_t value = s_job.result;
    if(s_job.is_page) {
        s_page_cache.offset=s_job.page_offset; s_page_cache.capacity=s_job.page_capacity;
        s_page_cache.count=s_job.count; s_page_cache.result=value; s_page_cache.valid=true;
        if(value == UI_RIDE_HISTORY_OK) memcpy(s_page_records,s_working,s_job.count*sizeof(s_working[0]));
        atomic_store_explicit(&s_job_state,0,memory_order_release);
        if(result) *result=value;
        return true;
    }
    if(value == UI_RIDE_HISTORY_OK) {
        s_page_cache.valid=false;
        if(!ui_ride_history_model_replace(s_working, s_job.count) ||
           !ui_ride_history_model_set_totals(&s_job.checkpoint.totals))
            value = UI_RIDE_HISTORY_INVALID_DATA;
        else {
            s_repository.checkpoint = s_job.checkpoint;
            s_repository.pending_valid = false;
            ++s_commits;
        }
    }
    s_repository.last_result = value;
    atomic_store_explicit(&s_job_state, 0, memory_order_release);
    if(result != NULL) *result = value;
    return true;
}

bool ui_ride_history_repository_process_page(void)
{
    return atomic_load_explicit(&s_job_state,memory_order_acquire)==3 && s_job.is_page &&
        ui_ride_history_repository_process(NULL);
}

ui_ride_history_result_t ui_ride_history_repository_page_worker(void)
{
    int expected=1;
    if(atomic_load_explicit(&s_job_state,memory_order_acquire)!=1 || !s_job.is_page ||
       !atomic_compare_exchange_strong(&s_job_state,&expected,2)) return UI_RIDE_HISTORY_INVALID_DATA;
    s_job.count=0;
    ui_ride_history_result_t result=s_job.storage.load_page(s_job.page_offset,s_working,
        s_job.page_capacity,&s_job.count,s_job.storage.context);
    if(s_job.count > s_job.page_capacity) result=UI_RIDE_HISTORY_INVALID_DATA;
    if(result == UI_RIDE_HISTORY_OK)
        for(size_t i=0;i<s_job.count;++i)
            if(!ui_ride_record_is_valid(&s_working[i])) result=UI_RIDE_HISTORY_INVALID_DATA;
    if(result != UI_RIDE_HISTORY_OK) s_job.count=0;
    s_job.result=result;
    atomic_store_explicit(&s_job_state,3,memory_order_release);
    return result;
}

ui_ride_history_result_t ui_ride_history_repository_request_page(size_t offset,
    ui_ride_record_t *records,size_t capacity,size_t *count)
{
    if(!records || !count || !capacity || capacity > UI_RIDE_HISTORY_PAGE_MAX) return UI_RIDE_HISTORY_INVALID_DATA;
    *count=0;
    if(ui_ride_history_repository_busy()) return UI_RIDE_HISTORY_PENDING;
    size_t cached=ui_ride_history_model_count();
    ui_ride_history_totals_t totals; ui_ride_history_model_totals(&totals);
    if((offset < cached && capacity <= cached-offset) || totals.ride_count <= cached)
        return ui_ride_history_repository_load_page(offset,records,capacity,count);
    if(s_page_cache.valid && s_page_cache.offset==offset && s_page_cache.capacity==capacity) {
        *count=s_page_cache.count;
        if(s_page_cache.result==UI_RIDE_HISTORY_OK) memcpy(records,s_page_records,*count*sizeof(records[0]));
        else s_page_cache.valid=false; /* A subsequent user request can retry. */
        return s_page_cache.result;
    }
    if(!s_repository.configured || !s_repository.storage.load_page || !s_request_page_worker)
        return UI_RIDE_HISTORY_NOT_CONFIGURED;
    s_job.is_page=true; s_job.storage=s_repository.storage;
    s_job.page_offset=offset; s_job.page_capacity=capacity;
    atomic_store_explicit(&s_job_state,1,memory_order_release);
    if(!s_request_page_worker(s_page_worker_context)) {
        atomic_store_explicit(&s_job_state,0,memory_order_release);
        return UI_RIDE_HISTORY_IO_ERROR;
    }
    return UI_RIDE_HISTORY_PENDING;
}

bool ui_ride_history_repository_busy(void) { return atomic_load(&s_job_state) != 0; }

ui_ride_history_result_t ui_ride_history_repository_commit_pending(void)
{
    if(!ui_ride_history_repository_begin_commit()) return UI_RIDE_HISTORY_INVALID_DATA;
    ui_ride_history_result_t result = ui_ride_history_repository_commit_worker();
    (void)ui_ride_history_repository_process(&result);
    return result;
}

bool ui_ride_history_repository_has_pending(void)
{
    return s_repository.pending_valid;
}

bool ui_ride_history_repository_get_pending(ui_ride_record_t *record)
{
    if(record == NULL || !s_repository.pending_valid) return false;
    *record = s_repository.pending;
    return true;
}

bool ui_ride_history_repository_recovery_supported(void)
{
    return s_repository.storage.load_checkpoint != NULL &&
           s_repository.storage.store_checkpoint != NULL;
}

ui_ride_history_result_t ui_ride_history_repository_load_page(
    size_t newest_offset, ui_ride_record_t *records, size_t capacity, size_t *count)
{
    if(records == NULL || count == NULL || capacity == 0 || ui_ride_history_repository_busy()) return UI_RIDE_HISTORY_INVALID_DATA;
    size_t cached = ui_ride_history_model_count();
    ui_ride_history_totals_t totals;
    ui_ride_history_model_totals(&totals);
    if((newest_offset < cached && capacity <= cached - newest_offset) || totals.ride_count <= cached) {
        *count = 0;
        for(size_t i = newest_offset; *count < capacity && i < cached; ++i)
            if(ui_ride_history_model_get_newest(i, &records[*count])) ++*count;
        return *count ? UI_RIDE_HISTORY_OK : UI_RIDE_HISTORY_EMPTY;
    }
    if(s_repository.storage.load_page != NULL)
        return s_repository.storage.load_page(newest_offset, records, capacity,
                                               count, s_repository.storage.context);
    *count = 0;
    for(size_t i = newest_offset; *count < capacity; ++i) {
        if(!ui_ride_history_model_get_newest(i, &records[*count])) break;
        ++*count;
    }
    return *count ? UI_RIDE_HISTORY_OK : UI_RIDE_HISTORY_EMPTY;
}

void ui_ride_history_repository_clear_pending(void)
{
    if(ui_ride_history_repository_busy()) return;
    s_repository.checkpoint.pending_valid = false;
    if(s_repository.storage.store_checkpoint != NULL) {
        s_repository.last_result = s_repository.storage.store_checkpoint(
            &s_repository.checkpoint, s_repository.storage.context);
        if(s_repository.last_result != UI_RIDE_HISTORY_OK) {
            s_repository.checkpoint.pending_valid = s_repository.pending_valid;
            return;
        }
    }
    memset(&s_repository.pending, 0, sizeof(s_repository.pending));
    s_repository.pending_valid = false;
}

ui_ride_history_result_t ui_ride_history_repository_last_result(void)
{
    return s_repository.last_result;
}
