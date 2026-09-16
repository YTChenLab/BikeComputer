# Phase 5 Interaction Dock 统一迁移

## 本阶段结论

Phase 5 已将 INFO、ACTION、CONFIRM、CONTROL、NAVIGATION、MEDIA 六类 Interaction Dock 内容迁入 `ui_bottom_surface.interaction_host`。旧 `ui_dock_view` 不再创建独立 `lv_layer_top()`、全屏透明对象或第二套底部卡片外框。

```text
lv_layer_top
  └─ ui_bottom_surface.root
       ├─ dynamic_host
       │    └─ Dynamic Card Viewport
       └─ interaction_host
            ├─ INFO content
            ├─ ACTION content
            ├─ CONFIRM content
            ├─ CONTROL content
            ├─ NAVIGATION content
            └─ MEDIA content
```

任意时刻只有 `dynamic_host` 或 `interaction_host` 可见。Interaction 结束后，由同一个 Surface 恢复此前的 Dynamic Card，不再叠加 Popup。

## 职责边界

- `ui_dock`：Interaction 业务状态、优先级队列、Pending 队列和恢复顺序的唯一真值。
- `ui_bottom_surface`：底部位置、尺寸、背景、外边框、圆角、动画和内容模式的唯一视觉真值。
- `ui_dock_view`：将 Dock 事件同步到 Surface，并管理六类内容模板和点击事件。
- `ui_dock_templates`：只绘制内部内容，不控制屏幕位置和外层 Surface 视觉。

`ui_dock_replace()` 对应 `ui_bottom_surface_replace_interaction()`，只原子替换当前 owner，不错误增加恢复栈。

## 尺寸策略

| Dock 类型 | Surface 尺寸 |
| --- | --- |
| INFO | Mini，156 px |
| ACTION | Mini，156 px |
| CONFIRM | Normal，226 px |
| CONTROL | Normal，226 px |
| NAVIGATION | Normal，226 px |
| MEDIA | Normal，226 px |

Expanded 继续由 Surface 保留，供后续复杂确认、列表或输入类交互使用。本阶段不为填满空间而滥用 Expanded。

## 优先级视觉

- P0 Critical：红色 2 px Surface 边框。
- P1 Riding Safety / P2 Ride State：黄色 2 px Surface 边框。
- P3 User Action / P4 Normal：标准 Surface 边框。

优先级视觉由 Surface 根据当前状态统一计算，内容模板不得覆盖外层边框。

## Command 反馈

设备设置类异步命令映射到同一 Interaction Dock：

- Pending：`PROCESSING` INFO，持续到最终结果。
- Success：`DONE` INFO，1.2 秒后恢复原 Dynamic Card。
- Failed / Timeout / Rejected：ACTION，显示 `FAILED`、`TIMEOUT` 或 `REJECTED`，可通过 `CLOSE` 关闭。

GNSS 定位、骑行结束和数据保存继续使用已有的领域专用 Dock 流程；音乐与高频骑行控制成功状态由原卡片或页面直接反馈，避免每次按键都抢占 Bottom Surface。失败状态仍可进入统一反馈。

命令反馈使用短英文状态词，是因为当前冻结的 MiSans 子集不包含所有新增中文字符。此策略避免修改已冻结字体并杜绝缺字方框。

## 验证覆盖

- 六类 Dock 模板均只存在于 `interaction_host`。
- Mini / Normal 尺寸映射正确。
- Show / Update / Replace / Override / Restore / Hide 与 Surface owner 同步。
- P0/P1/P2 优先级边框正确。
- Pending → Success → 自动恢复。
- Failed → CLOSE → Dynamic Card 恢复。
- Dashboard 不活跃时，高优先级 Interaction 仍可显示；结束后 Surface 正确隐藏。
- top layer 只保留一个 Bottom Surface 根对象。

## 后续边界

Phase 6 才开始逐页迁移页面到 Design Token 与统一 Router 生命周期。本阶段没有修改 P1 顶部状态栏、实时速度、黄色速度条、2×3 数据区、背景资源或 MiSans 字体文件。

## 当前验证结果

- Anaconda/MSYS2 UCRT64 全量编译通过。
- CTest 7/7 通过。
- Simulator 静态场景：平均 59 FPS，最低 58 FPS，LVGL 忙碌度平均 5%，峰值 11%。
- Simulator 100 ms 数据更新场景：平均 59 FPS，最低 59 FPS，LVGL 忙碌度平均 6%，峰值 13%。

CPU 数值来自 `100 - lv_timer_get_idle()`，代表 LVGL handler 忙碌度，不等同于 Windows 整机 CPU 使用率。
