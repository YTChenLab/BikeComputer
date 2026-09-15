# 更新记录

本项目遵循语义化版本编号，Git 标签格式为 `vMAJOR.MINOR.PATCH`。

## 未发布（目标 v1.1.0）

- 新增 `ui_app_state`，统一管理页面路由、骑行状态、状态栏、连接、控制中心、媒体与导航状态。
- 为 `ui_router` 增加页面创建、进入、离开和销毁生命周期，并暂停隐藏页面的刷新定时器。
- 建立全局状态栏绑定注册表，消除各页面各自保存时间、电量和连接状态的问题。
- 将 PC 鼠标、滚轮和键盘输入转换为平台无关的 UI 语义事件，为 STM32 输入适配预留统一接口。
- Dynamic Card 音乐状态改由全局状态驱动，触摸和 Encoder 共用同一份播放状态。
- 新增 App State 与 Router Lifecycle 自动化测试。

## v1.0.0 - 2026-09-15

- 建立 BikeComputer GitHub 总仓库目录结构。
- 导入当前 LVGL PC Simulator 与 BikeComputer UI 完整源码快照。
- 将当前有本地修改的 LVGL `9.3.0-dev` 源码纳入仓库，保证当前 UI 可复现。
- 固化 Anaconda/MSYS2 UCRT64 的 CMake preset、VS Code 工作区和测试入口。
- 纳入当前 UI、字体、背景、图标、导航原型、Core Layer、Dock 和测试代码。
