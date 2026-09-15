# UI App State 与 Router 生命周期架构

## 目标

本阶段只迁移 UI 的状态所有权和页面生命周期，不改变已经确认的页面视觉。它解决以下问题：

- WiFi、蓝牙、GPS、声音、亮度和媒体状态散落在页面或组件静态变量中。
- 不同页面状态栏显示不同步。
- 页面隐藏后仍运行高频定时器，增加 CPU 占用并可能修改不可见对象。
- SDL 键鼠事件直接进入 UI 逻辑，无法平滑替换为 STM32 GPIO 与实体编码器。

## 单向数据流

```text
PC SDL / STM32 GPIO / Service
              |
              v
       UI_INPUT_* 或状态更新
              |
              v
        ui_app_state（唯一真值）
              |
              +--> 状态栏注册表
              +--> Control Center
              +--> Dynamic Card / Music
              +--> 页面与服务适配器
```

页面和组件不再各自保存系统级状态。交互事件只调用 `ui_app_state_set_*()`，订阅者根据 Change Mask 更新视图或服务。

## `ui_app_state`

文件：

- `src/ui/core/ui_app_state.h`
- `src/ui/core/ui_app_state.c`

统一状态域：

- `ROUTE`：当前页面。
- `RIDE`：待机、骑行、暂停和结束流程状态。
- `STATUS`：时间、电池、电池充电和卫星数。
- `CONNECTIVITY`：WiFi、蓝牙、GPS 开关、连接状态和设备名称。
- `CONTROL`：声音、亮度和省电模式。
- `MEDIA`：播放状态、当前曲目和播放模式。
- `NAVIGATION`：导航页活动状态。

订阅回调在内部锁释放后执行，避免回调再次写入 App State 时产生递归锁死。没有实际值变化时不会增加 `version` 或发出通知。

## Router 生命周期

`ui_router_page_t` 支持：

- `create`：可选的懒创建入口。
- `on_enter`：页面真正显示后启动页面资源。
- `on_leave`：页面离开前暂停定时器或输入处理。
- `on_destroy`：Router 释放页面前清理外部资源。

当前已接入：

- Dashboard：仅在页面活动时运行速度过渡动画。
- Navigation：仅在页面活动时运行 UI 刷新与 PC 模拟数据定时器。

新增页面应使用 `ui_router_register_page()` 注册，并把页面周期性任务绑定到生命周期，不得依赖“对象仍存在”等价于“页面可见”。

## 状态栏绑定

`ui_page_common` 中的状态栏注册表统一订阅 App State。页面创建状态栏时调用 `ui_page_status_create()`；之后无需直接设置时间、电量、卫星、WiFi 或蓝牙。

这保证 Dashboard、Navigation、Summary、Profile、Settings 和 System 页面在切换时使用同一快照，不再出现硬编码 `10:24`、`82%` 或连接状态滞后。

## 平台输入边界

UI 层仅接收：

- `UI_INPUT_ENCODER_CW`
- `UI_INPUT_ENCODER_CCW`
- `UI_INPUT_ENCODER_PRESS`
- `UI_INPUT_BACK`
- `UI_INPUT_PAGE_PREV/NEXT`
- `UI_INPUT_TOUCH_BEGIN/MOVE/END/CANCEL`

PC 的 `hal.c` 将 SDL 鼠标、滚轮和键盘转换为上述事件。STM32 端后续只需提供相同语义事件，Router、Dynamic Card 和页面代码不需要知道输入来源。

## 约束与后续工作

- 本阶段没有修改 P1 顶部状态栏、实时速度、黄色速度条、2×3 数据区、Dynamic Card 几何尺寸、MiSans 字体或背景资源。
- 当前页面仍以启动时创建为主，Router 已具备懒创建接口，但应在下一阶段按页面逐个迁移，避免一次性改变启动时序。
- 下一阶段应把 Ride Data Model、地图服务和硬件状态通过显式 Adapter 接入 App State，并增加页面切换压力测试和状态一致性测试。
- STM32 侧必须在 LVGL/UI 线程处理 App State 通知；外设任务应通过队列提交语义事件或状态快照，不应直接调用 LVGL 对象 API。
