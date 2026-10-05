#ifndef UI_NAVIGATION_ADAPTER_EXAMPLE_H
#define UI_NAVIGATION_ADAPTER_EXAMPLE_H
#include "platform/stm32/ui_stm32_services.h"
#include "services/ui_nav_store.h"
typedef struct {
    /* These callbacks copy requests to worker queues; no storage/routing I/O here. */
    bool (*enqueue_navigation)(const ui_nav_request_t *request,void *context);
    bool (*enqueue_map)(const ui_nav_map_request_t *request,void *context);
    void (*cancel_navigation)(uint32_t request_id,void *context);
    void (*cancel_map)(uint32_t request_id,void *context);
    void *context;
    uint32_t capabilities;
} bike_navigation_transport_t;
bool bike_ui_attach_navigation(const bike_navigation_transport_t *transport);
#endif
