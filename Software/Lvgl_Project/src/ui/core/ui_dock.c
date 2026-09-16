#include "ui_dock.h"

#include <string.h>

typedef struct
{
    ui_dock_card_t active;
    ui_dock_card_t restore[UI_DOCK_STACK_MAX];
    ui_dock_card_t pending[UI_DOCK_PENDING_MAX];
    uint8_t restore_count;
    uint8_t pending_count;
    uint32_t dropped_count;
    bool visible;
    ui_dock_observer_cb_t observer;
    void *observer_user_data;
} ui_dock_context_t;

static ui_dock_context_t s_dock;

static bool valid_card(const ui_dock_card_t *card)
{
    return card != NULL && card->id != 0 && card->type >= UI_DOCK_TYPE_INFO &&
           card->type < UI_DOCK_TYPE_COUNT &&
           card->priority >= UI_DOCK_PRIORITY_P0_CRITICAL &&
           card->priority <= UI_DOCK_PRIORITY_P4_NORMAL;
}

static void notify(ui_dock_event_t event, const ui_dock_card_t *card)
{
    if(s_dock.observer != NULL) {
        s_dock.observer(event, card, s_dock.observer_user_data);
    }
}

static void copy_card(ui_dock_card_t *destination, const ui_dock_card_t *source)
{
    *destination = *source;
    destination->title[UI_DOCK_TITLE_MAX - 1] = '\0';
    destination->message[UI_DOCK_MESSAGE_MAX - 1] = '\0';
    destination->primary_action[UI_DOCK_ACTION_MAX - 1] = '\0';
    destination->secondary_action[UI_DOCK_ACTION_MAX - 1] = '\0';
}

static bool push_restore(const ui_dock_card_t *card)
{
    if(s_dock.restore_count >= UI_DOCK_STACK_MAX) {
        uint8_t worst = 0;
        for(uint8_t i = 1; i < s_dock.restore_count; i++) {
            if(s_dock.restore[i].priority > s_dock.restore[worst].priority)
                worst = i;
        }
        if(card->priority <= s_dock.restore[worst].priority) {
            ui_dock_card_t dropped = s_dock.restore[worst];
            copy_card(&s_dock.restore[worst], card);
            s_dock.dropped_count++;
            notify(UI_DOCK_EVENT_DROPPED, &dropped);
        }
        else {
            s_dock.dropped_count++;
            notify(UI_DOCK_EVENT_DROPPED, card);
        }
        return true;
    }
    copy_card(&s_dock.restore[s_dock.restore_count++], card);
    return true;
}

static bool queue_pending(const ui_dock_card_t *card)
{
    for(uint8_t i = 0; i < s_dock.pending_count; i++) {
        if(s_dock.pending[i].id == card->id) {
            copy_card(&s_dock.pending[i], card);
            notify(UI_DOCK_EVENT_QUEUED, card);
            return true;
        }
    }
    if(s_dock.pending_count >= UI_DOCK_PENDING_MAX) {
        uint8_t worst = 0;
        for(uint8_t i = 1; i < s_dock.pending_count; i++) {
            if(s_dock.pending[i].priority > s_dock.pending[worst].priority)
                worst = i;
        }
        if(card->priority <= s_dock.pending[worst].priority) {
            ui_dock_card_t dropped = s_dock.pending[worst];
            copy_card(&s_dock.pending[worst], card);
            s_dock.dropped_count++;
            notify(UI_DOCK_EVENT_DROPPED, &dropped);
            notify(UI_DOCK_EVENT_QUEUED, card);
            return true;
        }
        s_dock.dropped_count++;
        notify(UI_DOCK_EVENT_DROPPED, card);
        return false;
    }
    copy_card(&s_dock.pending[s_dock.pending_count++], card);
    notify(UI_DOCK_EVENT_QUEUED, card);
    return true;
}

static bool activate_next(void)
{
    uint8_t pending_selected = 0;
    bool has_pending = s_dock.pending_count > 0;
    if(has_pending) {
        for(uint8_t i = 1; i < s_dock.pending_count; i++) {
            if(s_dock.pending[i].priority < s_dock.pending[pending_selected].priority)
                pending_selected = i;
        }
    }

    bool restore_first = s_dock.restore_count > 0 &&
                         (!has_pending ||
                          s_dock.restore[s_dock.restore_count - 1].priority <=
                          s_dock.pending[pending_selected].priority);
    if(restore_first) {
        copy_card(&s_dock.active, &s_dock.restore[--s_dock.restore_count]);
        s_dock.visible = true;
        notify(UI_DOCK_EVENT_RESTORED, &s_dock.active);
        return true;
    }
    if(has_pending) {
        copy_card(&s_dock.active, &s_dock.pending[pending_selected]);
        for(uint8_t i = pending_selected + 1; i < s_dock.pending_count; i++) {
            s_dock.pending[i - 1] = s_dock.pending[i];
        }
        s_dock.pending_count--;
        s_dock.visible = true;
        notify(UI_DOCK_EVENT_SHOWN, &s_dock.active);
        return true;
    }
    memset(&s_dock.active, 0, sizeof(s_dock.active));
    s_dock.visible = false;
    return false;
}

static bool update_collection(ui_dock_card_t *cards, uint8_t count,
                              const ui_dock_card_t *card)
{
    for(uint8_t i = 0; i < count; i++) {
        if(cards[i].id == card->id) {
            copy_card(&cards[i], card);
            return true;
        }
    }
    return false;
}

static bool remove_collection(ui_dock_card_t *cards, uint8_t *count, uint32_t id)
{
    for(uint8_t i = 0; i < *count; i++) {
        if(cards[i].id == id) {
            for(uint8_t j = i + 1; j < *count; j++) cards[j - 1] = cards[j];
            (*count)--;
            return true;
        }
    }
    return false;
}

void ui_dock_init(void)
{
    memset(&s_dock, 0, sizeof(s_dock));
}

void ui_dock_set_observer(ui_dock_observer_cb_t observer, void *user_data)
{
    s_dock.observer = observer;
    s_dock.observer_user_data = user_data;
}

bool ui_dock_show(const ui_dock_card_t *card)
{
    if(!valid_card(card)) return false;
    if(!s_dock.visible) {
        copy_card(&s_dock.active, card);
        s_dock.visible = true;
        notify(UI_DOCK_EVENT_SHOWN, &s_dock.active);
        return true;
    }
    if(card->id == s_dock.active.id) return ui_dock_update(card);
    if(card->priority <= s_dock.active.priority) {
        if(!push_restore(&s_dock.active)) return false;
        notify(UI_DOCK_EVENT_OVERRIDDEN, &s_dock.active);
        copy_card(&s_dock.active, card);
        notify(UI_DOCK_EVENT_SHOWN, &s_dock.active);
        return true;
    }
    return queue_pending(card);
}

bool ui_dock_hide(uint32_t id)
{
    if(id == 0) return false;
    if(s_dock.visible && s_dock.active.id == id) {
        ui_dock_card_t hidden = s_dock.active;
        s_dock.visible = false;
        notify(UI_DOCK_EVENT_HIDDEN, &hidden);
        (void)activate_next();
        return true;
    }
    if(remove_collection(s_dock.restore, &s_dock.restore_count, id)) return true;
    return remove_collection(s_dock.pending, &s_dock.pending_count, id);
}

bool ui_dock_update(const ui_dock_card_t *card)
{
    if(!valid_card(card)) return false;
    if(s_dock.visible && s_dock.active.id == card->id) {
        bool type_changed = s_dock.active.type != card->type;
        copy_card(&s_dock.active, card);
        notify(type_changed ? UI_DOCK_EVENT_REPLACED : UI_DOCK_EVENT_UPDATED,
               &s_dock.active);
        return true;
    }
    if(update_collection(s_dock.restore, s_dock.restore_count, card)) return true;
    return update_collection(s_dock.pending, s_dock.pending_count, card);
}

bool ui_dock_replace(const ui_dock_card_t *card)
{
    if(!valid_card(card)) return false;
    if(!s_dock.visible) return ui_dock_show(card);
    copy_card(&s_dock.active, card);
    notify(UI_DOCK_EVENT_REPLACED, &s_dock.active);
    return true;
}

bool ui_dock_invoke(uint32_t id, ui_dock_interaction_t interaction,
                    int32_t value)
{
    if(!s_dock.visible || s_dock.active.id != id) return false;
    if(interaction == UI_DOCK_INTERACTION_VALUE_CHANGED) {
        if(value < s_dock.active.minimum) value = s_dock.active.minimum;
        if(value > s_dock.active.maximum) value = s_dock.active.maximum;
        s_dock.active.value = value;
    }
    ui_dock_action_cb_t callback = s_dock.active.action_cb;
    void *context = s_dock.active.context;
    if(callback != NULL) callback(id, interaction, value, context);
    if(interaction == UI_DOCK_INTERACTION_DISMISS) {
        if(!s_dock.active.dismissible) return false;
        return ui_dock_hide(id);
    }
    return callback != NULL;
}

bool ui_dock_is_visible(void)
{
    return s_dock.visible;
}

const ui_dock_card_t *ui_dock_current(void)
{
    return s_dock.visible ? &s_dock.active : NULL;
}

uint8_t ui_dock_restore_depth(void)
{
    return s_dock.restore_count;
}

uint8_t ui_dock_pending_count(void)
{
    return s_dock.pending_count;
}

uint32_t ui_dock_dropped_count(void)
{
    return s_dock.dropped_count;
}

void ui_dock_clear(void)
{
    ui_dock_observer_cb_t observer = s_dock.observer;
    void *user_data = s_dock.observer_user_data;
    uint32_t dropped_count = s_dock.dropped_count;
    if(s_dock.visible) {
        ui_dock_card_t hidden = s_dock.active;
        s_dock.visible = false;
        notify(UI_DOCK_EVENT_HIDDEN, &hidden);
    }
    memset(&s_dock, 0, sizeof(s_dock));
    s_dock.observer = observer;
    s_dock.observer_user_data = user_data;
    s_dock.dropped_count = dropped_count;
}
