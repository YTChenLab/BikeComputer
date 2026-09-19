# Phase 7D/7E 控制中心精简与胶囊动效报告

> 架构修订：当前 Compact/Expanded 由同一个 `ui_control_center` Panel 通过高度 Morph 实现；Expanded 上滑直接关闭，声音/亮度使用 Inline Adjust。以 `PHASE_7G_UNIFIED_CONTROL_SURFACE_REPORT.md` 为准。

## 完整控制中心精简

完整控制中心保留八项系统能力，不重写 Command/App State：

```text
WiFi | Bluetooth
Sound
Brightness
GPS | Power Save | Touch Lock | Battery
```

视觉面板高度固定为 `462 px`。根 Overlay 仍为 `480 × 854` 的透明可点击输入层，用于阻止主页面穿透点击；实际不透明视觉只存在于顶部 462 px，下半主页面保持可见。

声音和亮度改为：

- 4 px 横向 Track；
- 12 px Thumb；
- 黄色 Fill；
- 右侧百分比；
- Touch Track 直接拖动；
- 点击行或 Encoder Press 进入共享 Bottom Interaction Surface CONTROL。

从完整控制中心进入 Bottom CONTROL 时，完整面板先退回快捷控制条，使底部 CONTROL 可见且可触摸。Track 内单击/拖动只提交数值，不会同时重复打开 Bottom CONTROL。

## 胶囊流体动效 V1

新增统一 Motion API：

- `ui_motion_focus_capsule()`：Focus Capsule 横向移动并短暂拉伸；
- `ui_motion_toggle_press/release()`：局部按压回弹；
- `ui_motion_pulse()`：Pending 或 Thumb 的单次脉冲；
- Fill Width 和 Thumb X 平滑推进；
- 完整控制中心使用 `UI_TOKEN_MOTION_CONTROL_EXPAND_MS`；
- Bottom Surface 继续使用几何 Morph，不对大型 Host 做 Scale/Opacity。

没有使用 Blur、Mask、Metaball、全屏 Scale、大对象透明动画或大图片 Transform。

## Gesture 与输入

- 全屏透明输入层阻止控制中心打开时触摸穿透；
- 视觉面板只占顶部 462 px；
- 横条 Touch 拖动按 X 坐标映射 0～100%；
- Track 外点击声音/亮度进入 Bottom CONTROL；
- Bottom CONTROL 可见时拥有触摸序列；
- Encoder Press 不在完整控制中心内部切换“调整模式”，而是进入 Bottom CONTROL；
- 返回键先退出 CONTROL，再逐级收起系统控制帘。

## Final Audit 修正

- Home 删除未实现且点击无反应的 Achievements 入口；
- Settings 上移并纳入新的 5 项 Encoder Focus 顺序；
- Legacy Profile 保留路由兼容，但标题明确标记为 `LEGACY PROFILE · DEPRECATED`；
- Legacy Profile 删除无功能 Achievements 按钮；
- 删除旧竖向百分比坐标 API，防止后续误用。

## 验证

真实 RGB565 framebuffer 检查结果：

- 462 px 面板完整；
- 下半页面可见；
- WiFi/蓝牙等宽；
- 声音/亮度横条完整；
- 四个状态卡对齐；
- 中文无缺字；
- 无裁切和重叠。

自动压力测试：

- 250 次 Focus Capsule/Pulse 循环；
- 250 次控制中心状态、数值和 Pending 更新；
- 动画结束数量为 0；
- 对象数量不增长；
- 无 `No memory`；
- 无 `Allocating layer buffer failed`。

当前自动化测试：`24/24 PASS`。

## 尚未替代的 RC 验收

加速自动压力测试不能替代 30～60 分钟真实 Simulator 交互测试。正式 `v1.1.2 RC` 前仍需：

- 可见窗口完整人工流程；
- FAILED/TIMEOUT/REJECTED/P0/P1 实际交互检查；
- 30～60 分钟 Quick Bar、Full Control Center、Dock、Navigation、Ride 和 History 混合压力；
- FPS、CPU、Heap、对象、Timer 和 Animation 峰值记录。
