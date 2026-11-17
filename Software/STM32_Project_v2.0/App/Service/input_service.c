
/*===========================================================
 * File  : input_service.c
 * Brief : 公路车码表 V1 实体输入识别服务
 *
 * 功能：
 * 1. 六路实体按压输入的25ms软件消抖
 * 2. POWER：单击/2000ms长按
 * 3. MENU ：单击/700ms长按，取消双击
 * 4. PREV/NEXT：单击
 * 5. BACK ：单击/700ms长按
 * 6. 当前PCB暂时屏蔽ENC_SW，编码器A/B旋转照常使用
 * 7. POWER事件送PowerQueue，其他事件送InputQueue
 *
 * 说明：
 * 只负责识别手势，不负责LVGL页面判断。
 *==========================================================*/

#include "input_service.h"             // Input Service公共函数声明
#include "bsp_input.h"                 // BSP GPIO读取与编码器接口
#include "app_events.h"                // STM32统一输入事件类型
#include "app_rtos.h"                  // FreeRTOS队列投递接口

/*==================== 输入参数配置 ====================*/

#define INPUT_DEBOUNCE_MS        25U     // 原始状态持续稳定25ms才确认
#define INPUT_LONG_MS           700U    // MENU/BACK长按时间
#define INPUT_POWER_LONG_MS     2000U   // POWER长按时间

#define INPUT_ENCODER_SW_ENABLED 0U     // 当前PCB禁用PG12按压开关

/*==================== 单个按键状态结构 ====================*/

typedef struct
{
    uint8_t stable_pressed;            // 消抖后确定的状态：1按下，0松开

    uint8_t candidate_pressed;         // 当前等待消抖确认的候选状态

    uint8_t initial_held;              // MCU启动时已经按住的按键

    uint8_t long_sent;                 // 本次按压是否已经产生LONG事件

    uint32_t candidate_since;          // 候选电平最近变化的时间

    uint32_t pressed_since;            // 确认稳定按下时的时间

} InputButtonState_t;

/*==================== 模块内部静态变量 ====================*/

static InputButtonState_t s_buttons[BSP_BUTTON_COUNT]; // 六路独立按键状态机

static volatile uint32_t s_droppedEvents = 0U;         // 事件队列投递失败次数

/*==================== 物理按键与事件类型映射 ====================*/

static const AppInputType_t s_eventTypes[BSP_BUTTON_COUNT] =
{
    APP_INPUT_KEY_POWER,                // POWER：电源键
    APP_INPUT_KEY_MENU,                 // MENU：菜单/确认键
    APP_INPUT_KEY_PREV,                 // PREV：上一页键
    APP_INPUT_KEY_NEXT,                 // NEXT：下一页键
    APP_INPUT_KEY_BACK,                 // BACK：返回键
    APP_INPUT_ENCODER_PRESS             // ENC_SW：保留下一版PCB接口
};

/*==================== 判断物理按键是否启用 ====================*/

static uint8_t Input_ButtonEnabled(BSP_ButtonId_t key)
{
#if (INPUT_ENCODER_SW_ENABLED == 0U)

    if (key == BSP_BUTTON_ENCODER)
    {
        return 0U;                      // 屏蔽PG12异常输入
    }

#endif

    return 1U;                          // 其他按键正常采样
}

/*==================== 获取长按时间阈值 ====================*/

static uint32_t Input_GetLongThresholdMs(BSP_ButtonId_t key)
{
    switch (key)
    {
        case BSP_BUTTON_POWER:
            return INPUT_POWER_LONG_MS; // POWER长按2000ms

        case BSP_BUTTON_MENU:
            return INPUT_LONG_MS;       // MENU长按700ms

        case BSP_BUTTON_BACK:
            return INPUT_LONG_MS;       // BACK长按700ms

        default:
            return 0U;                  // 其他按键不支持长按
    }
}

/*==================== 统一分发输入事件 ====================*/

static void Input_Emit(
    AppInputType_t type,                // 事件的物理输入来源
    AppInputAction_t action,            // DOWN/UP/SHORT/LONG/ROTATE
    int32_t value,                      // 编码器档位，普通按键为0
    uint32_t now_ms                     // 单调毫秒时间戳
)
{
    AppInputEvent_t event;              // STM32内部标准输入事件

    BaseType_t sendResult;              // FreeRTOS事件投递结果

    /*==================== 构造标准事件 ====================*/

    event.type = type;                  // 写入事件来源

    event.action = action;              // 写入识别出的动作

    event.value = value;                // 写入档位或普通按键数值0

    event.timestamp_ms = now_ms;        // 记录事件发生时间

    /*==================== POWER事件独立分流 ====================*/

    if (type == APP_INPUT_KEY_POWER)
    {
        /*
         * POWER属于系统级输入。
         *
         * POWER的SHORT和LONG由PowerService处理。
         *
         * DOWN/UP只用于识别按键生命周期，
         * 不允许直接触发息屏、亮屏、关机等操作。
         */

        if ((action != APP_INPUT_ACTION_SHORT) &&
            (action != APP_INPUT_ACTION_LONG))
        {
            return;                     // 丢弃POWER的中间状态事件
        }

        sendResult = App_RTOS_PostPowerEvent(
            action,                     // 单击或者长按
            now_ms                      // 实际事件时间戳
        );
    }
    else
    {
        /*
         * MENU/PREV/NEXT/BACK以及编码器，
         * 继续走现有的InputQueue数据通道。
         *
         * App_RTOS_PostInputEvent内部先经过
         * Input_RouterConvert()进行协议转换。
         */

        sendResult = App_RTOS_PostInputEvent(
            &event,                    // 输入服务产生的原始事件
            0U                         // 队列满时立即返回，不阻塞InputTask
        );
    }

    /*==================== 统计投递失败 ====================*/

    if (sendResult != pdPASS)
    {
        s_droppedEvents++;              // 记录一次事件投递失败
    }
}

/*==================== 初始化输入识别服务 ====================*/

void Input_ServiceInit(uint32_t now_ms)
{
    uint32_t i;                         // 六路按键的遍历索引

    BSP_Input_Init();                   // 初始化底层输入和编码器A/B状态

    s_droppedEvents = 0U;               // 清除上一次运行的失败计数

    for (i = 0U; i < (uint32_t)BSP_BUTTON_COUNT; i++)
    {
        InputButtonState_t *state;      // 当前按键的状态结构指针

        uint8_t pressed = 0U;           // 当前按键的原始状态

        state = &s_buttons[i];          // 指向当前按键状态机

        if (Input_ButtonEnabled((BSP_ButtonId_t)i) != 0U)
        {
            pressed = BSP_Input_IsPressed(
                (BSP_ButtonId_t)i       // 从BSP读取当前GPIO状态
            );
        }

        /*
         * 初始化时将当前电平视为初始稳定状态。
         *
         * 如果MCU启动期间某个按键已经被按下，
         * 不能把它误认为一次新的用户操作。
         */

        state->stable_pressed = pressed;    // 记录初始稳定状态

        state->candidate_pressed = pressed; // 候选状态与初始状态一致

        state->initial_held = pressed;      // 标记启动时已有的按压

        state->long_sent = 0U;              // 尚未发送长按事件

        state->candidate_since = now_ms;    // 初始化消抖时间起点

        state->pressed_since = now_ms;      // 初始化按下计时起点
    }
}

/*==================== 周期扫描所有实体输入 ====================*/

void Input_ServicePoll(uint32_t now_ms)
{
    uint32_t i;                         // 按键数组索引

    int32_t steps;                      // 编码器累计的完整档位数

    /*==================== 遍历所有实体按键 ====================*/

    for (i = 0U; i < (uint32_t)BSP_BUTTON_COUNT; i++)
    {
        BSP_ButtonId_t key;             // 当前物理按键编号

        InputButtonState_t *state;      // 当前按键状态机

        uint32_t long_threshold;        // 当前按键长按时间阈值

        uint8_t raw;                    // 本次读取到的原始按键状态

        key = (BSP_ButtonId_t)i;        // 数组索引转换为BSP按键编号

        state = &s_buttons[i];          // 取得当前按键的状态机

        if (Input_ButtonEnabled(key) == 0U)
        {
            continue;                   // 屏蔽PG12异常按压输入
        }

        raw = BSP_Input_IsPressed(key); // 读取GPIO：1按下，0松开

        long_threshold = Input_GetLongThresholdMs(
            key                         // 获取对应的长按阈值
        );

        /*==================== 第一步：检查原始状态变化 ====================*/

        if (raw != state->candidate_pressed)
        {
            state->candidate_pressed = raw; // 保存新的候选状态

            state->candidate_since = now_ms; // 重新开始25ms消抖计时
        }

        /*
         * 只有满足以下两个条件才确认状态切换：
         *
         * 1. 候选状态与当前稳定状态不同。
         * 2. 候选状态已经持续至少25ms。
         *
         * 这样可以过滤机械触点在按下、释放时的电平抖动。
         */

        if ((state->candidate_pressed != state->stable_pressed) &&
            ((uint32_t)(now_ms - state->candidate_since) >= INPUT_DEBOUNCE_MS))
        {
            state->stable_pressed = state->candidate_pressed; // 正式确认状态变化

            /*==================== 第二步：确认稳定按下 ====================*/

            if (state->stable_pressed != 0U)
            {
                state->pressed_since = now_ms; // 从稳定按下时刻开始计算长按

                state->long_sent = 0U;        // 新的一次按压清除长按标志

                if (state->initial_held == 0U)
                {
                    Input_Emit(
                        s_eventTypes[i],      // 按键来源
                        APP_INPUT_ACTION_DOWN,// 按键已经按下
                        0,                    // 普通按键不携带数值
                        now_ms                // 按下事件时间戳
                    );
                }
            }

            /*==================== 第三步：确认稳定松开 ====================*/

            else
            {
                if (state->initial_held != 0U)
                {
                    /*
                     * 上电时已经按住的按键，
                     * 第一次释放只恢复状态机。
                     *
                     * 不能在启动时产生错误的SHORT或LONG。
                     */

                    state->initial_held = 0U; // 解除启动遗留按压保护

                    state->long_sent = 0U;   // 清除长按标志
                }
                else
                {
                    uint32_t held_time;      // 本次按住的有效时长

                    held_time = (uint32_t)(
                        state->candidate_since -
                        state->pressed_since
                    );

                    /*
                     * 检查长按是否恰好发生在采样边界。
                     *
                     * 如果松开前已经满足长按时间，
                     * 但是周期扫描还没来得及发送LONG，
                     * 则在这里补发一次。
                     */

                    if ((long_threshold != 0U) &&
                        (state->long_sent == 0U) &&
                        (held_time >= long_threshold))
                    {
                        state->long_sent = 1U; // 标记本次已经产生长按

                        Input_Emit(
                            s_eventTypes[i],  // 按键来源
                            APP_INPUT_ACTION_LONG,
                            0,
                            now_ms
                        );
                    }

                    /*==================== 发送松开事件 ====================*/

                    Input_Emit(
                        s_eventTypes[i],      // 按键来源
                        APP_INPUT_ACTION_UP,  // 按键已经松开
                        0,
                        now_ms
                    );

                    /*==================== 判断是否为单击 ====================*/

                    if (state->long_sent == 0U)
                    {
                        /*
                         * 本次按压未产生LONG，
                         * 因此正式确认为一次SHORT。
                         *
                         * 当前V1版本已经取消双击，
                         * 所以不再等待原来的250ms窗口。
                         */

                        Input_Emit(
                            s_eventTypes[i],  // 按键来源
                            APP_INPUT_ACTION_SHORT,
                            0,
                            now_ms
                        );
                    }

                    /*
                     * 如果long_sent已经等于1，
                     * 就不会再产生SHORT。
                     *
                     * 因此MENU长按打开快捷中心之后，
                     * 松开按键不会额外触发确认。
                     */
                }
            }
        }

        /*==================== 第四步：持续按住期间识别长按 ====================*/

        if ((long_threshold != 0U) &&
            (state->stable_pressed != 0U) &&
            (state->candidate_pressed != 0U) &&
            (state->initial_held == 0U) &&
            (state->long_sent == 0U))
        {
            if ((uint32_t)(now_ms - state->pressed_since) >= long_threshold)
            {
                state->long_sent = 1U;       // 确保每次按下只触发一次LONG

                Input_Emit(
                    s_eventTypes[i],         // POWER、MENU或者BACK
                    APP_INPUT_ACTION_LONG,   // 发送长按事件
                    0,                       // 普通按键数值为0
                    now_ms                   // 达到长按阈值的时间
                );
            }
        }
    }

    /*==================== 第五步：读取编码器旋转档位 ====================*/

    /*
     * 编码器A/B相的边沿识别仍然由BSP完成。
     *
     * BSP负责：
     * 1. EXTI双边沿处理
     * 2. 四相状态转换解码
     * 3. 顺逆时针判断
     * 4. 完整档位累计
     *
     * Input Service只读取已累计完成的档位。
     */

    steps = BSP_Encoder_TakeDetents();   // 读取并清空本轮累计档位

    if (steps > 0)
    {
        Input_Emit(
            APP_INPUT_ENCODER_CW,        // 顺时针旋转
            APP_INPUT_ACTION_ROTATE,     // 编码器旋转事件
            steps,                       // 正数档位
            now_ms
        );
    }
    else if (steps < 0)
    {
        Input_Emit(
            APP_INPUT_ENCODER_CCW,       // 逆时针旋转
            APP_INPUT_ACTION_ROTATE,     // 编码器旋转事件
            steps,                       // 负数档位
            now_ms
        );
    }
}

/*==================== 获取输入事件丢失次数 ====================*/

uint32_t Input_ServiceGetDroppedEvents(void)
{
    return s_droppedEvents;              // 返回累计的队列投递失败次数
}
