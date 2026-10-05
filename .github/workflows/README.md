# 自动构建与发布

[lvgl-ui.yml](lvgl-ui.yml) 为 LVGL PC 工程执行版本核对、资源校验、构建、回归测试及 Windows 打包发布。

| 触发方式 | 执行内容 |
| --- | --- |
| 推送 `main`、`feature/**`、`release/**` | 构建与回归测试 |
| 面向 `main` 的 Pull Request | 构建与回归测试 |
| 推送 `v*` 标签 | 检查版本，成功后打包并创建 GitHub Release |
| 手动运行，`release_tag` 留空 | 仅执行所选分支的 CI |
| 手动运行，填写已有 `v*` 标签 | 用当前工作流构建原标签源码，成功后创建尚未发布的 Release |

CI 明确使用 UCRT64 Python 与其 Pillow 依赖，并兼容历史校验清单在 Windows 检出后的 CRLF。失败时上传配置、构建、测试与打包诊断文件，保留 7 天。

修复工作流后，从最新 `main` 新建手动运行；重跑旧记录仍沿用原提交中的工作流。成功发布需确认构建、发布任务与 Release 附件；主分支 CI 的绿色标记只说明相应检查通过。

详细操作见 [版本发布流程](../../Software/Lvgl_Project/docs/RELEASE_WORKFLOW.md)。
