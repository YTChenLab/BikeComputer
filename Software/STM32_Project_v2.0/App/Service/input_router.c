
/*===========================================================
 * File  : input_router.c
 * Brief : STM32输入事件到LVGL物理输入协议的转换
 *
 * V1实体键映射：
 *
 * MENU SHORT -> ENCODER_BUTTON CLICK（确认）
 * MENU LONG  -> MENU CLICK（打开快捷控制中心）
 * BACK LONG  -> BACK LONG_START（骑行操作菜单）
 *
 * MENU DOUBLE取消。
 *
 * PREV、NEXT、POWER和编码器旋转保持原有协议。
 *
 * 本文件不负责：
 * 1. GPIO读取
 * 2. 按键消抖
 * 3. FreeRTOS队列创建
 * 4. LVGL页面状态判断
 *==========================================================*/

#include "input_router.h"              // Router接口及事件结构定义

#include <stddef.h>                   // NULL空指针定义

/*==================== 转换为12字节LVGL物理事件 ====================*/

InputRouterResult_t Input_RouterConvert(
    const AppInputEvent_t *source,          // Input Service生成的STM32事件
    ui_physical_input_event_t *destination  // 输出LVGL标准12字节事件
)
{
    ui_physical_key_t mapped_key;          // 映射后的LVGL按键编号

    ui_physical_action_t mapped_action;    // 映射后的LVGL动作编号

    /*==================== 第一：检查输入输出参数 ====================*/

    if ((source == NULL) || (destination == NULL))
    {
        return INPUT_ROUTER_INVALID;       // 避免访问无效内存
    }

    /*==================== 第二：清空输出事件 ====================*/

    destination->key = (uint8_t)UI_PHYSICAL_KEY_NONE; // 默认无按键

    destination->action = (uint8_t)UI_PHYSICAL_ACTION_NONE; // 默认无动作

    destination->reserved = 0U;           // 12字节协议保留字段必须为0

    destination->encoder_delta = 0;       // 普通按键不携带旋转档位

    destination->timestamp_ms = 0U;       // 成功转换后再写入时间

    /*==================== 第三：优先处理编码器旋转 ====================*/

    if ((source->type == APP_INPUT_ENCODER_CW) ||
        (source->type == APP_INPUT_ENCODER_CCW))
    {
        if (source->action != APP_INPUT_ACTION_ROTATE)
        {
            return INPUT_ROUTER_INVALID;  // 旋转事件必须使用ROTATE动作
        }

        /*
         * 顺时针的档位数必须为正。
         * 逆时针的档位数必须为负。
         */

        if (((source->type == APP_INPUT_ENCODER_CW) &&
             (source->value <= 0)) ||
            ((source->type == APP_INPUT_ENCODER_CCW) &&
             (source->value >= 0)))
        {
            return INPUT_ROUTER_INVALID;  // 方向和数值不一致
        }

        /*
         * 单个LVGL输入事件最多携带64个档位。
         *
         * 超过这个范围时，不在Router中截断。
         * 继续使用App_RTOS原有的编码器分包逻辑。
         */

        if ((source->value > UI_PHYSICAL_ENCODER_MAX_DELTA) ||
            (source->value < -UI_PHYSICAL_ENCODER_MAX_DELTA))
        {
            return INPUT_ROUTER_DELTA_RANGE; // 请求RTOS进行分包
        }

        destination->key =
            (uint8_t)UI_PHYSICAL_KEY_ENCODER; // 编码器物理编号

        destination->action =
            (uint8_t)UI_PHYSICAL_ENCODER_DELTA; // 旋转动作

        destination->encoder_delta = source->value; // 实际档位增量

        destination->timestamp_ms = source->timestamp_ms; // 时间戳

        return INPUT_ROUTER_OK;           // 旋转事件转换完成
    }

    /*==================== 第四：普通按键参数检查 ====================*/

    if (source->value != 0)
    {
        return INPUT_ROUTER_INVALID;      // 普通按键不允许携带档位数
    }

    /*==================== 第五：按键来源映射 ====================*/

    switch (source->type)
    {
        case APP_INPUT_KEY_POWER:
        {
            mapped_key = UI_PHYSICAL_KEY_POWER; // POWER保持不变

            break;
        }

        case APP_INPUT_KEY_MENU:
        {
            /*
             * MENU单击：
             * 模拟原来的编码器按压确认。
             *
             * LVGL会将该事件识别为：
             * UI_INPUT_ENCODER_PRESS。
             */

            if (source->action == APP_INPUT_ACTION_SHORT)
            {
                mapped_key =
                    UI_PHYSICAL_KEY_ENCODER_BUTTON; // 单击作为确认
            }

            /*
             * MENU长按：
             * 复用原来的MENU点击入口。
             *
             * LVGL会将它识别为：
             * UI_INPUT_TOGGLE_CONTROL_CENTER。
             *
             * 具体打开哪一层由后续UI状态机决定。
             */

            else if (source->action == APP_INPUT_ACTION_LONG)
            {
                mapped_key =
                    UI_PHYSICAL_KEY_MENU;      // 长按保留MENU身份
            }

            /*
             * DOWN/UP属于中间状态。
             *
             * 统一归为编码器按键身份，
             * 但不会产生CLICK操作。
             */

            else if ((source->action == APP_INPUT_ACTION_DOWN) ||
                     (source->action == APP_INPUT_ACTION_UP))
            {
                mapped_key =
                    UI_PHYSICAL_KEY_ENCODER_BUTTON;
            }

            else
            {
                return INPUT_ROUTER_INVALID;  // MENU双击已经取消
            }

            break;
        }

        case APP_INPUT_KEY_PREV:
        {
            mapped_key = UI_PHYSICAL_KEY_PREV; // 上一页功能不变

            break;
        }

        case APP_INPUT_KEY_NEXT:
        {
            mapped_key = UI_PHYSICAL_KEY_NEXT; // 下一页功能不变

            break;
        }

        case APP_INPUT_KEY_BACK:
        {
            mapped_key = UI_PHYSICAL_KEY_BACK; // 返回键身份保持不变

            break;
        }

        case APP_INPUT_ENCODER_PRESS:
        {
            /*
             * 当前PCB已在Input Service屏蔽PG12。
             *
             * 但这里仍保留编码器确认的映射。
             * 下一版PCB修复后可以直接恢复使用。
             */

            mapped_key =
                UI_PHYSICAL_KEY_ENCODER_BUTTON;

            break;
        }

        default:
        {
            return INPUT_ROUTER_INVALID;   // 未知按键来源
        }
    }

    /*==================== 第六：按键动作映射 ====================*/

    switch (source->action)
    {
        case APP_INPUT_ACTION_DOWN:
        {
            mapped_action = UI_PHYSICAL_KEY_DOWN; // 消抖后按下

            break;
        }

        case APP_INPUT_ACTION_UP:
        {
            mapped_action = UI_PHYSICAL_KEY_UP; // 消抖后松开

            break;
        }

        case APP_INPUT_ACTION_SHORT:
        {
            mapped_action = UI_PHYSICAL_KEY_CLICK; // 短按对应点击

            break;
        }

        case APP_INPUT_ACTION_LONG:
        {
            if (source->type == APP_INPUT_KEY_MENU)
            {
                /*
                 * 关键映射：
                 *
                 * MENU在STM32层产生LONG，
                 * 到LVGL层转换为MENU CLICK。
                 *
                 * 因为现有LVGL的MENU CLICK，
                 * 已经对应快捷控制中心入口。
                 */

                mapped_action = UI_PHYSICAL_KEY_CLICK;
            }

            else if ((source->type == APP_INPUT_KEY_POWER) ||
                     (source->type == APP_INPUT_KEY_BACK))
            {
                /*
                 * POWER长按协议保持不变。
                 *
                 * BACK长按使用原有LONG_START协议，
                 * 后续由UI判断是否处于骑行页面。
                 */

                mapped_action = UI_PHYSICAL_KEY_LONG_START;
            }

            else
            {
                return INPUT_ROUTER_INVALID; // 其他按键不支持长按
            }

            break;
        }

        case APP_INPUT_ACTION_DOUBLE:
        {
            return INPUT_ROUTER_INVALID;  // 当前V1版本取消全部双击
        }

        default:
        {
            return INPUT_ROUTER_INVALID;  // 不接受未定义动作
        }
    }

    /*==================== 第七：组装最终标准事件 ====================*/

    destination->key = (uint8_t)mapped_key; // 保存LVGL按键编号

    destination->action = (uint8_t)mapped_action; // 保存LVGL动作编号

    destination->timestamp_ms = source->timestamp_ms; // 保存时间戳

    return INPUT_ROUTER_OK;               // 转换成功
}
