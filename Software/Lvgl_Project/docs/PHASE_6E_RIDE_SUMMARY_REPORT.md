# Phase 6E — Ride Summary

## 本阶段结果

Ride Summary 已从硬编码演示页迁移为真实数据驱动页面。页面数据来自独立的 `ui_ride_summary_model`，并与现有骑行状态机、自动保存流程、Home 数据模型和 Bottom Interaction Surface 生命周期连接。

## 数据闭环

- `GPS_READY -> RIDING` 时记录本次骑行的距离与时间基线。
- 仅在 `RIDING` 状态累计有效统计；暂停期间不重复采样。
- 结束并保存成功后冻结 Summary 快照并标记为 `SAVED`。
- 保存失败继续停留在既有重试流程，不会提前进入 Summary。
- Summary 完成后回到 Home；Home 的总里程、总时间、总爬升和最近一次骑行同步更新。

## Summary 数据

- 本次距离
- 骑行时间
- 平均速度
- 最大速度
- 累计爬升
- 平均心率
- 平均功率
- 平均踏频
- 本地保存状态

当前 PC Simulator 数据仍为模拟遥测；模型算法和页面接口可直接复用，STM32 阶段只需由真实骑行数据源驱动 `ui_ride_data_model`。

## UI

- 顶部 `RIDE COMPLETE` 完成标题。
- 黄色描边主距离卡。
- 2×4 统一指标卡阵列。
- 保存状态作为第八张状态卡，不再使用重复的“保存记录”按钮。
- 底部单一黄色完成按钮，支持整块触摸。
- 页面所有卡片保持不透明，继续使用当前全局背景、状态栏和字体系统。

## Motion

- 主距离使用 0 到最终值的 Count-up Reveal。
- 主卡、指标卡和完成按钮使用延迟 Stagger Reveal。
- 新增 `ui_motion_translate_fade_delayed()`，动画仍统一由 `ui_motion` 管理。
- 动效仅修改位置和对象透明度，不创建全屏或大尺寸离屏图层。

## 输入

- 鼠标左键点击完成按钮：返回 Home。
- Encoder Press：执行完成。
- Encoder Rotate：保持完成按钮焦点并提供 Focus 反馈。
- BACK：在 Summary 状态直接完成并返回 Home，不会回到已结束的 Dashboard。

## Simulator 数据修正

PC 遥测距离改为按 `speed × 800 ms` 积分，骑行时间使用毫秒余数累计，避免旧实现每 800 ms 随机增加 10–80 m 导致 Summary 平均速度严重失真。

## 验证

- Debug 构建成功。
- 新增 `ui_ride_summary_model_test`，验证距离、时长、均速、极速、爬升、心率、功率、踏频和保存状态。
- 新增 `ui_summary_page_test`，验证 480×854 边界、Reveal 动画、无大图层内存失败及 Encoder 完成动作。
- 新增中文文案已纳入 MiSans 字形覆盖检查。
- CTest：15/15 通过。

## 下一阶段

按 V4 路线进入 Phase 6F — History：先实现日期、距离、时间、平均速度和爬升的 V1 列表，不在第一版加入复杂图表。

