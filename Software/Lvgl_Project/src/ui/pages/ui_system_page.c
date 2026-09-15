#include "ui_system_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_theme.h"

#include <stdint.h>

static ui_status_bar_t s_status;
static ui_page_action_cb_t s_callback;
static void *s_user_data;

static void action_event(lv_event_t *event)
{
    if(s_callback != NULL)
        s_callback((ui_page_action_t)(uintptr_t)lv_event_get_user_data(event),
                   s_user_data);
}

lv_obj_t *ui_system_page_create(ui_page_action_cb_t callback,
                                void *user_data, bool wifi, bool bluetooth)
{
    s_callback = callback;
    s_user_data = user_data;
    lv_obj_t *screen = ui_page_screen_create();
    ui_page_status_create(&s_status, screen, wifi, bluetooth);
    ui_page_title(screen, "系统信息", "SYSTEM INFORMATION");
    const char *keys[] = {
        "设备型号", "固件版本", "序列号", "存储空间", "系统健康"
    };
    const char *values[] = {
        "BIKE-SV 01", "V1.0.0", "UI-20260909", "72%", "良好"
    };
    for(int32_t i = 0; i < 5; i++) {
        int32_t y = 175 + i * 70;
        ui_page_line(screen, 20, y + 51, 440, 1, UI_COLOR_WHITE);
        ui_page_label(screen, 30, y, 170, keys[i], UI_FONT_PAGE_TEXT,
                      LV_TEXT_ALIGN_LEFT);
        ui_page_label(screen, 205, y, 225, values[i], UI_FONT_PAGE_TEXT,
                      LV_TEXT_ALIGN_RIGHT);
    }
    ui_page_button(screen, 20, 570, 440, 52, "恢复出厂设置", action_event,
                   (void *)(uintptr_t)UI_PAGE_ACTION_POWER_CANCEL);
    ui_page_button(screen, 20, 640, 440, 52, "返回设置", action_event,
                   (void *)(uintptr_t)UI_PAGE_ACTION_SETTINGS);
    return screen;
}

void ui_system_page_set_connectivity(bool wifi, bool bluetooth)
{
    ui_page_status_set_connectivity(&s_status, wifi, bluetooth);
}
