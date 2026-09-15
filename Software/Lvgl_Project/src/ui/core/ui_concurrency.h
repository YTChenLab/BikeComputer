#ifndef UI_CONCURRENCY_H
#define UI_CONCURRENCY_H

typedef void (*ui_concurrency_lock_cb_t)(void *context);
typedef void (*ui_concurrency_unlock_cb_t)(void *context);

/*
 * PC uses the built-in atomic lock. STM32 may register an RTOS mutex or a
 * short critical-section pair before producer tasks start.
 * These APIs are task/thread safe, not ISR entry points.
 */
void ui_concurrency_set_port(ui_concurrency_lock_cb_t lock,
                             ui_concurrency_unlock_cb_t unlock,
                             void *context);
void ui_concurrency_reset_port(void);
void ui_concurrency_lock(void);
void ui_concurrency_unlock(void);

#endif /* UI_CONCURRENCY_H */
