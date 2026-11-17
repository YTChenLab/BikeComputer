#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>

/*==============================
 * 1. 输入来源
 *    一个枚举代表一个物理输入来源
 *    保留已有枚举名称及顺序
================================*/

typedef enum
{
    APP_INPUT_NONE = 0,
    /* 五个实体功能按键 */
    APP_INPUT_KEY_POWER,
    APP_INPUT_KEY_MENU,
    APP_INPUT_KEY_PREV,
    APP_INPUT_KEY_NEXT,
    APP_INPUT_KEY_BACK,
    /* 旋转编码器 */
    APP_INPUT_ENCODER_CW,
    APP_INPUT_ENCODER_CCW,
    APP_INPUT_ENCODER_PRESS

} AppInputType_t;

/*===========================================================
 * 2. 输入动作
 *
 * DOWN   : 消抖后确认按下
 * UP     : 消抖后确认松开
 * SHORT  : 确认一次短按
 * DOUBLE : 确认一次双击
 * LONG   : 达到长按阈值，只触发一次
 * REPEAT : 长按后的连续操作
 * ROTATE : 编码器旋转
 *
 * 注意：
 * DOWN、UP 是物理状态事件。
 * SHORT、LONG 等是逻辑手势事件。
 *==========================================================*/
typedef enum
{
    APP_INPUT_ACTION_DOWN = 0,

    APP_INPUT_ACTION_UP,
    APP_INPUT_ACTION_SHORT,
    APP_INPUT_ACTION_DOUBLE,
    APP_INPUT_ACTION_LONG,
    APP_INPUT_ACTION_REPEAT,
    APP_INPUT_ACTION_ROTATE
}AppInputAction_t;


/*===========================================================
 * 3. 统一输入事件
 *
 * type         : 输入来自哪个设备
 * action       : 发生了什么动作
 * value        : 数值，旋转事件使用有符号档位数
 * timestamp_ms : 事件生成时的毫秒时间戳
 *
 * 这个结构通过 FreeRTOS Queue 传递。
 *==========================================================*/

typedef struct
{
    AppInputType_t type;
    int32_t value;
    AppInputAction_t action;
    uint32_t timestamp_ms;

} AppInputEvent_t;

#endif
