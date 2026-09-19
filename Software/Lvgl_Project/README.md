# BikeComputer LVGL UI

BikeComputer 公路车码表的 LVGL UI 工程和 Windows PC Simulator。

## 版本信息

- BikeComputer UI 工程版本：`v1.1.1`
- LVGL 源码版本：`9.3.0-dev`
- 目标分辨率：`480 × 854`
- 色彩格式：RGB565
- 当前 PC 工具链：Anaconda/MSYS2 UCRT64

本目录中的 `lvgl/` 是当前可运行版本对应的源码快照。它包含项目现阶段依赖的本地修改，因此没有继续作为上游 Git submodule 保存。后续升级底层 LVGL 时，应单独记录上游 commit、迁移差异和回归测试结果。

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
$env:PATH = "D:\Professional Software\Anaconda\Bike_SpeedView\msys2-x86_64\ucrt64\bin;D:\Professional Software\Anaconda\Bike_SpeedView\msys2-x86_64\usr\bin;$env:PATH"
.\bin\main.exe
```

`main.exe` 依赖 UCRT64 的 SDL2 与运行时 DLL。若不先注入上述 `PATH`，Windows 会在创建窗口前直接退出。使用 `simulator.code-workspace` 启动时已自动配置该环境。

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
返回键骑行控制与自动暂停规则见 [docs/RIDE_BACK_KEY_AND_AUTO_PAUSE.md](docs/RIDE_BACK_KEY_AND_AUTO_PAUSE.md)。

## 版本发布约定

- 修复 Bug，不改变现有功能边界：提升 PATCH，例如 `v1.0.1`
- 增加兼容功能或完成一个较大阶段：提升 MINOR，例如 `v1.1.0`
- 重构整套 UI、交互或不兼容架构：提升 MAJOR，例如 `v2.0.0`
- 每次发布同步更新 `VERSION`、`CHANGELOG.md`，并创建同名 Git tag

## 当前边界

- Navigation 已升级为 V2：500 px 地图区、92 px 的 1×3 数据条和 460×188 固定 Navigation Command Dock；当前地图仍是 LVGL Mock 几何，不等同于真实地图引擎。
- PC 已完成最近 100 条骑行记录的版本化文件持久化、CRC32 校验、临时文件替换、备份恢复、Home 聚合和 History 列表；History 当前展示最近 50 条。
- STM32 工程尚未导入；历史存储抽象接口已经预留，但 FatFs/TF 卡后端、掉电测试和真机性能尚未实现或验证。
- Home / Ride Hub 已按 UI V4 完成第一版真实 LVGL 页面；头像、山景与最近路线仍是可替换的本地 Mock 图形资源。
- Dashboard 的顶部状态栏、实时速度、黄色速度条、2×3 数据区、背景资源和 MiSans 字体保持冻结。
