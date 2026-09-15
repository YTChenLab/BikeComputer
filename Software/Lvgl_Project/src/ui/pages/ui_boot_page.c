#include "ui_boot_page.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_theme.h"

lv_obj_t *ui_boot_page_create(void)
{
    lv_obj_t *screen = ui_page_screen_create();
    ui_page_label(screen, 0, 245, 480, "智能骑行码表", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(screen, 0, 278, 480, "SMART BIKE COMPUTER",
                  UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_CENTER);
    ui_page_line(screen, 120, 342, 240, 3, UI_COLOR_YELLOW);
    ui_page_label(screen, 0, 380, 480, "系统启动中", UI_FONT_PAGE_TEXT,
                  LV_TEXT_ALIGN_CENTER);
    ui_page_label(screen, 0, 418, 480,
                  "初始化显示屏  ·  卫星定位  ·  蓝牙连接",
                  UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_CENTER);
    ui_page_label(screen, 0, 780, 480, "按 M 进入快捷控制",
                  UI_FONT_PAGE_TEXT, LV_TEXT_ALIGN_CENTER);
    return screen;
}
