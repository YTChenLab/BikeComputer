#include "ui_profile_page.h"

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

lv_obj_t *ui_profile_page_create(ui_page_action_cb_t callback,
                                 void *user_data, bool wifi, bool bluetooth)
{
    s_callback = callback;
    s_user_data = user_data;
    lv_obj_t *screen = ui_page_screen_create();
    ui_page_status_create(&s_status, screen, wifi, bluetooth);
    ui_page_title(screen, "个人主页", "PROFILE");

    lv_obj_t *identity = ui_page_card(screen, 20, 164, 440, 120);
    lv_obj_t *avatar = ui_page_box(identity, 18, 22, 76, 76,
                                   UI_COLOR_YELLOW, 2);
    lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
    ui_page_label(avatar, 0, 26, 76, "骑", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(identity, 112, 24, 170, "骑行者", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_LEFT);
    ui_page_label(identity, 112, 56, 170, "等级 12", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_LEFT);
    ui_page_label(identity, 282, 30, 132, "骑行次数", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_RIGHT);
    ui_page_label(identity, 282, 61, 132, "58", &ui_font_misans_num_48,
                  LV_TEXT_ALIGN_RIGHT);

    lv_obj_t *stats = ui_page_card(screen, 20, 302, 440, 90);
    ui_page_label(stats, 14, 14, 132, "总里程", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(stats, 154, 14, 132, "累计时间", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(stats, 294, 14, 132, "成就", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(stats, 14, 43, 132, "2418 km", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(stats, 154, 43, 132, "86 h", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(stats, 294, 43, 132, "18", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);

    ui_page_label(screen, 24, 417, 432, "最近骑行", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_LEFT);
    lv_obj_t *history = ui_page_card(screen, 20, 448, 440, 104);
    ui_page_label(history, 18, 16, 240, "公路训练", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_LEFT);
    ui_page_label(history, 18, 49, 240, "50.25 km · 01:30:25",
                  UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_LEFT);
    ui_page_label(history, 300, 32, 112, "查看详情", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_RIGHT);
    lv_obj_add_flag(history, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(history, action_event, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)UI_PAGE_ACTION_HISTORY);

    ui_page_label(screen, 24, 577, 432, "成就", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_LEFT);
    ui_page_button(screen, 20, 608, 440, 58, "首次百公里挑战  ·  已解锁",
                   action_event, (void *)(uintptr_t)UI_PAGE_ACTION_ACHIEVEMENT);
    ui_page_button(screen, 20, 684, 440, 58, "设置", action_event,
                   (void *)(uintptr_t)UI_PAGE_ACTION_SETTINGS);
    return screen;
}

void ui_profile_page_set_connectivity(bool wifi, bool bluetooth)
{
    ui_page_status_set_connectivity(&s_status, wifi, bluetooth);
}
