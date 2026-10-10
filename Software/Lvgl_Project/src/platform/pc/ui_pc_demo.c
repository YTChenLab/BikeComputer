#include "services/ui_navigation_service.h"
static bool s_demo_enabled = true;
static void (*s_demo_step)(void);
void ui_navigation_service_register_demo_step(void (*callback)(void)) { s_demo_step=callback; }
bool ui_navigation_service_demo_step(void)
{ if(!s_demo_enabled||!s_demo_step) return false; s_demo_step(); return true; }
void ui_navigation_service_set_demo_enabled(bool enabled) { s_demo_enabled = enabled; }
bool ui_navigation_service_demo_enabled(void) { return s_demo_enabled; }
