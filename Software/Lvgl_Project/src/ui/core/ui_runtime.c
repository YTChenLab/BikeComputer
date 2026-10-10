#include "ui_runtime.h"
#include <stdatomic.h>
#include <stddef.h>

_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "UiTask diagnostics require lock-free 32-bit C11 atomics");
static ui_runtime_port_t s_port;
static atomic_uint s_counters[UI_DIAG_COUNT];
static atomic_uint s_calls, s_last_us, s_max_us;

bool ui_runtime_register(const ui_runtime_port_t *port)
{
    if(s_port.now_ms || !port || !port->now_ms || !port->is_ui_task || !port->is_isr ||
       !port->fault || port->is_isr(port->context) || !port->is_ui_task(port->context)) return false;
    s_port = *port;
    return true;
}
bool ui_runtime_registered(void) { return s_port.now_ms != NULL; }
bool ui_runtime_is_isr(void) { return s_port.is_isr && s_port.is_isr(s_port.context); }
void ui_runtime_count(ui_diagnostic_counter_t counter)
{
    if((unsigned)counter < UI_DIAG_COUNT)
        atomic_fetch_add_explicit(&s_counters[counter], 1U, memory_order_relaxed);
}
bool ui_runtime_require_task(void)
{
#if defined(UI_TARGET_STM32)
    if(!ui_runtime_registered()) {
        ui_runtime_count(UI_DIAG_CONTEXT_REJECTED);
        return false;
    }
#endif
    if(!ui_runtime_is_isr()) return true;
    ui_runtime_count(UI_DIAG_CONTEXT_REJECTED);
    return false;
}
bool ui_runtime_require_ui(void)
{
    if(!ui_runtime_require_task()) return false;
    if(!s_port.is_ui_task || s_port.is_ui_task(s_port.context)) return true;
    ui_runtime_count(UI_DIAG_CONTEXT_REJECTED);
    return false;
}
bool ui_runtime_require_worker(void)
{
    if(!ui_runtime_require_task()) return false;
    if(!s_port.is_ui_task || !s_port.is_ui_task(s_port.context)) return true;
    ui_runtime_count(UI_DIAG_CONTEXT_REJECTED);
    return false;
}
uint32_t ui_runtime_now_ms(void) { return s_port.now_ms ? s_port.now_ms(s_port.context) : 0; }
uint32_t ui_runtime_now_us(void)
{ return s_port.now_us ? s_port.now_us(s_port.context) : ui_runtime_now_ms() * 1000U; }
void ui_runtime_fault(const char *reason)
{ if(s_port.fault) s_port.fault(reason, s_port.context); }
void ui_runtime_record_handler(uint32_t duration_us)
{
    /* Only UiTask writes handler timing; diagnostics may read from another task. */
    atomic_fetch_add_explicit(&s_calls, 1U, memory_order_relaxed);
    atomic_store_explicit(&s_last_us, duration_us, memory_order_relaxed);
    if(duration_us > atomic_load_explicit(&s_max_us, memory_order_relaxed))
        atomic_store_explicit(&s_max_us, duration_us, memory_order_relaxed);
}
void ui_runtime_get_diagnostics(ui_runtime_diagnostics_t *out)
{
    if(!out) return;
    for(unsigned i = 0; i < UI_DIAG_COUNT; ++i)
        out->counters[i] = atomic_load_explicit(&s_counters[i], memory_order_relaxed);
    out->handler_calls = atomic_load_explicit(&s_calls, memory_order_relaxed);
    out->handler_last_us = atomic_load_explicit(&s_last_us, memory_order_relaxed);
    out->handler_max_us = atomic_load_explicit(&s_max_us, memory_order_relaxed);
    out->microsecond_clock = s_port.now_us != NULL;
}
