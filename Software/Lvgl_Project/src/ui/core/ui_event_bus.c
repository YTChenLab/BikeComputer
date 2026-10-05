#include "ui_event_bus.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    ui_event_t event;
    uint32_t sequence;
} ui_event_envelope_t;

typedef struct
{
    int32_t id;
    uint64_t mask;
    ui_event_callback_t callback;
    void *user_data;
} ui_event_subscription_t;

typedef struct
{
    ui_event_envelope_t queue[UI_EVENT_BUS_QUEUE_MAX];
    ui_event_subscription_t subscribers[UI_EVENT_BUS_SUBSCRIBER_MAX];
    uint8_t queue_count;
    uint8_t subscriber_count;
    uint32_t next_sequence;
    int32_t next_subscription_id;
    uint32_t dropped_count;
    bool processing;
    ui_event_overflow_cb_t overflow;
    void *overflow_user_data;
} ui_event_bus_context_t;

static ui_event_bus_context_t s_bus;

static bool valid_event(const ui_event_t *event)
{
    return event != NULL && event->type >= UI_EVENT_BOOT_COMPLETE &&
           event->type < UI_EVENT_COUNT &&
           event->priority >= UI_EVENT_PRIORITY_P0_CRITICAL &&
           event->priority <= UI_EVENT_PRIORITY_P4_NORMAL;
}

static void copy_event(ui_event_t *destination, const ui_event_t *source)
{
    *destination = *source;
    destination->title[UI_EVENT_TITLE_MAX - 1] = '\0';
    destination->message[UI_EVENT_MESSAGE_MAX - 1] = '\0';
    destination->primary_action[UI_EVENT_ACTION_MAX - 1] = '\0';
    destination->secondary_action[UI_EVENT_ACTION_MAX - 1] = '\0';
}

static void report_overflow(const ui_event_t *incoming,
                            const ui_event_t *displaced,
                            ui_event_overflow_reason_t reason)
{
    s_bus.dropped_count++;
    if(s_bus.overflow != NULL)
        s_bus.overflow(incoming, displaced, reason, s_bus.overflow_user_data);
}

void ui_event_bus_init(void)
{
    memset(&s_bus, 0, sizeof(s_bus));
    s_bus.next_sequence = 1;
    s_bus.next_subscription_id = 1;
}

int32_t ui_event_bus_subscribe(uint64_t event_mask,
                               ui_event_callback_t callback,
                               void *user_data)
{
    if(event_mask == 0 || callback == NULL ||
       s_bus.subscriber_count >= UI_EVENT_BUS_SUBSCRIBER_MAX) return -1;
    ui_event_subscription_t *slot =
        &s_bus.subscribers[s_bus.subscriber_count++];
    slot->id = s_bus.next_subscription_id++;
    slot->mask = event_mask;
    slot->callback = callback;
    slot->user_data = user_data;
    return slot->id;
}

bool ui_event_bus_unsubscribe(int32_t subscription_id)
{
    for(uint8_t i = 0; i < s_bus.subscriber_count; i++) {
        if(s_bus.subscribers[i].id != subscription_id) continue;
        for(uint8_t j = i + 1; j < s_bus.subscriber_count; j++)
            s_bus.subscribers[j - 1] = s_bus.subscribers[j];
        s_bus.subscriber_count--;
        return true;
    }
    return false;
}

void ui_event_bus_set_overflow_callback(ui_event_overflow_cb_t callback,
                                        void *user_data)
{
    s_bus.overflow = callback;
    s_bus.overflow_user_data = user_data;
}

bool ui_event_bus_publish(const ui_event_t *event)
{
    if(!valid_event(event)) return false;

    if(event->dedupe_key != 0) {
        for(uint8_t i = 0; i < s_bus.queue_count; i++) {
            if(s_bus.queue[i].event.type == event->type &&
               s_bus.queue[i].event.dedupe_key == event->dedupe_key) {
                copy_event(&s_bus.queue[i].event, event);
                return true;
            }
        }
    }

    if(s_bus.queue_count < UI_EVENT_BUS_QUEUE_MAX) {
        ui_event_envelope_t *slot = &s_bus.queue[s_bus.queue_count++];
        copy_event(&slot->event, event);
        slot->sequence = s_bus.next_sequence++;
        return true;
    }

    uint8_t worst = 0;
    for(uint8_t i = 1; i < s_bus.queue_count; i++) {
        if(s_bus.queue[i].event.priority > s_bus.queue[worst].event.priority ||
           (s_bus.queue[i].event.priority == s_bus.queue[worst].event.priority &&
            s_bus.queue[i].sequence < s_bus.queue[worst].sequence)) worst = i;
    }

    if(event->priority <= s_bus.queue[worst].event.priority) {
        ui_event_t displaced = s_bus.queue[worst].event;
        copy_event(&s_bus.queue[worst].event, event);
        s_bus.queue[worst].sequence = s_bus.next_sequence++;
        report_overflow(event, &displaced, UI_EVENT_OVERFLOW_EVICTED_QUEUED);
        return true;
    }

    report_overflow(event, NULL, UI_EVENT_OVERFLOW_DROPPED_INCOMING);
    return false;
}

static uint8_t select_next(void)
{
    uint8_t selected = 0;
    for(uint8_t i = 1; i < s_bus.queue_count; i++) {
        if(s_bus.queue[i].event.priority < s_bus.queue[selected].event.priority ||
           (s_bus.queue[i].event.priority == s_bus.queue[selected].event.priority &&
            s_bus.queue[i].sequence < s_bus.queue[selected].sequence)) selected = i;
    }
    return selected;
}

uint8_t ui_event_bus_process(uint8_t max_events)
{
    if(s_bus.processing || max_events == 0) return 0;
    s_bus.processing = true;
    uint8_t processed = 0;
    while(s_bus.queue_count > 0 && processed < max_events) {
        uint8_t selected = select_next();
        ui_event_t event = s_bus.queue[selected].event;
        for(uint8_t i = selected + 1; i < s_bus.queue_count; i++)
            s_bus.queue[i - 1] = s_bus.queue[i];
        s_bus.queue_count--;

        uint64_t mask = UI_EVENT_MASK(event.type);
        for(uint8_t i = 0; i < s_bus.subscriber_count; i++) {
            ui_event_subscription_t subscription = s_bus.subscribers[i];
            if((subscription.mask & mask) != 0 && subscription.callback != NULL)
                subscription.callback(&event, subscription.user_data);
        }
        processed++;
    }
    s_bus.processing = false;
    return processed;
}

uint8_t ui_event_bus_pending_count(void)
{
    return s_bus.queue_count;
}

uint32_t ui_event_bus_dropped_count(void)
{
    return s_bus.dropped_count;
}

void ui_event_bus_clear(void)
{
    s_bus.queue_count = 0;
}
uint8_t ui_event_bus_remove_source(uint32_t source_id)
{
    uint8_t kept=0,old=s_bus.queue_count;
    for(uint8_t i=0;i<old;i++) if(s_bus.queue[i].event.source_id!=source_id) s_bus.queue[kept++]=s_bus.queue[i];
    s_bus.queue_count=kept; return old-kept;
}
uint8_t ui_event_bus_remove_mask(uint64_t mask)
{
    uint8_t kept=0,old=s_bus.queue_count;
    for(uint8_t i=0;i<old;i++) if(!(mask & UI_EVENT_MASK(s_bus.queue[i].event.type)))
        s_bus.queue[kept++]=s_bus.queue[i];
    s_bus.queue_count=kept;return old-kept;
}
