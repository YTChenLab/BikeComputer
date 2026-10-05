#include "ui_navigation_adapter_example.h"
#include <stddef.h>
static bike_navigation_transport_t s_transport;
static bool dispatch(const ui_nav_request_t *request,void *context)
{ (void)context; return s_transport.enqueue_navigation(request,s_transport.context); }
static bool map_request(const ui_nav_map_request_t *request,void *context)
{ (void)context; return s_transport.enqueue_map(request,s_transport.context); }
static void cancel_navigation(uint32_t id,void *context)
{ (void)context; if(s_transport.cancel_navigation) s_transport.cancel_navigation(id,s_transport.context); }
static void cancel_map(uint32_t id,void *context)
{ (void)context; if(s_transport.cancel_map) s_transport.cancel_map(id,s_transport.context); }
bool bike_ui_attach_navigation(const bike_navigation_transport_t *transport)
{
    if(!transport || !transport->enqueue_navigation || !transport->enqueue_map) return false;
    s_transport=*transport;
    const ui_nav_backend_t navigation={dispatch,cancel_navigation,NULL,NULL};
    const ui_nav_map_backend_t maps={map_request,cancel_map,NULL};
    bool attached=ui_stm32_services_register_navigation(&navigation,&maps);
    if(attached)ui_nav_store_set_capabilities(transport->capabilities);
    return attached;
}
