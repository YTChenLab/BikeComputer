#include "platform/ui_platform_internal.h"
#include <stdatomic.h>
static atomic_flag s_default_lock = ATOMIC_FLAG_INIT;
void ui_platform_default_lock(void)
{ while(atomic_flag_test_and_set_explicit(&s_default_lock, memory_order_acquire)) {} }
void ui_platform_default_unlock(void)
{ atomic_flag_clear_explicit(&s_default_lock, memory_order_release); }
void ui_platform_default_lock_reset(void) { ui_platform_default_unlock(); }
void ui_platform_check_lock_context(void) {}
