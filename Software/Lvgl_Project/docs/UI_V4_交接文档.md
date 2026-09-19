# Bike Computer UI Final Handover & Development Roadmap V1.0

## 0. 文档定位

本文档用于正式冻结当前公路车码表 UI/UX 方向，并作为后续 Codex 开发的统一执行规范。

当前已经完成底层架构阶段，后续不再大范围修改：

- Router
- App State
- Command / Action
- Gesture Ownership
- Bottom Interaction Surface
- Dynamic Card Dock
- Design Token 基础
- PC Simulator 测试框架

从本文件开始，项目进入：

> **Home / Ride Hub 落地 → 全局视觉统一 → 交互闭环 → PC 验收 → STM32 真机移植**

---

# 1. 当前冻结的产品总原则

整套 UI 继续采用：

> **Minimal Page Architecture + Event-driven Bottom Interaction Surface + Context-preserving Interaction**

中文：

> **少页面架构 + 事件驱动底部统一交互表面 + 上下文保持式交互**

核心原则：

1. 页面只承载长期、复杂任务。
2. 轻量确认、状态反馈、临时控制优先使用 Bottom Interaction Surface。
3. 不再增加传统小弹窗覆盖底部卡片。
4. 用户在骑行过程中尽量保持当前页面上下文。
5. Touch、Encoder、实体按键最终都进入统一状态和命令通道。

---

# 2. Home / Ride Hub 最终视觉方案

当前 Home UI 正式冻结为已确认的“黑底卡片式 Ride Hub”。

## 2.1 全局背景

- 整页纯黑背景。
- 不使用额外底部装饰区。
- 不显示 Bottom Interaction Surface 占位框。
- 页面最底部保留适量纯黑安全留白。
- Bottom Interaction Surface 默认隐藏，仅在系统事件发生时出现。

---

## 2.2 顶部状态栏

沿用当前统一状态栏：

- 左：GNSS / Satellite + 数量
- 中：时间
- 右：WiFi
- Bluetooth
- 电池百分比
- 动态电池图标

要求：

- 使用现有公共 Status Bar。
- 不为 Home 单独实现另一套。
- 图标、文字、基线、间距与 Dashboard 一致。

---

## 2.3 用户信息卡

第一张卡片：

- 左侧圆形头像
- 用户名
- 一条简短英文 / 状态标语
- 右侧可放极简骑行语义文字

视觉：

- 黑灰卡片
- 微弱边框
- 小圆角
- 不做手机 App 式个人中心
- 不加入社交信息

---

## 2.4 累计骑行数据主卡

第二张大卡片是 Home 的核心视觉区域。

内容：

- `TOTAL DISTANCE`
- 大号累计里程
- `km`
- 累计时间
- 累计爬升

视觉：

- 黑灰底
- 可使用深色 / 灰阶山地公路背景图
- 文字必须保持高对比度
- 大号数字是该卡片视觉焦点
- 黄色只作为少量强调，不大面积铺满

示例：

```text
TOTAL DISTANCE

2,864 km

TOTAL TIME        TOTAL ASCENT
124 h 36 m        28,417 m
```

---

# 3. 主操作区

## 3.1 开始骑行

主按钮：

```text
开始骑行
START RIDE
```

视觉：

- 黄色实心主按钮
- 黑色骑行图标
- 黑色主文字
- 右侧箭头
- 整页最高优先级操作

该按钮必须比其他操作更醒目。

---

## 3.2 开始导航

次按钮：

```text
开始导航
START NAVIGATION
```

视觉：

- 深灰 / 黑色卡片
- 白色定位图标
- 白色主文字
- 次级英文文字
- 右侧箭头

与开始骑行横向并列。

---

# 4. 最近一次骑行卡

显示：

- 最近一次骑行
- 日期
- 时间
- 路线缩略图
- 距离
- 用时
- 爬升
- 一句短骑行描述

第一版允许：

- 使用 PC Mock 路线缩略图
- 使用静态 / 伪路线图

后续真实历史轨迹再接入。

---

# 5. 功能入口列表

底部只保留三项：

```text
历史记录
我的成就
系统设置
```

每项：

- 左侧图标
- 中文主标题
- 英文副标题
- 右侧箭头
- 深灰卡片
- 黑色页面背景

禁止再增加底部导航栏。

---

# 6. Home 底部区域最终定义

当前 UI 图中不再绘制任何：

- Bottom Interaction Surface 占位框
- 底部 slogan 区
- 底部手势条
- 底部导航栏

Home 默认视觉必须干净结束于：

```text
系统设置
```

下方只保留纯黑安全边距。

但系统架构中：

`Bottom Interaction Surface` 仍然存在。

其状态：

```text
HOME NORMAL
→ Surface Hidden
```

当发生：

- 低电量
- 连接失败
- 系统错误
- 命令 Pending
- 全局警告

时：

Surface 才临时出现。

注意：

> 只是“默认不显示”，不是删除 Bottom Interaction Surface 架构。

---

# 7. Home 数据模型

Home 所有正式数据必须来自 App State / Ride Summary Model。

禁止页面长期硬编码最终业务数据。

至少需要：

```text
User
- name
- avatar

Lifetime
- total_distance
- total_time
- total_ascent

Last Ride
- date
- distance
- duration
- ascent
- route_preview
```

PC Simulator 可使用 Mock 数据。

---

# 8. Home 交互流程

## 8.1 Boot

最终启动路径：

```text
BOOT
↓
HOME / RIDE HUB
```

Home 成为开机默认根页面。

---

## 8.2 开始骑行

用户点击：

```text
开始骑行
```

执行：

```text
HOME
↓
PRE_RIDE
↓
RIDE DASHBOARD
↓
Bottom Interaction Surface
↓
GPS SEARCHING
```

点击“开始骑行”时：

> 不开始计时。

---

## 8.3 GPS Searching

显示：

```text
GPS
正在定位…
SAT xx
```

类型：

```text
INFO
P2 Ride State
```

---

## 8.4 GPS Ready

同一个 Bottom Interaction Surface 原位更新：

```text
GPS READY

定位完成

GO
```

禁止：

- 新 Popup
- 新页面
- 小弹窗覆盖 Dynamic Card

---

## 8.5 GO

用户点击 GO 后：

通过异步 Command Service：

```text
QUEUED
↓
PENDING
↓
SUCCESS / FAILED / TIMEOUT / REJECTED
```

Success 后：

- 启动骑行计时
- 启动轨迹记录
- 启动里程累计
- 启动平均 / 最大统计
- 进入 RIDING

随后恢复：

```text
Dynamic Card Mode
```

---

# 9. Control Center 最终职责

顶部下拉 Quick Control Center 只负责系统级快捷控制。

保留：

- WiFi
- Bluetooth
- Brightness
- Sound
- GPS Status
- Power Mode
- Touch Lock
- Battery Status

删除：

- 开始骑行
- 暂停骑行
- 恢复骑行
- 结束骑行

最终语义：

> 顶部 = 系统快捷控制
> 中部 = 当前任务
> 底部 = 当前事件与即时操作

---

# 10. 骑行过程中页面域

## 非 RIDING

根页面：

```text
Home
```

可进入：

- Navigation
- Settings
- History

## RIDING

核心页面：

```text
Ride Dashboard ↔ Navigation
```

RIDING 状态下：

禁止通过普通横滑误进入 Home。

---

# 11. Ride Dashboard 处理原则

当前 Dashboard 视觉已经基本冻结。

后续只允许：

- 接 Home 流程
- PRE_RIDE
- GPS Searching
- GPS Ready
- GO
- Bottom Surface 状态
- 动效统一
- Token 迁移
- 小范围兼容修正

禁止：

- 重做速度区
- 重做 2×3 数据
- 推翻 Dynamic Card
- 大幅更换 Dashboard 风格

---

# 12. Bottom Interaction Surface 最终规则

现有架构：

```text
Bottom Interaction Surface
├─ Dynamic Card Mode
└─ Interaction Mode
```

继续保持。

## Dynamic Card Mode

- Extended Ride Data
- Navigation
- Music
- Weather

## Interaction Mode

- INFO
- ACTION
- CONFIRM
- CONTROL
- NAVIGATION
- MEDIA

---

# 13. Interaction Surface 核心规则

必须继续遵守：

> 内容变，不换容器。
> 状态变，不叠弹窗。
> 事件结束后恢复原上下文。

支持：

- P0~P4 Priority
- Priority Override
- Restore Stack
- Pending
- Success
- Failed
- Timeout
- Rejected

---

# 14. 页面正式范围

V1.0 / V1.1 当前核心页面：

```text
P01 Home / Ride Hub
P02 Ride Dashboard
P03 Navigation
P04 Ride Summary
P05 History
P06 Settings
P07 System / Detail
P08 Power
```

Boot / Power Off 作为系统页面。

Achievements 第一版可以是轻量页面或 Placeholder。

---

# 15. 后续开发总路线

---

## Phase 6A — Home / Ride Hub 实现

当前立即执行。

目标：

- Profile → Home / Ride Hub
- 完成已冻结 UI
- Boot → Home
- Start Ride → Dashboard
- GPS Searching
- GPS Ready
- GO
- 删除 Control Center Ride Controls

交付：

```text
PHASE_6A_HOME_RIDE_HUB_REPORT.md
```

---

## Phase 6B — Quick Control Center 视觉统一

统一：

- WiFi
- Bluetooth
- Sound
- Brightness
- GPS Status
- Power Mode
- Touch Lock

要求：

- Token 化
- Focus
- Touch / Encoder
- Pending / Success / Failed
- 下拉跟手动画
- 状态同步

---

## Phase 6C — Navigation 视觉统一

保留当前导航页结构。

重点统一：

- 地图区
- 数据区
- 转向区
- 图标
- 字体
- Spacing
- Focus
- Bottom Surface Slot
- 导航抢占事件

真实地图引擎暂不阻塞 UI。

---

## Phase 6D — Settings / System / Power

建立统一：

```text
List
Detail
Toggle
Slider
Selection
Info Row
```

简单设置：

→ Bottom Surface CONTROL

复杂设置：

→ Detail Page

---

## Phase 6E — Ride Summary

完成最终商用视觉。

重点：

```text
RIDE COMPLETE
距离
时间
均速
极速
爬升
心率
功率
踏频
```

增加：

- 主数字 Reveal
- 数据 Stagger Reveal
- Save 状态
- 完成按钮

---

## Phase 6F — History

V1 第一版：

- 日期
- 距离
- 时间
- 均速
- 爬升

暂不做复杂详情图表。

---

# 16. Dynamic Card 完整化

Music Card 已经较完整。

继续补齐：

## Extended Data

- 数据刷新
- Focus
- Encoder
- Touch
- 动效

## Navigation Card

- 下一转向
- 距离
- 路名
- 抢占
- 恢复

## Weather Card

V1 只需：

- 温度
- 天气
- 风速
- 降雨概率

---

# 17. Motion System 最终统一

所有页面和组件统一调用 `ui_motion`。

必须覆盖：

```text
PRESS
FOCUS
PAGE
REVEAL
DATA
DOCK ENTER
DOCK UPDATE
DOCK EXPAND
DOCK EXIT
CONTROL CENTER PULL
```

禁止页面自己散落 LVGL Animation 参数。

---

# 18. Sound System 最终统一

统一：

```text
SFX_FOCUS
SFX_PRESS
SFX_CONFIRM
SFX_BACK
SFX_SUCCESS
SFX_WARNING
SFX_CRITICAL
SFX_NAV
```

所有页面只触发语义事件。

不允许页面自己绑定随机音效文件。

---

# 19. Design System 继续完善

现有 Token 继续扩展：

```text
Color
Typography
Spacing
Radius
Border
Opacity
Icon Size
Page Margin
Card Padding
Motion Duration
Motion Easing
Pressed
Focused
Disabled
Loading
Warning
Error
```

后续新页面禁止新增无管理硬编码。

---

# 20. PC Simulator 最终闭环测试

全 UI 完成后必须跑完整产品流程：

```text
Boot
↓
Home
↓
Start Ride
↓
GPS Searching
↓
GPS Ready
↓
GO
↓
Riding
↓
Music
↓
Navigation Override
↓
Restore Music
↓
Auto Pause
↓
Resume
↓
Low Battery
↓
End Ride Confirm
↓
Cancel
↓
End Again
↓
Save
↓
Summary
↓
Home
```

---

# 21. 异常测试

必须模拟：

```text
GPS Failed
GPS Timeout
WiFi Failed
BLE Failed
Command Rejected
Command Timeout
Save Failed
Low Battery
Sensor Lost
```

所有异常优先通过 Bottom Interaction Surface 表达。

---

# 22. PC 性能验收

最终至少检查：

- FPS
- LVGL Handler Busy
- Heap
- Peak Heap
- Object Count
- Timer Count
- Animation Count
- Image Cache
- Font Cache

长时间压力测试：

- 页面切换
- Dock 抢占
- Control Center
- Music
- Navigation
- Ride Data Refresh

检查：

- 泄漏
- Timer 残留
- Animation 残留
- 堆碎片
- 状态失配

---

# 23. UI 冻结条件

PC Simulator 达到以下条件后，进入 UI RC：

- 所有核心页面视觉统一
- Home 完整
- Dashboard 完整
- Navigation 完整
- Settings 完整
- Summary 完整
- History 基础完成
- Bottom Surface 完整
- Control Center 完整
- Motion 完整
- Sound 接口完整
- Command 异常闭环完整
- 自动化测试通过
- 长时间压力测试通过

标记：

```text
UI V1.1 RC
```

---

# 24. Phase 8 — STM32 真机移植

PC UI RC 后才正式进入真机。

顺序：

```text
LTDC
↓
SDRAM
↓
LVGL
↓
Touch
↓
Encoder
↓
5 Keys
↓
TF
↓
GNSS
↓
Sensors
↓
ESP32
↓
Audio
```

---

# 25. STM32 Backend

PC：

```text
ui_command_service
↓
PC Mock Backend
```

STM32：

```text
ui_command_service
↓
STM32 Backend
↓
GNSS / ESP32 / Driver / RTOS
```

上层 UI 不改变。

---

# 26. STM32 真机重点验收

重点验证：

- LTDC 帧率
- SDRAM 带宽
- DMA2D
- Cache
- Touch 延迟
- Encoder 响应
- 实体按键
- TF 读取
- GNSS
- BLE / WiFi
- 音效
- 图片加载
- 字体
- 长时间稳定性

---

# 27. 真实地图作为独立子项目

真实地图不阻塞 UI 主线。

采用：

```text
Navigation UI
↓
Map Provider Interface
↓
PC Mock Map / STM32 Tile Map
```

后续单独完成：

- TF Tile Reader
- Tile Cache
- 经纬度投影
- Map Renderer
- Route Data
- Offline Navigation
- Re-route

---

# 28. 字体管理

继续采用 MiSans 子集。

建立正式字形清单。

每新增中文：

1. 检查字形是否存在
2. 缺失则统一重新生成子集
3. 禁止临时混用多个未知字体

---

# 29. 当前最高优先级任务

Codex 下一步只执行：

> **Phase 6A — Home / Ride Hub**

不要同时开始：

- 真实地图
- STM32
- 完整 History Detail
- 复杂 Achievements
- 新的大架构模块

---

# 30. Codex 当前执行指令

```text
你现在进入 Bike Computer UI 项目的 Phase 6A。

请首先完整阅读：

1. Bike_Computer_UI_UX_Master_Design_V1.0.md
2. Bike_Computer_UI_Codex_Handover_V1.0.md
3. Bike_Computer_UI_Final_Handover_And_Roadmap_V1.0.md

当前 Home / Ride Hub 的视觉方案已经正式冻结。

必须严格按照当前确认的 Home UI 实现：

- 全局纯黑背景；
- 顶部统一状态栏；
- 用户头像 / 用户名信息卡；
- TOTAL DISTANCE 大卡；
- 累计时间 / 累计爬升；
- 黄色“开始骑行”主按钮；
- 深灰“开始导航”次按钮；
- 最近一次骑行卡；
- 历史记录；
- 我的成就；
- 系统设置；
- 页面底部不显示任何固定 Dock、Footer、Slogan 或导航栏；
- 下方只保留纯黑安全边距；
- Bottom Interaction Surface 架构保留，但 Home 默认 Hidden。

同时完成：

BOOT
→ HOME
→ START RIDE
→ RIDE DASHBOARD
→ GPS SEARCHING
→ GPS READY
→ GO

并从 Quick Control Center 删除：

- 开始骑行
- 暂停 / 恢复
- 结束骑行

禁止：

- 推翻 Dashboard
- 重构 Bottom Interaction Surface
- 新建第二套 App State
- 新建第二套 Command Bus
- 开始真实地图
- 开始 STM32 真机移植

完成后：

- Build
- Run
- 自动化测试
- Home / Ride Flow 验证
- 性能检查
- 输出 `PHASE_6A_HOME_RIDE_HUB_REPORT.md`
```

---

# 31. 最终冻结结论

从本文件开始：

## Home

视觉方案冻结。

## Dashboard

现有结构冻结。

## Bottom Interaction Surface

架构冻结。

## Control Center

职责冻结为系统快捷控制。

## Ride Flow

冻结为：

```text
BOOT
↓
HOME
↓
START RIDE
↓
DASHBOARD
↓
GPS SEARCHING
↓
GPS READY
↓
GO
↓
RIDING
↓
END CONFIRM
↓
SAVE
↓
SUMMARY
↓
HOME
```

## 后续工作

只允许：

- 视觉统一
- 功能补全
- 动效统一
- 音效统一
- 性能优化
- 真机适配

除非出现明确的新产品需求，不再改变整个 UI 信息架构。
