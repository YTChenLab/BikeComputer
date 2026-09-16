# Phase 6 页面 Slot 与 Router 生命周期迁移

## 本阶段目标

Phase 6 将唯一的 `ui_bottom_surface.root` 从 Dashboard 专用挂载升级为页面级生命周期资源。Surface 不再使用 `lv_layer_top()` 备用路径，也不会在页面之外创建第二个 Popup。

## 页面对象关系

每个可交互页面都创建相同几何的透明 Slot：

```text
Current Page Screen
  └─ Bottom Surface Slot (x=8, y=614, w=464, h=226)
       └─ ui_bottom_surface.root
            ├─ dynamic_host
            └─ interaction_host
```

Slot 只提供页面内挂载位置，不绘制背景、不接收点击。Surface 自身继续统一拥有背景、边框、圆角、尺寸和动画。

## 路由生命周期

- `on_enter`：把唯一 Surface 挂到当前页面 Slot。
- `on_leave`：卸载并隐藏 Surface，防止页面过渡期间残留。
- Dashboard：`dynamic_content_visible = true`，正常显示 Dynamic Card。
- Navigation / Summary / Profile / Settings / System / Power：正常状态隐藏 Surface；发生 Interaction 时在固定 Slot 内显示，结束后恢复该页面原内容。
- Boot / Power Off：没有 Slot，Surface 保持卸载和隐藏。

Interaction 状态、owner、优先级栈不会因页面切换被清空。若事件在无 Slot 阶段到达，状态会保留；进入下一个有 Slot 的页面时再显示。

## 固定行为

- 所有页面 Slot 内 `root.x = 0`。
- Normal 与 Mini 请求均以固定 464×226 区域显示，避免退化为小弹窗。
- Expanded 从 Slot 底边向上扩展。
- Surface 根对象永远不是 `lv_layer_top()` 的子对象。
- Dynamic Card 的 active card、滚动位置和控制焦点不因页面切换而重建。

## 本阶段边界

本阶段只统一页面挂载和 Router 生命周期，没有重排导航、个人、设置、总结、系统页面的现有业务内容。下一阶段才逐页将颜色、间距、字体角色和组件尺寸迁移到 Design Token。

## 验证

- UCRT64 全量编译通过。
- CTest 7/7 通过。
- 测试覆盖 Dashboard 动态内容、非首页 Interaction、卸载期间事件保留、250 次跨 Slot 重挂载、Mini 固定 Normal 几何和 top layer 无 Surface 根对象。
- Simulator 静态场景：平均 59 FPS，最低 58 FPS，LVGL 忙碌度平均 5%，峰值 11%。
- Simulator 100 ms 数据更新场景：平均 59 FPS，最低 58 FPS，LVGL 忙碌度平均 5%，峰值 10%。

CPU 数值来自 `100 - lv_timer_get_idle()`，代表 LVGL handler 忙碌度，不等同于 Windows 整机 CPU 使用率。
