/**
 * @file    ui_gnss_bridge.h
 * @brief   PU-252 GNSS 与 BikeComputer LVGL UI 的数据桥接接口
 *
 * @details
 * 本模块负责将 GnssService 发布的真实定位数据，
 * 转换为现有 LVGL UI 数据模型所使用的格式。
 *
 * 调用约束：
 * 1. 仅允许在 FreeRTOS UITask 中调用。
 * 2. 不直接操作 UART、DMA 或 GNSS 解析器。
 * 3. 不创建 LVGL 控件。
 * 4. 不修改现有仪表盘的界面布局。
 *
 * 数据流：
 *
 * GNSSTask
 *    |
 *    v
 * GnssService
 *    |
 *    v
 * UiGnssBridge
 *    |
 *    v
 * UI STM32 Services
 *    |
 *    v
 * LVGL 数据模型
 */

#ifndef UI_GNSS_BRIDGE_H
#define UI_GNSS_BRIDGE_H

#include <stdint.h>                       // uint32_t 类型

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 周期性检查并提交最新 GNSS 数据
 *
 * @param now_ms 当前单调时间，单位毫秒
 *
 * @note
 * 仅在 UI 成功启动后，由 UITask 调用。
 * 内部限制状态检查频率，并避免重复提交相同数据。
 */
void UI_GnssBridge_Poll(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* UI_GNSS_BRIDGE_H */
