#ifndef UI_DEVICES_PAGE_H
#define UI_DEVICES_PAGE_H

#include "ui_page_action.h"
#include "../core/ui_input.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

/* A small, board-independent view model for future BLE discovery.  The
 * transport layer can update a device row without changing page layout. */
typedef enum
{
    UI_DEVICE_PHONE = 0,
    UI_DEVICE_HEART_RATE,
    UI_DEVICE_WATCH,
    UI_DEVICE_POWER,
    UI_DEVICE_SPEED_CADENCE,
    UI_DEVICE_COUNT
} ui_device_kind_t;
/* Nonblocking UI-thread requests. Genuine connection state is supplied through
 * set_status; accepting a request never changes a row to "connected". */
typedef struct {
    bool (*scan)(void *context);
    bool (*connect)(ui_device_kind_t kind,void *context);
    void *context;
} ui_device_connection_port_t;
void ui_devices_page_register_connection_port(const ui_device_connection_port_t *port);
bool ui_devices_page_request_scan(void);
bool ui_devices_page_request_selected(void);

lv_obj_t *ui_devices_page_create(ui_page_action_cb_t callback,
                                 void *user_data);
void ui_devices_page_set_active(bool active);
bool ui_devices_page_handle_input(ui_input_event_t input);
bool ui_devices_page_contains_scroll_point(int32_t x, int32_t y);
unsigned ui_devices_page_selected_device(void);
void ui_devices_page_set_status(ui_device_kind_t kind, const char *status,
                                uint32_t color);
void ui_devices_page_set_feedback(const char *message);

#endif /* UI_DEVICES_PAGE_H */
