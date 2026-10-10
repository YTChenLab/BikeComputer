#ifndef UI_RUNTIME_H
#define UI_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_DIAG_CONTEXT_REJECTED, UI_DIAG_MAILBOX_FAILURE,
    UI_DIAG_DISPLAY_FAILURE, UI_DIAG_ENCODER_CLIPPED,
    UI_DIAG_INPUT_INVALID, UI_DIAG_COUNT
} ui_diagnostic_counter_t;
typedef struct {
    uint32_t (*now_ms)(void *context);
    uint32_t (*now_us)(void *context); /* Optional; wrapping monotonic clock. */
    bool (*is_ui_task)(void *context);
    bool (*is_isr)(void *context);
    void (*fault)(const char *reason, void *context);
    void *context;
} ui_runtime_port_t;
typedef struct {
    uint32_t counters[UI_DIAG_COUNT];
    uint32_t handler_calls, handler_last_us, handler_max_us;
    bool microsecond_clock;
} ui_runtime_diagnostics_t;

/* Register once on UiTask before producers; callbacks must read cached state. */
bool ui_runtime_register(const ui_runtime_port_t *port);
bool ui_runtime_registered(void);
bool ui_runtime_is_isr(void);
bool ui_runtime_require_task(void);
bool ui_runtime_require_worker(void);
bool ui_runtime_require_ui(void);
uint32_t ui_runtime_now_ms(void);
uint32_t ui_runtime_now_us(void);
void ui_runtime_fault(const char *reason);
void ui_runtime_count(ui_diagnostic_counter_t counter);
void ui_runtime_record_handler(uint32_t duration_us);
void ui_runtime_get_diagnostics(ui_runtime_diagnostics_t *diagnostics);
#endif
