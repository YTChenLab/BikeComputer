#include "ui/core/ui_memory.h"
#include "ui_settings_page.h"

#include "ui_page_common.h"
#include "../components/settings/ui_settings_list.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"

#include <stdio.h>

static ui_status_bar_t s_status;
static UI_LARGE_RAM ui_settings_list_t s_list;
static ui_page_action_cb_t s_callback;
static void *s_user_data;
static int s_storage_state = -1;

static void list_action(uint16_t id, void *user_data)
{
    (void)user_data;
    if(s_callback == NULL) return;
    switch((ui_settings_item_t)id) {
        case UI_SETTINGS_ITEM_WIFI:
            s_callback(UI_PAGE_ACTION_WIFI_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_BLUETOOTH:
            s_callback(UI_PAGE_ACTION_BLUETOOTH_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_GPS:
            s_callback(UI_PAGE_ACTION_GPS_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_SOUND:
            s_callback(UI_PAGE_ACTION_SOUND_CONTROL, s_user_data); break;
        case UI_SETTINGS_ITEM_BRIGHTNESS:
            s_callback(UI_PAGE_ACTION_BRIGHTNESS_CONTROL, s_user_data); break;
        case UI_SETTINGS_ITEM_POWER_SAVE:
            s_callback(UI_PAGE_ACTION_POWER_SAVE_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_TOUCH_LOCK:
            s_callback(UI_PAGE_ACTION_TOUCH_LOCK_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_SYSTEM:
            s_callback(UI_PAGE_ACTION_SYSTEM, s_user_data); break;
        case UI_SETTINGS_ITEM_POWER:
            s_callback(UI_PAGE_ACTION_POWER_MENU, s_user_data); break;
        case UI_SETTINGS_ITEM_AUTO_PAUSE:
            s_callback(UI_PAGE_ACTION_AUTO_PAUSE_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_REDUCED_MOTION:
            s_callback(UI_PAGE_ACTION_REDUCED_MOTION_TOGGLE, s_user_data); break;
        case UI_SETTINGS_ITEM_IDLE_SLEEP:
            s_callback(UI_PAGE_ACTION_IDLE_SLEEP_CYCLE, s_user_data); break;
        case UI_SETTINGS_ITEM_ZONES:
            s_callback(UI_PAGE_ACTION_ZONE_EDITOR, s_user_data); break;
        case UI_SETTINGS_ITEM_DASHBOARD_LAYOUT:
            s_callback(UI_PAGE_ACTION_DASHBOARD_LAYOUT, s_user_data); break;
        default: break;
    }
}

lv_obj_t *ui_settings_page_create(ui_page_action_cb_t callback,
                                  void *user_data)
{
    s_callback = callback;
    s_user_data = user_data;
    lv_obj_t *screen = ui_page_screen_create();
    lv_obj_set_style_bg_image_src(screen, NULL, 0);
    lv_obj_set_style_bg_color(screen, UI_TOKEN_COLOR_CANVAS, 0);
    ui_page_status_create(&s_status, screen);
    ui_page_title(screen, "系统设置", "SETTINGS");

    ui_settings_list_create(&s_list, screen, 8, 156, 464, UI_PAGE_LIST_WITH_FOOTNOTE_BOTTOM - 156,
                            list_action, NULL);
    (void)ui_settings_list_set_row_size(&s_list,UI_TOKEN_LIST_LARGE_ROW_HEIGHT,UI_TOKEN_LIST_LARGE_ROW_GAP);
    ui_liquid_selection_set_wide_focus(&s_list.liquid,true);
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_WIFI,
        UI_SETTINGS_ROW_TOGGLE, "WiFi", "网络连接与传输", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_BLUETOOTH,
        UI_SETTINGS_ROW_TOGGLE, "蓝牙", "连接骑行传感器", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_GPS,
        UI_SETTINGS_ROW_TOGGLE, "卫星定位", "卫星定位与导航", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_SOUND,
        UI_SETTINGS_ROW_SLIDER, "声音", "提示音音量", "62%");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_BRIGHTNESS,
        UI_SETTINGS_ROW_SLIDER, "亮度", "屏幕背光亮度", "75%");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_POWER_SAVE,
        UI_SETTINGS_ROW_TOGGLE, "省电模式", "降低部分系统功耗", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_TOUCH_LOCK,
        UI_SETTINGS_ROW_TOGGLE, "触摸锁定", "禁用触摸，保留实体键", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_SYSTEM,
        UI_SETTINGS_ROW_ACTION, "系统信息", "设备、版本与存储", "查看");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_POWER,
        UI_SETTINGS_ROW_ACTION, "电源设置", "关机或重新启动", "打开");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_AUTO_PAUSE,
        UI_SETTINGS_ROW_TOGGLE, "自动暂停", "低速暂停，恢复后继续", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_REDUCED_MOTION,
        UI_SETTINGS_ROW_TOGGLE, "减少动态效果", "减少界面弹性与转场", "");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_IDLE_SLEEP,
        UI_SETTINGS_ROW_SELECTION, "自动息屏", "息屏期间继续骑行记录", "关闭");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_ZONES,
        UI_SETTINGS_ROW_ACTION, "个人骑行区间", "心率、功率区间阈值", "编辑");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_DASHBOARD_LAYOUT,
        UI_SETTINGS_ROW_ACTION, "骑行数据布局", "预设布局与六格指标", "巡航");
    (void)ui_settings_list_add(&s_list, UI_SETTINGS_ITEM_CONFIGURATION_STORAGE,
        UI_SETTINGS_ROW_INFO, "配置存储", "自动保存本机偏好", "未接入");
    static const struct { ui_settings_item_t item; const char *symbol; } icons[] = {
        {UI_SETTINGS_ITEM_WIFI,LV_SYMBOL_WIFI},
        {UI_SETTINGS_ITEM_BLUETOOTH,LV_SYMBOL_BLUETOOTH},
        {UI_SETTINGS_ITEM_GPS,LV_SYMBOL_GPS},
        {UI_SETTINGS_ITEM_SOUND,LV_SYMBOL_VOLUME_MAX},
        {UI_SETTINGS_ITEM_BRIGHTNESS,LV_SYMBOL_IMAGE},
        {UI_SETTINGS_ITEM_POWER_SAVE,LV_SYMBOL_CHARGE},
        {UI_SETTINGS_ITEM_TOUCH_LOCK,LV_SYMBOL_CLOSE},
        {UI_SETTINGS_ITEM_SYSTEM,LV_SYMBOL_LIST},
        {UI_SETTINGS_ITEM_POWER,LV_SYMBOL_POWER},
        {UI_SETTINGS_ITEM_AUTO_PAUSE,LV_SYMBOL_PAUSE},
        {UI_SETTINGS_ITEM_REDUCED_MOTION,LV_SYMBOL_REFRESH},
        {UI_SETTINGS_ITEM_IDLE_SLEEP,LV_SYMBOL_EYE_CLOSE},
        {UI_SETTINGS_ITEM_ZONES,LV_SYMBOL_BARS},
        {UI_SETTINGS_ITEM_DASHBOARD_LAYOUT,LV_SYMBOL_BARS},
        {UI_SETTINGS_ITEM_CONFIGURATION_STORAGE,LV_SYMBOL_SAVE}
    };
    for(unsigned i=0;i<sizeof(icons)/sizeof(icons[0]);++i)
        ui_settings_list_set_accent(&s_list,icons[i].item,UI_TOKEN_ACCENT_HEX,icons[i].symbol);
    lv_obj_t *scroll_hint=ui_page_label(screen,20,UI_PAGE_FOOTNOTE_Y,440,"",UI_FONT_PAGE_TEXT,LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_color(scroll_hint,UI_TOKEN_COLOR_TEXT_SECONDARY,0);
    ui_settings_list_set_scroll_hint(&s_list,scroll_hint);
    s_storage_state=-1;
    ui_settings_page_set_storage_state(ui_preferences_storage_state());

    ui_app_state_t state;
    if(ui_app_state_get(&state)) ui_settings_page_set_state(&state);
    return screen;
}

void ui_settings_page_set_state(const ui_app_state_t *state)
{
    if(state == NULL || s_list.root == NULL) return;
    char text[16];
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_WIFI,
                                      state->wifi_enabled);
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_BLUETOOTH,
                                      state->bluetooth_enabled);
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_GPS,
                                      state->gps_enabled);
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_POWER_SAVE,
                                      state->power_save_enabled);
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_TOUCH_LOCK,
                                      state->touch_locked);
    snprintf(text, sizeof(text), "%u%%", state->sound_level);
    (void)ui_settings_list_set_value(&s_list, UI_SETTINGS_ITEM_SOUND, text);
    snprintf(text, sizeof(text), "%u%%", state->brightness_level);
    (void)ui_settings_list_set_value(&s_list, UI_SETTINGS_ITEM_BRIGHTNESS,
                                     text);
}

void ui_settings_page_set_preferences(bool auto_pause, bool reduced_motion, uint32_t idle_sleep_ms)
{
    if(s_list.root == NULL) return;
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_AUTO_PAUSE, auto_pause);
    (void)ui_settings_list_set_toggle(&s_list, UI_SETTINGS_ITEM_REDUCED_MOTION, reduced_motion);
    char text[20];
    if(idle_sleep_ms == 0) snprintf(text,sizeof(text),"关闭");
    else snprintf(text,sizeof(text),"%lu 秒",(unsigned long)(idle_sleep_ms/1000));
    (void)ui_settings_list_set_value(&s_list, UI_SETTINGS_ITEM_IDLE_SLEEP, text);
}

void ui_settings_page_set_storage_state(ui_preferences_storage_state_t state)
{
    if(s_list.root == NULL || s_storage_state == (int)state) return;
    s_storage_state=state;
    const char *text=state==UI_PREF_STORAGE_READY ? "已保存" :
        state==UI_PREF_STORAGE_DEFAULT ? "默认配置" :
        state==UI_PREF_STORAGE_PENDING ? "待保存" :
        state==UI_PREF_STORAGE_FAILED ? "保存失败" : "未接入";
    (void)ui_settings_list_set_value(&s_list,UI_SETTINGS_ITEM_CONFIGURATION_STORAGE,text);
    (void)ui_settings_list_set_feedback(&s_list,UI_SETTINGS_ITEM_CONFIGURATION_STORAGE,
        state==UI_PREF_STORAGE_FAILED ? UI_SETTINGS_ROW_FAILED : UI_SETTINGS_ROW_IDLE);
}

void ui_settings_page_set_dashboard_preset(unsigned preset)
{
    if(s_list.root != NULL)
        (void)ui_settings_list_set_value(&s_list, UI_SETTINGS_ITEM_DASHBOARD_LAYOUT,
            ui_dashboard_preset_name(preset));
}

void ui_settings_page_set_feedback(ui_settings_item_t item,
                                   ui_settings_feedback_t feedback)
{
    ui_settings_row_feedback_t row_feedback = UI_SETTINGS_ROW_IDLE;
    if(feedback == UI_SETTINGS_FEEDBACK_PENDING)
        row_feedback = UI_SETTINGS_ROW_PENDING;
    else if(feedback == UI_SETTINGS_FEEDBACK_FAILED)
        row_feedback = UI_SETTINGS_ROW_FAILED;
    (void)ui_settings_list_set_feedback(&s_list, (uint16_t)item,
                                        row_feedback);
}

bool ui_settings_page_handle_input(ui_input_event_t input)
{
    return ui_settings_list_handle_input(&s_list, input);
}

bool ui_settings_page_handle_key(uint32_t key)
{
    if(key == LV_KEY_DOWN)
        return ui_settings_page_handle_input(UI_INPUT_ENCODER_CW);
    if(key == LV_KEY_UP)
        return ui_settings_page_handle_input(UI_INPUT_ENCODER_CCW);
    if(key == LV_KEY_ENTER)
        return ui_settings_page_handle_input(UI_INPUT_ENCODER_PRESS);
    return false;
}

bool ui_settings_page_contains_scroll_point(int32_t x, int32_t y)
{
    return ui_settings_list_contains_point(&s_list, x, y);
}
