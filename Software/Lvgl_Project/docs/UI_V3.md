# Bike Computer Home / Ride Hub — Codex Handover Specification V1.0

## 0. 文档定位

本文档用于将公路车智能码表 UI 项目的下一阶段开发工作正式交接给 Codex。

当前阶段不再进行新的底层架构扩张，重点进入：

> **Phase 6A：Home / Ride Hub 产品入口落地 + 骑行前半流程闭环**

Codex 必须在完整理解当前工程架构与既有 UI/UX 总规范后执行本任务。

本文档与以下总规范同时生效：

1. `Bike_Computer_UI_UX_Master_Design_V1.0.md`
2. `Bike_Computer_UI_Codex_Handover_V1.0.md`

若三份文档存在冲突，本文件仅覆盖 Home / Ride Hub 当前阶段的具体实现要求；整体 UX 总原则仍以 Master Design 为准。

---

# 1. 当前工程基线

当前 PC Simulator 已经具备：

- LVGL `9.3.0-dev`
- 480×854、RGB565 显示基准
- 全局统一背景
- 统一状态栏
- Ride Dashboard 基础视觉
- Dynamic Card Dock
- Router / 页面生命周期
- Gesture Ownership
- `ui_app_state`
- 异步 Command / Action 系统
- Bottom Interaction Surface
- Priority Override / Restore Stack
- Design Token 第一版
- PC Mock
- 自动化测试
- 约 59 FPS 稳定模拟表现

当前架构阶段已经基本完成。

本阶段不要继续增加新的“大架构”。

---

# 2. 当前页面与身份调整

当前页面包括：

- Boot
- Dashboard
- Navigation
- Ride Summary
- Profile
- Settings
- System
- Power
- Power Off

其中：

## Dashboard

后续正式定义为：

> **Ride Dashboard**

它不再是开机默认首页，而是“骑行准备 + 正式骑行”的任务页。

## Profile

本阶段建议直接重构为：

> **Home / Ride Hub**

不要同时保留一个独立 Profile 页再新增 Home，避免页面重复。

---

# 3. 本阶段核心目标

本阶段不是只画一个静态首页。

必须打通：

```text
BOOT
  ↓
HOME / RIDE HUB
  ↓
START RIDE
  ↓
RIDE DASHBOARD
  ↓
GPS SEARCHING
  ↓
GPS READY
  ↓
GO
```

到 GO 之前均属于骑行准备阶段。

只有点击 GO 并收到成功结果后，才正式进入 `RIDING`。

---

# 4. Home / Ride Hub 产品定位

Home 不是传统手机式“我的”。

它是整台码表的：

> **Ride Hub / 骑行主页**

负责：

- 用户头像 / 用户身份
- 累计骑行里程
- 累计骑行时间
- 累计爬升
- 最近一次骑行
- 开始骑行
- 开始导航
- 历史记录入口
- 成就入口
- 系统设置入口

用户开机后应立刻知道：

1. 我是谁
2. 我累计骑了多少
3. 最近一次骑行是什么
4. 现在可以做什么

---

# 5. Home 页面结构

建议分为 5 个区块。

## H01 — Status Bar

继续复用统一状态栏：

- GNSS / Satellite
- 时间
- WiFi
- Bluetooth
- Battery %

禁止 Home 自己重写状态栏。

## H02 — User Identity

显示：

- 头像
- 用户名称 / ID
- 简短状态文本（可选）

第一版不做复杂账号体系。

## H03 — Lifetime Ride Summary

建议显示：

- 累计骑行里程
- 累计骑行时间
- 累计爬升

其中累计骑行里程为主视觉焦点。

示例：

```text
TOTAL DISTANCE

2,864 km

TIME        ASCENT
128 h       18,430 m
```

## H04 — Primary Actions

必须包含：

### 主按钮

```text
开始骑行
```

这是整页最高优先级操作。

### 次按钮

```text
开始导航
```

第一版进入现有 Navigation 页面。

## H05 — Recent / Utilities

包含：

### 最近一次骑行

显示：

- 日期
- 距离
- 时间
- 爬升

### 功能入口

- 历史记录
- 我的成就
- 系统设置

---

# 6. Home 视觉层级

建议：

```text
第一优先级
开始骑行

第二优先级
累计里程
开始导航

第三优先级
最近一次骑行

第四优先级
历史 / 成就 / 设置
```

禁止 WiFi、蓝牙、亮度、省电等系统开关抢占 Home 主视觉。

---

# 7. Home 不负责的内容

以下内容不属于 Home：

- 亮度
- 音量
- WiFi 开关
- Bluetooth 开关
- GPS 开关
- Power Mode
- 骑行暂停
- 骑行恢复
- 骑行结束
- 导航转向事件

它们分别属于：

- Quick Control Center
- Bottom Interaction Surface
- Ride Dashboard
- Navigation

---

# 8. Quick Control Center 边界

顶部 Quick Control Center 保留。

其职责：

> 用户主动修改全局系统状态。

V1.0 推荐内容：

- WiFi
- Bluetooth
- Brightness
- Sound
- Touch Lock
- Power Mode
- GNSS Status
- Battery Status

---

# 9. Control Center 必须删除的骑行操作

当前 Control Center 中的：

- 开始骑行
- 暂停 / 恢复骑行
- 结束骑行

本阶段必须删除。

最终职责：

```text
开始骑行
→ Home

GO
→ Bottom Interaction Surface

暂停 / 恢复
→ Ride State + Bottom Interaction Surface

结束骑行
→ 长按实体键 + Expanded Confirm Surface
```

---

# 10. Bottom Interaction Surface 约束

当前架构：

```text
Bottom Interaction Surface
├─ Dynamic Card Mode
└─ Interaction Mode
```

必须保持。

禁止重新拆成两个互相叠加的底部视觉组件。

核心规则：

> 内容变，不换容器。
> 状态变，不叠弹窗。
> 空间不足时，由同一个 Surface 自身展开。

---

# 11. Dynamic Card 与 Interaction 的职责

## Dynamic Card Mode

常驻功能卡：

- Extended Ride Data
- Navigation
- Music
- Weather

## Interaction Mode

事件交互：

- INFO
- ACTION
- CONFIRM
- CONTROL
- NAVIGATION
- MEDIA

---

# 12. Home 与 Bottom Surface

Home 页面继续预留 Bottom Interaction Surface Slot。

默认：

```text
Surface = Hidden
```

如发生全局系统事件，可临时显示：

- 低电量
- 系统错误
- 连接失败
- 更新提示
- 设备异常

Home 不常驻显示四张骑行 Dynamic Card。

---

# 13. Start Ride 流程

用户点击：

```text
开始骑行
```

必须执行：

```text
1. Submit Start Prepare Action
2. ui_state_machine → PRE_RIDE
3. Router → Ride Dashboard
4. Ride Dashboard on_enter()
5. Bottom Surface → Interaction Mode
6. 显示 GPS Searching
```

注意：

> 点击“开始骑行”不等于正式开始计时。

---

# 14. PRE_RIDE 状态

进入 Ride Dashboard 后：

```text
STATE = PRE_RIDE
```

此时：

- 骑行计时未正式启动
- Ride Session 尚未进入正式记录
- Bottom Surface 显示 GPS Searching

建议：

```text
GPS
正在定位…
SAT 08
```

类型：

```text
INFO
Priority = P2 Ride State
```

---

# 15. GPS Ready

GNSS 返回 Ready 后：

Bottom Surface 原位更新：

```text
GPS READY

定位完成

GO
```

类型：

```text
ACTION
Priority = P2
```

禁止：

- 新建独立小 Popup
- 叠加 GO 弹窗
- 跳到新页面

必须使用同一个 Bottom Interaction Surface。

---

# 16. GO

用户点击 GO 后：

真正执行：

- Start Ride Session
- Start Timer
- Start Track Recording
- Start Distance Accumulation
- Start AVG / MAX Statistics
- Start Ride Data Logging

状态：

```text
PRE_RIDE
→ GPS_READY
→ RIDING
```

命令必须经过 `ui_command_service`。

UI 不得直接假定成功。

---

# 17. GO Command 状态反馈

必须支持：

```text
QUEUED
PENDING
SUCCESS
FAILED
TIMEOUT
REJECTED
```

## Pending

Bottom Surface：

```text
正在开始骑行…
```

## Success

短暂显示：

```text
骑行开始
```

随后恢复：

```text
Dynamic Card Mode
```

## Failed / Timeout

显示：

```text
无法开始骑行

[重试]
```

不得直接进入 RIDING。

---

# 18. Ride Dashboard 正式职责

GO 成功后：

```text
RIDING
```

此时：

- 骑行时间开始增长
- 里程开始累计
- 平均速度开始统计
- 最大速度开始更新
- Ride Data 生效
- Dynamic Card Mode 恢复

---

# 19. 页面域调整

当前横向页面逻辑需要调整。

## 非骑行状态

主要根页面：

- Home
- Navigation
- Settings / History

不要求固定三页横向链。

## RIDING 状态

允许快速：

```text
Ride Dashboard ↔ Navigation
```

## 禁止

骑行中通过普通横滑：

```text
Dashboard
→ Navigation
→ Home
```

误回 Home。

---

# 20. Back 行为

## Home

Home 作为 Root：

- Back 默认不跳转到未知页面
- 后续可接系统退出逻辑

## PRE_RIDE

Back 可调用 Bottom Surface：

```text
取消本次骑行准备？
```

## RIDING

Back 不直接终止骑行。

---

# 21. 长按结束骑行

保持既有总设计：

```text
长按指定实体键
↓
Bottom Surface
Normal → Expanded
↓
结束本次骑行？

继续      结束
```

选择继续：

恢复之前的 Dynamic Card。

选择结束：

```text
Save
→ Ride Summary
```

Control Center 不负责结束骑行。

---

# 22. Home 动效

全部使用现有 `ui_motion`。

建议：

## 页面进入

```text
Opacity 0 → 255
Translate Y +8 → 0
```

## 主按钮

```text
Scale 100 → 94 → 100
```

## 累计数据 Reveal

轻量：

```text
Opacity 0 → 255
Y +6 → 0
```

禁止：

- 粒子
- 实时 Blur
- 序列帧
- 大量持续动画

---

# 23. Home 音效

统一使用 `ui_sound`。

建议：

- 开始骑行按钮：`SFX_PRESS`
- 进入 PRE_RIDE：`SFX_CONFIRM`
- GPS Ready：`SFX_SUCCESS`
- GO 成功：`SFX_CONFIRM`
- 失败：`SFX_WARNING`

禁止 Home 自建独立音效体系。

---

# 24. Home 数据来源

禁止最终业务数据硬编码。

统一从 App State / Ride History Summary 获取。

建议模型：

```c
typedef struct {
    uint32_t total_distance_m;
    uint32_t total_ride_time_s;
    uint32_t total_ascent_m;

    bool has_last_ride;

    uint32_t last_distance_m;
    uint32_t last_duration_s;
    uint32_t last_ascent_m;
    uint32_t last_timestamp;
} ui_home_model_t;
```

具体命名可结合现有工程调整。

---

# 25. PC Simulator Mock

必须支持：

- 无历史骑行
- 有历史骑行
- 大累计里程
- 长用户名
- 最近骑行数据

验证：

- 中文不裁切
- 数字不溢出
- 单位不重叠
- 长文本不破坏布局

---

# 26. Empty State

无历史记录时：

```text
暂无骑行记录
完成第一次骑行后将在这里显示
```

禁止显示脏数据或空白异常区域。

---

# 27. Achievements 策略

“我的成就”可保留入口。

当前阶段：

- 允许 Placeholder
- 允许简单静态页
- 不要求完整成就系统

不得拖慢 Home 主流程。

---

# 28. Settings 入口

Home 中：

```text
系统设置
```

进入现有 Settings。

禁止 Home 内直接塞系统设置控件。

---

# 29. Navigation 入口

点击：

```text
开始导航
```

进入现有 Navigation。

当前阶段保证：

```text
Home → Navigation
```

路由正确即可。

---

# 30. Design System 要求

Home 必须优先使用现有 Design Token。

禁止新增无必要硬编码：

- Color
- Radius
- Spacing
- Font Size
- Animation Duration
- Border Width

Token 不够时：

先补 Token，再使用。

---

# 31. 推荐公共组件

优先复用或建立：

```text
ui_status_bar
ui_primary_button
ui_secondary_button
ui_list_row
ui_data_value
ui_summary_card
ui_avatar
```

禁止 Home 独立造一套无法复用的组件。

---

# 32. Home 与 Surface Slot

Home 继续使用页面内 Slot 机制。

要求：

- 不使用 `lv_layer_top()`
- Router 进入 Home 时挂载 Surface
- Router 离开 Home 时卸载
- Home 为 Surface 预留安全区域

默认可隐藏。

---

# 33. 视觉风格约束

保持现有整体设计语言：

- 黑 / 深灰
- 白色文字
- 黄色 Accent
- MiSans
- Rhodes Island / PRTS 氛围
- 工业科技感
- 高信息密度但不拥挤

不要做成：

- 手机 App 用户中心
- 大量彩色卡片
- 过度毛玻璃
- 过度渐变
- 过度圆润

---

# 34. Dashboard 冻结规则

本阶段不要重新设计 Dashboard。

允许修改：

- 接入 Home 流程
- PRE_RIDE / GPS / GO
- Bottom Surface 状态
- Router / Command / Motion
- 小范围兼容调整

禁止：

- 重做速度区
- 重做 2×3 数据区
- 推翻 Dynamic Card 视觉
- 大改 Dashboard 已冻结结构

---

# 35. Control Center 修改

本阶段必须删除：

- 开始骑行
- 暂停 / 恢复骑行
- 结束骑行

保留：

- WiFi
- Bluetooth
- Sound
- Brightness
- GPS Status
- Power Mode

Touch Lock 视当前实现状态保留或补充。

---

# 36. 推荐开发步骤

## Step 1

只读检查：

- Profile
- Router
- Control Center
- Ride State
- Dashboard on_enter
- Bottom Surface
- Start Ride Command
- Home 数据模型

输出修改计划。

## Step 2

将 Profile 重构为 Home / Ride Hub。

先完成静态布局与数据绑定。

## Step 3

Boot 默认进入 Home。

## Step 4

打通：

```text
Home
→ PRE_RIDE
→ Dashboard
→ GPS Searching
```

## Step 5

实现：

```text
GPS Ready
→ GO
```

## Step 6

实现 GO Command：

```text
Pending
Success
Failed
Timeout
Rejected
```

## Step 7

Control Center 删除 Ride Controls。

## Step 8

调整 Riding Page Domain。

## Step 9

Build + Run + Test。

---

# 37. 验收用例

## Case 1

```text
Boot
→ Home
```

通过。

## Case 2

```text
Home
→ Settings
→ Back
→ Home
```

通过。

## Case 3

```text
Home
→ Navigation
→ Back
→ Home
```

通过。

## Case 4

```text
Home
→ Start Ride
→ Dashboard
→ GPS Searching
```

通过。

## Case 5

```text
GPS Searching
→ GPS Ready
→ GO
→ Pending
→ Success
→ Riding
```

通过。

## Case 6

GO Failed：

```text
GO
→ Pending
→ Failed
→ Retry
```

通过。

## Case 7

GO Timeout：

通过。

## Case 8

PRE_RIDE Back：

不破坏状态机。

## Case 9

RIDING 横向手势：

只在允许的 Riding Page Domain 内切换。

## Case 10

Control Center：

不存在 Start / Pause / End Ride。

---

# 38. 性能要求

Home 完成后至少保证：

- 静态接近 59/60 FPS
- 页面切换无明显掉帧
- Bottom Surface 更新无明显卡顿
- 页面退出后无高频无效刷新
- Router 往返无对象泄漏
- Home ↔ Dashboard 重复 100 次无异常

---

# 39. 自动化测试建议

补充：

```text
Home Route Test
Start Ride Flow Test
Pre Ride State Test
GPS Ready Test
GO Success Test
GO Failed Test
GO Timeout Test
Control Center Ride Action Absence Test
Riding Page Domain Test
```

---

# 40. 本阶段输出报告

完成后生成：

```text
PHASE_6A_HOME_RIDE_HUB_REPORT.md
```

至少包括：

- 已完成
- 新增文件
- 修改文件
- Profile → Home 迁移情况
- Control Center 修改
- Ride Flow 状态
- Bottom Surface 状态
- Command 状态
- 自动化测试
- FPS / Busy
- 已知问题
- 下一阶段建议

---

# 41. 本阶段禁止事项

禁止：

- 大规模重构 Bottom Surface
- 重写 Router
- 再造第二套 App State
- 再建 Command Bus
- 新增大量页面
- 重做 Dashboard
- 开始真实地图引擎
- 开始 STM32 真机移植
- 开始复杂成就系统
- 开始完整 History Detail

当前必须聚焦：

> **Home + Ride Entry Flow**

---

# 42. 下一阶段

本阶段完成后进入：

## Phase 6B

正式统一：

- Quick Control Center
- Home
- Navigation
- Settings
- System
- Summary
- History

并全面迁移：

- Design Token
- Typography
- Spacing
- Icon
- Motion
- Component Style

---

# 43. Codex 当前执行指令

```text
你现在进入 Bike Computer UI 项目的 Phase 6A。

请首先完整阅读：

1. Bike_Computer_UI_UX_Master_Design_V1.0.md
2. Bike_Computer_UI_Codex_Handover_V1.0.md
3. Bike_Computer_Home_Ride_Hub_Codex_Handover_V1.0.md

本阶段核心目标：

将现有 Profile 重构为 Home / Ride Hub，并打通：

BOOT
→ HOME
→ START RIDE
→ RIDE DASHBOARD
→ GPS SEARCHING
→ GPS READY
→ GO

同时：

- 从 Quick Control Center 删除开始骑行、暂停/恢复、结束骑行；
- 保留当前 Bottom Interaction Surface 架构；
- 不推翻 Dashboard；
- 不新增新的大架构；
- 不开始真实地图或 STM32 真机工作。

先进行只读检查并给出本阶段修改计划，然后开始实现。

完成后必须：

- Build
- Run
- 执行自动化测试
- 验证 Home / Ride Flow
- 输出 PHASE_6A_HOME_RIDE_HUB_REPORT.md
```

---

# 44. 最终验收目标

本阶段完成后，产品入口正式冻结为：

```text
BOOT
  ↓
HOME / RIDE HUB
  ↓
START RIDE
  ↓
RIDE DASHBOARD
  ↓
GPS SEARCHING
  ↓
GPS READY
  ↓
GO
  ↓
RIDING
```

并且：

- Home 成为开机默认主页
- Dashboard 成为骑行任务页
- Control Center 只负责系统快捷控制
- Bottom Interaction Surface 负责动态事件与即时交互
- 骑行生命周期不再分散在多个页面和控件中

完成该阶段后，正式进入全局 UI 视觉统一阶段。
