# UI Command / Action 架构

## 目标

Phase 3 建立从 UI 到 PC/STM32 执行层的统一异步命令通道。UI 点击只代表“提交请求”，不能直接修改设备状态；只有平台执行成功并通过 `ui_state_adapter` 回传后，App State 才发生变化。

```text
Touch / Encoder / Interaction content
                 |
       ui_command_service_submit
                 |
       bounded command queue
                 |
          PC or STM32 backend
                 |
      +----------+-----------+
      |                      |
command completion       state snapshot
      |                      |
ui_command_service      ui_state_adapter
      |                      |
      +----------+-----------+
                 |
             UI thread
```

## 命令生命周期

```text
QUEUED -> PENDING -> SUCCESS
                  -> FAILED
                  -> TIMEOUT

QUEUED -> REJECTED
```

`ui_command_service` 同一时间只向 backend 派发一条命令，避免多个外设命令乱序完成。命令完成回调、超时回调和状态快照最终都在 UI Tick 内消费。

## 队列规则

- WiFi、蓝牙、GPS、省电模式、声音、亮度及绝对媒体设置属于 latest-value 命令；尚未派发的同类命令会合并为最后一个目标值。
- 上一首、播放/暂停、下一首和播放模式循环属于相对命令，必须逐条执行，不能合并。
- 骑行开始、暂停、恢复、结束、保存、上传以及 GNSS 定位请求会去重，防止按钮连击重复推进状态机。
- 队列有固定容量，不进行运行时内存分配。

## PC Simulator

`ui_pc_services` 提供异步 Mock backend：

- 普通命令默认延迟 80 ms。
- GNSS 定位沿用 900 ms 模拟延迟。
- 骑行保存沿用 700 ms 模拟延迟。
- 成功后先提交权威状态快照，再提交命令完成结果。
- 测试可配置下一次命令、定位或保存失败。

## STM32 接入

`ui_stm32_service_port_t.dispatch_command` 接收 UI 命令。驱动任务完成后调用：

```c
ui_stm32_services_complete_command(
    command_id,
    UI_COMMAND_STATUS_SUCCESS,
    0,
    &state_snapshot,
    changed_fields);
```

如果命令不改变 App State，例如保存或骑行控制，`state_snapshot` 可以为 `NULL`，`changed_fields` 为 0。该 API 只能由任务上下文调用，ISR 必须先通知任务。

## UI 约束

- 页面和组件不得直接调用 `ui_app_state_set_connectivity()`、`ui_app_state_set_gnss()`、`ui_app_state_set_controls()` 或 `ui_app_state_set_media()` 模拟硬件成功。
- 命令失败结果当前保留在日志和观察者接口中；在 `ui_bottom_surface` 完成前，不新增独立失败 Popup，避免继续扩大双底部容器问题。
- Phase 4/5 将把 Pending、Success、Failed 和 Timeout 映射为统一 Bottom Interaction Surface 内容状态。

## 兼容边界

`ui_system_services` 暂时保留，供旧 GNSS/存储接口和回归测试使用。新 UI 入口已经迁移到 `ui_command_service`，后续 STM32 工程接入稳定后再删除旧接口。
