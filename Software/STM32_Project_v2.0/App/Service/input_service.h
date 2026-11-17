
#ifndef INPUT_SERVICE_H
#define INPUT_SERVICE_H

#include <stdint.h>                     // 提供uint32_t等整数类型

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================
 * Input Service公共接口
 *
 * 负责：
 * 1. 实体按键软件消抖
 * 2. 短按、长按、双击识别
 * 3. 编码器档位事件生成
 * 4. POWER与普通UI输入事件分流
 *
 * 不负责：
 * 1. LVGL页面操作
 * 2. 实际电源硬件控制
 *==========================================================*/

/* 初始化输入状态机，由InputTask启动时调用一次 */
void Input_ServiceInit(uint32_t now_ms);

/*
 * 执行一次输入状态扫描。
 *
 * 当前InputTask每10ms调用一次。
 * 软件消抖阈值为25ms。
 *
 * now_ms使用系统单调毫秒时间。
 */
void Input_ServicePoll(uint32_t now_ms);

/*
 * 获取输入事件投递失败次数。
 *
 * 包括普通InputQueue与PowerQueue发送失败。
 * 计数器由InputTask维护。
 */
uint32_t Input_ServiceGetDroppedEvents(void);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_SERVICE_H */
