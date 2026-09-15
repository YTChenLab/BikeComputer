# Bottom Interaction Surface 最高优先级约束

## 唯一视觉容器

`Dynamic Card Dock` 与 `Interaction Dock` 是两个业务逻辑模块，但最终不得拥有两个互相独立或互相叠加的底部视觉容器。二者必须共享唯一的 `ui_bottom_surface`。

```text
Bottom Interaction Surface
    |
    +-- Normal Content Mode
    |      Dynamic Card content
    |
    +-- Event Interaction Mode
           INFO / ACTION / CONFIRM / CONTROL
           NAVIGATION / MEDIA content
```

核心原则：

- 内容变，不换容器。
- 状态变，不叠弹窗。
- 高优先级事件暂时替换原内容。
- 事件结束后恢复原 Dynamic Card、滚动位置和控制焦点。
- 空间不足时由同一个 Surface 从 Normal 平滑切换到 Expanded。

## Surface 状态

尺寸状态：

- `MINI`
- `NORMAL`
- `EXPANDED`

内容模式：

- `DYNAMIC_CARD_MODE`
- `INTERACTION_MODE`

`ui_bottom_surface` 统一拥有并管理：

- 屏幕位置与宽度
- Mini / Normal / Expanded 高度
- 背景、边框、圆角和 Padding
- 入场、退出与展开动画
- 内容模式切换
- Priority Override
- Stack Restore
- Touch / Encoder Focus 路由

`ui_dynamic_card` 与 `ui_interaction_dock` 只能创建内容，不得设置底层位置、外框、背景或独立入场动画。

## 抢占与恢复

进入 Event Interaction Mode 前至少保存：

- 当前 Dynamic Card ID
- Viewport Snap 位置
- Browser / Control 模式
- Neutral Focus 或内部控件焦点
- Surface 尺寸状态

事件结束、超时或被撤销后，必须恢复同一份状态，不能默认返回 Card 0。

## 当前工程事实与迁移边界

Phase 4 已创建 `ui_bottom_surface`，并将 Dynamic Card 迁入 `dynamic_host`。四张 Card 不再分别控制外层背景、边框、圆角和屏幕位置。

旧 `ui_dock_view` 仍暂时使用独立 top-layer 根对象，这是 Phase 5 必须删除的迁移债务。Phase 5 将 INFO/ACTION/CONFIRM/CONTROL/NAVIGATION/MEDIA 内容迁入 `interaction_host`。迁移完成前不得把旧叠加方式扩展到新功能。
