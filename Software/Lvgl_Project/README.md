# BikeComputer LVGL UI

BikeComputer 公路车码表的 LVGL UI 工程和 Windows PC Simulator。

## 版本信息

- BikeComputer UI 工程版本：`v1.0.0`
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
- 鼠标右键 / `Esc`：返回
- 鼠标滚轮 / 方向键：模拟 Encoder 旋转
- `Enter`：模拟 Encoder 按下

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

## 版本发布约定

- 修复 Bug，不改变现有功能边界：提升 PATCH，例如 `v1.0.1`
- 增加兼容功能或完成一个较大阶段：提升 MINOR，例如 `v1.1.0`
- 重构整套 UI、交互或不兼容架构：提升 MAJOR，例如 `v2.0.0`
- 每次发布同步更新 `VERSION`、`CHANGELOG.md`，并创建同名 Git tag

## 当前边界

- Navigation 当前主要是 UI 与离线地图架构原型，不等同于完整真实地图引擎。
- STM32 工程尚未导入，PC 服务接口已经为后续硬件适配预留。
- 当前背景、MiSans 字体和既有页面视觉均按 v1.0.0 快照保留。
