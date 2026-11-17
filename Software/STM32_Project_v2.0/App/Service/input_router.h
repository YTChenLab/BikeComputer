
#ifndef INPUT_ROUTER_H
#define INPUT_ROUTER_H

#include <stdint.h>                        // 固定宽度整数类型
#include "app_events.h"                    // STM32内部输入事件类型
#include "ui/core/ui_physical_input.h"     // LVGL 12字节标准事件结构

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================
 * 一、Input Router转换结果
 *
 * Router只负责数据检查与协议转换。
 * 不进行GPIO读取、消抖、队列操作或者LVGL控制。
 *==========================================================*/
typedef enum
{
    INPUT_ROUTER_OK = 0,                   // 输入合法，已成功转换
    INPUT_ROUTER_INVALID,                  // 输入类型、动作或参数非法
    INPUT_ROUTER_DELTA_RANGE               // 编码器档位超过单条事件允许范围
} InputRouterResult_t;

/*===========================================================
 * 二、STM32内部事件转换为LVGL标准事件
 *
 * source：
 *   InputService生成的STM32内部输入事件。
 *
 * destination：
 *   转换后的12字节UI标准事件。
 *
 * 返回：
 *   INPUT_ROUTER_OK表示转换成功。
 *   INPUT_ROUTER_INVALID表示输入非法。
 *   INPUT_ROUTER_DELTA_RANGE表示需要编码器分包。
 *
 * 注意：
 * Router不将事件直接发送给LVGL。
 *==========================================================*/
InputRouterResult_t Input_RouterConvert(
    const AppInputEvent_t *source,         // 输入：STM32内部事件
    ui_physical_input_event_t *destination // 输出：LVGL标准物理事件
);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_ROUTER_H */
