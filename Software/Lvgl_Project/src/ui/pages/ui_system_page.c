#include "ui/core/ui_memory.h"
#include "ui_system_page.h"

#include "ui_page_common.h"
#include "../core/ui_version.h"
#include "../components/settings/ui_settings_list.h"

enum
{
    SYSTEM_ROW_MODEL = 0,
    SYSTEM_ROW_UI_VERSION,
    SYSTEM_ROW_LVGL,
    SYSTEM_ROW_STORAGE,
    SYSTEM_ROW_TARGET,
    SYSTEM_ROW_HEALTH,
    SYSTEM_ROW_POWER,
    SYSTEM_ROW_FIRMWARE
};

static ui_status_bar_t s_status;
static UI_LARGE_RAM ui_settings_list_t s_list;
static ui_page_action_cb_t s_callback;
static void *s_user_data;

static void list_action(uint16_t id, void *user_data)
{
    (void)user_data;
    if(id == SYSTEM_ROW_POWER && s_callback != NULL)
        s_callback(UI_PAGE_ACTION_POWER_MENU, s_user_data);
}

lv_obj_t *ui_system_page_create(ui_page_action_cb_t callback,
                                void *user_data)
{
    s_callback = callback;
    s_user_data = user_data;
    lv_obj_t *screen = ui_page_screen_create();
    ui_page_status_create(&s_status, screen);
    ui_page_title(screen, "系统信息", "SYSTEM INFORMATION");
    /* This page owns the full viewport, so diagnostics can be inspected
     * without a bottom dock covering the last rows. */
    ui_settings_list_create(&s_list, screen, 20, 156, 440, UI_PAGE_CONTENT_BOTTOM - 156,
                            list_action, NULL);
    (void)ui_settings_list_add(&s_list, SYSTEM_ROW_MODEL,
        UI_SETTINGS_ROW_INFO, "设备型号", "DEVICE MODEL", "BIKE COMPUTER");
    (void)ui_settings_list_add(&s_list, SYSTEM_ROW_UI_VERSION,
        UI_SETTINGS_ROW_INFO, "UI 版本", "SOFTWARE RELEASE", UI_SOFTWARE_VERSION);
    (void)ui_settings_list_add(&s_list, SYSTEM_ROW_LVGL,
        UI_SETTINGS_ROW_INFO, "LVGL 版本", "GRAPHICS RUNTIME", "9.3-dev");
    (void)ui_settings_list_add(&s_list, SYSTEM_ROW_STORAGE,
        UI_SETTINGS_ROW_INFO, "存储设备", "STORAGE STATUS", "待诊断");
    (void)ui_settings_list_add(&s_list, SYSTEM_ROW_TARGET,
        UI_SETTINGS_ROW_INFO, "TARGET", "HARDWARE PLATFORM", "STM32 + LTDC");
    (void)ui_settings_list_add(&s_list, SYSTEM_ROW_HEALTH,
        UI_SETTINGS_ROW_INFO, "系统健康", "SYSTEM HEALTH", "待诊断");
    (void)ui_settings_list_add(&s_list, SYSTEM_ROW_POWER,
        UI_SETTINGS_ROW_ACTION, "电源设置", "关机或重新启动", "OPEN");
    (void)ui_settings_list_add(&s_list, SYSTEM_ROW_FIRMWARE,
        UI_SETTINGS_ROW_INFO, "硬件固件", "FIRMWARE VERSION", "待接入");
    return screen;
}

void ui_system_page_set_info(const ui_system_info_t *info)
{
    if(info == NULL || s_list.root == NULL) return;
    (void)ui_settings_list_set_value(&s_list, SYSTEM_ROW_MODEL,
                                     info->device_model);
    (void)ui_settings_list_set_value(&s_list, SYSTEM_ROW_UI_VERSION,
                                     info->ui_version);
    (void)ui_settings_list_set_value(&s_list, SYSTEM_ROW_LVGL,
                                     info->lvgl_version);
    (void)ui_settings_list_set_value(&s_list, SYSTEM_ROW_STORAGE,
                                     info->storage_status);
    (void)ui_settings_list_set_value(&s_list, SYSTEM_ROW_TARGET,
                                     info->hardware_target);
    (void)ui_settings_list_set_value(&s_list, SYSTEM_ROW_HEALTH,
                                     info->system_health);
    if(info->firmware_version != NULL)
        (void)ui_settings_list_set_value(&s_list, SYSTEM_ROW_FIRMWARE, info->firmware_version);
}

bool ui_system_page_handle_input(ui_input_event_t input)
{
    return ui_settings_list_handle_input(&s_list, input);
}

bool ui_system_page_contains_scroll_point(int32_t x, int32_t y)
{
    return ui_settings_list_contains_point(&s_list, x, y);
}
