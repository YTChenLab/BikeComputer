# Bike Computer UI 版本交接文档 V1.1.1

## 0. 文档信息

| 项目 | 内容 |
| --- | --- |
| 交接版本 | `v1.1.1` 功能基线 |
| 文档日期 | 2026-09-19 |
| 工程路径 | `D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project` |
| UI 框架 | LVGL `9.3.0-dev` 本地源码快照 |
| PC 平台 | Windows PC Simulator、SDL2、MSYS2 UCRT64 |
| 目标平台 | STM32 + LTDC + SDRAM + Touch + Encoder + 实体按键 + TF 卡 |
| 目标分辨率 | `480 × 854`，RGB565 |
| 发布标识 | Git tag `v1.1.1` |
| 自动化测试 | 2026-09-19 实测 `20/20 PASS` |

### 0.1 版本事实与边界

本文档定义并记录正式冻结的 **Bike Computer UI v1.1.1** 基线。`VERSION`、README、CHANGELOG、系统信息页、发布提交与 Git tag 使用同一版本号；后续开发应从 `v1.1.1` 标签建立分支，不能直接修改该标签。

本次冻结只声明 PC Simulator 范围内已经实现并验证的 UI、交互和平台抽象。STM32、TF/FatFs、真实 GNSS、真实地图、WiFi/BLE 和传感器能力仍按本文的“接口预留”或“未实现”状态管理，不因版本发布而视为真机完成。

### 0.2 完成度术语

本文使用四种状态，避免把 PC 演示能力误写成真机能力：

| 状态 | 含义 |
| --- | --- |
| 已完成 | UI、状态、交互和自动化测试均已接入当前工程 |
| PC 可演示 | 使用 Mock 服务或 PC 文件后端可完整演示，但不代表真实硬件已接入 |
| 接口预留 | 上层契约已定义，STM32 后端或真实数据服务仍需实现 |
| 未实现 | 当前仅有入口、占位或规划，不能视为可交付功能 |

---

## 1. 当前产品与架构结论

当前 UI 已从早期页面直连逻辑迁移为：

```text
PC SDL / STM32 GPIO / 外设任务
               │
               ▼
        语义输入与服务适配层
               │
               ▼
    App State / Command / Event Bus
               │
        ┌──────┴──────┐
        ▼             ▼
      Router     Ride State Machine
        │             │
        └──────┬──────┘
               ▼
       页面 / 公共组件 / 模型
               │
               ▼
      Bottom Interaction Surface
```

当前冻结的产品原则：

1. 页面承载长期和复杂任务，临时反馈优先进入统一底部交互表面。
2. `ui_app_state` 是路由、连接、系统控制、媒体和导航状态的唯一全局真值来源。
3. Touch、Encoder、键盘和最终实体按键只产生 `UI_INPUT_*` 语义事件。
4. UI 不直接伪造外设成功状态，操作经 `ui_command_service` 异步闭环。
5. `Dynamic Card Dock` 与 `Interaction Dock` 共用一个 `Bottom Interaction Surface`，禁止叠加第二个底部弹窗。
6. 所有 LVGL 对象只能在 UI 线程修改；生产者任务只能提交数据、状态或语义事件。
7. PC Mock 和 STM32 Backend 共用上层接口，页面不得依赖 SDL、FatFs 或具体驱动。

---

## 2. 工程目录与核心模块

```text
Lvgl_Project/
├─ assets/                       背景、图标、字体源与资源
├─ docs/                         架构、阶段报告与交接文档
├─ lvgl/                         LVGL 9.3.0-dev 本地源码快照
├─ src/
│  ├─ hal/                       SDL 显示和输入适配
│  ├─ platform/
│  │  ├─ pc/                     PC Mock、PC 历史文件后端
│  │  └─ stm32/                  STM32 服务契约与注册接口
│  ├─ services/                  Command、导航、历史 Repository 等
│  └─ ui/
│     ├─ components/             Dynamic Card、导航、设置列表、Dock
│     ├─ core/                   Router、App State、状态机、Motion、Sound
│     ├─ design/                 Design Token 与公共 Surface Style
│     ├─ fonts/                  MiSans/LVGL 字体资源
│     ├─ models/                 Home、Ride Data、Summary、History 模型
│     ├─ overlays/               Quick Control Center
│     └─ pages/                  页面实现
├─ tests/                        20 组自动化测试
├─ tools/                        字体与资源生成工具
├─ CMakeLists.txt
├─ CMakePresets.json
├─ VERSION
└─ simulator.code-workspace
```

### 2.1 UI Core 已完成模块

| 模块 | 当前职责 | 状态 |
| --- | --- | --- |
| `ui_router` | 路由历史、replace/back、页面生命周期 | 已完成 |
| `ui_state_machine` | 骑行主状态转换 | 已完成 |
| `ui_app_state` | 全局可观察状态唯一真值 | 已完成 |
| `ui_input_queue` | 平台无关输入队列、MOVE 合并、关键输入保留 | 已完成 |
| `ui_event_bus` | 业务事件发布、优先级与去重入口 | 已完成 |
| `ui_command_service` | Pending/Success/Failed/Timeout/Rejected 异步命令 | 已完成 |
| `ui_motion` | 页面、按钮、Dock 和 Reveal 动效入口 | 已完成基础能力 |
| `ui_sound` | 语义音效接口 | 接口预留，真实音频后端未完成 |
| `ui_bottom_surface` | 底部唯一视觉容器、模式/尺寸/挂载 | 已完成 |
| `ui_dock` | Interaction 卡片、优先级、排队、恢复 | 已完成 |
| `ui_dock_controller` | Event 到 Dock 内容映射 | 已完成 |
| `ui_dynamic_card_manager` | 四卡浏览、Snap、Encoder/Touch 控制 | 已完成 |
| `ui_state_adapter` | 外设状态快照进入 UI 线程 | 已完成 |
| `ui_navigation_service` | 导航模型生产者入口 | 已完成 |
| `ui_ride_history_repository` | 骑行记录持久化事务与后端隔离 | 已完成 |

---

## 3. 页面清单与完成状态

当前 Router 注册路由：

```text
UI_ROUTE_BOOT
UI_ROUTE_HOME
UI_ROUTE_RIDE_DASHBOARD
UI_ROUTE_NAVIGATION
UI_ROUTE_RIDE_SUMMARY
UI_ROUTE_HISTORY
UI_ROUTE_SETTINGS
UI_ROUTE_PROFILE
UI_ROUTE_SYSTEM
UI_ROUTE_POWER
UI_ROUTE_POWER_OFF
```

| 页面 | 当前角色 | 状态 | 关键边界 |
| --- | --- | --- | --- |
| Boot | 启动页，约 1.6 s 后进入 Home | 已完成 | 真机启动资源初始化尚未接入 |
| Home / Ride Hub | 非骑行根页面和产品入口 | 已完成，PC 可演示 | 用户头像和部分视觉资源仍为 Mock |
| Ride Dashboard | 骑行准备与正式骑行主页面 | 已完成并冻结主要视觉 | 数据来自 PC Mock，真机传感器未接入 |
| Navigation V2 | 地图、1×3 数据条、固定导航指令坞 | 已完成，PC 可演示 | 地图仍为 Mock 几何，不是真实地图 |
| Ride Summary | 本次骑行总结与保存结果 | 已完成 | 数据真实来自本次 PC 模拟遥测 |
| History | 汇总与最近 50 条列表 | 已完成，PC 持久化 | 单次详情、图表、GPX/FIT 未实现 |
| Settings | 系统控制列表 | 已完成，PC 可演示 | 真实无线/背光/音频驱动未接入 |
| System | 设备和系统只读信息 | 已完成基础 UI | 真机型号、TF 容量、序列号仍为 PC Mock |
| Power | 关机、重启、取消与确认 | 已完成，PC 可演示 | 真机电源管理未接入 |
| Power Off | 关机状态屏 | 已完成，PC 可演示 | 真机待机/断电行为未接入 |
| Profile | 早期个人主页页面仍保留 | 遗留兼容 | 当前主产品入口已由 Home 替代，普通导航不可达 |
| Achievements | Home 入口存在 | 未实现 | 点击后当前故意不弹占位信息，不污染 Bottom Surface |

> 注意：旧 `PHASE_6C_NAVIGATION_REPORT.md` 描述的是早期“地图 + 2×2 数据卡”阶段。当前源码已经升级为 Navigation V2，本交接文档与当前源码优先级更高。

---

## 4. 已完成 UI 设计

### 4.1 全局背景和公共状态栏

- 目标尺寸固定为 `480 × 854`，RGB565。
- 页面使用统一不透明背景资源，不把 Home 页面内容当作其他页面背景。
- 页面切换后不透出上一页对象。
- 统一状态栏显示：卫星/GNSS 数量、时间、WiFi、Bluetooth、电池百分比和电池图标。
- 状态栏数据来自 `ui_app_state`，各页面不再持有独立副本。
- 状态栏横向分隔线与主要文字采用高对比色。
- Simulator 右下角启用 LVGL FPS/CPU 性能监视器。

### 4.2 Home / Ride Hub

Home 已替代早期 Profile 成为开机后的根页面，包含：

- 用户身份卡：头像、用户名、简短状态文本；
- 累计骑行主卡：总里程、总时间、总爬升；
- 黄色主操作卡：开始骑行；
- 深色次操作卡：开始导航；
- 最近一次骑行卡：日期、距离、用时、爬升和路线预览；
- 历史记录入口；
- 我的成就入口；
- 系统设置入口。

交互状态：

- 卡片完整区域可点击，不局限于文字或图标；
- Touch 有按压反馈；
- Encoder 可移动焦点并执行；
- Home 默认不显示 Bottom Interaction Surface；
- 历史数据由 Repository 聚合后更新 Home，不再使用固定累计数字；
- Achievements 仍未开发，当前点击无动作，也不会弹出无意义占位 Dock。

### 4.3 Ride Dashboard

Dashboard 已确认并冻结的区域：

- 顶部统一状态栏；
- 实时速度区域；
- 黄色速度条；
- 2×3 核心数据区；
- 当前背景资源；
- MiSans 字体与既有尺寸比例。

2×3 数据包含当前骑行核心指标，例如心率、功率、踏频、坡度、里程和骑行时间。页面底部旧导航栏、状态条和多余装饰已被 Dynamic Card Dock 替代。

后续不得在无新产品决策的情况下重构上述冻结区域，只允许接入真实数据、修复兼容问题和做可量化的性能优化。

### 4.4 Dynamic Card Dock

Dynamic Card Dock 是 Dashboard 底部 `Bottom Interaction Surface` 的 Normal Content Mode，不是独立弹窗。

当前四张独立 LVGL Card：

1. `Extended Ride Data`：平均速度、最大速度、当前海拔、累计爬升、卡路里、温度、心率区间、圈数；
2. `Navigation`：下一转向、距离、道路、剩余里程、ETA、剩余爬升；
3. `Music`：曲目、歌手、进度、上一首、播放/暂停、下一首和播放模式；
4. `Weather`：温度、天气、风速、降雨概率、空气质量。

实现约束与现状：

- 四张 Card 尺寸完全一致；
- Viewport 仅纵向滚动；
- 使用 LVGL Vertical Scroll + Center Snap + `LV_OBJ_FLAG_SCROLL_ONE`；
- Card 间距为 0，一屏一卡；
- Scroll End 后根据视口中心计算 `active_card`；
- 左侧四点 Indicator 是固定 Overlay，不参与滚动；
- Touch 与 Encoder 共用同一份 `active_card`；
- Encoder 使用 `lv_obj_scroll_to_view(..., LV_ANIM_ON)` 平滑切卡；
- Dock 内 Touch Down 后到 Release 全程拥有手势，不会中途触发全局下拉或上滑。

Encoder 状态机：

```text
CARD_BROWSER
  Rotate -> 上一张/下一张 Card
  Press  -> CARD_CONTROL / Neutral Focus

CARD_CONTROL + Neutral Focus
  Rotate -> 进入当前 Card 内部控件焦点
  Press  -> 退出到 CARD_BROWSER

CARD_CONTROL + Focused Control
  Rotate -> 移动焦点
  Press  -> 执行
  BACK   -> CARD_BROWSER
```

Music Card 已完成真实 UI 交互：

- 上一首、播放/暂停、下一首和播放模式整块可点击；
- 点击有缩放、黄色边框、背景和阴影反馈；
- Encoder 焦点顺序严格为：上一首 → 播放/暂停 → 下一首 → 播放模式；
- 播放模式循环：顺序播放 → 单曲循环 → 随机播放；
- UI 操作提交 `UI_COMMAND_MEDIA_*`，PC Mock 更新全局媒体状态；
- 当前并未播放真实音频，真实播放器/蓝牙媒体后端仍未接入。

### 4.5 Bottom Interaction Surface / Interaction Dock

当前底部只有一个视觉容器：

```text
Bottom Interaction Surface
├─ Dynamic Card Mode
└─ Interaction Mode
   ├─ INFO
   ├─ ACTION
   ├─ CONFIRM
   ├─ CONTROL
   ├─ NAVIGATION
   └─ MEDIA
```

已经完成：

- `MINI / NORMAL / EXPANDED` 尺寸状态；
- `P0 Critical` 到 `P4 Normal` 优先级；
- show、hide、update、replace；
- Priority Override、排队和 Restore；
- Dynamic Card 状态、滚动位置与焦点恢复；
- Pending、Success、Failed、Timeout、Rejected 反馈；
- 页面 Slot 挂载与 Router 生命周期；
- Interaction 退出后不闪现底层 Dynamic Card；
- 无 Dynamic Card 页面退出 Interaction 时直接隐藏，不显示空黄色外框；
- 图标统一放大和居中，标题/关键信息提高字号；
- Navigation Interaction 使用结构化方向值和大号黄色方向箭头。

必须长期遵守：

> 内容变，不换容器；状态变，不叠弹窗；事件结束，恢复原上下文。

禁止重新创建独立的底部 Popup、第二个 Interaction Dock 或与 Dynamic Card 叠加的浮层。

### 4.6 Quick Control Center

Control Center 是唯一的全屏系统快捷控制覆盖层，不承载骑行开始、暂停、恢复和结束业务。

当前布局：

- 第一层：WiFi、Bluetooth 两张等尺寸连接卡；
- 第二层左侧：Sound、Display 两个竖向滑条；
- 第二层右侧：GPS、Power Save 两张功能卡；
- 第三层：Touch Lock 和只读 Battery Status。

已经完成：

- 整张功能卡均可点击；
- WiFi/Bluetooth 区分 Disabled、Enabled/Waiting、Connected；
- WiFi 名称单行省略，避免溢出；
- GPS 根据卫星数显示无信号、一般或良好；
- 声音/亮度滑动时本地预览，Release 时只提交一次命令；
- Encoder 可循环移动焦点；
- 在滑条上 Press 进入调节，Rotate 以 5% 步长变化，再 Press 退出；
- Pending/Failed 状态在对应卡片原位显示；
- 下拉和上推只改变 Overlay 的 Y 坐标，不对 480×854 根对象做缩放或透明动画；
- Touch Lock 阻止普通触摸序列，但保留实体/PC 专用菜单键入口。

### 4.7 Navigation V2

当前 Navigation 页面结构：

```text
统一状态栏
500 px 地图区
92 px 1×3 数据条
188 px 固定 Navigation Command Dock
```

地图区域：

- 当前道路与路线由 LVGL 自绘 Mock 几何构成；
- 支持鼠标拖动和轻量惯性；
- 支持缩放、North Up/Heading Up、回中；
- 比例尺、指南针、放大、缩小和定位 HUD 固定在地图上层；
- 地图缩放采用坐标级变换，不整体 transform 大对象，避免近 0.9 MiB 中间图层申请导致卡死；
- Touch Down 在地图内后，本次序列由地图持有，不触发全局手势。

1×3 数据条：

- 实时速度：数值与 `km/h` 同行居中；
- 剩余里程：数值与 `km` 同行居中；
- 预计到达：`HH:MM` 居中；
- 使用专用 40 px 数字字体、16 px 单位字体和 20 px 标题字体；
- 已修复数字和单位裁切、基线不齐与列宽不足问题。

固定 Navigation Command Dock：

- 尺寸 `460 × 188`；
- 左侧约 40% 为大号粗体黄色矢量箭头/状态图标；
- 右侧约 60% 为“前方、距离、单位、动作、道路名”；
- 距离、单位和转向动作在同一视觉行中；
- 支持直行、左/右转、轻微转弯、急转、掉头、环岛和到达；
- 支持 `CRUISE / APPROACH / IMMINENT / NOW / REROUTING / GPS_WEAK / ARRIVED` 状态；
- 状态变化使用图标缩放和黄色强调条动画，不创建大面积离屏图层；
- Navigation 页面可见时直接更新固定 Command Dock，不再额外弹出导航 Interaction；
- 页面不可见时，方向变化才发布 P1 导航安全事件，临时抢占当前页面的 Bottom Surface，超时后恢复原内容。

PC 开发态可按 `N` 在 10 组导航状态之间循环验证。

真实地图边界：

- 当前不是西安市真实地图，也不是在线地图；
- LVGL 只负责显示、裁切和控件，不提供地图数据、地名搜索或路径规划；
- 已设计 TF 卡离线瓦片方案，但 `navpack`、投影、3×3 Tile Cache、JPEG 解码和真实路线引擎尚未实现。

### 4.8 Settings / System / Power

共享 `ui_settings_list` 已统一：

- List、Action、Toggle、Slider、Selection 和 Info Row；
- 整行 Touch 热区；
- Press/Focus 反馈；
- Encoder 上下选择、按下执行、自动滚入可视区。

Settings 当前包含：

- WiFi、Bluetooth、GPS；
- Sound、Brightness；
- Power Save、Touch Lock；
- System Information；
- Power Settings。

System 当前展示设备型号、UI 版本、LVGL 版本、存储设备、目标平台和系统健康。PC 无法提供的真机信息明确标识为 Mock，不伪装为真实硬件数据。

Power 使用同一 Bottom Surface 显示关机/重启 `CONFIRM`；YES 执行，NO/BACK 取消并恢复页面，不创建独立弹窗。

### 4.9 Ride Summary

Summary 已由真实 `ui_ride_summary_model` 驱动，包含：

- `RIDE COMPLETE` 标题；
- 黄色描边主距离卡；
- 骑行时间、平均速度、最大速度、累计爬升；
- 平均心率、平均功率、平均踏频、保存状态；
- 单一黄色完成按钮。

数据行为：

- `GPS_READY -> RIDING` 时记录本次骑行基线；
- 只在 RIDING 状态累计有效统计；
- 暂停期间不重复采样；
- 保存成功后冻结快照并标记 `SAVED`；
- 保存失败不进入 Summary，保留待重试记录；
- 完成后返回 Home，并更新 Home 累计数据和最近骑行。

动效：主距离 Count-up、主卡/指标卡/完成按钮 Stagger Reveal。已修复首次布局未完成时读取错误坐标导致对象堆叠的问题。

### 4.10 History 与 PC 持久化

History V1 已完成：

- 汇总骑行次数、累计距离、累计时间；
- 空状态；
- 最新优先列表；
- 每行显示日期、距离、时长、平均速度和爬升；
- Touch 滚动、整行点击和 Encoder 焦点滚动；
- Repository 保留最近 100 条，页面最多创建最近 50 条以控制 LVGL 对象数量。

PC 文件后端：

```text
runtime/ride_history.dat
runtime/ride_history.dat.tmp
runtime/ride_history.dat.bak
```

已实现：

- 显式 little-endian 二进制格式；
- 格式版本和 Header；
- CRC32；
- 临时文件提交；
- 原文件备份；
- 主文件损坏/缺失时校验并恢复备份；
- 未知格式版本拒绝加载；
- 记录 ID 去重、最近 100 条淘汰；
- 失败不污染内存模型，Pending 可重试。

STM32 仅预留 `ui_stm32_services_register_history_storage()`，FatFs/TF 卡、RTC、掉电和拔卡场景尚未实现。

---

## 5. 骑行状态机与完整业务流程

### 5.1 状态定义

```text
IDLE
HOME
PRE_RIDE
GPS_READY
RIDING
PAUSED
FINISHING
SUMMARY
```

### 5.2 主流程

```text
BOOT_COMPLETE
  -> HOME

START_REQUEST
  -> PRE_RIDE
  -> Dashboard
  -> GPS SEARCHING

GPS_READY
  -> GPS_READY 状态
  -> Bottom Surface 原位显示 GO

GO + UI_COMMAND_RIDE_START SUCCESS
  -> RIDING
  -> 启动计时/距离/统计
  -> 恢复 Dynamic Card

PAUSE / RESUME
  -> 异步命令
  -> PAUSED / RIDING

FINISH_REQUEST
  -> FINISHING
  -> Bottom Surface 原位确认

NO / BACK
  -> 恢复结束前 RIDING 或 PAUSED

YES
  -> Finish Command
  -> Save Command
  -> Repository 持久化
  -> SUMMARY

SUMMARY DISMISSED
  -> HOME
```

关键规则：

- 点击“开始骑行”不会立刻开始计时；
- 只有 GO 命令成功才进入 RIDING；
- 保存成功前不更新 Home 聚合，不把 Summary 标为 SAVED；
- 结束确认、保存和错误都刷新同一个 Bottom Surface；
- 结束流程取消后恢复此前 Dynamic Card 和骑行状态；
- 骑行域中页面主链固定为 Dashboard ↔ Navigation，不允许普通横滑进入 Home。

### 5.3 暂停与返回键

| 操作 | RIDING | PAUSED | 其他页面 |
| --- | --- | --- | --- |
| 单击返回 | 返回上一级；结束确认中取消确认 | 同左 | Router Back |
| 双击返回 | 手动暂停 | 手动恢复 | 无骑行控制动作 |
| 长按返回约 1.2 s | 打开结束确认 | 打开结束确认 | 无骑行控制动作 |

为了区分单击和双击，单击在释放后约等待 280 ms 才发出。长按成立后不会再产生单击或双击。

### 5.4 自动暂停

当前原型规则：

- 速度持续不高于 `1.0 km/h` 达 3 s：自动暂停；
- 自动暂停后速度持续不低于 `3.0 km/h` 达 1 s：自动恢复；
- 手动暂停不会被速度变化自动恢复；
- 暂停和恢复仍通过异步 Command 闭环。

这些阈值仅为 PC 原型参数。真机必须结合轮速传感器、GNSS 抖动、隧道和高楼环境重新标定。

---

## 6. 手势与 Gesture Ownership

### 6.1 全局触摸手势

- 左键按下、移动、释放模拟触屏完整序列；
- 页面外部区域下滑：跟手拉出 Control Center；
- Control Center 内上滑：跟手收起并恢复之前页面；
- 非骑行态横滑：Home ↔ Navigation；
- 骑行态横滑：Dashboard ↔ Navigation；
- 页面外部区域上滑超过阈值：返回当前域根页面；
- 全局页面滑动判定阈值当前为约 72 px。

### 6.2 所有权规则

Touch Down 时确定所有权，并锁定到 Release/Cancel：

- 起点在 Dynamic Card Viewport 内：由 Dynamic Card 消费；
- 起点在 Navigation Map 内：由 Map 消费；
- 起点在 Control Center Slider 内：由 Slider 消费；
- 即使指针拖出原区域，也不会中途交给全局手势；
- Touch Lock 打开后普通触摸序列被阻止。

此规则用于防止 Card 内上下滚动时误拉出 Control Center，以及地图拖动时误切页。

---

## 7. PC Simulator 操作绑定

### 7.1 常用绑定

| PC 输入 | 语义事件/行为 |
| --- | --- |
| 鼠标左键按下/拖动/释放 | Touch Down/Move/Up；点击、拖卡、拖地图、拖滑条 |
| 鼠标右键单击 | 返回；约 280 ms 后确认单击 |
| 鼠标右键双击 | 骑行中手动暂停/恢复 |
| 鼠标右键长按约 1.2 s | 骑行中打开结束骑行确认 |
| 鼠标滚轮向下 | `UI_INPUT_ENCODER_CW` |
| 鼠标滚轮向上 | `UI_INPUT_ENCODER_CCW` |
| `Down` | Encoder CW |
| `Up` | Encoder CCW |
| `Enter` / 小键盘 Enter | Encoder Press |
| `Esc` / `Backspace` 单击 | 返回，使用相同单击延迟 |
| `Esc` / `Backspace` 双击 | 手动暂停/恢复 |
| `Esc` / `Backspace` 长按 | 结束骑行确认 |
| `Left` | 页面上一页 |
| `Right` | 页面下一页 |

### 7.2 开发调试快捷键

| 按键 | 行为 | 备注 |
| --- | --- | --- |
| `M` | 打开/关闭 Control Center | Touch Lock 后仍可使用 |
| `O` | 打开 Power 页面 | PC 调试入口 |
| `P` | 立即切换 Power Off/开机 | 绕过电源确认，仅用于调试 |
| `[` | 返回 Home | 代码枚举名仍是 OPEN_PROFILE，但实际行为已改为 Home |
| `]` | 打开 Settings | PC 调试入口 |
| `S` | 切换“移动/停止”模拟 | 用于验证自动暂停/恢复 |
| `N` | Navigation V2 下一演示状态 | 仅导航页有效，共 10 个状态 |

### 7.3 页面内 Encoder 行为

- Home：移动卡片焦点，Press 执行；
- Dashboard：浏览 Dynamic Card，Press 进入 Control；
- Music Card：按视觉顺序选择四个控制并执行；
- Navigation：CW/CCW 分别缩放，Press 回中；
- Control Center：移动焦点，滑条进入/退出调节；
- Settings/System/Power：移动列表焦点并执行整行；
- History：焦点浏览并自动滚动；
- Summary：焦点保持在完成按钮，Press 返回 Home；
- Interaction Dock：按内容类型处理 Confirm、Control、Action、Navigation 和 Media。

---

## 8. 真机输入与平台解耦

UI 层只接收：

```c
UI_INPUT_ENCODER_CW
UI_INPUT_ENCODER_CCW
UI_INPUT_ENCODER_PRESS
UI_INPUT_BACK
UI_INPUT_BACK_DOUBLE
UI_INPUT_BACK_LONG
UI_INPUT_PAGE_PREV
UI_INPUT_PAGE_NEXT
UI_INPUT_TOGGLE_CONTROL_CENTER
UI_INPUT_TOUCH_BEGIN
UI_INPUT_TOUCH_MOVE
UI_INPUT_TOUCH_END
UI_INPUT_TOUCH_CANCEL
```

STM32 适配要求：

- GPIO/Encoder/Touch 驱动不得直接调用页面函数；
- ISR 只记录状态或通知 RTOS 任务；
- 任务调用 `ui_stm32_services_submit_input()` 等提交接口；
- App State 通知和 LVGL 对象修改只能在 UI 线程执行；
- 实体返回键应在平台层生成单击、双击、长按语义，或复用同一识别器；
- 真机必须保留 Touch Lock 状态下可用的物理解锁/菜单入口。

---

## 9. 数据、命令与服务逻辑

### 9.1 App State 统一状态域

- Route；
- Ride State；
- 时间、电量、充电、卫星数；
- WiFi、Bluetooth、GPS 及连接名称；
- 声音、亮度、省电、触摸锁；
- 媒体播放、曲目、播放模式；
- Navigation Active。

值没有变化时不会增加 version，也不会重复通知。订阅回调在内部锁释放后执行，避免回调再次写状态造成递归锁死。

### 9.2 Command Service

当前命令覆盖：

- WiFi、Bluetooth、GNSS 开关与定位；
- Power Save、Touch Lock；
- Sound、Brightness；
- Media 上一首、播放/暂停、下一首、模式；
- Ride Start、Pause、Resume、Finish、Save、Upload。

命令状态：

```text
PENDING
SUCCESS
FAILED
TIMEOUT
REJECTED
```

UI 只在权威成功回执后更新最终状态，避免开关看似改变但真实外设失败。

### 9.3 服务适配和线程边界

```text
GNSS / Sensors / BLE / WiFi / Storage / SDL
                       │
                       ▼
        producer-safe mailbox / input queue
                       │
                       ▼
                 20 ms UI Tick
                       │
                       ▼
       App State / Models / Event Bus / Pages
```

- 连续 Touch Move 会合并为最新坐标；
- 队列满时优先保留 Touch End、Cancel 和 Back；
- Navigation 页面隐藏时只保存最新模型，不刷新隐藏对象；
- 页面重新进入后立即应用最新快照；
- Router 生命周期暂停隐藏页面的高频 Timer。

---

## 10. Design System、字体与动效

### 10.1 Design Token

当前已建立颜色、字体、间距、圆角、边框、透明度、焦点和 Surface 语义 Token。新页面应优先复用 `src/ui/design/` 与 `ui_theme.h`，禁止继续散落无管理硬编码。

### 10.2 字体

当前字体体系包含：

- Dashboard 专用中文、数字、单位和速度字体；
- 通用 MiSans UI 20 px；
- Bottom Surface 中文强调 MiSans 24 px；
- Navigation 40 px 数字、16 px 单位、32 px 转向动作字体；
- Montserrat 仅用于其覆盖范围内的英文、数字和 LVGL Symbol。

已建立：

- `tools/generate_ui_font.py` 自动收集运行时文案码点；
- CMake 字体生成依赖；
- `ui_font_runtime_codepoints.h`；
- 全运行时字符覆盖测试。

新增中文文案时必须重新生成子集并运行 `ui_font_coverage_test`，不得随意把中文放进 ASCII-only Montserrat，否则会再次出现方框字。

### 10.3 动效与内存约束

- 按钮采用小对象 scale、边框、颜色和阴影反馈；
- 页面和 Control Center 主要使用位置动画；
- Bottom Surface 使用位置与宽高几何形变，不缩放整个大容器；
- 禁止对 480×854 根对象或大型 Dock Host 使用整体透明/缩放动画；
- 禁止对大地图对象使用 LVGL transform zoom；
- 原因：ARGB8888 中间图层可能一次消耗数百 KiB，而当前 `LV_MEM_SIZE` 仅 1 MiB。

---

## 11. 当前构建、性能与自动化测试

### 11.1 构建命令

```powershell
& "D:/Professional Software/Anaconda/Bike_SpeedView/msys2-x86_64/ucrt64/bin/cmake.exe" --preset bike-ui-ucrt64
& "D:/Professional Software/Anaconda/Bike_SpeedView/msys2-x86_64/ucrt64/bin/cmake.exe" --build --preset bike-ui-ucrt64 --parallel
& "D:/Professional Software/Anaconda/Bike_SpeedView/msys2-x86_64/ucrt64/bin/ctest.exe" --preset bike-ui-ucrt64 --output-on-failure
```

运行：

```powershell
$env:PATH = "D:\Professional Software\Anaconda\Bike_SpeedView\msys2-x86_64\ucrt64\bin;D:\Professional Software\Anaconda\Bike_SpeedView\msys2-x86_64\usr\bin;$env:PATH"
.\bin\main.exe
```

VS Code 推荐直接打开 `simulator.code-workspace`，再按 `Ctrl+F5`。如果没有注入 UCRT64 DLL 路径，`main.exe` 可能在窗口创建前直接退出。

### 11.2 2026-09-18 自动化结果

```text
20/20 PASS
Total Test Time: 1.20 s
```

通过的测试：

1. `ui_font_coverage_test`
2. `ui_app_state_test`
3. `ui_home_data_model_test`
4. `ui_back_key_gesture_test`
5. `ui_auto_pause_test`
6. `ui_core_logic_test`
7. `ui_dock_view_test`
8. `ui_control_center_test`
9. `ui_navigation_test`
10. `ui_router_lifecycle_test`
11. `ui_service_adapter_test`
12. `ui_command_service_test`
13. `ui_bottom_surface_test`
14. `ui_settings_list_test`
15. `ui_ride_summary_model_test`
16. `ui_ride_history_model_test`
17. `ui_ride_history_repository_test`
18. `ui_pc_ride_history_storage_test`
19. `ui_history_page_test`
20. `ui_summary_page_test`

测试覆盖包括字体、Router 生命周期、输入手势、自动暂停、Command、Bottom Surface、Control Center、Navigation、Summary、History 和持久化损坏恢复。

### 11.3 性能基线

历史阶段 PC 基准约为：

```text
fps_avg ≈ 59
fps_min ≈ 58~59
cpu_avg ≈ 0~1%
cpu_max 通常 ≈ 1~3%
```

该数据只代表 Windows PC Simulator，不代表 STM32 性能。发布前仍需进行长时间页面切换、Dock 抢占、地图拖动、骑行数据刷新和存储压力测试。

当前关键配置：

- `LV_MEM_SIZE = 1 MiB`；
- `LV_COLOR_DEPTH = 16`；
- `LV_USE_PERF_MONITOR = 1`；
- `LV_USE_MEM_MONITOR = 0`；
- LVGL Assert 当前关闭。

正式 RC 前建议在专用 Debug 配置中打开必要 Assert 和内存监控，再执行压力测试；不能只依赖当前 Release 风格运行结果。

---

## 12. 当前已知问题、风险和未完成项

### 12.1 发布与版本管理

- v1.1.1 已形成可追踪发布提交并创建同名 Git tag；
- `VERSION`、README、CHANGELOG 和系统页面显示已经同步；
- 发布范围限定为 `Software/Lvgl_Project`，仓库内其他硬件资料不属于本次 UI 发布提交。

### 12.2 页面和产品功能

- Achievements 未实现；
- Legacy Profile 仍在代码中，但不再是主入口；
- History 没有 Ride Detail、筛选、删除、导出和趋势图；
- Home 头像、累计卡装饰和路线缩略图仍为本地 Mock；
- 上传骑行只有命令链路，没有真实网络服务；
- Sound 语义接口存在，真实音效文件和硬件播放链路未完成。

### 12.3 导航与地图

- 当前地图只是 Mock 几何；
- 西安市 TF 离线地图包、合法数据来源和署名尚未落地；
- `navpack` 格式、索引、投影、瓦片读取和缓存未实现；
- 真实路径规划、偏航重规划和道路匹配未实现；
- U-turn 及复杂道路语义需要在真实数据接入后继续验证；
- PC 能验证共用地图核心，但不能替代 SDMMC、JPEG、DMA2D、SDRAM、D-Cache 和真实 GNSS 的真机测试。

### 12.4 真机后端

- STM32 工程尚未在本仓库完成导入和联调；
- LTDC、SDRAM、DMA2D、Touch、Encoder、实体按键未完成硬件验收；
- WiFi、BLE、GNSS、电量、背光、音量仍由 PC Mock 驱动；
- TF/FatFs 历史存储和突然掉电恢复未实现；
- RTC、时区、设备序列号、固件版本和真实存储容量未接入；
- Touch Lock 必须在真机保留可靠物理解锁路径。

### 12.5 资源与性能

- 新增 24 px 中文强调字体约增加 88 KiB 资源成本；
- 多套 MiSans 子集需要纳入 MCU Flash/外部资源预算；
- 当前 LVGL 通用 Heap 为 1 MiB，必须继续避免大型离屏图层；
- History 页面为了内存只创建最近 50 条，不能直接无限增加对象；
- 真机需要重新测量 LTDC 帧率、SDRAM 带宽、对象峰值、Timer、Animation 和缓存占用。

---

## 13. 后续完整开发路线

### Phase 7A：冻结并发布 v1.1.1 基线（已完成）

目标：让当前功能状态可复现、可回退、可交接。

任务：

1. 已审查当前全部 modified/untracked 文件；
2. 已限定发布范围，未混入仓库根目录的硬件表格；
3. 已更新 `VERSION` 为 `1.1.1`；
4. 已更新 `README.md` 当前版本和 Navigation V2 说明；
5. 已将 `CHANGELOG.md` 收敛为 v1.1.1 发布记录；
6. 已重新 configure/build/test；
7. 已完成 Simulator 启动冒烟验收：进程运行 10 秒保持存活，未出现 `No memory`、异常退出或启动错误；
8. 已提交并创建 `v1.1.1` Git tag。

冻结实测依据：全新 CMake 配置与构建通过、20/20 自动化测试通过、Simulator 启动冒烟无崩溃和 `No memory`、版本元数据一致。受当前自动化环境无法控制原生 SDL 窗口的限制，完整 Home → 骑行 → 暂停 → 结束 → Summary → History 人工流程仍需在 Phase 7B 视觉 RC 中由可见 Simulator 补验；该限制不应被误写为已完成真机或完整人工验收。

### Phase 7B：PC 产品闭环与视觉 RC

目标：在不依赖真实硬件的前提下，把所有核心 UI 流程做成可稳定验收的 RC。

任务：

- 全页面 480×854 截图回归；
- 字体、裁切、单位基线、状态栏和 Surface 一致性复查；
- 完整流程自动化/手工回归；
- GPS Failed、Command Timeout、Storage Error、Low Battery 等异常流程；
- 30~60 分钟页面切换与 Dock 抢占压力测试；
- 记录 Heap、对象数、Timer、Animation、FPS 和 CPU 峰值；
- 完成 Achievements 的产品决策：实现轻量页或删除入口；
- 决定是否删除 Legacy Profile，避免重复信息架构。

### Phase 7C：Ride Detail 与骑行数据文件

目标：完成 History V2，但不把高频采样塞进 100 条索引文件。

任务：

- 定义按 record ID 加载的 Ride Detail 数据契约；
- 概览、分段、心率、功率、踏频、海拔和路线摘要；
- 独立扩展文件保存高频采样；
- GPX/FIT 导入导出策略；
- 删除、筛选和存储空间管理；
- 文件格式版本迁移与错误恢复测试。

### Phase 7D：西安离线地图 MVP

目标：在 PC Simulator 上先验证与真机共用的地图核心。

任务：

1. 确定合法地图数据来源、授权和署名；
2. 生成西安小范围、三个整数 zoom 的样包；
3. 定义带版本和 CRC 的 `xian.navpack`；
4. 实现 PC STDIO Tile Provider；
5. 实现 WGS-84/Web Mercator 投影固定测试向量；
6. 实现固定复用的 3×3 Tile View 和 LRU Cache；
7. 实现缺瓦片占位、异步请求合并和整数级缩放；
8. 叠加路线 polyline、GNSS 回放和偏航状态；
9. 固定坐标截图 Golden Test。

第一版建议由手机/云端计算路线，经 BLE 下发 polyline、转向点、道路名、剩余距离和 ETA；不建议立即在 STM32 上实现完整离线路径规划。

### Phase 8A：STM32 显示与输入 Bring-up

顺序：

```text
LTDC
-> SDRAM
-> LVGL Display Buffer
-> DMA2D
-> Touch
-> Encoder
-> 返回/菜单等实体键
```

验收：

- 480×854 RGB565 正确显示；
- Cache Clean/Invalidate 正确；
- 页面和地图无撕裂、花屏、越界；
- Touch 坐标校准和手势所有权一致；
- Encoder 与 PC 语义一致；
- 返回键单击/双击/长按行为一致；
- 稳定帧率和可接受输入延迟。

### Phase 8B：真实系统服务

按风险从低到高接入：

- 电池、电量和充电状态；
- 背光与亮度；
- 音量和提示音；
- GNSS 和卫星数；
- 轮速、踏频、心率、功率；
- WiFi/Bluetooth/ESP32；
- Touch Lock 和 Power Save；
- 关机、重启和低功耗。

所有后端只实现现有 Command/State Adapter 契约，不允许页面直接调用驱动。

### Phase 8C：TF 历史存储与地图瓦片

任务：

- FatFs/SDMMC 后端实现；
- History Storage Adapter；
- RTC/UTC/时区；
- TF 拔出、写保护、满盘、坏文件和低速卡错误映射；
- 突然掉电和备份恢复；
- Hardware JPEG + DMA2D RGB565；
- SDRAM Tile Cache 和 D-Cache 一致性；
- Tile Load Latency、Cache Hit、FPS 和峰值内存统计。

### Phase 9：系统级联调与 RC

完整场景：

```text
Boot
-> Home
-> Start Ride
-> GPS Searching / Ready
-> GO
-> Riding
-> Dynamic Card / Music
-> Navigation Safety Override
-> Restore
-> Auto Pause / Manual Pause
-> Low Battery / Sensor Lost
-> End Confirm / Cancel / Confirm
-> Save to TF
-> Summary
-> History
-> Reboot and Reload
```

必须完成：

- 8 小时以上稳定性测试；
- 断电、拔卡、弱 GNSS、无线失败和传感器掉线；
- Heap/SDRAM 泄漏和碎片检查；
- 任务优先级、队列拥塞和 UI 线程阻塞检查；
- 户外阳光、手套触摸、雨滴误触和骑行震动场景；
- 功耗、热量和电池续航评估。

### Phase 10：v1.2 / v2 产品规划

v1.2 可考虑：

- Ride Detail；
- 西安离线地图 MVP；
- 真机基本服务；
- 更完整异常和存储管理。

v2.0 仅在出现不兼容的信息架构或整套 UI 重构时使用，例如账号体系、插件系统、完整训练平台或全新页面导航。普通功能增加不应直接提升到 v2。

---

## 14. 下一位开发者的首轮工作建议

不要立即增加新页面。建议按以下顺序接手：

1. 阅读本文件、`UI_V4_交接文档.md` 和 Phase 6A~6F 报告；
2. 检查 `git status`，确认当前未提交改动的归属；
3. 按本文命令完成 configure/build/20 项测试；
4. 手工跑一遍 Home → Ride → Pause → End → Save → Summary → History；
5. 在 Navigation 按 `N` 检查 10 个 Command Dock 状态；
6. 验证 Dynamic Card Touch Snap、Music 四个控件和 Control Center；
7. 修正版本元数据并形成 v1.1.1 可追踪提交；
8. 再开始 Phase 7B 或离线地图 MVP。

---

## 15. 不得破坏的架构与视觉约束

1. 不得重构 Dashboard 已冻结的状态栏、速度区、黄色速度条、2×3 数据区、背景和字体比例。
2. 不得重新引入底部导航栏。
3. 不得把 Dynamic Card 与 Interaction Dock 做成两个视觉容器。
4. 不得使用独立 Popup 覆盖 Bottom Interaction Surface。
5. 不得让页面直接依赖 SDL 或 STM32 Driver。
6. 不得由外设任务直接修改 LVGL 对象。
7. 不得用整张截图替代真实 LVGL 控件和独立 Card。
8. 不得对大对象使用高内存 transform/opacity 动画。
9. 不得在新增中文后跳过字体子集和覆盖测试。
10. 不得把 PC Mock 数据描述为真实硬件能力。
11. 不得在未验证 TF、SDRAM、DMA2D 和 GNSS 前宣称真机地图完成。
12. 不得把未提交的工作区状态当作正式发布版本。

---

## 16. v1.1.1 交接验收清单

### UI 与交互

- [x] Home / Ride Hub 完成
- [x] Dashboard 冻结区保持不变
- [x] 四张 Dynamic Card 独立对象
- [x] Vertical Scroll + Snap + One Card Per Scroll
- [x] Music Card Touch/Encoder 控制
- [x] Bottom Interaction Surface 单一容器
- [x] INFO/ACTION/CONFIRM/CONTROL/NAVIGATION/MEDIA
- [x] Control Center 系统快捷控制
- [x] Navigation V2 页面和固定 Command Dock
- [x] Settings/System/Power
- [x] Ride Summary
- [x] History V1 与 PC 持久化
- [ ] Achievements
- [ ] Ride Detail
- [ ] 真实地图

### 骑行逻辑

- [x] Boot → Home
- [x] Start Ride → GPS Searching → GPS Ready → GO
- [x] Ride Start 异步成功后进入 RIDING
- [x] 返回键单击/双击/长按
- [x] 手动暂停/恢复
- [x] PC 自动暂停/恢复原型
- [x] 结束确认 YES/NO/BACK
- [x] Save 成功后进入 Summary
- [x] Home 和 History 聚合刷新
- [ ] 真机传感器和阈值标定

### 输入与平台

- [x] SDL 输入语义化
- [x] Gesture Ownership
- [x] Encoder 语义输入
- [x] STM32 提交接口预留
- [ ] STM32 Touch/Encoder/按键联调
- [ ] 真机 Touch Lock 解锁方案验收

### 质量

- [x] 20/20 自动化测试通过
- [x] 字体运行时码点覆盖测试
- [x] 大图层内存回归测试
- [x] History 损坏和备份恢复测试
- [ ] 长时间 PC 压力测试
- [ ] STM32 性能和内存验收
- [x] v1.1.1 版本元数据、提交与 Git tag

---

## 17. 相关文档

- `UI_V4_交接文档.md`：产品与 UX 总路线；
- `PHASE_6A_HOME_RIDE_HUB_REPORT.md`：Home 与骑行前流程；
- `PHASE_6B_CONTROL_CENTER_REPORT.md`：快捷控制中心；
- `PHASE_6C_NAVIGATION_REPORT.md`：导航早期阶段记录，当前布局以源码和本文为准；
- `PHASE_6D_SETTINGS_SYSTEM_POWER_REPORT.md`：设置、系统和电源；
- `PHASE_6E_RIDE_SUMMARY_REPORT.md`：骑行总结；
- `PHASE_6F_HISTORY_AND_STORAGE_REPORT.md`：历史和 PC 持久化；
- `RIDE_BACK_KEY_AND_AUTO_PAUSE.md`：返回键与自动暂停；
- `BOTTOM_INTERACTION_SURFACE_ARCHITECTURE.md`：底部唯一视觉容器；
- `UI_APP_STATE_AND_ROUTER_ARCHITECTURE.md`：状态和页面生命周期；
- `UI_COMMAND_ACTION_ARCHITECTURE.md`：命令异步闭环；
- `UI_SERVICE_ADAPTER_ARCHITECTURE.md`：平台服务和线程边界；
- `NAVIGATION_REAL_MAP_ARCHITECTURE.md`：真实地图与 STM32 落地路线。

---

## 18. 最终交接结论

当前工程已经完成从“可展示的 LVGL 页面集合”到“具备 Router、App State、骑行状态机、异步命令、统一 Bottom Interaction Surface、平台语义输入、数据模型和 PC 持久化”的系统化迁移。

现阶段最重要的判断是：

- UI 架构和 PC 交互主链已经基本成立；
- 核心页面已覆盖 Home、Dashboard、Navigation、Summary、History、Settings、System 和 Power；
- Navigation V2 和 Interaction Dock 的当前视觉已进入可继续精修的基础状态；
- 真实地图、真实传感器、无线、音频、TF/FatFs 和电源管理仍属于后端与真机阶段；
- 下一步不应继续无边界扩张 UI，而应先冻结 v1.1.1、形成可追踪版本，再进入 PC RC 和 STM32 Bring-up。

本文件是后续开发的 v1.1.1 交接入口。若旧阶段文档与当前源码、自动化测试或本文冲突，应先核对当前源码，不得直接按旧截图或旧阶段说明回退已经完成的架构。
