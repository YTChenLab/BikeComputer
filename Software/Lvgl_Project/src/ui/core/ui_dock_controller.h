#ifndef UI_DOCK_CONTROLLER_H
#define UI_DOCK_CONTROLLER_H

#include <stdint.h>

void ui_dock_controller_init(void);
void ui_dock_controller_deinit(void);
void ui_dock_controller_tick(uint32_t elapsed_ms);
uint32_t ui_dock_controller_mapping_failures(void);

#endif /* UI_DOCK_CONTROLLER_H */
