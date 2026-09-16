#ifndef UI_BOTTOM_SURFACE_H
#define UI_BOTTOM_SURFACE_H

#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

#define UI_BOTTOM_SURFACE_STACK_MAX 6U
#define UI_BOTTOM_SURFACE_NO_OWNER  0U

typedef enum
{
    UI_BOTTOM_SURFACE_SIZE_MINI = 0,
    UI_BOTTOM_SURFACE_SIZE_NORMAL,
    UI_BOTTOM_SURFACE_SIZE_EXPANDED,
    UI_BOTTOM_SURFACE_SIZE_COUNT
} ui_bottom_surface_size_t;

typedef enum
{
    UI_BOTTOM_SURFACE_DYNAMIC_CARD_MODE = 0,
    UI_BOTTOM_SURFACE_INTERACTION_MODE
} ui_bottom_surface_mode_t;

typedef enum
{
    UI_BOTTOM_SURFACE_PRIORITY_P0_CRITICAL = 0,
    UI_BOTTOM_SURFACE_PRIORITY_P1_RIDING_SAFETY,
    UI_BOTTOM_SURFACE_PRIORITY_P2_RIDE_STATE,
    UI_BOTTOM_SURFACE_PRIORITY_P3_USER_ACTION,
    UI_BOTTOM_SURFACE_PRIORITY_P4_NORMAL
} ui_bottom_surface_priority_t;

typedef struct
{
    ui_bottom_surface_mode_t mode;
    ui_bottom_surface_size_t size;
    ui_bottom_surface_priority_t priority;
    uint32_t owner_id;
    uint32_t version;
    uint8_t restore_depth;
    bool mounted;
    bool dynamic_content_visible;
} ui_bottom_surface_state_t;

typedef void (*ui_bottom_surface_observer_cb_t)(
    const ui_bottom_surface_state_t *state,
    void *user_data);

void ui_bottom_surface_init(void);
bool ui_bottom_surface_create(lv_obj_t *parent);
bool ui_bottom_surface_is_created(void);

lv_obj_t *ui_bottom_surface_root(void);
lv_obj_t *ui_bottom_surface_dynamic_host(void);
lv_obj_t *ui_bottom_surface_interaction_host(void);

bool ui_bottom_surface_mount(lv_obj_t *slot,
                             bool dynamic_content_visible);
void ui_bottom_surface_unmount(void);
void ui_bottom_surface_set_dynamic_focus(bool focused);
bool ui_bottom_surface_present_interaction(
    uint32_t owner_id,
    ui_bottom_surface_size_t size,
    ui_bottom_surface_priority_t priority,
    bool animated);
bool ui_bottom_surface_update_interaction(uint32_t owner_id,
                                          ui_bottom_surface_size_t size,
                                          bool animated);
bool ui_bottom_surface_replace_interaction(
    uint32_t owner_id,
    ui_bottom_surface_size_t size,
    ui_bottom_surface_priority_t priority,
    bool animated);
bool ui_bottom_surface_dismiss_interaction(uint32_t owner_id,
                                           bool animated);

bool ui_bottom_surface_get_state(ui_bottom_surface_state_t *state);
void ui_bottom_surface_set_observer(ui_bottom_surface_observer_cb_t observer,
                                    void *user_data);

#endif /* UI_BOTTOM_SURFACE_H */
