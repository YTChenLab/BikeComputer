#include "ui_power_page.h"

#include "ui_page_common.h"
#include "../core/ui_theme.h"

#include <stdint.h>

static ui_page_action_cb_t s_callback;
static void *s_user_data;

static void action_event(lv_event_t *event)
{
    if(s_callback != NULL)
        s_callback((ui_page_action_t)(uintptr_t)lv_event_get_user_data(event),
                   s_user_data);
}

lv_obj_t *ui_power_page_create(ui_page_action_cb_t callback, void *user_data)
{
    s_callback = callback;
    s_user_data = user_data;
    lv_obj_t *screen = ui_page_screen_create();
    ui_page_label(screen, 0, 245, 480, "电源菜单", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(screen, 0, 280, 480, "POWER MENU", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_button(screen, 80, 350, 320, 54, "关机", action_event,
                   (void *)(uintptr_t)UI_PAGE_ACTION_POWER_OFF);
    ui_page_button(screen, 80, 423, 320, 54, "重新启动", action_event,
                   (void *)(uintptr_t)UI_PAGE_ACTION_POWER_RESTART);
    ui_page_button(screen, 80, 496, 320, 54, "取消", action_event,
                   (void *)(uintptr_t)UI_PAGE_ACTION_POWER_CANCEL);
    return screen;
}
