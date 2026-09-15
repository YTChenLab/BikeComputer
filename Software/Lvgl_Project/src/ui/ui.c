#include "ui.h"

#include "core/ui_app.h"
#include "core/ui_background.h"
#include "core/ui_theme.h"


/*
 * ============================================================
 * UI 初始化
 * ============================================================
 */

void ui_init(void)
{
    /*
     * --------------------------------------------------------
     * 初始化 UI Theme
     * --------------------------------------------------------
     */

    ui_theme_init();


    /*
     * 全局背景只在显示底层创建一次。
     * 后续切换页面时，不应在页面创建函数中重复创建背景。
     */
    ui_background_init();


    /*
     * --------------------------------------------------------
     * 创建首页
     * --------------------------------------------------------
     */

    ui_app_init();
}


void ui_handle_key(uint32_t key)
{
    ui_app_handle_key(key);
}

void ui_handle_input(ui_input_event_t input)
{
    ui_app_handle_input(input);
}

void ui_handle_pointer(const ui_input_pointer_event_t *event)
{
    ui_app_handle_pointer(event);
}

void ui_handle_back(void)
{
    ui_app_handle_back();
}
