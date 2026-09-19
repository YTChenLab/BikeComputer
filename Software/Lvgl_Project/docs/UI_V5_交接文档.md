# Bike Computer UI 版本交接文档 V1.1.2

## 0. 文档信息

| 项目 | 内容 |
| --- | --- |
| 交接版本 | `v1.1.2` UI Finalization Baseline |
| 基线版本 | `v1.1.1` |
| 目标 | 完成 LVGL UI 第一阶段最终封版前的增量完善 |
| UI 框架 | LVGL `9.3.0-dev` |
| 目标分辨率 | `480 × 854`，RGB565 |
| PC 平台 | Windows PC Simulator + SDL2 + MSYS2 UCRT64 |
| 目标硬件 | STM32 + LTDC + SDRAM + DMA2D + Touch + Encoder + 实体按键 + TF + ESP32-S3 |
| 本版本性质 | 在 v1.1.1 架构上做 UI/交互增量完善，不重构核心架构 |

---

# 1. v1.1.2 的版本定位

`v1.1.1` 已经完成：

- Router
- App State
- Ride State Machine
- Semantic Input
- Command Service
- Event Bus
- Bottom Interaction Surface
- Dynamic Card Dock
- Quick Control Center
- Home / Ride Hub
- Ride Dashboard
- Navigation V2
- Ride Summary
- History V1
- Settings / System / Power
- PC Mock
- PC 持久化
- 自动化测试

`v1.1.2` 不重新设计整套产品信息架构。

本版本只完成：

> **快捷控制条 V1 + 顶部控制与底部动态交互坞联动 + 完整控制中心精简 + 胶囊流体动效 V1 + UI 最终查漏补缺 + PC RC 验收**

---

# 2. v1.1.2 总体冻结原则

本版本必须继续遵守：

1. `ui_app_state` 是全局状态唯一真值来源。
2. UI 不直接伪造硬件成功状态。
3. 所有系统操作统一通过 `ui_command_service`。
4. Touch、Encoder、实体键统一进入语义输入层。
5. Dynamic Card 与 Interaction 共用唯一 `Bottom Interaction Surface`。
6. 不创建第二个底部 Popup。
7. 外设任务不能直接修改 LVGL 对象。
8. PC Mock 与 STM32 Backend 共用上层接口。
9. Dashboard 冻结区不重构。
10. 本版本所有 UI 与动效必须满足 STM32 + SDRAM + LTDC + DMA2D 真机资源边界。

---

# 3. v1.1.2 新增核心能力

本版本新增四项：

```text
1. 快捷控制条 V1
2. 快捷控制条 ↔ 动态交互坞上下联动
3. 完整控制中心 UI 精简
4. 胶囊流体动效系统 V1
```

---

# 4. 顶部控制系统正式定义

顶部控制系统分为两级：

```text
系统控制帘
├─ 一级：快捷控制条
└─ 二级：完整控制中心
```

中文名称统一使用：

- `快捷控制条`
- `完整控制中心`
- `系统控制帘`
- `动态交互坞`

不再在产品文档中使用 Quick Ribbon、System Shade 等英文主名称。

---

# 5. 快捷控制条 V1

## 5.1 第一版固定四项

v1.1.2 暂时不做状态动态切换。

固定四项：

```text
WiFi | 蓝牙 | 声音 | 亮度
```

顺序固定：

```text
Slot 0 = WiFi
Slot 1 = Bluetooth
Slot 2 = Sound
Slot 3 = Brightness
```

第一版不随以下状态变化：

- Home
- PRE_RIDE
- RIDING
- NAVIGATION
- LOW_BATTERY

后续版本再做场景动态切换。

---

# 6. 快捷控制条视觉结构

快捷控制条通过：

```text
顶部第一次下滑
```

呼出。

建议视觉：

- 位于状态栏下方
- 单行四项
- 黑 / 深灰背景
- 白色图标与文字
- 黄色 Focus Accent
- 四项宽度一致
- 不使用复杂 Card 嵌套
- 不遮挡过多主页面

建议高度：

```text
约 80 ~ 110 px
```

---

# 7. 快捷控制条输入逻辑

## 7.1 Touch

Touch 可直接点击四项。

## 7.2 Encoder

快捷控制条打开后：

```text
Rotate
→ 左右切换 Focus

Press
→ 执行当前项目
```

快捷控制条内 Encoder 焦点顺序固定：

```text
WiFi
→ 蓝牙
→ 声音
→ 亮度
→ WiFi
```

支持循环。

---

# 8. 开关类交互

v1.1.2 中：

```text
WiFi
蓝牙
```

视为开关 / 异步命令类。

操作流程：

```text
Focus
↓
Press
↓
Submit Command
↓
PENDING
↓
SUCCESS / FAILED / TIMEOUT / REJECTED
```

快捷控制条不能直接伪造最终状态。

最终状态必须来自：

```text
ui_command_service
↓
Backend
↓
App State
↓
UI
```

---

# 9. WiFi / 蓝牙反馈规则

如果快捷控制条仍打开：

```text
PENDING / SUCCESS / FAILED
```

优先在快捷控制条原位反馈。

例如：

```text
蓝牙
连接中…
```

成功：

```text
蓝牙
已连接
```

失败：

```text
蓝牙
连接失败
```

---

# 10. “任务下沉”机制

如果：

```text
用户从快捷控制条发起异步任务
↓
任务仍为 Pending
↓
用户关闭快捷控制条
```

则：

```text
动态交互坞接管任务状态
```

例如：

```text
蓝牙
正在连接功率计…

ASSIOMA DUO
```

任务成功后：

```text
功率计已连接
```

短暂显示后恢复原 Dynamic Card。

---

# 11. 避免双重反馈

同一任务不能同时出现：

```text
顶部 Pending
+
底部 Pending
```

规则：

```text
系统控制帘打开
→ 顶部拥有该任务反馈

系统控制帘关闭
→ 动态交互坞拥有该任务反馈
```

同一时刻只有一个主反馈位置。

---

# 12. 数值类交互

v1.1.2 中：

```text
声音
亮度
```

视为数值类。

快捷控制条只负责：

> 选择要调整什么。

动态交互坞负责：

> 怎么调。

---

# 13. 声音 / 亮度快捷调节流程

例如选择：

```text
亮度
75%
```

Encoder Press：

```text
快捷控制条保持 Focus
↓
Bottom Interaction Surface
切换到 CONTROL
↓
显示亮度调节
```

底部：

```text
亮度

━━━━━━━●━━━━
75%
```

然后：

```text
Encoder CW   -> +5%
Encoder CCW  -> -5%
```

无需再次 Press 才进入数值调节。

---

# 14. 数值调整确认逻辑

数值调整中：

```text
Rotate
→ 实时预览

Press
→ 退出调节并保留当前值

Back
→ 退出当前调节
```

Brightness / Sound 第一版采用：

> 实时预览 + 当前值保留。

无需做复杂“确认后才写入”的二次流程。

---

# 15. 动态交互坞恢复

数值调节结束：

```text
Bottom Surface CONTROL
↓
退出
↓
Restore Stack
↓
恢复原 Dynamic Card
```

例如：

```text
Music Card
↓
Brightness Control
↓
Music Card
```

并恢复：

- active_card
- scroll position
- focus
- card internal state

---

# 16. 第二次下滑：完整控制中心

快捷控制条打开后：

```text
继续向下拖动
```

进入：

```text
完整控制中心
```

状态：

```text
CONTROL_CENTER_CLOSED
↓
QUICK_BAR
↓
FULL_CONTROL_CENTER
```

---

# 17. 完整控制中心 V1.1.2 布局

保留 v1.1.1 功能：

```text
WiFi
Bluetooth
Sound
Brightness
GPS
Power Save
Touch Lock
Battery
```

但重做视觉密度。

推荐布局：

```text
系统控制
────────────────────────

WiFi                  蓝牙
已连接                已连接

────────────────────────

亮度  75%
━━━━━━●━━━━━━━━━━━━

声音  60%
━━━━●━━━━━━━━━━━━━━

────────────────────────

GPS        省电       触摸锁      电量
良好       标准       关闭        78%
```

---

# 18. 完整控制中心尺寸

不再要求铺满屏幕。

建议：

```text
Height ≈ 400 ~ 470 px
```

约：

```text
45% ~ 55% screen height
```

主页面下半部分仍然可见。

---

# 19. 完整控制中心数值条

Brightness / Sound：

改为：

```text
细横条 Slider
```

建议：

```text
Track Height = 2 ~ 5 px
Thumb Size   = 8 ~ 14 px
```

Touch：

```text
可直接拖动横条
```

Encoder：

```text
Focus
↓
Press
↓
动态交互坞精调
```

第一版继续复用现有 Bottom Surface CONTROL 逻辑。

---

# 20. 顶部与底部的职责冻结

## 顶部：系统控制帘

负责：

> “我主动想修改什么？”

包括：

- WiFi
- Bluetooth
- Sound
- Brightness
- GPS
- Power Mode
- Touch Lock
- Battery Status

## 中间：任务页面

负责：

> “我现在正在做什么？”

## 底部：动态交互坞

负责：

> “系统现在发生了什么？”  
> “我现在能马上做什么？”

---

# 21. 上下联动优先级规则

## 普通 Dynamic Card

例如：

- Ride Data
- Music
- Weather

系统控制帘打开时：

```text
暂时隐藏 / 降低交互优先级
```

其状态必须保留。

系统控制帘关闭：

```text
恢复此前 Dynamic Card
```

---

# 22. 高优先级事件抢占系统控制帘

如果系统控制帘打开时出现：

## P0 Critical

例如：

- Save Failed
- Critical Battery
- Critical Storage Error

或：

## P1 Riding Safety

例如：

- Immediate Turn
- Severe Off-route
- Navigation Safety Warning

必须：

```text
系统控制帘自动收起
↓
Bottom Interaction Surface 抢占
↓
显示 P0 / P1
```

安全事件优先。

---

# 23. 胶囊流体动效系统 V1

本版本正式加入：

> **胶囊流体动效系统**

注意：

v1.1.2 只实现“伪液体”视觉。

不实现真实流体物理。

---

# 24. 胶囊流体动效设计原则

通过以下基础属性组合：

```text
X / Y
Width / Height
Small Scale
Opacity
Border
Color
Progress Fill
```

模拟：

- 流动
- 吸附
- 挤压
- 回弹
- 胶囊 Morph

---

# 25. 快捷控制条焦点胶囊

快捷控制条只创建：

```text
1 个 focus_capsule
```

不为四个选项各创建独立动态 Focus Layer。

Encoder 旋转：

```text
WiFi
↓
Bluetooth
```

focus_capsule：

```text
正常宽度
→ 横向轻微拉伸
→ 向目标移动
→ 轻微压缩
→ 恢复
```

建议时间：

```text
120 ~ 180 ms
```

---

# 26. Focus 胶囊资源实现原则

禁止使用：

- 大面积 Blur
- 实时 Mask
- Metaball
- 大透明图层

推荐：

```text
lv_obj_set_x
lv_obj_set_width
local scale
border color
background opacity
```

必须保持局部小对象动画。

---

# 27. 开关胶囊动效

WiFi / Bluetooth 切换：

```text
Press
↓
Scale 100%
→ 96%
→ 102%
→ 100%
```

状态：

```text
OFF = 深灰
ON  = 黄色 Accent
```

Pending：

```text
轻微 Pulse
```

不使用高频无限动画。

---

# 28. 数值液体条动效

Brightness / Sound：

数值变化时：

```text
Fill Width
Thumb X
Thumb Small Scale
Value Text
```

同步更新。

例如：

```text
75%
↓
80%
```

效果：

```text
黄色 Fill 平滑推进
Thumb 轻微膨胀
数值轻微 Y Translate / Fade
```

---

# 29. Bottom Surface Morph

Dynamic Card → Control：

不直接闪切。

使用：

```text
旧内容淡出 / 上移
↓
Surface geometry 轻微变化
↓
新内容淡入
```

建议：

```text
180 ~ 240 ms
```

不做整张大型容器 Scale。

---

# 30. 动效统一 Token

建议补充：

```text
MOTION_CAPSULE_FOCUS      150 ms
MOTION_CAPSULE_PRESS      140 ms
MOTION_TOGGLE             180 ms
MOTION_VALUE_STEP          90 ms
MOTION_SURFACE_SWITCH     200 ms
MOTION_CONTROL_EXPAND     260 ms
```

统一进入：

```text
ui_motion
```

禁止每个页面散落独立时长。

---

# 31. 硬件资源约束

v1.1.2 所有 UI 和动效必须以真机可运行作为硬约束。

当前硬件目标：

```text
STM32
+ External SDRAM
+ LTDC
+ DMA2D
+ 480×854 RGB565
```

一张 RGB565 Framebuffer：

```text
480 × 854 × 2
≈ 0.78 MiB
```

双缓冲：

```text
≈ 1.56 MiB
```

应放外部 SDRAM。

---

# 32. v1.1.2 禁止的视觉效果

禁止：

- 实时高斯模糊
- 大面积毛玻璃
- 大面积动态透明
- 整个 480×854 Root Scale
- 大对象整体 Opacity 动画
- 真实流体模拟
- Metaball
- 大范围 Mask
- 动态复杂阴影
- 大图片实时缩放 / 旋转
- 大地图 transform zoom

---

# 33. v1.1.2 推荐动画

优先：

```text
Translate X/Y
Width / Height
Small Scale
Local Opacity
Border
Color
Progress Fill
Small Icon Motion
```

核心原则：

> 只动小对象，不动整屏。

---

# 34. 快捷控制条对象复用

快捷控制条始终只创建：

```text
quick_item[4]
focus_capsule
```

第一版固定内容。

禁止：

```text
打开一次
→ create 4 items

关闭
→ delete

再次打开
→ create
```

建议 Overlay 创建后长期复用。

减少：

- Heap fragmentation
- Object churn
- Timer churn

---

# 35. 系统控制帘与 Gesture Ownership

必须继续遵守 v1.1.1：

Touch Down 时确定所有权，直到：

```text
Release / Cancel
```

第一段下拉：

```text
Quick Bar Owns Gesture
```

继续下拉：

```text
Control Center Owns Gesture
```

不得中途交给：

- Dynamic Card
- Map
- Page Swipe

---

# 36. Menu Key 建议

v1.1.2 可先预留：

```text
Menu Short
→ Quick Control Bar

Menu Long
→ Full Control Center
```

如果当前实体键语义尚未正式接入 PC，可先：

- 保留 API
- 暂不强制实现

真机阶段再绑定。

---

# 37. Dashboard 约束

Dashboard 冻结区继续保持：

- Status Bar
- Speed
- Yellow Speed Bar
- 2×3 Ride Data
- Background
- MiSans 比例
- Dynamic Card Geometry

v1.1.2 仅允许：

- 快捷控制条 Overlay
- Bottom Surface 联动
- Motion Token 接入
- Bug Fix
- Performance Fix

---

# 38. Navigation 约束

Navigation V2 不重构。

继续保持：

```text
Status Bar
Map
1×3 Data
Navigation Command Dock
```

v1.1.2 只需确保：

```text
Quick Bar
Full Control Center
P1 Navigation Override
```

三者不冲突。

---

# 39. Home 约束

Home 已冻结。

不增加新的系统开关。

快捷控制条通过顶部 Overlay 打开。

Home 本身不增加：

- WiFi按钮
- 蓝牙按钮
- Sound Slider
- Brightness Slider

避免重复入口。

---

# 40. Settings 约束

Settings 保留完整设置入口。

快捷控制条、完整控制中心、Settings 三个入口最终都必须调用：

```text
同一 Command
同一 App State
```

禁止建立三套业务逻辑。

---

# 41. v1.1.2 暂不开发的内容

本版本不做：

- Dynamic Quick Bar Context Switching
- Achievements 完整页
- Ride Detail
- GPX
- FIT
- 真实地图
- STM32 Driver
- BLE 真实传感器
- ESP32 真后端
- FatFs 真存储
- 真实 Audio Backend

---

# 42. Achievements 处理

v1.1.2 建议：

```text
暂时隐藏 Home 的 Achievements 入口
```

如果不隐藏：

必须明确显示 Disabled / Coming Later。

不允许：

```text
点击完全无反应
```

---

# 43. Legacy Profile

v1.1.2 冻结前检查：

- `UI_ROUTE_PROFILE`
- `OPEN_PROFILE`
- 旧 Profile Page
- 旧快捷键

如果确认：

```text
没有业务依赖
```

建议删除 Legacy Profile。

如果暂时保留：

必须加：

```text
LEGACY / DEPRECATED
```

并从正常导航完全隔离。

---

# 44. v1.1.2 开发阶段

## Phase 7A — 冻结 v1.1.1 基线

先完成：

```text
git status
↓
Review modified/untracked
↓
Build
↓
20/20 Test
↓
Commit
↓
Tag v1.1.1
```

不得跳过。

## Phase 7B — 快捷控制条 V1

开发：

```text
ui_quick_control_bar.c
ui_quick_control_bar.h
```

若现有 Control Center 架构适合：

可作为内部子组件实现，避免无意义新模块。

要求：

- 固定四项
- Touch
- Encoder
- Focus Capsule
- WiFi/Bluetooth Command
- Sound/Brightness Entry
- App State Binding

## Phase 7C — 上下联动

完成：

```text
Quick Bar
↕
Bottom Interaction Surface
```

包括：

- Sound/Brightness 调节
- Task Handoff
- Restore
- Pending Ownership
- P0/P1 Override

## Phase 7D — 完整控制中心精简

保留现有功能。

只调整：

- Layout
- Density
- Horizontal Slider
- Panel Height
- Visual Hierarchy

不要重写 Command / State 逻辑。

## Phase 7E — 胶囊流体动效 V1

扩展：

```text
ui_motion
```

完成：

- Focus Capsule
- Toggle Press
- Value Fill
- Thumb Motion
- Surface Morph
- Control Center Snap

## Phase 7F — UI Final Audit

逐页检查：

```text
Boot
Home
Dashboard
Navigation
Summary
History
Settings
System
Power
Power Off
```

检查：

- Font
- Clip
- Unit
- Baseline
- Spacing
- Radius
- Focus
- Touch
- Encoder
- Back
- Surface
- Empty
- Loading
- Error

---

# 45. 字体最终检查

新增 v1.1.2 文案后必须：

```text
generate_ui_font.py
```

并运行：

```text
ui_font_coverage_test
```

任何新增中文不得绕过字体覆盖测试。

---

# 46. v1.1.2 新增测试建议

在原 20 项测试基础上增加：

```text
ui_quick_control_bar_test
ui_quick_control_bar_encoder_test
ui_quick_control_bar_command_test
ui_quick_bar_bottom_surface_link_test
ui_control_center_compact_layout_test
ui_capsule_motion_logic_test
```

目标：

```text
至少 26/26 PASS
```

若测试拆分方式不同，以实际工程结构为准。

---

# 47. 快捷控制条测试矩阵

必须验证：

## WiFi

```text
OFF
PENDING
ON
FAILED
TIMEOUT
REJECTED
```

## Bluetooth

同上。

## Sound

```text
0%
5%
50%
95%
100%
```

## Brightness

同上。

---

# 48. 上下联动测试

Case 1：

```text
Quick Bar
→ Bluetooth
→ Pending
→ Quick Bar close
→ Bottom Surface Pending
→ Success
→ Restore
```

Case 2：

```text
Quick Bar
→ Brightness
→ Bottom Control
→ Encoder +5%
→ Press
→ Restore Dynamic Card
```

Case 3：

```text
Full Control Center Open
→ P1 Navigation Event
→ Control Center Auto Close
→ Bottom Navigation Override
```

Case 4：

```text
Control Center Open
→ P0 Save Failed
→ Auto Close
→ Critical Interaction
```

---

# 49. PC 完整回归流程

必须跑：

```text
Boot
↓
Home
↓
Quick Bar
↓
WiFi Toggle
↓
Bluetooth Toggle
↓
Sound
↓
Bottom Control
↓
Brightness
↓
Bottom Control
↓
Full Control Center
↓
Close
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
Dynamic Card
↓
Music
↓
Navigation
↓
Pause / Resume
↓
End
↓
Save
↓
Summary
↓
Home
↓
History
```

---

# 50. 异常回归

必须覆盖：

```text
WiFi FAILED
Bluetooth TIMEOUT
Brightness REJECTED
GPS FAILED
Save FAILED
Low Battery
Sensor Lost
Storage Error
```

重点验证：

```text
顶部 / 底部不重复反馈
Priority 正确
Restore 正确
```

---

# 51. PC 压力测试

v1.1.2 RC 前至少执行：

```text
30 ~ 60 min
```

循环：

- Quick Bar Open/Close
- Quick Bar Focus
- Brightness/Sound Control
- Full Control Center
- Dynamic Card Scroll
- Navigation Event
- Page Switch
- Ride Start/End
- History Update

记录：

```text
FPS
LVGL Handler Busy
Heap
Peak Heap
Object Count
Timer Count
Animation Count
```

---

# 52. Debug 配置

建议 RC 压力测试使用专用 Debug 配置：

```text
LV_USE_MEM_MONITOR = 1
Necessary Assert   = ON
```

Release / Final 再根据资源需求关闭。

---

# 53. v1.1.2 RC 验收条件

只有满足：

- 原 v1.1.1 功能无回归
- 快捷控制条可用
- 上下联动可用
- 完整控制中心精简完成
- 胶囊流体动效 V1 完成
- 字体覆盖通过
- 自动测试通过
- 完整产品流程通过
- 异常流程通过
- 30~60 min 压力测试通过
- 无持续 Heap 增长
- 无 Timer / Anim 泄漏
- 无 `No memory`
- 无明显 FPS 持续下降

才可标记：

```text
v1.1.2 RC
```

---

# 54. v1.1.2 正式版本管理

RC 通过后同步：

```text
VERSION
README.md
CHANGELOG.md
```

然后：

```text
git add
git commit
git tag v1.1.2
```

要求：

- 工作区干净
- 版本元数据一致
- 文档与源码一致
- 测试记录归档

---

# 55. v1.1.2 后续开发主线

v1.1.2 完成后：

> 停止大规模 LVGL UI 设计。

正式进入：

# STM32 Firmware Bring-up

---

# 56. STM32 后续开发顺序

建议顺序：

```text
1. STM32CubeMX 基础工程
2. Clock / MPU / Cache
3. FMC SDRAM
4. LTDC
5. DMA2D
6. LVGL Port
7. Touch
8. Encoder
9. 5 Physical Keys
10. FreeRTOS Task Architecture
11. GNSS
12. Barometer
13. Ride Engine
14. BLE Sensors
15. ESP32-S3
16. TF / FatFs
17. Audio
18. Real Map
```

---

# 57. 真机 LVGL 移植目标

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
Driver / RTOS Task
```

UI 页面不改。

---

# 58. 后续动态快捷控制条

v1.1.2 不做。

未来可升级：

```text
HOME
WiFi | 蓝牙 | 声音 | 亮度

PRE_RIDE
GPS | 蓝牙 | 声音 | 亮度

RIDING
触摸锁 | 蓝牙 | 声音 | 亮度

NAVIGATION
触摸锁 | 蓝牙 | 导航音量 | 亮度

LOW BATTERY
省电 | 蓝牙 | 声音 | 亮度
```

必须在 v1.1.2 真机交互验证后再决定是否加入。

---

# 59. v1.1.2 不得破坏的内容

1. 不重构 Dashboard 冻结区。
2. 不重新引入底部导航栏。
3. 不拆分 Bottom Interaction Surface。
4. 不增加第二套 Popup。
5. 不建立第二套 App State。
6. 不建立第二套 Command Bus。
7. 不让页面直接调用 Driver。
8. 不让后台任务直接修改 LVGL。
9. 不用截图替代 LVGL 控件。
10. 不为液体效果引入高成本图形算法。
11. 不开始真实地图。
12. 不开始 STM32 Driver。
13. 不把 PC Mock 描述成真机能力。

---

# 60. Codex 当前执行指令

```text
当前进入 Bike Computer UI v1.1.2 开发阶段。

基线：
v1.1.1

目标：
在现有架构上完成 UI Finalization，不重构核心架构。

请首先：

1. 阅读 `Bike_Computer_UI_交接文档_V1.1.1.md`
2. 阅读本文件 `Bike_Computer_UI_交接文档_V1.1.2.md`
3. 检查 git status
4. 先将 v1.1.1 形成可回退基线

然后按以下顺序执行：

Phase 7B：
实现固定四项快捷控制条：
WiFi / 蓝牙 / 声音 / 亮度

Phase 7C：
实现快捷控制条与 Bottom Interaction Surface 联动：
- WiFi/Bluetooth 异步任务反馈
- Task Handoff
- Sound/Brightness CONTROL
- Restore Stack
- P0/P1 Override

Phase 7D：
精简完整控制中心：
- 不满屏
- 亮度/声音改为细横条
- 保留现有系统控制功能
- 不重写 Command / App State

Phase 7E：
实现胶囊流体动效 V1：
- 单一 Focus Capsule
- Focus 流动
- Toggle 回弹
- Slider Fill
- Thumb Motion
- Surface Morph
- 所有参数进入 ui_motion / design token

Phase 7F：
执行全页面 UI Final Audit。

硬件约束：
所有动画必须面向 STM32 + SDRAM + LTDC + DMA2D 实现。
禁止 Blur、Metaball、全屏 Scale、大面积动态 Opacity、大对象 Transform。

完成后：
- Build
- Test
- Font Coverage
- Full Product Regression
- Failure Regression
- 30~60 min Stress Test
- 输出 `PHASE_7_UI_V1.1.2_FINALIZATION_REPORT.md`

在所有 RC 条件满足之前，不开始：
- STM32 Driver
- Real Map
- Ride Detail
- Full Achievements
```

---

# 61. v1.1.2 最终产品交互结论

第一版顶部控制固定为：

```text
WiFi | 蓝牙 | 声音 | 亮度
```

顶部第一次下滑：

```text
快捷控制条
```

顶部继续下滑：

```text
完整控制中心
```

数值类：

```text
快捷控制条选择
↓
动态交互坞精调
```

异步类：

```text
顶部发起
↓
顶部打开时原位反馈
↓
顶部关闭后任务下沉到动态交互坞
```

高优先级事件：

```text
P0 / P1
↓
系统控制帘自动收起
↓
动态交互坞抢占
```

所有动效：

```text
胶囊流体动效 V1
```

实现方式：

```text
小对象局部动画
```

不使用高成本实时图形算法。

---

# 62. 最终版本路线

```text
v1.1.1
当前完整功能基线
        ↓
v1.1.2
快捷控制条
上下联动
控制中心精简
胶囊流体动效
UI Final Audit
        ↓
v1.1.2 RC
PC 完整回归
异常回归
压力测试
        ↓
v1.1.2 Final
LVGL 第一阶段正式封版
        ↓
STM32 Firmware Bring-up
```

从 `v1.1.2 Final` 开始：

> LVGL 原则上只接入真实数据、真实 Backend 和硬件适配，不再进行无明确产品需求的大规模信息架构重构。
