#include "ui/core/ui_memory.h"
#include "ui_power_page.h"

#include "ui_page_common.h"
#include "../components/settings/ui_settings_list.h"

enum
{
    POWER_ROW_OFF = 0,
    POWER_ROW_RESTART,
    POWER_ROW_CANCEL
};

static ui_status_bar_t s_status;
static UI_LARGE_RAM ui_settings_list_t s_list;
static ui_page_action_cb_t s_callback;
static void *s_user_data;

static void list_action(uint16_t id, void *user_data)
{
    (void)user_data;
    if(s_callback == NULL) return;
    if(id == POWER_ROW_OFF)
        s_callback(UI_PAGE_ACTION_POWER_OFF, s_user_data);
    else if(id == POWER_ROW_RESTART)
        s_callback(UI_PAGE_ACTION_POWER_RESTART, s_user_data);
    else if(id == POWER_ROW_CANCEL)
        s_callback(UI_PAGE_ACTION_POWER_CANCEL, s_user_data);
}

lv_obj_t *ui_power_page_create(ui_page_action_cb_t callback, void *user_data)
{
    s_callback = callback;
    s_user_data = user_data;
    lv_obj_t *screen = ui_page_screen_create();
    ui_page_status_create(&s_status, screen);
    ui_page_title(screen, "电源设置", "POWER");
    ui_settings_list_create(&s_list, screen, 20, 176, 440, 164,
                            list_action, NULL);
    (void)ui_settings_list_add(&s_list, POWER_ROW_OFF,
        UI_SETTINGS_ROW_ACTION, "关机", "POWER OFF DEVICE", "SELECT");
    (void)ui_settings_list_add(&s_list, POWER_ROW_RESTART,
        UI_SETTINGS_ROW_ACTION, "重新启动", "RESTART SYSTEM", "SELECT");
    (void)ui_settings_list_add(&s_list, POWER_ROW_CANCEL,
        UI_SETTINGS_ROW_ACTION, "取消", "RETURN TO PREVIOUS PAGE", "BACK");
    /* Keep cancel in the encoder order while presenting the shared footer. */
    ui_settings_row_t *cancel = &s_list.rows[POWER_ROW_CANCEL];
    lv_obj_set_parent(cancel->root, screen);
    lv_obj_set_pos(cancel->root, 16, UI_PAGE_ACTION_Y);
    lv_obj_set_size(cancel->root, 448, UI_PAGE_ACTION_HEIGHT);
    for(uint32_t i = 0; i < lv_obj_get_child_count(cancel->root); ++i)
        lv_obj_add_flag(lv_obj_get_child(cancel->root, i), LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_parent(cancel->title, cancel->root);
    lv_obj_clear_flag(cancel->title, LV_OBJ_FLAG_HIDDEN);
    ui_align_label_center(cancel->title);
    return screen;
}

bool ui_power_page_handle_input(ui_input_event_t input)
{
    return ui_settings_list_handle_input(&s_list, input);
}
