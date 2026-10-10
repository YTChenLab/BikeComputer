#include "ui_concurrency.h"

#include "platform/ui_platform_internal.h"
#include <stddef.h>

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
    ui_platform_default_lock_reset();
}

void ui_concurrency_lock(void)
{
    ui_platform_check_lock_context();
    if(s_lock != NULL) {
        s_lock(s_context);
        return;
    }
    ui_platform_default_lock();
}

void ui_concurrency_unlock(void)
{
    if(s_unlock != NULL) {
        s_unlock(s_context);
        return;
    }
    ui_platform_default_unlock();
}
