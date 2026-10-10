#include "ui/core/ui_memory.h"
#include "ui_event_bridge.h"

#include "ui_concurrency.h"
#include "ui_event_bus.h"

#include <string.h>

typedef struct
{
    ui_event_t queue[UI_EVENT_BRIDGE_QUEUE_MAX];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint32_t dropped;
    uint32_t forward_failures;
} ui_event_bridge_context_t;

static UI_LARGE_RAM ui_event_bridge_context_t s_bridge;

static bool valid_event(const ui_event_t *event)
{
    return event != NULL && event->type >= UI_EVENT_BOOT_COMPLETE &&
           event->type < UI_EVENT_COUNT &&
           event->priority >= UI_EVENT_PRIORITY_P0_CRITICAL &&
           event->priority <= UI_EVENT_PRIORITY_P4_NORMAL;
}

static uint8_t queue_index(uint8_t logical_index)
{
    return (uint8_t)((s_bridge.head + logical_index) %
                     UI_EVENT_BRIDGE_QUEUE_MAX);
}

static void copy_event(ui_event_t *destination, const ui_event_t *source)
{
    *destination = *source;
    destination->title[UI_EVENT_TITLE_MAX - 1] = '\0';
    destination->message[UI_EVENT_MESSAGE_MAX - 1] = '\0';
    destination->primary_action[UI_EVENT_ACTION_MAX - 1] = '\0';
    destination->secondary_action[UI_EVENT_ACTION_MAX - 1] = '\0';
}

static void remove_logical_index(uint8_t logical_index)
{
    for(uint8_t i = logical_index + 1U; i < s_bridge.count; i++)
        s_bridge.queue[queue_index((uint8_t)(i - 1U))] =
            s_bridge.queue[queue_index(i)];
    s_bridge.count--;
    s_bridge.tail = queue_index(s_bridge.count);
}

void ui_event_bridge_init(void)
{
    ui_concurrency_lock();
    memset(&s_bridge, 0, sizeof(s_bridge));
    ui_concurrency_unlock();
}

bool ui_event_bridge_post(const ui_event_t *event)
{
    if(!valid_event(event)) return false;
    ui_concurrency_lock();

    if(event->dedupe_key != 0U) {
        for(uint8_t i = 0; i < s_bridge.count; i++) {
            ui_event_t *queued = &s_bridge.queue[queue_index(i)];
            if(queued->type == event->type &&
               queued->dedupe_key == event->dedupe_key) {
                copy_event(queued, event);
                ui_concurrency_unlock();
                return true;
            }
        }
    }

    if(s_bridge.count >= UI_EVENT_BRIDGE_QUEUE_MAX) {
        uint8_t worst = 0;
        for(uint8_t i = 1; i < s_bridge.count; i++) {
            if(s_bridge.queue[queue_index(i)].priority >
               s_bridge.queue[queue_index(worst)].priority) worst = i;
        }
        if(event->priority >
           s_bridge.queue[queue_index(worst)].priority) {
            s_bridge.dropped++;
            ui_concurrency_unlock();
            return false;
        }
        remove_logical_index(worst);
        s_bridge.dropped++;
    }
    copy_event(&s_bridge.queue[s_bridge.tail], event);
    s_bridge.tail = (uint8_t)((s_bridge.tail + 1U) % UI_EVENT_BRIDGE_QUEUE_MAX);
    s_bridge.count++;
    ui_concurrency_unlock();
    return true;
}

uint8_t ui_event_bridge_drain(uint8_t max_events)
{
    uint8_t forwarded = 0;
    while(forwarded < max_events) {
        ui_event_t event;
        ui_concurrency_lock();
        if(s_bridge.count == 0) {
            ui_concurrency_unlock();
            break;
        }
        event = s_bridge.queue[s_bridge.head];
        s_bridge.head = (uint8_t)((s_bridge.head + 1U) %
                                  UI_EVENT_BRIDGE_QUEUE_MAX);
        s_bridge.count--;
        ui_concurrency_unlock();

        if(!ui_event_bus_publish(&event)) {
            ui_concurrency_lock();
            s_bridge.forward_failures++;
            ui_concurrency_unlock();
        }
        forwarded++;
    }
    return forwarded;
}

uint8_t ui_event_bridge_pending_count(void)
{
    ui_concurrency_lock();
    uint8_t count = s_bridge.count;
    ui_concurrency_unlock();
    return count;
}

uint32_t ui_event_bridge_dropped_count(void)
{
    ui_concurrency_lock();
    uint32_t count = s_bridge.dropped;
    ui_concurrency_unlock();
    return count;
}

uint32_t ui_event_bridge_forward_failures(void)
{
    ui_concurrency_lock();
    uint32_t count = s_bridge.forward_failures;
    ui_concurrency_unlock();
    return count;
}
