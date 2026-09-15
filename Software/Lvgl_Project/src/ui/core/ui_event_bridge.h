#ifndef UI_EVENT_BRIDGE_H
#define UI_EVENT_BRIDGE_H

#include "ui_event.h"

#include <stdbool.h>
#include <stdint.h>

#define UI_EVENT_BRIDGE_QUEUE_MAX 16

/* Producer-task API. Never dispatches callbacks and never touches LVGL. */
void ui_event_bridge_init(void);
bool ui_event_bridge_post(const ui_event_t *event);

/* UI-thread API. Forwards queued events to ui_event_bus. */
uint8_t ui_event_bridge_drain(uint8_t max_events);
uint8_t ui_event_bridge_pending_count(void);
uint32_t ui_event_bridge_dropped_count(void);
uint32_t ui_event_bridge_forward_failures(void);

#endif /* UI_EVENT_BRIDGE_H */
