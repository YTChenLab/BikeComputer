# BikeComputer

公路车码表软硬件与结构设计总仓库。

## 仓库结构

```text
BikeComputer/
├─ Software/
│  ├─ Lvgl_Project/       LVGL PC Simulator 与 UI 源码
│  └─ STM32_Project/      STM32 固件工程（预留）
├─ Hardware/              原理图、PCB 与硬件资料（预留）
└─ 3D_Structure/          外壳和结构件模型（预留）
```

## 当前版本

- BikeComputer LVGL UI 工程版本：`v1.3.0`
- 内置 LVGL 源码版本：`9.3.0-dev`
- PC 构建环境：Anaconda/MSYS2 UCRT64 + GCC + Ninja + CMake

工程版本与 LVGL 库版本是两个不同概念。仓库标签用于标识 BikeComputer 产品工程版本。

## 版本规则

- 小型修复：`1.0.0 -> 1.0.1 -> 1.0.2`
- 向后兼容的较大功能更新：`1.0.x -> 1.1.0 -> 1.2.0 -> 1.3.0`
- 整套 UI 架构或视觉系统升级：`1.x -> 2.0.0`
- Git 标签统一使用 `vMAJOR.MINOR.PATCH`，例如 `v1.0.0`

LVGL UI 的构建、运行、输入映射和目录说明见 [Software/Lvgl_Project/README.md](Software/Lvgl_Project/README.md)。
