# Phase 7B 快捷控制条 V1 实现报告

> 架构修订：本报告记录的是历史阶段。当前产品由 Phase 7G 收敛为统一 `ui_control_center` 的 `COMPACT` 状态；不再创建独立 Quick Bar Overlay。以 `PHASE_7G_UNIFIED_CONTROL_SURFACE_REPORT.md` 为准。

## 阶段定位

本阶段从正式冻结的 Git tag `v1.1.1` 建立 `feature/ui-v1.1.2-finalization` 分支，按 `UI_V5_交接文档.md` 进入 v1.1.2 UI Finalization 开发。V5 是交接文档修订号；本段记录的是开发阶段背景，最终发布元数据已升级为 `1.1.2`。

Dashboard 已冻结的状态栏、实时速度、黄色速度条、2×3 数据区、背景、MiSans 比例和 Dynamic Card Geometry 均未重构。

## 已完成

### 快捷控制条组件

新增 `ui_quick_control_bar`，作为长期复用的 LVGL Overlay，只创建：

- 四个固定控制项：WiFi、蓝牙、声音、亮度；
- 一个共享 `focus_capsule`；
- 对应图标、标题和状态文本。

组件支持：

- 整项 Touch 点击；
- Encoder CW/CCW 循环切换 Focus；
- Encoder Press 执行当前项；
- WiFi/蓝牙开启、连接、Pending 和 Failed 状态；
- 声音/亮度百分比显示；
- 小对象按压反馈和 Focus Capsule 横向移动。

组件不直接修改 App State，也不伪造硬件成功。WiFi 和蓝牙继续通过 `ui_command_service` 提交，最终状态来自 Backend 回传后的 `ui_app_state`。

### 系统控制帘状态机

新增唯一状态来源 `ui_system_control_shade`：

```text
CLOSED
  ↓ 第一次下滑
QUICK_BAR
  ↓ 继续下滑
FULL_CONTROL_CENTER
```

反向上滑逐级收起：

```text
FULL_CONTROL_CENTER → QUICK_BAR → CLOSED
```

一次手势最多切换相邻一级，避免视觉位置与状态变量不同步。

### Bottom Surface 最小联动

声音和亮度遵循 V5 的职责划分：

```text
快捷控制条：选择调节对象
Bottom Interaction Surface CONTROL：执行精调
```

进入 CONTROL 后，Encoder 立即以 5% 步进调节。Press 或 Back 退出 CONTROL，Bottom Surface 按既有 Dock/Surface 恢复逻辑恢复此前内容，快捷控制条保留当前 Focus。

如果用户在 CONTROL 仍打开时继续下拉进入完整控制中心，应用会先退出 Bottom CONTROL，再把 Encoder 所有权交给完整控制中心，禁止隐藏控件继续响应输入。

### Gesture Ownership

- Dashboard Dynamic Card 内起手：仍由 Dynamic Card 全程持有；
- Navigation Map 内起手：仍由 Map 全程持有；
- 快捷控制条或完整控制中心起手：由系统控制帘持有；
- 快捷控制条打开且 Bottom CONTROL 可见时，从 Bottom Surface 内起手：由 Bottom Surface 持有到 Release，不能误展开完整控制中心。

### 字体与资源

新增中文全部使用 MiSans 运行时字体。构建会自动重新生成字体子集和码点清单；本阶段新增后运行时字符为 433 个。曾在视觉测试中发现“继续下拉展开”误用 ASCII-only Montserrat，已改为 MiSans，缺字警告已消除。

## PC 操作

| 操作 | 行为 |
| --- | --- |
| 页面顶部第一次下滑 | 打开快捷控制条 |
| 快捷控制条继续下滑 | 展开完整控制中心 |
| 快捷控制条上滑 | 关闭控制帘 |
| 完整控制中心上滑 | 返回快捷控制条 |
| 滚轮/Up/Down | 切换当前顶部 Focus，或调节 Bottom CONTROL |
| Enter | 执行当前项；CONTROL 中退出调节 |
| 右键/Esc/Backspace | 先退出 Bottom CONTROL，再逐级收起控制帘 |
| M | CLOSED → QUICK_BAR → FULL → CLOSED 调试循环 |

## 自动化验证

新增：

- `ui_system_control_shade_test`
- `ui_quick_control_bar_test`

覆盖：

- 三级状态合法转换和非法转换拒绝；
- 四项真实 LVGL 对象尺寸与边界；
- Encoder 正向、反向和循环 Focus；
- Press 回调；
- Pending 阻止重复提交；
- Failed 状态；
- Touch 点击声音入口；
- Focus Capsule 同步；
- 无大图层内存错误；
- 实际 RGB565 framebuffer 渲染检查。

本阶段实测：

```text
Build: PASS
CTest: 22/22 PASS
Font Coverage: PASS
Quick Bar framebuffer: PASS
No missing glyph warning: PASS
No memory/layer allocation warning: PASS
```

## 尚未完成

以下属于后续 V5 阶段，不在 Phase 7B 中伪装为已完成：

- Phase 7C：异步任务关闭顶部后的 Bottom Surface Task Handoff；
- Phase 7C：顶部与底部同一任务的反馈所有权切换；
- Phase 7C：P0/P1 事件自动收起系统控制帘并抢占 Bottom Surface；
- Phase 7D：完整控制中心改为 400～470 px 紧凑面板和横向细滑条；
- Phase 7E：Toggle、Value Fill、Thumb 和 Surface Morph 完整动效；
- Phase 7F：全页面 Final Audit、异常回归和 30～60 分钟压力测试；
- v1.1.2 版本元数据、提交和 Git tag。

## 下一步

下一阶段直接进入 Phase 7C，先建立“顶部反馈所有权”与任务下沉状态模型，再接 P0/P1 自动收帘。不得通过同时发布顶部状态和通用 `UI_EVENT_COMMAND_*` 的方式制造双重反馈。
