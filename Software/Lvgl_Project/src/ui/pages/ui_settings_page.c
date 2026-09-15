#include "ui_settings_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_sound.h"
#include "../core/ui_theme.h"

#include <stdint.h>

#define SETTINGS_ROW_COUNT 8

static ui_status_bar_t s_status;
static ui_page_action_cb_t s_callback;
static void *s_user_data;
static lv_obj_t *s_rows[SETTINGS_ROW_COUNT];
static uint8_t s_selection;

static const char * const s_names[SETTINGS_ROW_COUNT] = {
    "传感器", "显示", "声音", "电池模式",
    "网络", "骑行设置", "系统", "关于设备"
};

static void update_selection(void)
{
    for(uint8_t i = 0; i < SETTINGS_ROW_COUNT; i++) {
        if(s_rows[i] == NULL) continue;
        lv_obj_set_style_border_color(s_rows[i],
                                      i == s_selection ? UI_COLOR_YELLOW :
                                                         UI_COLOR_WHITE, 0);
        lv_obj_set_style_border_width(s_rows[i], i == s_selection ? 2 : 1, 0);
    }
}

static void row_event(lv_event_t *event)
{
    uint8_t index = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    if(index >= SETTINGS_ROW_COUNT) return;
    s_selection = index;
    update_selection();
    ui_sound_play(UI_SFX_PRESS);
    if(index == 6 && s_callback != NULL)
        s_callback(UI_PAGE_ACTION_SYSTEM, s_user_data);
}

lv_obj_t *ui_settings_page_create(ui_page_action_cb_t callback,
                                  void *user_data)
{
    s_callback = callback;
    s_user_data = user_data;
    s_selection = 0;
    lv_obj_t *screen = ui_page_screen_create();
    ui_page_status_create(&s_status, screen);
    ui_page_title(screen, "设置", "SETTINGS");

    const char *group_titles[] = {"设备连接", "骑行体验", "系统"};
    const uint8_t group_count[] = {3, 3, 2};
    const char *states[] = {
        "已连接", "自动", "标准", "省电", "已连接", "默认", "", ""
    };
    int32_t row_index = 0;
    int32_t y = 162;
    for(int32_t group = 0; group < 3; group++) {
        ui_page_label(screen, 28, y, 220, group_titles[group],
                      UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_LEFT);
        y += 28;
        lv_obj_t *card = ui_page_card(screen, 20, y, 440,
                                      group_count[group] * 54);
        for(int32_t row = 0; row < group_count[group]; row++, row_index++) {
            int32_t row_y = row * 54;
            s_rows[row_index] = ui_page_box(card, 0, row_y, 440, 54,
                                            row_index == 0 ? UI_COLOR_YELLOW :
                                                             UI_COLOR_WHITE,
                                            row_index == 0 ? 2 : 0);
            lv_obj_set_style_bg_opa(s_rows[row_index], LV_OPA_TRANSP, 0);
            lv_obj_set_style_radius(s_rows[row_index], 0, 0);
            lv_obj_add_flag(s_rows[row_index], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(s_rows[row_index], row_event, LV_EVENT_CLICKED,
                                (void *)(uintptr_t)row_index);
            ui_page_label(s_rows[row_index], 18, 16, 180, s_names[row_index],
                          UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_LEFT);
            ui_page_label(s_rows[row_index], 210, 16, 160, states[row_index],
                          UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_RIGHT);
            ui_page_label(s_rows[row_index], 382, 16, 34, ">",
                          UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_RIGHT);
            if(row + 1 < group_count[group])
                ui_page_line(card, 18, row_y + 53, 404, 1, UI_COLOR_WHITE);
        }
        y += group_count[group] * 54 + 18;
    }
    return screen;
}

bool ui_settings_page_handle_key(uint32_t key)
{
    if(key == LV_KEY_UP && s_selection > 0) s_selection--;
    else if(key == LV_KEY_DOWN && s_selection + 1 < SETTINGS_ROW_COUNT)
        s_selection++;
    else if(key == LV_KEY_ENTER) {
        if(s_selection == 6 && s_callback != NULL)
            s_callback(UI_PAGE_ACTION_SYSTEM, s_user_data);
        return true;
    }
    else return false;

    update_selection();
    ui_sound_play(UI_SFX_FOCUS);
    return true;
}
