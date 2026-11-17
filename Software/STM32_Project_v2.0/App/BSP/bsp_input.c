/*===========================================================
 * File  : bsp_input.c
 * Brief : STM32H743 Board Input Driver
 *
 * 硬件：
 * POWER  PA0
 * MENU   PD12
 * PREV   PD13
 * NEXT   PC13
 * BACK   PD11
 * ENC_SW PG12
 *
 * Encoder A : PG3
 * Encoder B : PD4
 *
 * 设计：
 * 1. 按键只进行 GPIO 原始读取
 * 2. 编码器使用 EXTI 双边沿中断
 * 3. 编码器通过四相状态转换表进行方向解码
 * 4. 中断只更新计数，不调用 FreeRTOS API
 * 5. InputTask 使用临界区安全获取档位数
 *==========================================================*/

#include "bsp_input.h"

/*===========================================================
 * 编码器配置参数
 *
 * REVERSE：
 * 0 = 保持当前方向
 * 1 = 将顺时针和逆时针方向交换
 *
 * EDGES_PER_DETENT：
 * 4 表示每累计四个合法状态转换形成一个档位。
 *
 * 该值取决于机械编码器实际结构，需要实测。
 *==========================================================*/

#define INPUT_ENCODER_REVERSE           0U

#define INPUT_ENCODER_EDGES_PER_DETENT  4U

#if ((INPUT_ENCODER_EDGES_PER_DETENT != 4U) && \
     (INPUT_ENCODER_EDGES_PER_DETENT != 2U))
#error "Encoder resolution must be 2 or 4"
#endif
/*===========================================================
 * 编码器中断共享状态
 *
 * volatile 的作用：告诉编译器，这些变量可能被异步中断修改。
 * 但 volatile 本身不能保证复合操作的原子性。
 *==========================================================*/
/* 驱动是否初始化完成 */
static volatile uint8_t s_encoderReady = 0U;

/* 上一次 A/B 组合状态 */
static volatile uint8_t s_previousAB = 0U;

/* 当前档位内累计的四相转换 */
static volatile int8_t s_partialTransition = 0;

/* 已完成的档位数，正负代表方向 */
static volatile int32_t s_completedDetents = 0;

/* 总边沿计数 */
static volatile uint32_t s_edgeCount = 0U;

/* 非法状态转换统计 */
static volatile uint32_t s_invalidTransitions = 0U;

/*===========================================================
 * 读取编码器 A/B 两路电平。
 * 注意两根线位于不同 GPIO 端口，两次读取不是严格同时发生。
 * A 放在 bit1
 * B 放在 bit0
 *
 * 返回的状态只有：
 *
 * 00 = 0
 * 01 = 1
 * 10 = 2
 * 11 = 3
 *==========================================================*/
static uint8_t BSP_ReadEncoderAB(void)
{
    uint8_t a;
    uint8_t b;
    a= (HAL_GPIO_ReadPin(GPIOG,GPIO_PIN_3) == GPIO_PIN_SET)? 1U : 0U;
    b= (HAL_GPIO_ReadPin(GPIOD,GPIO_PIN_4) == GPIO_PIN_SET)? 1U : 0U;
    return (uint8_t)((a << 1U) | b);
}
/*===========================================================
 * 输入驱动初始化。
 *
 * 不能直接假定编码器初始状态为 00。
 *
 * 如果机械编码器停在 11，
 * 初始状态强行设置为 00 就可能产生错误方向。
 *
 * 因此初始化时必须读取实际 A/B 电平。
 *==========================================================*/
void BSP_Input_Init(void)
{
    /*
     * 保存当前中断屏蔽状态。
     *
     * 因为 EXTI 可以异步修改共享数据，
     * 初始化多个共享变量时需要避免被中断打断。
     */
    uint32_t primask;
    primask = __get_PRIMASK();
    __disable_irq();
    s_encoderReady = 0U;
    s_previousAB = BSP_ReadEncoderAB();
    s_partialTransition = 0;
    s_completedDetents = 0;
    s_edgeCount = 0U;
    s_invalidTransitions = 0U;
    s_encoderReady = 1U;
    /*
     * 恢复原来的中断状态。
     *
     * 不能简单地直接 __enable_irq()，
     * 因为调用前可能原本就处于关中断状态。
     */
    __set_PRIMASK(primask);
}

/*===========================================================
 * 原始按键读取。
 *
 * 所有按键假定：
 *
 * 松开 = 高电平
 * 按下 = 低电平
 *
 * 本函数不包含 HAL_Delay()。
 *==========================================================*/

uint8_t BSP_Input_IsPressed(BSP_ButtonId_t button)
{
    GPIO_TypeDef *gpio_port;
    uint16_t pin;

    switch (button)
    {
        case BSP_BUTTON_POWER:
            gpio_port = GPIOA;
            pin = GPIO_PIN_0;
            break;

        case BSP_BUTTON_MENU:
            gpio_port = GPIOD;
            pin = GPIO_PIN_12;
            break;

        case BSP_BUTTON_PREV:
            gpio_port = GPIOD;
            pin = GPIO_PIN_13;
            break;

        case BSP_BUTTON_NEXT:
            gpio_port = GPIOC;
            pin = GPIO_PIN_13;
            break;

        case BSP_BUTTON_BACK:
            gpio_port = GPIOD;
            pin = GPIO_PIN_11;
            break;

        case BSP_BUTTON_ENCODER:
            gpio_port = GPIOG;
            pin = GPIO_PIN_12;
            break;

        default:
            return 0U; /* 非法按键 ID */
    }
    if (HAL_GPIO_ReadPin(gpio_port, pin) == GPIO_PIN_RESET)
    {
        return 1U; /* 按下 */
    }
    return 0U; /* 松开 */
}

/*===========================================================
 * 编码器四相解码函数。
 *
 * 由 EXTI 回调触发。
 *
 * 每次只读取 A/B，并根据前后状态判断：
 *
 * +1 : 一个方向的合法转换
 * -1 : 另一个方向的合法转换
 *  0 : 无变化或非法转换
 *
 * 不进行串口打印。
 * 不操作 LVGL。
 * 不调用 FreeRTOS Queue。
 *==========================================================*/


void BSP_Encoder_OnGPIOEdge(uint16_t gpio_pin)
{
    /*
     * 状态转换查询表。
     *
     * index = (oldAB << 2) | newAB
     *
     * oldAB/newAB 各有4种情况。
     *
     * 因此共有16种组合。
     */
    static const int8_t transitionTable[16] =
    {
        0,  -1,  1,  0,   /* oldAB = 00 */
        1,   0,  0, -1,   /* oldAB = 01 */
       -1,   0,  0,  1,   /* oldAB = 10 */
        0,   1, -1,  0    /* oldAB = 11 */
    };
    uint8_t previous;
    uint8_t current;
    int8_t  delta;
    int8_t  partial;
    /*尚未初始化，不处理 */
    if(s_encoderReady == 0U)
    {
        return;
    }
    /*只处理编码器对应的EXTI3/EXTI4 */
    if((gpio_pin != GPIO_PIN_3)&&(gpio_pin != GPIO_PIN_4))
    {
        return;
    }

    previous = s_previousAB;
    current = BSP_ReadEncoderAB();
    /*两次状态完全相同，不产生旋转 */
    if(current == previous)
    {
        return;
    }

    /*两次状态完全相同，不产生旋转 */
    s_previousAB = current;
    s_edgeCount++;

/*
* 如果 A/B 两位同时发生变化，
* 说明观察到了非法跃迁。
* 可能原因：
* - 机械抖动
* - 中断处理延迟
* - 中间状态被漏采
* - 信号干扰
*/
    if((uint8_t)(current ^ previous) == 3U)
    {
        s_invalidTransitions++;
        s_partialTransition = 0;
        return;
    }
    /* 查询前后状态对应的方向增量*/
    delta = transitionTable[
        (previous << 2U) | current
    ];


    #if INPUT_ENCODER_REVERSE
        delta = (int8_t)(-delta);
    #endif

    /*累计一个档位内部的状态变化*/
    partial = (int8_t)(s_partialTransition + delta);
    if(partial >= (int8_t)INPUT_ENCODER_EDGES_PER_DETENT)
    {
        s_completedDetents++;
        partial = 0;
    }
    else if (partial <=-(int8_t)INPUT_ENCODER_EDGES_PER_DETENT)
    {
        s_completedDetents--;
        partial = 0;
    }
    s_partialTransition = partial;
}

/*===========================================================
 * 获取并清除编码器累计档位。
 *
 * 为什么需要临界区？
 *
 * 因为以下两句不是一个原子操作：
 *
 * result = s_completedDetents;
 * s_completedDetents = 0;
 *
 * 如果两句之间恰好发生 EXTI，
 * 新增的档位可能在清零时丢失。
 *
 * 因此用极短临界区保护读取+清零。
 *==========================================================*/

int32_t BSP_Encoder_TakeDetents(void)
{
    int32_t result;
    uint32_t primask;
    primask = __get_PRIMASK();
    __disable_irq();
    result = s_completedDetents;
    s_completedDetents = 0;
    __set_PRIMASK(primask);
    return result;    
}

/*===========================================================
 * 调试统计读取接口
 *==========================================================*/
uint32_t BSP_Encoder_GetEdgeCount(void)
{
    return s_edgeCount;
}
uint32_t BSP_Encoder_GetInvalidTransitions(void)
{
    return s_invalidTransitions;
}

/*===========================================================
 * HAL GPIO EXTI 用户回调。
 *
 * STM32 HAL 提供弱定义的同名函数。
 *
 * 我们在自己的 BSP 文件中提供正式实现，
 * 不需要修改 CubeMX 的 stm32h7xx_it.c。
 *
 * 注意：整个工程只能有一个强定义的
 * HAL_GPIO_EXTI_Callback()。
 *
 * 将来其他 EXTI 设备也应通过这里集中分发。
 *==========================================================*/

 void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    BSP_Encoder_OnGPIOEdge(GPIO_Pin);
}

