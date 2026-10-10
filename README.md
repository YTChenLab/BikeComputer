# BikeComputer

公路车码表的软件、硬件与结构设计仓库。当前已归档的主要软件为 **LVGL UI v1.5.2**，包含 PC／STM32 平台适配及接入说明，可在 Windows PC 模拟器中构建和运行。

## 目录导航

| 路径 | 用途 | 说明入口 |
| --- | --- | --- |
| `Software/Lvgl_Project/` | LVGL UI、导航资源、PC 模拟器和回归测试 | [工程说明](Software/Lvgl_Project/README.md) |
| `Software/STM32_Project/` | STM32 固件目录，目前 Git 中保留预留说明 | [固件说明](Software/STM32_Project/README.md) |
| `Hardware/` | 原理图、PCB、BOM 与制造资料的归档入口 | [硬件说明](Hardware/README.md) |
| `3D_Structure/` | 外壳、固定座与其他结构件的归档入口 | [结构说明](3D_Structure/README.md) |
| `.github/workflows/` | 自动构建、测试与 Windows 软件发布 | [自动化说明](.github/workflows/README.md) |
| `.gitattributes` | 文本换行、二进制文件与地图 Git LFS 规则 | [资源与发布流程](Software/Lvgl_Project/docs/RELEASE_WORKFLOW.md) |
| `.gitignore` | 构建产物与本机状态的忽略规则 | [协作与提交规范](CONTRIBUTING.md) |

软件目录总览见 [Software/README.md](Software/README.md)。硬件和结构目录当前在 Git 中仅有说明文件，设计文件的归档状态以各目录实际提交为准。

## 版本与状态

| 项目 | 当前信息 |
| --- | --- |
| BikeComputer LVGL UI | `v1.5.2` |
| 内置 LVGL 源码 | `9.3.0-dev` |
| PC 工具链 | MSYS2 UCRT64、GCC、Ninja、CMake、包含 Pillow 的 Python |
| STM32、硬件和结构 | 分别维护归档与验收状态 |

当前 `v1.x` 标签归档 LVGL UI 软件，不代表固件、硬件和整机完成同版本验收。软件版本以 `Software/Lvgl_Project/VERSION` 和对应 Git 标签为准；下载包的发布状态以 GitHub Releases 为准。

## 常用入口

- [构建、运行与输入映射](Software/Lvgl_Project/README.md)
- [软件更新记录](Software/Lvgl_Project/CHANGELOG.md)
- [v1.5.2 更新内容与验证边界](Software/Lvgl_Project/docs/V1.5.2_RELEASE_NOTES.md)
- [STM32 v1.5.2 接口适配与接入说明](Software/Lvgl_Project/docs/STM32_V1.5.2_INTEGRATION.md)
- [离线导航搜索覆盖与限制](Software/Lvgl_Project/docs/导航搜索准确性_20261005.md)
- [分支、Git LFS 与版本发布流程](Software/Lvgl_Project/docs/RELEASE_WORKFLOW.md)
- [协作与提交命名规范](CONTRIBUTING.md)

## 版本规则

- 修复缺陷：递增补丁版本，例如 `1.5.1 → 1.5.2`。
- 增加兼容功能：递增次版本，例如 `1.5.x → 1.6.0`。
- 引入不兼容接口或数据格式：递增主版本；视觉调整本身不必升级主版本。
- 发布标签使用 `vMAJOR.MINOR.PATCH`；文档整理和 CI 修复可单独提交，不自动变更应用版本。

GitHub 文件列表旁的文字是最近修改该路径的提交摘要，表示修改历史。长期目录用途见各目录 README；当前软件版本见上方版本表。
