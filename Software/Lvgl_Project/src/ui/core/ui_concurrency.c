#include "ui_concurrency.h"

#include <stdatomic.h>
#include <stddef.h>

static atomic_flag s_default_lock = ATOMIC_FLAG_INIT;
static ui_concurrency_lock_cb_t s_lock;
static ui_concurrency_unlock_cb_t s_unlock;
static void *s_context;

void ui_concurrency_set_port(ui_concurrency_lock_cb_t lock,
                             ui_concurrency_unlock_cb_t unlock,
                             void *context)
{
    if(lock == NULL || unlock == NULL) {
        ui_concurrency_reset_port();
        return;
    }
    s_lock = lock;
    s_unlock = unlock;
    s_context = context;
}

void ui_concurrency_reset_port(void)
{
    s_lock = NULL;
    s_unlock = NULL;
    s_context = NULL;
    atomic_flag_clear_explicit(&s_default_lock, memory_order_release);
}

void ui_concurrency_lock(void)
{
    if(s_lock != NULL) {
        s_lock(s_context);
        return;
    }
    while(atomic_flag_test_and_set_explicit(&s_default_lock,
                                             memory_order_acquire)) {
    }
}

void ui_concurrency_unlock(void)
{
    if(s_unlock != NULL) {
        s_unlock(s_context);
        return;
    }
    atomic_flag_clear_explicit(&s_default_lock, memory_order_release);
}
