
#ifndef BSP_INPUT_H
#define BSP_INPUT_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/*===========================================================
 * 物理按键 ID
 *
 * 顺序必须与输入服务中的映射表一致。
 *==========================================================*/
typedef enum
{
    BSP_BUTTON_POWER = 0,
    BSP_BUTTON_MENU,
    BSP_BUTTON_PREV,
    BSP_BUTTON_NEXT,
    BSP_BUTTON_BACK,
    BSP_BUTTON_ENCODER,

    BSP_BUTTON_COUNT


} BSP_ButtonId_t;

/*===========================================================
 * 初始化输入驱动的软件状态。
 *
 * GPIO 本身由 CubeMX 的 MX_GPIO_Init() 初始化。
 * 本函数不重新配置 GPIO。
 *==========================================================*/
void BSP_Input_Init(void);
/*===========================================================
 * 读取按键物理状态。
 *
 * 返回：
 * 1 = 正在按下
 * 0 = 当前松开
 *
 * 注意：这里只读取原始电平，不进行软件消抖。
 *==========================================================*/
uint8_t BSP_Input_IsPressed(BSP_ButtonId_t button);

/*===========================================================
 * 编码器 EXTI 回调处理函数。
 *
 * 只应该由 HAL_GPIO_EXTI_Callback() 调用。
 *==========================================================*/
void BSP_Encoder_OnGPIOEdge(uint16_t gpio_pin);

/*===========================================================
 * 获取并清零已累计的完整编码器档位。
 *
 * >0 : 正向旋转
 * <0 : 反向旋转
 *  0 : 没有净旋转量
 *
 * 仅由 InputTask 调用。
 *==========================================================*/

int32_t BSP_Encoder_TakeDetents(void);

 /* 编码器调试统计 */
uint32_t BSP_Encoder_GetEdgeCount(void);

uint32_t BSP_Encoder_GetInvalidTransitions(void);

#ifdef __cplusplus
}
#endif

#endif

