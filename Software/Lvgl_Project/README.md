# BikeComputer LVGL UI

BikeComputer 公路车码表的 LVGL UI 工程和 Windows PC Simulator。

当前页面按最终产品布局统一 16 px 底边距，底部操作按钮统一 72 px；图标、按钮与双行文字组按实际字体居中。性能信息以半透明系统浮层显示，不预留页面空间。规范和真实渲染见 [全局底部与对齐规范](docs/全局底部与对齐规范_20261002.md)。

## 版本信息

- BikeComputer UI 工程版本：`v1.4.0`
- LVGL 源码版本：`9.3.0-dev`
- 目标分辨率：`480 × 854`
- 色彩格式：RGB565
- 当前 PC 工具链：Anaconda/MSYS2 UCRT64

本目录中的 `lvgl/` 是当前可运行版本对应的源码快照。它包含项目现阶段依赖的本地修改，因此没有继续作为上游 Git submodule 保存。后续升级底层 LVGL 时，应单独记录上游 commit、迁移差异和回归测试结果。

本轮软件增加连续弹性胶囊动效、骑行/保存/电源状态闭环、带质量和时间的数据接口、后台存储及历史分页、配置持久化和个人区间编辑。硬件可通过任务安全邮箱和异步命令 ACK 接入；无需从传感器任务直接调用 LVGL。

v1.4.0 的归档范围、版本迭代日志与发布验证见 [发布记录](docs/V1.4.0_RELEASE_NOTES.md)。候选阶段的软件交付说明见 [软件交付与验收](docs/软件交付与验收_v1.4.0-rc1.md)；真实数据、命令、任务与存储接入见 [硬件数据与任务接口](docs/硬件数据与任务接口_v1.4.0-rc1.md)。本版本归档当前 LVGL 软件基线，PC 验证不代表 STM32 性能或整机商用验收完成。

当前启动页已接入用户提供的三张 LOGO，使用 5 秒 PRTS 两幕动画；完成后约 280 ms 淡出，首页按八组自上而下错开淡入，并轻微上移 12 px 落位，每组 360 ms、组间错开 60 ms，总计约 780 ms，随后开放交互。首次启动、重新启动和关机后开机沿用同一过渡，减少动效设置下跳过演出和过渡。源码、接入与验证边界见 [PRTS 开机动画接入](docs/PRTS开机动画接入_20261001.md)。

其他 UI 细化包括速度区居中的骑行状态、语义区间色与 16 项成就档案；官方产品对照、真实预览和剩余工作见 [UI 精致度审查与商用对照](docs/UI精致度审查与商用对照_20261001.md)。

骑行页已恢复按速度变色的平行四边形进度条，放大粗体区间编号并加厚区间条；新增巡航/训练/爬坡方案、六格指标编辑与持久化，以及四项一页的扩展数据。操作与实际预览见 [骑行页优化与布局使用说明](docs/骑行页优化与布局使用说明_20261001.md)。

当前 UI 采用纯黑画布、中性灰表面、白色文字和黄色主题。个人主页改为开放身份区、骑行统计胶囊、一个成就模块及两条轻量入口；首页水印、设置滚动条／图标／开关和成就页同步收敛为黄色。保留新版 PRTS 开机、首页入场、真实数据与 ACK 流程，实际预览见 [黑黄主题与个人主页重构](docs/黑黄主题与个人主页重构_20261001.md)。上一轮数据口径与实现记录见 [首页个人主页与设置细化](docs/首页个人主页与设置细化_20261001.md)。

HOME 与 Profile 已按最终视觉基准落地：HOME 保留身份卡、Lifetime 摘要、开始骑行／导航、最近一次骑行和历史／设备与传感器／收藏路线／设置入口；Profile 采用身份卡、2×2 总数据、成就进度、最近 30 天活动矩阵及五个功能入口。示例数字继续由历史统计、成就和首页数据模型提供；最近骑行缩略图在真实轨迹字段接入前使用中性路线图标，不伪造轨迹。收藏路线与设备连接已有独立页面和动作，保持真实空态及未接入反馈，后续可接入真实后端。

## 目录说明

```text
Lvgl_Project/
├─ assets/                 背景、字体源文件、图标和生成资源
├─ docs/                   地图、背景资源和交互实现说明
├─ lvgl/                   当前项目使用的 LVGL 9.3.0-dev 源码快照
├─ src/
│  ├─ hal/                 PC Simulator 输入与显示适配
│  ├─ platform/            平台抽象
│  ├─ services/            PC/STM32 服务接口与模拟数据源
│  └─ ui/                  页面、组件、Core、Design Token、Dock、模型和字体
├─ tests/                  UI Core 与 Dock 自动化测试
├─ tools/                  字体和资源生成工具
├─ CMakePresets.json       UCRT64 配置、构建和测试 preset
└─ simulator.code-workspace
```

`build/`、`bin/` 和 `.tools/` 是本地生成目录，不纳入 Git。

## 构建环境

当前 preset 和 VS Code 工作区使用以下工具链目录：

```text
D:/Professional Software/Anaconda/Bike_SpeedView/msys2-x86_64/ucrt64/bin
```

至少需要：

- `gcc.exe` / `g++.exe`
- `cmake.exe`
- `ninja.exe`
- `gdb.exe`（调试时使用）
- UCRT64 SDL2 开发库

如果工具链安装到了其他位置，需要同步修改 `CMakePresets.json`、`simulator.code-workspace` 和 `.vscode/` 中的路径。

## 命令行构建与测试

在本目录打开 PowerShell：

```powershell
& "D:/Professional Software/Anaconda/Bike_SpeedView/msys2-x86_64/ucrt64/bin/cmake.exe" --preset bike-ui-ucrt64
& "D:/Professional Software/Anaconda/Bike_SpeedView/msys2-x86_64/ucrt64/bin/cmake.exe" --build --preset bike-ui-ucrt64 --parallel
& "D:/Professional Software/Anaconda/Bike_SpeedView/msys2-x86_64/ucrt64/bin/ctest.exe" --preset bike-ui-ucrt64
```

运行 Simulator：

```powershell
.\启动模拟器.cmd
```

Windows 构建会自动将现有工具链的 `SDL2.dll` 复制到 `bin/main.exe` 旁，运行模拟器无需注入工具链 `PATH`。双击项目根目录的 `启动模拟器.cmd` 会使用正确工作目录，确保相对路径的背景和持久化资源可以读取。使用 `simulator.code-workspace` 启动仍沿用已有调试配置。

模拟器默认在右下角显示 LVGL 性能信息：FPS、CPU 占用估计，以及渲染和刷屏耗时（毫秒）。启动前设置环境变量 `UI_SHOW_PERFORMANCE=0` 可隐藏，删除该变量或设为 `1` 可显示。这里的 CPU 为 LVGL 根据定时器空闲时间计算的占用估计，不是 Windows 整机 CPU 使用率；PC 数据也不代表 STM32 真机性能。

模拟器默认以接近 3 英寸的尺寸显示，内部仍为 480×854。`F1` 为实物尺寸估算，`F2` 为 50%，`F3` 为原始尺寸，`+/-` 调整大小并记住偏好。要精确匹配实物，请按 F1 用尺子将画面高度校准到约 66.43 mm，详见 [模拟器尺寸与校准](docs/模拟器尺寸与校准_20261002.md)。

## VS Code 运行

1. 使用 VS Code 打开 `simulator.code-workspace`。
2. 确认 CMake Tools 选择 `bike-ui-ucrt64` preset。
3. 按 `Ctrl+F5` 运行，或选择 `LVGL Simulator` 调试配置。

## PC 输入映射

- 鼠标左键：模拟触摸按下、拖动和释放
- 鼠标右键 / `Esc` / `Backspace` 单击：返回上一级（为识别双击，释放后约 280 ms 执行）
- 鼠标右键 / `Esc` / `Backspace` 双击：骑行中手动暂停；暂停后再次双击恢复
- 鼠标右键 / `Esc` / `Backspace` 长按约 1.2 s：骑行中打开“结束骑行”确认
- 鼠标滚轮 / 方向键：模拟 Encoder 旋转
- `Enter`：模拟 Encoder 按下
- 页面顶部第一次下滑：打开统一 System Control Surface 的 `COMPACT` 状态；继续下滑：同一 Surface Morph 到 `EXPANDED`
- `COMPACT` 上滑关闭；`EXPANDED` 上滑直接关闭，不返回 Compact
- 控制中心内滚轮/方向键：循环选择当前状态可用控件；`Enter` 执行
- `EXPANDED` 中声音或亮度直接 Inline Adjust；滚轮/方向键以 5% 步进调节，`Enter` 或返回键提交并保持控制中心，不呼出 Bottom CONTROL
- `M`：按“关闭 → Compact → Expanded → 关闭”循环切换，便于 PC 调试
- `S`：PC 调试专用，切换“移动/停止”；停止 3 s 后自动暂停，恢复移动 1 s 后自动恢复

具体手势所有权和页面行为以 `src/hal/hal.c` 与 `src/ui/` 当前实现为准。

## UI Core 状态架构

- `ui_app_state` 是路由、骑行、状态栏、连接、控制、媒体和导航状态的统一真值来源。
- `ui_router` 负责页面历史与 `on_enter/on_leave/on_destroy` 生命周期。
- `ui_page_common` 将所有页面状态栏绑定到统一 App State。
- `hal.c` 只负责把 SDL 输入转换为 `UI_INPUT_*` 语义事件，页面代码不依赖 SDL。
- `ui_state_adapter`、`ui_navigation_service` 和 `ui_input_queue` 负责把 PC/STM32 生产者数据安全地送到 UI 线程。
- `ui_command_service` 负责把 UI 操作异步下发到 PC/STM32，并统一处理 Pending、Success、Failed、Timeout 和 Rejected。
- `ui_bottom_surface` 是 Dynamic Card 和 Interaction 内容唯一允许使用的底部视觉容器。
- `ui/design` 提供颜色、间距、圆角、边框、几何与动画语义 Token。

详细设计与后续接入规则见 [docs/UI_APP_STATE_AND_ROUTER_ARCHITECTURE.md](docs/UI_APP_STATE_AND_ROUTER_ARCHITECTURE.md)。
平台与 STM32 接入边界见 [docs/UI_SERVICE_ADAPTER_ARCHITECTURE.md](docs/UI_SERVICE_ADAPTER_ARCHITECTURE.md)。
命令闭环见 [docs/UI_COMMAND_ACTION_ARCHITECTURE.md](docs/UI_COMMAND_ACTION_ARCHITECTURE.md)。
底部唯一视觉容器约束见 [docs/BOTTOM_INTERACTION_SURFACE_ARCHITECTURE.md](docs/BOTTOM_INTERACTION_SURFACE_ARCHITECTURE.md)。
Phase 4 实现与迁移边界见 [docs/UI_DESIGN_SYSTEM_PHASE4.md](docs/UI_DESIGN_SYSTEM_PHASE4.md)。
Phase 5 Interaction Dock 统一迁移见 [docs/UI_INTERACTION_DOCK_PHASE5.md](docs/UI_INTERACTION_DOCK_PHASE5.md)。
Phase 5.1 原位切换修正见 [docs/UI_BOTTOM_SURFACE_IN_PLACE_FIX.md](docs/UI_BOTTOM_SURFACE_IN_PLACE_FIX.md)。
Phase 6 页面 Slot 与 Router 生命周期迁移见 [docs/UI_PAGE_MIGRATION_PHASE6.md](docs/UI_PAGE_MIGRATION_PHASE6.md)。
Phase 6A Home / Ride Hub 实现与验收见 [docs/PHASE_6A_HOME_RIDE_HUB_REPORT.md](docs/PHASE_6A_HOME_RIDE_HUB_REPORT.md)。
Phase 6B Quick Control Center 实现与验收见 [docs/PHASE_6B_CONTROL_CENTER_REPORT.md](docs/PHASE_6B_CONTROL_CENTER_REPORT.md)。
Phase 6C Navigation 视觉与交互统一见 [docs/PHASE_6C_NAVIGATION_REPORT.md](docs/PHASE_6C_NAVIGATION_REPORT.md)。
Phase 6D Settings / System / Power 实现见 [docs/PHASE_6D_SETTINGS_SYSTEM_POWER_REPORT.md](docs/PHASE_6D_SETTINGS_SYSTEM_POWER_REPORT.md)。
Phase 6E Ride Summary 实现见 [docs/PHASE_6E_RIDE_SUMMARY_REPORT.md](docs/PHASE_6E_RIDE_SUMMARY_REPORT.md)。
Phase 6F 骑行历史与 PC 持久化见 [docs/PHASE_6F_HISTORY_AND_STORAGE_REPORT.md](docs/PHASE_6F_HISTORY_AND_STORAGE_REPORT.md)。
Phase 7B 快捷控制条 V1 实现见 [docs/PHASE_7B_QUICK_CONTROL_BAR_REPORT.md](docs/PHASE_7B_QUICK_CONTROL_BAR_REPORT.md)。
Phase 7C 顶部/底部反馈联动见 [docs/PHASE_7C_CONTROL_FEEDBACK_HANDOFF_REPORT.md](docs/PHASE_7C_CONTROL_FEEDBACK_HANDOFF_REPORT.md)。
Phase 7D/7E 控制中心精简与胶囊动效见 [docs/PHASE_7D_7E_CONTROL_CENTER_MOTION_REPORT.md](docs/PHASE_7D_7E_CONTROL_CENTER_MOTION_REPORT.md)。
Phase 7G 统一 System Control Surface、三态 Morph 和 Inline Adjust 见 [docs/PHASE_7G_UNIFIED_CONTROL_SURFACE_REPORT.md](docs/PHASE_7G_UNIFIED_CONTROL_SURFACE_REPORT.md)。
Phase 7 控制中心最终化记录见 [docs/PHASE_7_UI_V1.1.2_FINALIZATION_REPORT.md](docs/PHASE_7_UI_V1.1.2_FINALIZATION_REPORT.md)。
`v1.2.0` 阶段的页面、交互、Mock 数据边界和验收证据见 [docs/PHASE_8_LVGL_SIMULATOR_COMPLETION_REPORT.md](docs/PHASE_8_LVGL_SIMULATOR_COMPLETION_REPORT.md)。
统一骑行报告的 v1.3.0 实现、数据边界与验收证据见 [docs/V1.3.0_RIDE_REPORT_IMPLEMENTATION.md](docs/V1.3.0_RIDE_REPORT_IMPLEMENTATION.md)。
v1.3.1 全局 UI、交互和稳定性验收见 [docs/v1.3.1报告.md](docs/v1.3.1报告.md)。
返回键骑行控制与自动暂停规则见 [docs/RIDE_BACK_KEY_AND_AUTO_PAUSE.md](docs/RIDE_BACK_KEY_AND_AUTO_PAUSE.md)。

## 版本发布约定

- 修复 Bug，不改变现有功能边界：提升 PATCH，例如 `v1.0.1`
- 增加兼容功能或完成一个较大阶段：提升 MINOR，例如 `v1.1.0`
- 重构整套 UI、交互或不兼容架构：提升 MAJOR，例如 `v2.0.0`
- 每次发布同步更新 `VERSION`、`CHANGELOG.md`，并创建同名 Git tag

## 当前边界

- 2026-10-02 完成最后一轮 UI 一致性与交互优化；审查结论、真实 LVGL 预览和待接入边界见 [最后一轮 UI 审查与优化](docs/最后一轮UI审查与优化_20261002.md)。本轮构建成功，32/32 回归通过，225 组整屏/分块渲染帧一致。
- Navigation 已升级为 V2：500 px 地图区、92 px 的 1×3 数据条和 460×188 固定 Navigation Command Dock；当前地图仍是 LVGL Mock 几何，不等同于真实地图引擎。
- PC 已完成骑行归档的版本化文件持久化、CRC32 校验、临时文件替换、备份恢复及 Home 聚合；RAM 保留最近 100 条，History 每页显示 10 条，旧记录由已注册的分页存储后端按需读取。
- STM32 工程尚未导入；历史存储抽象接口已经预留，但 FatFs/TF 卡后端、掉电测试和真机性能尚未实现或验证。
- Home / Ride Hub 使用配置姓名首字头像、静态品牌水印与真实已保存骑行摘要；摘要不展示固定假路线，缺失距离或爬升显示 `--`。
- Dashboard 使用七段按速度变色的平行四边形条、六格可配置数据和 MiSans 字体；当前支持巡航、训练、爬坡预设及字段替换，格数固定为六格。
- 个人页的 30 天活动按后端 UTC 和当前已载入记录计算，缺少日期时显示缺失状态；完整按当地自然日的归档聚合仍需后端提供。
- 设备页已有五类外设状态显示和本页反馈接口；蓝牙扫描、配对和恢复仍待接入。收藏路线是独立空态页，路线目录和导入尚未实现。
