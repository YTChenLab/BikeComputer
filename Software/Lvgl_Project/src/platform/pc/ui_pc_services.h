#ifndef UI_PC_SERVICES_H
#define UI_PC_SERVICES_H

#include <stdbool.h>
#include <stdint.h>

#define UI_PC_GNSS_DELAY_MS     900U
#define UI_PC_STORAGE_DELAY_MS  700U
#define UI_PC_COMMAND_DELAY_MS   80U
#define UI_PC_COMMAND_ERROR_SIMULATED (-1001)

/* Register the PC simulator backend behind the platform-neutral service API. */
void ui_pc_services_register(void);

/* Test/development controls. Each configured result is consumed once. */
void ui_pc_services_set_next_gnss_result(bool success);
void ui_pc_services_set_next_storage_result(bool success);
void ui_pc_services_set_next_command_result(bool success);
bool ui_pc_services_gnss_busy(void);
bool ui_pc_services_storage_busy(void);

#endif /* UI_PC_SERVICES_H */
