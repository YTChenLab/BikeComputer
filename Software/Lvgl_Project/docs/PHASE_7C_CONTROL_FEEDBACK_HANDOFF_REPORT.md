# Phase 7C 顶部与 Bottom Surface 联动报告

## 阶段目标

本阶段建立系统控制帘与 Bottom Interaction Surface 之间的单一反馈所有权，解决以下问题：

- 异步控制任务关闭顶部后丢失反馈；
- 同一任务同时显示顶部 Pending 与底部 Pending；
- 高优先级事件出现时控制中心仍挡住安全信息；
- Pending 卡进入 restore stack 后无法在重新打开顶部时移除；
- 关机重启后恢复过期命令反馈。

## 单一反馈所有权

新增 `ui_control_feedback_ownership`，当前管理：

```text
WiFi
Bluetooth
GPS
Power Save
Touch Lock
Sound
Brightness
```

其中声音/亮度有两种 owner 表达：完整控制中心直接拖动时由 TOP 原位反馈；Bottom CONTROL 精调时由 CONTROL 卡自身持有。退出 CONTROL 但命令仍 Pending 时，才转为通用 Bottom Pending，禁止同一 Bottom Surface 同时出现 CONTROL 与第二张命令卡。

状态：

```text
NONE
TOP
BOTTOM
```

规则：

```text
系统控制帘打开 + 命令 Pending
→ owner = TOP
→ 只在快捷控制条或完整控制中心原位反馈

系统控制帘关闭 + 命令仍 Pending
→ TOP → BOTTOM
→ Bottom Surface 显示同一任务 Pending

任务下沉期间重新打开快捷控制条
→ BOTTOM → TOP
→ 从 Dock active / pending / restore 中移除命令反馈
→ 顶部继续显示同一 Pending
```

同一时刻只有一个 owner，不创建第二个 Bottom Surface，也不创建独立 Popup。

## 最终结果路由

Success、Failed、Timeout、Rejected 只发送给任务的当前 owner：

- TOP：更新顶部状态，不发布 Bottom Command 卡；
- BOTTOM：发布 Bottom Command 结果，顶部强制回到 Idle；
- Backend 直接 Rejected、未产生 Pending：根据结果到达时控制帘是否可见选择 owner。

Quick Bar 的 Failed 项再次执行时先清除旧失败状态，再提交新命令。

## P0/P1 抢占

应用现在先于 Dock Controller 订阅 Event Bus。收到：

```text
P0 Critical
P1 Riding Safety
```

执行顺序固定为：

```text
退出 Bottom CONTROL
→ 收起系统控制帘
→ 若存在 Pending 控制任务则下沉
→ Dock Controller 映射 P0/P1
→ P0/P1 抢占 Bottom Surface
```

P0/P1 活跃时，键盘、Encoder 或 Touch 都不能重新打开系统控制帘。安全事件结束后，Dock 按已有 restore stack 恢复此前 Pending 或 Dynamic Card。

## Restore Stack 清理

为 `ui_dock_controller` 增加受控接口：

```c
ui_dock_controller_hide_command_feedback()
```

它只移除固定的 `DOCK_ID_COMMAND`，可处理：

- 当前 active 命令卡；
- 被 P0/P1 压入 restore stack 的命令卡；
- 尚未展示的 pending 命令卡；
- 对应自动关闭 timer。

未暴露 Bottom Surface 容器控制权，也没有建立第二个恢复栈。

## 电源域边界

Power Off、Restart 和 PC `P` 电源切换均同步清空：

- Dock；
- Event Bus 队列；
- Control Feedback Ownership。

因此命令晚到结果不会在重新开机后恢复旧卡。

## 测试

新增：

```text
ui_control_feedback_handoff_test
```

覆盖：

- Managed/Unmanaged 命令分类；
- TOP → BOTTOM 下沉；
- BOTTOM → TOP 回收；
- Bottom Pending 展示；
- P1 抢占并把 Pending 压入 restore stack；
- 从 restore stack 移除过期命令反馈；
- Bottom Success 更新与自动关闭；
- ownership 完成清理。

当前实测：

```text
Build: PASS
CTest: 23/23 PASS
Font Coverage: PASS
git diff --check: PASS
```

## 未完成边界

以下仍属于后续阶段：

- Phase 7D：完整控制中心 400～470 px 紧凑布局；
- Phase 7D：声音与亮度横向细滑条；
- Phase 7E：Toggle、Fill、Thumb 和 Surface Morph 完整动效；
- Phase 7F：异常注入、完整人工流程和 30～60 分钟压力测试。

本阶段没有修改 Dashboard 冻结区，也没有更新 `VERSION` 或创建 `v1.1.2` tag。
