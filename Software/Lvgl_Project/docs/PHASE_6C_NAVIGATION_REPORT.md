# Phase 6C Navigation 视觉与交互统一报告

## 阶段结论

Navigation 保留现有“地图 + 2×2 数据卡”结构，没有引入真实地图引擎，也没有把页面改造成图片。地图拖拽、缩放、回中、位置更新和导航数据邮箱继续沿用既有 LVGL 对象与服务接口。

## 视觉与布局

- 新增 `ui_nav_layout.h`，统一 480×854 页面边距、地图尺寸、分区间距、卡片尺寸和 2×2 Grid 参数；
- 地图区与数据卡使用 Design Token 的 Surface、Border、Radius 和主文字角色；
- 实时速度与累计里程保持“数值 + 单位”整体水平居中，并按基线靠近卡片下沿；
- ETA 主值保持居中并靠近下沿；
- 航向卡保留粗体大转向图形，指令文字独立位于底部，不与标题或箭头重叠；
- 地图 HUD 的指南针、放大、缩小和回中控件统一为 48×48 触控目标。

## Touch 与 Encoder Focus

- 鼠标左键仍可直接拖动地图并点击 HUD 控件；
- 鼠标滚轮/Encoder CW 执行放大并聚焦放大控件；
- 鼠标滚轮/Encoder CCW 执行缩小并聚焦缩小控件；
- Enter/Encoder Press 执行回中并聚焦定位控件；
- Focus、Active 和普通状态由 `ui_nav_control` 统一绘制，触摸和语义输入调用同一业务回调；
- 地图区域继续在 PRESS 到 RELEASE 期间持有手势所有权，不会误触全局页面切换或 Control Center。

## Navigation Override

导航方向改变时，页面向 Event Bus 发布 `UI_EVENT_NAVIGATION_TURN`：

- 优先级：`P1_RIDING_SAFETY`；
- 内容：下一转向距离与方向；
- 展示位置：当前页面已经挂载的唯一 Bottom Interaction Surface Slot；
- 展示方式：原位从页面正常内容切换为 Navigation Interaction；
- 结束方式：默认 5 秒超时后恢复此前 Surface 状态。

该事件在 Navigation 页面可见或暂时切到 Dashboard 时都能由同一导航模型更新触发。没有新增独立 Popup 或第二个底部容器。

## 内存与动画约束

旧转向内容使用位置加整块透明度动画，LVGL 可能为对象创建 ARGB8888 中间图层。本阶段改为纯纵向位移动画，避免为 227×114 内容区分配额外混合图层。地图缩放继续通过轻量自绘几何和数值动画完成，不缩放 620×620 LVGL 对象本身。

## 自动化验收

新增 `ui_navigation_test` 并扩展 `ui_dock_view_test`，覆盖：

- 地图与 2×2 卡片全部位于 480×854 可视区域；
- 四张卡尺寸一致，无重叠和越界；
- Encoder 放大、缩小和回中状态正确；
- Focus 与执行动作同步；
- 转向模型变化发布 P1 Navigation 事件；
- Bottom Surface 原位抢占，超时后恢复 Dynamic Card Mode；
- 真实 LVGL 刷新期间没有 `No memory` 或 Layer Buffer 分配失败。

全量 CTest：12/12 通过。

## 阶段边界与后续工作

- 当前道路与路线仍是 LVGL Mock 几何，不代表真实西安地图；
- TF 卡瓦片读取、空间索引、坐标投影、路线计算和缓存淘汰属于后续 Offline Map Engine，不应放进 UI 线程；
- 下一阶段按 V4 路线进入 Phase 6D，统一 Settings / System / Power 的 List、Detail、Toggle、Slider、Selection 和 Info Row。
