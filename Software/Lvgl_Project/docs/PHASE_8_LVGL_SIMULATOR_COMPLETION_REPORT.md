# Phase 8 LVGL Simulator 完整化报告

## 1. 文档信息

| 项目 | 内容 |
| --- | --- |
| 日期 | 2026-09-21 |
| 发布版本 | `v1.2.0` |
| 开发基线 | `v1.1.2` / `ee1dd35` |
| 工作分支 | `feature/lvgl-simulator-complete` |
| LVGL | `9.3.0-dev` |
| 分辨率 | `480 x 854` / RGB565 |
| 本阶段范围 | PC Simulator 的页面、UI、交互、状态机、Mock 数据与持久化闭环 |
| 不在本阶段 | STM32 驱动、真实传感器、TF/FatFs、真实地图、真实音频与联网后端 |

本阶段不修改已经冻结的 Dashboard 顶部状态栏、实时速度、黄色速度条、
2x3 核心数据区、背景资源和 MiSans 视觉比例。

## 2. 页面完成矩阵

| 页面 | 状态 | 主要能力 |
| --- | --- | --- |
| Boot | 完成 | 启动页、自动进入 Home、重启流程 |
| Home / Ride Hub | 完成 | 身份、累计数据、开始骑行、开始导航、最近骑行、历史、成就、设置入口 |
| Ride Dashboard | 完成并冻结 | 骑行主数据、Dynamic Card Dock、状态栏、骑行状态反馈 |
| Navigation V2 | 完成 Simulator UI | 地图几何 Mock、缩放/定位、1x3 数据条、导航指令坞、状态切换 |
| Ride Summary | 完成 | 保存结果、8 项指标、完成返回 Home |
| History | 完成 | 最多 50 条记录、汇总、滚动、Encoder、进入详情 |
| Ride Detail | 完成 | 日期、路线预览、距离/时间/速度/爬升/心率/功率/踏频/记录 ID |
| Profile | 完成 | 用户信息、累计指标、成就/历史/设置入口、Encoder 与 Touch |
| Achievements | 完成 | 6 项成就、实时进度、解锁状态、滚动与 Encoder |
| Settings | 完成 | WiFi、蓝牙、GNSS、声音、亮度、省电、触摸锁、系统、电源 |
| System | 完成 Simulator UI | 设备、UI/LVGL 版本、存储、平台和健康状态 |
| Power | 完成 | 关机、重启、取消与统一确认交互坞 |
| Power Off | 完成 | 关机态与返回/重新启动模拟 |

## 3. 统一交互体系

- Pointer、Encoder、键盘和返回键统一转换为语义输入。
- 鼠标左键模拟 Touch；右键模拟 Back。
- 单击 Back 返回上一级；双击 Back 手动暂停/恢复；长按 Back 请求结束骑行。
- Home、Dashboard、Navigation 支持横向页面切换。
- 顶部下滑打开快捷控制条，再次下滑展开完整控制中心。
- 完整控制中心内直接调节声音和亮度，不再额外弹出底部控制卡。
- Dynamic Card 和 Interaction Dock 共用唯一 Bottom Interaction Surface。
- History、Achievements、Settings、System 的列表触摸序列拥有独立 Gesture Ownership，
  列表滚动不会误触发控制中心或全局上滑返回。
- Dynamic Card、Navigation Map 和 Bottom Surface 继续保持 PRESS 到 RELEASE 全程锁定。

## 4. Dynamic Card 与数据模型

四张独立 LVGL Card 保持真实纵向滚动、Snap 和统一 `active_card`：

1. Extended Ride Data：平均/最大速度、海拔、累计爬升、卡路里、温度、心率区间、圈数。
2. Navigation：转向距离、道路、剩余里程、剩余爬升和 ETA。
3. Music：上一首、播放/暂停、下一首、三种播放模式循环。
4. Weather：温度、天气、骑行建议、风速、降雨概率和 AQI。

Ride、Navigation、Weather、Media 均通过平台无关模型或 App State 更新。
Navigation 的周期 Mock 已从页面移到 `platform/pc`，页面不再包含 PC 定时器。

## 5. 数据与持久化

- PC 服务生成骑行、天气、导航、连接状态和媒体 Mock 数据。
- 骑行结束后生成 Summary，并通过 Repository 保存到
  `runtime/ride_history.dat`。
- Home、Profile、Achievements、History 和 Ride Detail 共享持久化历史聚合结果。
- PC Mock Backend 与 STM32 Backend 使用相同 Command、State、Navigation 和 Input 接口。

## 6. 字体与渲染修复

- MiSans UI 子集当前覆盖 443 glyph / 444 required codepoints。
- Profile 大数字与 `km` 单位拆分，避免数字专用字体缺少字母。
- Achievements 大数字与 `%` 拆分，并移除 Montserrat 不覆盖的中点字符。
- 页面渲染测试现在把 `glyph dsc. not found`、`No memory` 和
  `Allocating layer buffer failed` 全部视为失败。
- 状态栏注册容量从 10 提升到 16，并以 12 个实例做回归验证。

## 7. 验证结果

### 7.1 自动化测试

```text
25/25 CTest passed
5 轮连续回归：125/125 passed
```

覆盖字体、App State、Home/Weather/Ride 模型、返回键、自动暂停、Dock、
Control Center、胶囊动效、Navigation、Router、Command、Bottom Surface、
Settings、Summary、History Repository/PC Storage、History/Detail、
Profile/Achievements。

### 7.2 页面运行时日志扫描

```text
PAGE_RENDER_LOG_SCAN: PASS
```

未发现缺字、图层缓冲分配失败或 LVGL 内存错误。

### 7.3 Simulator 存活

默认 Simulator 连续运行 10 秒无崩溃、无内存错误日志，随后由测试主动终止。

另执行 30 分钟无人值守动态 Mock 持续运行，共采样 360 次：进程正常结束，
未出现 `No memory`、图层缓冲失败、缺字、断言或崩溃日志。资源加载完成后的
第 5 分钟至第 30 分钟，Private Memory 从 90.06 MB 变化至 91.38 MB，
峰值 95.51 MB；Working Set 峰值 90.52 MB。该结果验证 Simulator 稳定性，
不等同于真实鼠标连续交互压力或 STM32 内存结论。

### 7.4 性能基准

```text
static: 10022 ms, average 59 FPS, minimum 59 FPS, CPU max 1%
data:   10009 ms, average 59 FPS, minimum 58 FPS, CPU max 1%
```

数值为当前 PC Simulator 环境结果，不代表 STM32 实机性能。

### 7.5 视觉检查

- Navigation 离屏渲染：地图、1x3 数据条、指令坞无越界和裁切。
- Profile 离屏渲染：中文、数字、单位、焦点卡片完整。
- Achievements 离屏渲染：中文、进度、百分号和滚动列表完整。

桌面可见自动点击工具在当前机器初始化失败，因此未声明完成全页面自动鼠标
端到端验收；关键交互由组件测试、路由测试、模型测试和 Simulator 存活覆盖，
最终手感仍需人工在 SDL 窗口确认。

## 8. 明确边界

以下不是“再画一个 LVGL 页面”即可完成，当前只能提供 UI、Mock 与接口：

- 真实 GNSS 定位、道路匹配、偏航重规划；
- 西安真实地图瓦片、投影、TF 索引、3x3 Tile Cache、JPEG 解码；
- BLE 心率/功率/踏频传感器；
- TF/FatFs、掉电恢复和拔卡异常；
- GPX/FIT 导入导出；
- 真实音频播放、媒体元数据和联网天气；
- STM32 LTDC、SDRAM、DMA2D、Touch、Encoder 与实体按键驱动。

在这些后端接入后，页面应继续复用本阶段的语义接口，不应重新实现 UI 业务逻辑。

## 9. 下一步

1. 人工执行完整产品流程与异常流程，重点确认 Touch 手感和动画节奏。
2. 根据验收结果修复 Simulator 侧问题，不扩张到 STM32 驱动。
3. `v1.2.0` 发布后仅接受有明确验收依据的 Simulator 修复，下一修复版本从
   `v1.2.1` 开始。
4. LVGL Simulator 正式冻结后，再启动 STM32 Firmware Bring-up。
