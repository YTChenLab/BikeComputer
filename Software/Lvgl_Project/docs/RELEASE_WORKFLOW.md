# 版本开发与发布流程

当前 `v1.x` 标签只归档 LVGL UI 软件。`main` 是完成构建和回归验证的集成分支；功能在 `feature/<主题>` 开发，历史版本由标签定位。长期保留一个旧版本分支不会自动更新 `main`。

## 日常开发

1. 从最新 `main` 建立功能分支，按完整功能提交源码、资源、测试和文档。
2. 仅暂存本次明确范围；不要在总仓库无检查地执行 `git add .`，以免纳入硬件资料、安装包、构建目录或运行存档。
3. 推送功能分支后建立面向 `main` 的 PR，检查改动和 CI；多人项目由其他维护者审核，单人项目仍应保留可检查的差异与验证证据。
4. 测试通过后集成到 `main`。旧版本同步优先保留已有提交历史，不重写旧标签。远端若已有新提交，先重新检查差异，不使用强推覆盖。

## 地图与本机环境

`.btiles`、`.bmap`、`.bgraph`、`.bpoi` 由仓库 `.gitattributes` 指定 Git LFS 管理；小型路线、元数据、来源和许可按普通文本管理。新增地图时同步 SHA-256 清单，关注 LFS 存储和下载配额。

```sh
git lfs install --local
git lfs pull
```

CI checkout 必须开启 `lfs: true`。本机环境可继续使用既有 `bike-ui-ucrt64` preset；其他机器使用 `bike-ui` preset 和自己的现有工具链。新装本机环境须先确定安装路径。

字体生成依赖 Pillow。Windows runner 同时存在预装 CPython 和 MSYS2 Python；依赖安装在其中一个解释器不代表其他解释器也可导入。CI 将 `Python3_EXECUTABLE` 明确指定为 `cygpath -m /ucrt64/bin/python.exe` 的结果，并用同一 Python 执行版本检查、字体生成、Python 测试与打包。首次配置前输出 Python 路径和 Pillow 版本；本机也可通过 `cmake --preset bike-ui -DPython3_EXECUTABLE=<已有解释器绝对路径>` 选择包含 Pillow 的 Python。

若构建、测试或打包失败，Actions 会附加 `ci-diagnostics-<运行ID>-<重试次数>`，包含配置、编译、CTest、打包日志及 CMakeCache，保留 7 天。这些日志管道使用 `pipefail`，不会因为 tee 成功而掩盖真正的失败。日志不包含运行时 Key 配置。

## 发布

1. 将 `VERSION`、`UI_SOFTWARE_VERSION`、README 和 CHANGELOG 同步到同一版本，写明验证结果与已知限制。
2. 使用干净构建目录执行全部测试，核对资源、提交范围及 `git diff --check`。
3. 集成发布提交到 `main`，再在该提交创建注释标签，例如 `git tag -a v1.5.0 -m "BikeComputer LVGL UI v1.5.0"`。
4. 推送已集成的主分支和标签。`lvgl-ui.yml` 对主分支、功能分支和 PR 执行 CI；`v*` 标签触发独立构建、测试及版本核对，成功后生成 Windows PC ZIP、SHA-256 和 GitHub Release。
5. 确认远端主分支与标签指向预期提交，并检查 Actions、Release 和资源下载。若工作流失败，保留失败证据、修复原因后重跑；已发布的旧标签不移动。

推荐提交记录将功能归档与发布配置分开。发布日志须区分 PC 软件验证、真实数据来源、模拟位置和整机实机验证。不要仅凭版本号判断成熟度。

## 恢复已有标签的失败发布

标签运行采用该标签提交中的工作流。修复主分支后，直接点击旧运行的 Re-run jobs 仍使用旧工作流，不能应用新修复。

1. 在 Actions 打开「LVGL UI 构建与发布」，点击 Run workflow，选择工作流分支 `main`。
2. 在 `release_tag` 填写已有标签，例如 `v1.5.1`；留空只执行所选分支的 CI，不创建 Release。
3. 工作流从完整 `refs/tags/<标签>` 检出源码并校验 VERSION，完成地图校验、构建、测试和打包后创建该标签的 Release。原标签不移动。
4. 本入口用于尚未成功发布的标签。若该标签已有 Release，应先核对它的状态与附件；现有发布不会被此入口覆盖。

校验清单必须使用 LF。生成脚本与 Git 属性共同固定换行；CI 在读取历史清单时只去掉每行末尾回车，仍验证所有资源的 SHA-256，错误哈希或缺失资源仍会失败。

## 2026-10-05 CI 修复验证

只使用 Git 提交 `639ba87` 中的文件，并加入本次 CMake 依赖检查，不带原工作区的构建缓存、运行记录或未跟踪文件。四个 LFS 地图对象按提交中的指针验证大小与 SHA-256 后使用。

显式指定现有 UCRT64 Python 3.14.7 / Pillow 12.3.0，资源七项校验、干净完整构建、44/44 CTest、Windows 打包、ZIP CRC、空 Key 默认离线配置与脱离工具链 PATH 的启动检查均通过。CTest 按 CI 的顺序执行，用时 226.06 秒；构建无警告或错误。工作流 YAML、六段 shell 语法及失败日志管道检查通过。

本地验证不替代 GitHub runner 的最终结果。主分支推送后查看新的 CI；恢复尚未发布的 v1.5.1 时，从最新 main 手动运行并填写 release_tag=v1.5.1。旧运行使用旧工作流，旧标签仍定位原应用发布提交。
