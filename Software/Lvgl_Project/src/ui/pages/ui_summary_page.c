#include "ui_summary_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_sizes.h"
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

lv_obj_t *ui_summary_page_create(ui_page_action_cb_t callback,
                                 void *user_data)
{
    s_callback = callback;
    s_user_data = user_data;
    lv_obj_t *screen = ui_page_screen_create();
    ui_page_status_create(&s_status, screen);
    ui_page_title(screen, "骑行完成", "RIDE SUMMARY");
    ui_page_label(screen, 24, 170, 432, "本次骑行", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(screen, 100, 205, 250, "50.25", &ui_font_misans_num_68,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(screen, 344, 247, 60, "km", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_LEFT);

    const char *names[] = {
        "骑行时间", "平均速度", "最大速度", "热量", "心率", "功率"
    };
    const char *values[] = {
        "01:30:25", "33.4 km/h", "48.7 km/h", "1028 kcal", "148 bpm", "201 W"
    };
    for(int32_t row = 0; row < 3; row++) {
        int32_t y = 310 + row * 105;
        ui_page_line(screen, UI_LEFT + 18, y - 12, UI_CONTENT_W - 36, 1,
                     UI_COLOR_WHITE);
        for(int32_t col = 0; col < 2; col++) {
            int32_t index = row * 2 + col;
            ui_page_label(screen, 32 + col * 222, y, 190, names[index],
                          UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_CENTER);
            ui_page_label(screen, 32 + col * 222, y + 32, 190, values[index],
                          UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_CENTER);
        }
    }
    ui_page_button(screen, 28, 660, 130, 52, "保存记录", action_event,
                   (void *)(uintptr_t)UI_PAGE_ACTION_SAVE_RIDE);
    ui_page_button(screen, 175, 660, 130, 52, "上传数据", action_event,
                   (void *)(uintptr_t)UI_PAGE_ACTION_UPLOAD_RIDE);
    ui_page_button(screen, 322, 660, 130, 52, "返回主页", action_event,
                   (void *)(uintptr_t)UI_PAGE_ACTION_HOME);
    return screen;
}
