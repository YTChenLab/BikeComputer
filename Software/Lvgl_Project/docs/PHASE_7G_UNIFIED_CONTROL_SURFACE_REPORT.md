# Phase 7G 统一 System Control Surface 报告

## 结论

本阶段将系统控制中心收敛为一个 `ui_control_center` 实例和一个 LVGL 根对象。它只有三个逻辑状态：

```text
CLOSED → COMPACT → EXPANDED
```

`COMPACT` 与 `EXPANDED` 共享同一个 Panel、同一组卡片、同一组滑条和同一个 Focus Capsule。展开不是创建第二个 Overlay，收起也不会切换到另一个页面。

## 交互行为

- 第一次下拉：从 `CLOSED` 进入 `COMPACT`，四项为 WiFi、蓝牙、声音、亮度。
- 第二次连续下拉：同一 Surface 通过高度 Morph 进入 `EXPANDED`。
- `EXPANDED` 上滑：直接关闭到 `CLOSED`，不会先回到 Compact。
- 展开态声音/亮度直接在控制中心 Inline Adjust，旋钮或拖动松手后只提交一次命令，不再呼出 Bottom CONTROL。
- Settings 页面原有 Bottom CONTROL 保留，但不属于 System Control Surface 的数值调节路径。
- Touch Down 后直到 Release 维持本次手势所有权；拖出边界也不会交给页面级下滑手势。
- Slider 先进行横向轴锁，避免调节数值时误触发 Surface 的纵向展开/关闭。

## 视觉与动效

- Compact 使用四项统一尺寸卡片和单一 Focus Capsule 磁吸/拉伸反馈。
- Expanded 使用统一横卡、横向滑条和底部状态卡。
- Panel 底部保留 24 px 圆角，中央 72×5 指示条随 Panel 高度移动。
- 拖动期间只更新 Panel 高度或 Surface Y；松手后只触发一次 Snap 动画。
- 中途状态卡在 Panel 足够容纳时才显示；连接/滑条文字在重排时用小对象文本透明度淡出再淡入。
- 视觉状态更新与 Slider 数值动画分离，连续 Pointer MOVE 不会反复取消/重启滑条动画，因此不会产生抽搐。
- 动画只作用于位置、尺寸、局部胶囊和滑条，不使用大对象 Scale、Blur、Metaball 或全屏离屏层。

## 代码边界

核心文件：

- `src/ui/overlays/ui_control_center.c/.h`：统一 Surface、Compact/Expanded 几何、Focus、Inline Adjust。
- `src/ui/core/ui_app.c`：Surface 手势所有权、三态切换、PC 输入绑定和命令提交。
- `src/ui/core/ui_system_control_shade.c/.h`：`CLOSED/COMPACT/EXPANDED` 逻辑状态机。
- `src/ui/core/ui_motion.c/.h`：局部动效和几何过渡。

旧的 `ui_quick_control_bar.c/.h` 与独立 Quick Bar 测试已经删除。产品代码不得重新引入第二个控制中心 Overlay。

## 自动化验收

`ui_control_center_test` 覆盖：

- Compact 四项真实对象边界；
- Expanded 7 项焦点和 Inline Adjust；
- Compact/Expanded 共用子对象数量；
- 以 7 px 步长扫描整个形态高度，四项卡片均保持在 Panel 内；
- 250 次状态/动画压力循环；
- 50 次 Compact ↔ Expanded 形态往返；
- 动画归零和对象数量不增长；
- 可选输出 Compact、Morph、Expanded RGB565 帧缓冲快照：
  `UI_CONTROL_CENTER_COMPACT_SNAPSHOT_PATH`、
  `UI_CONTROL_CENTER_MORPH_SNAPSHOT_PATH`、
  `UI_CONTROL_CENTER_SNAPSHOT_PATH`。

当前构建与 CTest 结果：23/23 通过。当前仍未宣称 v1.1.2 RC，人工 Simulator 长时压力和异常注入验收仍是发布前条件。

## 后续工作

1. 用可见 Simulator 验证第一次下拉、第二次展开、Expanded 上滑关闭和 Slider 轴锁。
2. 采集 Compact/Morph/Expanded 帧，核对不同字体和长 WiFi 名称的省略与边界。
3. 建立开启 `LV_USE_MEM_MONITOR`/`LV_USE_ASSERT` 的 Debug 压力配置，记录 FPS、Heap、Peak Heap、Timer 和 Animation。
4. 完成人工异常注入后，才更新 `VERSION` 并创建 `v1.1.2` tag。
