# Phase 6A Home / Ride Hub 实现报告

## 1. 阶段目标

本阶段依据 `UI_V4_交接文档.md` 建立新的 Home / Ride Hub，并将其设为 Boot 完成后的默认根页面。Dashboard 已确认区域不做视觉重构。

## 2. 已完成内容

### Home 页面

- 新增纯黑背景的 `ui_home_page`，目标尺寸为 480 × 854。
- 复用统一状态栏组件和全局 App State。
- 建立用户身份卡、累计骑行数据卡、开始骑行、开始导航、最近一次骑行、历史记录、我的成就和系统设置入口。
- 所有可操作区域均为完整 LVGL 对象和完整卡片点击区域，不使用整页截图。
- 增加鼠标按压反馈和 Encoder 焦点反馈。
- Home 默认不显示 Bottom Interaction Surface；只有事件发生时才占用统一底部 Surface。

### 数据模型

- 新增 `ui_home_data_model`，负责用户信息、累计里程/时间/爬升和最近骑行摘要。
- 页面只订阅模型并刷新 Label，不持有业务数据真值。
- PC Simulator 当前使用 Mock 数据，接口可由后续 STM32/存储服务更新。

### 路由与状态机

- Home 注册为 `UI_ROUTE_HOME`，Boot 后默认进入 Home。
- 非骑行态主链：Home ↔ Navigation。
- 骑行态主链：Dashboard ↔ Navigation。
- 开始骑行流程调整为：

```text
Home
  → Dashboard
  → PRE_RIDE
  → GPS searching
  → GPS ready
  → GO command pending
  → RIDING
```

- `GO` 不再提前伪造骑行成功；只有 `UI_COMMAND_RIDE_START` 成功后才发布 `UI_EVENT_RIDE_GO`。
- GPS 搜索、GPS Ready、骑行开始反馈与 Dynamic Card 共用同一个 Bottom Interaction Surface，未创建独立 Popup。

### Control Center

- 删除开始骑行、暂停骑行和结束骑行三组重复业务按钮。
- 保留 WiFi、蓝牙、声音、亮度、GPS 和省电模式快捷控制。

### 字体

- 重新生成 `ui_font_misans_ui_20.c`，补齐 Home 与 GPS 流程新增中文字符。

## 3. 新增文件

```text
src/ui/models/ui_home_data_model.c
src/ui/models/ui_home_data_model.h
src/ui/pages/ui_home_page.c
src/ui/pages/ui_home_page.h
tests/ui_home_data_model_test.c
docs/PHASE_6A_HOME_RIDE_HUB_REPORT.md
```

## 4. 修改的关键文件

```text
CMakeLists.txt
src/ui/core/ui_app.c
src/ui/core/ui_dock_controller.c
src/ui/core/ui_event.h
src/ui/overlays/ui_control_center.c
src/ui/overlays/ui_control_center.h
src/ui/pages/ui_page_action.h
src/ui/fonts/ui_font_misans_ui_20.c
tests/ui_core_logic_test.c
tests/ui_dock_view_test.c
README.md
CHANGELOG.md
```

## 5. 验收结果

- [x] Simulator 启动后进入 Home。
- [x] Home 480 × 854 范围内无对象越界或相邻页面透出。
- [x] 用户卡、累计数据、操作入口、最近骑行和三个功能行完整显示。
- [x] 开始骑行完整卡片可点击，并有按压反馈。
- [x] 开始骑行后进入 Dashboard 与 PRE_RIDE。
- [x] GPS 搜索与 GPS Ready 在同一个 Bottom Interaction Surface 内原位切换。
- [x] 点击 GO 后等待异步命令结果，再进入 RIDING。
- [x] 骑行开始提示结束后恢复 Dynamic Card。
- [x] Control Center 不再包含重复骑行控制按钮。
- [x] UCRT64 编译成功。
- [x] CTest 8/8 通过。

## 6. 性能结果

2026-09-16，Windows PC Simulator，UCRT64，10 秒基准：

```text
static: fps_avg=59, fps_min=58, cpu_avg=1%, cpu_max=3%
data:   fps_avg=59, fps_min=58, cpu_avg=1%, cpu_max=3%
```

首次并行执行 static/data 基准时 static 出现一次 `cpu_max=30%` 的启动抖动；单独复测恢复为 3%，因此不视为持续性能回退，但后续仍应在长期压力测试中观察峰值。

## 7. 已知边界

- 头像、累计数据卡山景和最近骑行路线是 LVGL 本地 Mock 图形，不是最终品牌资源或真实轨迹。
- History 页面将在后续阶段实现；当前入口通过统一 Bottom Surface 给出占位信息。
- Achievement 当前仅保留入口和占位反馈。
- 原 Profile 页面为兼容旧路由暂时保留，但已退出非骑行态主导航链。
- Home 数据尚未接入 TF 卡持久化、用户账号或 STM32 骑行记录服务。

## 8. 下一阶段

1. Phase 6B：按 V4 约束重构 Control Center 内容密度和状态表达，但继续复用现有命令服务。
2. Phase 6C：完善 Navigation 与真实离线地图数据接口，不在 UI 层直接解析完整地图数据库。
3. Phase 6D：实现 Settings 子页面与持久化设置模型。
4. Phase 6E：实现 Ride Summary 与骑行记录落盘闭环。
5. Phase 6F：实现 History 列表、详情和 Home 最近骑行数据联动。
