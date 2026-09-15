/**
 * @file ui_theme.c
 * @brief 公路车码表 UI 公共主题实现
 */

#include "ui_theme.h"


/**
 * @brief 初始化 UI 公共主题
 *
 * 当前阶段所有字体通过 ui_theme.h 映射到项目内置的
 * MiSans 字体，因此这里暂时没有额外初始化内容。
 *
 * 后续可以在这里统一初始化：
 *
 * - 中文字体
 * - 数字字体
 * - 公共 Style
 * - Transition
 * - 按钮样式
 * - 页面公共样式
 */
void ui_theme_init(void)
{
    /*
     * 当前阶段无需执行额外代码。
     *
     * 注意：
     * 本函数不能写成 static。
     *
     * 因为 ui.c 会跨文件调用 ui_theme_init()。
     */
}
