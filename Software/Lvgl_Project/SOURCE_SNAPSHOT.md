# v1.0.0 源码快照来源

本文件记录首次迁入 BikeComputer 总仓库时的源码来源，便于后续审计和升级 LVGL。

## 来源

- 导入时间：2026-09-15（Asia/Shanghai）
- 原工程路径：`D:/Postgraduate_Program/111AAALearn_Lab/111AAABike_SpeedView/Software/Lvgl_UI_v1.0.0/lv_port_pc_vscode`
- 原工程分支：`master`
- 原工程 HEAD：`2ab7eefcc2de348723d2b13c658f39dd4972134f`
- LVGL 工作目录实际 HEAD：`2cd6cc1efc56dfdda9372ef7a5016f3f7de0d86c`
- 原工程记录的 LVGL gitlink：`f45e3a3e87f517ee134d52d01ad7c8a887efd199`

## 快照策略

导入时原工程和 LVGL 子模块均包含尚未形成独立提交的本地修改，而且 LVGL 工作目录 HEAD 与原工程记录的 gitlink 不一致。若只迁移 submodule 引用，GitHub 上的 v1.0.0 将无法还原当前可运行 UI。

因此本版本采用以下策略：

- 把当前 `lvgl/` 工作目录作为普通源码目录纳入仓库。
- 不迁移旧工程和 LVGL 内部的 `.git` 元数据。
- 不迁移 `build/`、`bin/`、`.tools/` 等本地生成目录。
- 保留当前 CMake、UI、字体、背景、图标、测试及资源工具。
- 后续升级 LVGL 时必须记录上游版本、迁移差异和回归测试结果。

