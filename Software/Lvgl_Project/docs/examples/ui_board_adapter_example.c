#include "ui_board_adapter_example.h"
#include <stddef.h>

bool bike_ui_attach_ports(const bike_ui_ports_t *ports)
{
    if(ports == NULL || ports->services.request_history_page_worker == NULL ||
       ports->history.load_page == NULL || ports->history.append_record == NULL ||
       ports->history.load_checkpoint == NULL || ports->history.store_checkpoint == NULL)
        return false;
    if(!ui_stm32_services_register(&ports->services)) return false;
    if(!ui_stm32_services_register_history_storage(&ports->history)) return false;
    return ui_preferences_register(&ports->preferences);
}

bool bike_ui_publish_snapshot(const ui_ride_data_t *snapshot)
{
    if(snapshot == NULL || !snapshot->quality_known || snapshot->session_id == 0 ||
       (snapshot->valid_flags & ~snapshot->capability_flags) != 0U ||
       ((snapshot->capability_flags | snapshot->valid_flags) >> UI_RIDE_FIELD_COUNT) != 0U)
        return false;
    ui_stm32_services_submit_ride_data(snapshot);
    return true;
}

bool bike_ui_save_worker(uint32_t command_id, ui_command_result_t *retry_result)
{
    if(retry_result==NULL) return false;
    /* STM32 dispatch wrapper has frozen the transaction before enqueueing SAVE.
     * Do not call commit_pending() here; it publishes UI models synchronously. */
    ui_ride_history_result_t result=ui_ride_history_repository_commit_worker();
    /* If the result queue is full, the board must retry this acknowledgment
     * with the same id without executing the storage transaction again. */
    *retry_result=(ui_command_result_t){.id=command_id,.type=UI_COMMAND_RIDE_SAVE,
        .status=result==UI_RIDE_HISTORY_OK ? UI_COMMAND_STATUS_SUCCESS : UI_COMMAND_STATUS_FAILED,
        .error_code=result==UI_RIDE_HISTORY_OK ? 0 : -(int32_t)result};
    return ui_stm32_services_complete_command(command_id,retry_result->status,
        retry_result->error_code,NULL,0);
}

void bike_ui_history_page_worker(void)
{
    (void)ui_ride_history_repository_page_worker();
}

void bike_ui_preferences_worker(void)
{
    ui_preferences_worker();
}
