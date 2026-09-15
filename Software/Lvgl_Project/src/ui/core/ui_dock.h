#ifndef UI_DOCK_H
#define UI_DOCK_H

#include <stdbool.h>
#include <stdint.h>

#define UI_DOCK_TITLE_MAX     40
#define UI_DOCK_MESSAGE_MAX   96
#define UI_DOCK_ACTION_MAX    24
#define UI_DOCK_STACK_MAX      6
#define UI_DOCK_PENDING_MAX    6

typedef enum
{
    UI_DOCK_TYPE_INFO = 0,
    UI_DOCK_TYPE_ACTION,
    UI_DOCK_TYPE_CONFIRM,
    UI_DOCK_TYPE_CONTROL,
    UI_DOCK_TYPE_NAVIGATION,
    UI_DOCK_TYPE_MEDIA,
    UI_DOCK_TYPE_COUNT
} ui_dock_type_t;

typedef enum
{
    UI_DOCK_PRIORITY_P0_CRITICAL = 0,
    UI_DOCK_PRIORITY_P1_RIDING_SAFETY,
    UI_DOCK_PRIORITY_P2_RIDE_STATE,
    UI_DOCK_PRIORITY_P3_USER_ACTION,
    UI_DOCK_PRIORITY_P4_NORMAL
} ui_dock_priority_t;

typedef enum
{
    UI_DOCK_EVENT_SHOWN = 0,
    UI_DOCK_EVENT_HIDDEN,
    UI_DOCK_EVENT_UPDATED,
    UI_DOCK_EVENT_REPLACED,
    UI_DOCK_EVENT_OVERRIDDEN,
    UI_DOCK_EVENT_RESTORED,
    UI_DOCK_EVENT_QUEUED,
    UI_DOCK_EVENT_DROPPED
} ui_dock_event_t;

typedef enum
{
    UI_DOCK_INTERACTION_PRIMARY = 0,
    UI_DOCK_INTERACTION_SECONDARY,
    UI_DOCK_INTERACTION_DECREMENT,
    UI_DOCK_INTERACTION_TOGGLE,
    UI_DOCK_INTERACTION_INCREMENT,
    UI_DOCK_INTERACTION_VALUE_CHANGED,
    UI_DOCK_INTERACTION_DISMISS
} ui_dock_interaction_t;

typedef void (*ui_dock_action_cb_t)(uint32_t card_id,
                                    ui_dock_interaction_t interaction,
                                    int32_t value,
                                    void *context);

typedef struct
{
    uint32_t id;
    ui_dock_type_t type;
    ui_dock_priority_t priority;
    char title[UI_DOCK_TITLE_MAX];
    char message[UI_DOCK_MESSAGE_MAX];
    char primary_action[UI_DOCK_ACTION_MAX];
    char secondary_action[UI_DOCK_ACTION_MAX];
    int32_t value;
    int32_t minimum;
    int32_t maximum;
    bool dismissible;
    ui_dock_action_cb_t action_cb;
    void *context;
} ui_dock_card_t;

typedef void (*ui_dock_observer_cb_t)(ui_dock_event_t event,
                                      const ui_dock_card_t *card,
                                      void *user_data);

void ui_dock_init(void);
void ui_dock_set_observer(ui_dock_observer_cb_t observer, void *user_data);
bool ui_dock_show(const ui_dock_card_t *card);
bool ui_dock_hide(uint32_t id);
bool ui_dock_update(const ui_dock_card_t *card);
bool ui_dock_replace(const ui_dock_card_t *card);
bool ui_dock_invoke(uint32_t id, ui_dock_interaction_t interaction,
                    int32_t value);
bool ui_dock_is_visible(void);
const ui_dock_card_t *ui_dock_current(void);
uint8_t ui_dock_restore_depth(void);
uint8_t ui_dock_pending_count(void);
uint32_t ui_dock_dropped_count(void);
void ui_dock_clear(void);

#endif /* UI_DOCK_H */
