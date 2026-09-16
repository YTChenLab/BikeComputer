# Phase 5.1 Bottom Surface 原位切换修正

## 修正原因

Phase 5 虽然删除了旧 Interaction Dock 的独立视觉根对象，但仍存在三项错误行为：

- Surface 创建在 `lv_layer_top()`，Dashboard 中仍表现为覆盖层。
- INFO/ACTION 映射为 Mini，导致 Surface 从 `y=614, h=226` 移动到 `y=684, h=156`。
- Interaction 内容继续使用从下方向上移动 50 px 的 Dock 入场动画。

因此 Phase 5 只完成了对象树合并，没有达到“内容变，不换容器”的视觉目标。

## 当前实现

Dashboard 页面创建固定 Slot：

```text
Dashboard Screen
  └─ Bottom Surface Slot (x=8, y=614, w=464, h=226)
       └─ ui_bottom_surface.root (x=0, y=0, w=464, h=226)
            ├─ dynamic_host
            └─ interaction_host
```

Dashboard 激活期间：

- 六类 Interaction 全部使用 Normal 464×226 Surface。
- Mode 切换不改变 Surface 的父对象、坐标、宽度或高度。
- 删除内容的纵向弹入动画。
- Dynamic 与 Interaction Host 使用 120 ms 原位交叉淡入。
- Surface 背景保持完全不透明，不会露出页面数据形成叠层。

Dashboard 离开后，Surface 可以暂时挂到 top layer 备用位置，以保证其他页面尚未完成 Slot 迁移前，P0/P1 安全事件仍然可见。Phase 6 会为其余页面建立正式 Slot，随后删除该备用视觉路径。

## 强制回归条件

- Dashboard 激活时 `ui_bottom_surface.root` 的父对象必须是页面 Slot。
- 任意 INFO/ACTION/CONFIRM/CONTROL/NAVIGATION/MEDIA 切换前后：
  - root `x=0`
  - root `y=0`
  - root `w=464`
  - root `h=226`
- Interaction 内容根对象始终 `y=0`。
- Dashboard 模式下 top layer 不得包含 Bottom Surface 根对象。
- 事件结束后恢复 Dynamic Card，并保留原 `active_card`。

这些条件已加入 `ui_bottom_surface_test` 和 `ui_dock_view_test`，防止以后重新退化为底部弹窗。

## 验证结果

- Anaconda/MSYS2 UCRT64 全量编译通过。
- CTest 7/7 通过。
- Simulator 静态场景：平均 59 FPS，最低 58 FPS，LVGL 忙碌度平均 4%，峰值 8%。
- Simulator 100 ms 数据更新场景：平均 59 FPS，最低 58 FPS，LVGL 忙碌度平均 6%，峰值 10%。
