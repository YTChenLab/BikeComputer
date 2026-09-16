# Phase 4 UI Design System 与 Bottom Surface

## 本阶段结论

Phase 4 已建立第一版设计 Token、公共 Surface 样式和唯一 `ui_bottom_surface`。P1 顶部状态栏、实时速度、黄色速度条、2×3 数据区、MiSans 字体及背景资源没有重构。

## Design Token

文件：

- `src/ui/design/ui_design_tokens.h`
- `src/ui/design/ui_component_style.h`
- `src/ui/design/ui_component_style.c`

第一版 Token 包含：

- Canvas、Surface、主/次文字、强调色、边框、结构线和禁用色。
- 2/4/8/12/16/20/24/32 间距阶梯。
- Small/Medium/Surface 圆角。
- Thin/Focus 边框。
- 480×854 屏幕和 Bottom Surface 几何。
- Fast/Standard/Surface 动画时长。

`ui_colors.h` 与 `ui_sizes.h` 暂时作为兼容别名继续存在，既有页面无需一次性重构。后续页面迁移必须优先使用语义 Token，禁止继续增加无归属的十六进制颜色和页面私有间距常量。

## Bottom Interaction Surface 对象树

```text
Current Page Screen
  └─ Bottom Surface Slot
       └─ ui_bottom_surface.root
            ├─ dynamic_host
            │    └─ Dynamic Card Viewport
            └─ interaction_host
                 └─ Interaction content
```

上图是 Phase 6 完成后的正式对象树。Phase 4 初版曾挂载到 `lv_layer_top()`，该方案已废弃，运行时代码不再包含备用 top-layer 路径。

Surface 是唯一拥有以下视觉属性的对象：

- 屏幕位置和宽度
- 背景和边框
- 圆角与裁切
- Mini / Normal / Expanded 高度
- Surface 位移动画和高度动画
- Dynamic Card `Neutral Focus` 的黄色外层焦点环

Dynamic Card 的四张卡已经改为透明内容对象，不再分别绘制外层背景、边框和圆角。滚动位置、active card 和内部 Focus 对象没有重建，因此 Surface 隐藏或事件抢占后可以保留原状态。

## Surface 状态模型

内容模式：

- `UI_BOTTOM_SURFACE_DYNAMIC_CARD_MODE`
- `UI_BOTTOM_SURFACE_INTERACTION_MODE`

尺寸状态：

- `UI_BOTTOM_SURFACE_SIZE_MINI`
- `UI_BOTTOM_SURFACE_SIZE_NORMAL`
- `UI_BOTTOM_SURFACE_SIZE_EXPANDED`

交互内容使用 owner ID 和 P0～P4 优先级。更高优先级内容可以暂时压入恢复栈；结束后恢复先前 owner。低优先级内容不能覆盖当前高优先级内容。

该状态机只允许在 LVGL UI 线程调用。跨任务事件仍需经过现有 Event Bridge 和 Command/State Adapter。

## 页面生命周期

- Dashboard `on_enter` 显示 Normal Dynamic Card Mode。
- Dashboard `on_leave` 隐藏 Normal 内容。
- Interaction Mode 可以在具有 Bottom Surface Slot 的页面内显示，并由 Router 生命周期挂载。
- Phase 4 初版 Surface 位于 top layer；Dashboard Slot 在 Phase 5.1 建立，其余页面 Slot 与完整 Router 生命周期在 Phase 6 完成。

## 当前迁移边界

已完成：

- Dynamic Card 接入 `dynamic_host`。
- Dynamic Card 外层视觉归属 Surface。
- Surface 尺寸、模式、优先级、抢占和恢复基础。
- 设计 Token 与公共 Surface Style。

Phase 4 结束时尚未完成、现已在 Phase 5/6 完成：

- 删除旧 `ui_dock_view` 独立 top-layer 根对象。
- INFO/ACTION/CONFIRM/CONTROL/NAVIGATION/MEDIA 模板迁入 `interaction_host`。
- Command Pending/Failed/Timeout 映射为 Surface 内容。
- 页面级 Slot、Router 挂载/卸载和跨页状态恢复。

Mini/Expanded 的后续复杂模板仍需按具体业务继续做视觉验收，但不得恢复独立 Layer 或双容器路径。

## 验证结果

验证环境为项目指定的 Anaconda/MSYS2 UCRT64 工具链：

- UCRT64 全量编译通过。
- CTest 7/7 通过，其中 `ui_bottom_surface_test` 覆盖对象树、三档尺寸、内容模式、焦点环、优先级抢占和栈恢复。
- Simulator 静态场景：平均 59 FPS，最低 59 FPS，LVGL 忙碌度平均 5%，峰值 10%。
- Simulator 100 ms 数据更新场景：平均 59 FPS，最低 58 FPS，LVGL 忙碌度平均 5%，峰值 11%。

这里的 CPU 数值来自 `100 - lv_timer_get_idle()`，表示 LVGL handler 忙碌度，不等同于 Windows 整机 CPU 占用。
