#ifndef UI_EVENT_BUS_H
#define UI_EVENT_BUS_H

#include "ui_event.h"

#include <stdbool.h>
#include <stdint.h>

#define UI_EVENT_BUS_QUEUE_MAX       16
#define UI_EVENT_BUS_SUBSCRIBER_MAX   8

typedef void (*ui_event_callback_t)(const ui_event_t *event, void *user_data);

typedef enum
{
    UI_EVENT_OVERFLOW_DROPPED_INCOMING = 0,
    UI_EVENT_OVERFLOW_EVICTED_QUEUED
} ui_event_overflow_reason_t;

typedef void (*ui_event_overflow_cb_t)(const ui_event_t *incoming,
                                       const ui_event_t *displaced,
                                       ui_event_overflow_reason_t reason,
                                       void *user_data);

void ui_event_bus_init(void);
int32_t ui_event_bus_subscribe(uint64_t event_mask,
                               ui_event_callback_t callback,
                               void *user_data);
bool ui_event_bus_unsubscribe(int32_t subscription_id);
void ui_event_bus_set_overflow_callback(ui_event_overflow_cb_t callback,
                                        void *user_data);
bool ui_event_bus_publish(const ui_event_t *event);
uint8_t ui_event_bus_process(uint8_t max_events);
uint8_t ui_event_bus_pending_count(void);
uint32_t ui_event_bus_dropped_count(void);
void ui_event_bus_clear(void);
uint8_t ui_event_bus_remove_source(uint32_t source_id);
uint8_t ui_event_bus_remove_mask(uint64_t mask);

#endif /* UI_EVENT_BUS_H */
