/**
 * @file    ui_altitude_bridge.h
 * @brief   SPL06 气压高度到 LVGL 数据模型的适配接口
 *
 * @details
 * 本模块负责：
 * 1. 从 AltitudeService 获取已经滤波的高度。
 * 2. 将高度转换成 LVGL UI 使用的整数米。
 * 3. 提交高度的来源、有效性和时间戳。
 * 4. 避免在没有新数据时重复提交。
 *
 * 数据流：
 * AltitudeService -> UI_AltitudeBridge -> LVGL RideDataModel
 *
 * 调用约束：
 * 1. 只在 UITask 中调用。
 * 2. 不能在中断中调用。
 * 3. 不直接修改 LVGL 对象。
 * 4. 不访问 SPL06 I2C 寄存器。
 */

#ifndef UI_ALTITUDE_BRIDGE_H                    // 防止头文件重复包含
#define UI_ALTITUDE_BRIDGE_H                    // 定义头文件保护宏

#include <stdint.h>                           // uint32_t 固定宽度整数类型

#ifdef __cplusplus                           // 兼容 C++ 编译环境
extern "C" {                                 // 使用 C 语言链接方式
#endif


/* ---------- 1. UI 海拔数据桥接接口 ---------- */

/**
 * @brief 周期更新 LVGL 骑行数据模型中的海拔字段
 *
 * @param now_ms 当前 HAL 系统毫秒时间
 *
 * @note
 * 由 UI_BoardPoll() 周期调用。
 * 仅当高度数据更新或有效状态变化时才提交。
 */
void UI_AltitudeBridge_Poll(uint32_t now_ms);


#ifdef __cplusplus                           // 结束 C++ 兼容声明
}                                              // 结束 extern "C"
#endif

#endif /* UI_ALTITUDE_BRIDGE_H */             // 结束头文件保护