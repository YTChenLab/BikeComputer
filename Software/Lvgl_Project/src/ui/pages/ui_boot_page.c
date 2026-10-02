#include "ui_boot_page.h"
#include "ui_boot_motion.h"
#include "../../services/ui_preferences.h"

static lv_obj_t *s_boot_screen;

static void deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event) == s_boot_screen) s_boot_screen = NULL;
}

lv_obj_t *ui_boot_page_create(void)
{
    s_boot_screen = ui_boot_motion_create(NULL);
    if(s_boot_screen != NULL)
        lv_obj_add_event_cb(s_boot_screen, deleted, LV_EVENT_DELETE, NULL);
    return s_boot_screen;
}

void ui_boot_page_set_active(bool active)
{
    ui_preferences_t preferences;
    bool reduced = active && ui_preferences_get(&preferences) && preferences.reduced_motion;
    ui_boot_motion_set_active(s_boot_screen, active, reduced);
}

bool ui_boot_page_is_complete(void)
{
    return ui_boot_motion_is_complete(s_boot_screen);
}
