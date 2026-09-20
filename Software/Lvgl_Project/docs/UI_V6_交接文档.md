# UI_V6_交接文档

## 0. 文档定位

本文档用于将当前公路车码表 LVGL 项目的“骑行总结 / 历史骑行详情”界面重构任务正式交接给 Codex。

本轮目标不是继续扩展新的业务架构，而是：

> **将现有 Ride Summary 与 Ride Detail 重构为统一的“骑行报告（Ride Report）”视觉与交互体系。**

当前工程已有：

- Router
- App State
- Ride State Machine
- Bottom Interaction Surface
- Dynamic Card
- History
- Ride Summary
- Ride Detail
- PC Simulator
- Mock / Repository 数据层

本轮优先复用现有模型、路由、状态和数据接口，不重新创建第二套骑行数据架构。

---

# 1. 当前问题

现有 Ride Detail 页面主要问题：

1. 信息层级过平；
2. 采用大量大小相同的小方框；
3. “一个指标一个卡片”，视觉碎片化；
4. 路线预览区域太弱，更像调试占位；
5. RECORD ID 等内部字段直接暴露给用户；
6. Simulator / Persisted 等开发提示进入正式 UI；
7. Summary 与 Detail 两套页面视觉逐渐分离；
8. 缺少速度、心率、功率、踏频、海拔等趋势分析；
9. 缺少心率区间 / 功率区间等骑行分析；
10. 没有形成“本次骑行报告”的完整信息层级。

因此本轮正式废弃“九宫格式骑行详情”的视觉方案。

---

# 2. 新页面正式名称

统一定义：

# 骑行报告
### RIDE REPORT

内部支持两种模式：

```text
SUMMARY_MODE
DETAIL_MODE
```

---

## 2.1 SUMMARY_MODE

来源：

```text
FINISH
↓
SAVE SUCCESS
↓
RIDE REPORT
```

顶部标题：

```text
骑行完成
RIDE COMPLETE
```

或保持统一：

```text
骑行报告
RIDE REPORT
```

底部允许显示：

```text
[完成]
```

点击后：

```text
→ Home
```

---

## 2.2 DETAIL_MODE

来源：

```text
History
↓
选择一条骑行记录
↓
RIDE REPORT
```

顶部标题：

```text
骑行报告
RIDE REPORT
```

或：

```text
骑行详情
RIDE DETAIL
```

推荐统一使用：

```text
骑行报告
RIDE REPORT
```

避免维护两套视觉。

DETAIL_MODE 不显示“完成”按钮，使用返回键 / Back 返回 History。

---

# 3. 总体信息架构

骑行报告使用：

> **纵向长页面 + 主题卡片分区**

而不是多页横向分页。

页面顺序：

```text
状态栏
↓
页面标题
↓
路线地图
↓
骑行身份信息
↓
核心骑行概览
↓
骑行表现概览
↓
速度分析
↓
心率分析
↓
心率区间
↓
功率分析
↓
功率区间
↓
踏频分析
↓
海拔分析
↓
能量 / 环境
↓
骑行总结
```

整页纵向滚动。

---

# 4. 视觉总原则

继续严格沿用项目现有 UI 风格：

- 480 × 854
- RGB565
- 纯黑背景
- 深灰 Surface
- 白色主文字
- 灰色次级文字
- 黄色 Accent
- MiSans
- 小圆角
- 细描边
- 工业设备感
- Rhodes / PRTS 氛围
- 高信息密度
- 克制，不做手机 App 白色卡片风格

参考手机骑行 App 的只有：

> **信息架构与数据组织方式。**

不复制白色背景、绿色主题、大圆角、手机 App 风格。

---

# 5. 页面顶部

顶部继续复用统一 Status Bar：

```text
卫星数
时间
WiFi
Bluetooth
电池百分比
电池图标
```

下一行：

```text
←
骑行报告
RIDE REPORT
                         ···
```

第一版右上角 `···` 可以预留但不要求实现菜单。

如果没有实际功能，可以先隐藏。

---

# 6. 顶部路线地图

地图是骑行报告第一视觉主体。

建议高度：

```text
250 ~ 280 px
```

宽度尽可能接近内容安全区。

---

## 6.1 地图显示内容

显示：

- 深灰地图底图
- 深灰道路
- 黄色骑行轨迹
- 起点
- 终点
- North 指示
- 比例尺

路线必须自动缩放到最大程度填充地图区域。

---

## 6.2 起终点视觉

起点：

```text
黄色实心圆
```

终点：

```text
黄色圆环 + 小旗
```

或其他项目已有统一导航图标。

---

## 6.3 当前阶段

真实地图未接入时：

```text
PC Simulator
→ Mock Map / Mock Route
```

但 UI 结构必须按真实地图最终结构设计。

以后：

```text
Saved Track
↓
Map Provider
↓
Route Overlay
```

页面不重构。

---

# 7. 骑行身份信息

地图下方加入一条骑行基本信息：

```text
户外骑行 / OUTDOOR RIDE
2026.09.19  23:19
```

未来可增加：

```text
西安 · 雁塔区
```

如果 Location 暂无真实数据，可以不显示。

禁止显示：

```text
Record ID
Simulator Preview
Persisted
Repository ID
```

这些内部字段只允许 Debug 模式查看。

---

# 8. 核心骑行概览卡

这一块是整个报告第二视觉焦点。

标题：

```text
骑行距离
DISTANCE
```

主数字：

```text
86.7 km
```

必须是该卡片最大数字。

---

## 8.1 次级指标

下面两列：

```text
骑行时间                平均速度
03:21:18                25.8 km/h
```

下一行：

```text
最高速度                累计爬升
47.0 km/h               1268 m
```

避免每个数据单独一个小卡片。

---

# 9. 骑行表现概览

标题：

```text
骑行表现
PERFORMANCE
```

第一版建议 2×3 数据矩阵：

```text
平均心率       平均功率       平均踏频
147 bpm        249 W          84 rpm

热量           最高海拔       总下降
1536 kcal      1826 m         1245 m
```

数据单元可有浅边框，但必须属于同一个主题区域。

---

# 10. 无传感器数据时的规则

禁止显示：

```text
0 W
0 bpm
0 rpm
```

如果本次骑行没有该传感器数据：

```text
隐藏该数据项
```

并自动重新排布剩余内容。

例如无功率计：

```text
平均心率
平均踏频
热量
最高海拔
总下降
```

不要硬塞空字段。

---

# 11. 速度分析卡

标题：

```text
速度
SPEED
```

顶部两个核心值：

```text
平均速度                    最高速度
25.8 km/h                   47.0 km/h
```

下方：

```text
Speed × Time
```

折线图。

---

## 11.1 Speed Chart 视觉

建议：

- Yellow line
- Dark grid
- Gray axis
- 少量黄色半透明填充
- 不使用手机 App 的绿色

X轴：

```text
时间
```

Y轴：

```text
km/h
```

---

# 12. 心率分析卡

标题：

```text
心率
HEART RATE
```

顶部：

```text
平均心率        最高心率        最低心率
147 bpm         182 bpm         102 bpm
```

下方：

```text
Heart Rate × Time
```

---

## 12.1 心率曲线颜色

正常区间：

```text
Yellow
```

高强度局部：

```text
Orange
Red
```

不要整张图多彩化。

---

# 13. 心率区间

心率卡内继续显示：

```text
心率区间
HEART RATE ZONES
```

建议五区：

```text
热身
燃脂
有氧
无氧
极限
```

显示：

```text
名称
时间
进度条
百分比
```

例如：

```text
热身        00:12:18    8%
燃脂        00:31:42   21%
有氧        00:58:26   39%
无氧        00:39:10   26%
极限        00:09:42    6%
```

颜色：

```text
Gray
Yellow-Green / Light Yellow
Yellow
Orange
Red
```

整体仍保持黑黄主视觉。

---

# 14. 功率分析

如果本次存在功率数据：

标题：

```text
功率
POWER
```

顶部：

```text
平均功率                 最大功率
249 W                    782 W
```

下方：

```text
Power × Time
```

曲线。

第一版不强制加入：

- NP
- IF
- TSS
- 左右平衡
- Torque Effectiveness
- Pedal Smoothness

这些留给后续高级版本。

---

# 15. 功率区间

建议第一版可加入：

```text
恢复
耐力
节奏
阈值
冲刺
```

显示：

```text
Duration
Percent
Progress Bar
```

如果功率区间计算逻辑当前尚未实现：

可以暂时隐藏这一部分。

不要使用伪业务数据进入正式版本。

---

# 16. 踏频分析

如果有踏频数据：

标题：

```text
踏频
CADENCE
```

顶部：

```text
平均踏频                最大踏频
84 rpm                  112 rpm
```

下方：

```text
Cadence × Time
```

曲线。

该卡片高度可低于速度 / 心率。

---

# 17. 海拔分析

这一块必须重点做好。

标题：

```text
海拔
ELEVATION
```

顶部：

```text
累计爬升      累计下降      最高海拔
1268 m        1245 m        1826 m
```

下方：

```text
Elevation Profile
```

X轴建议：

```text
Distance
```

Y轴：

```text
Elevation
```

使用：

- Yellow line
- Dark grid
- Yellow low-opacity fill

以后真实高度可由：

```text
GNSS
+
Barometer
+
Height Fusion
```

提供。

页面结构保持不变。

---

# 18. 能量与环境

标题：

```text
能量与环境
ENERGY & ENVIRONMENT
```

第一版可以显示：

```text
热量消耗
1536 kcal

运动时间
03:21:18

平均温度
21.4 °C
```

“补水建议”如果当前没有正式算法：

不建议第一版显示为正式功能。

如果要保留：

必须明确为估算值 / 建议值，并有真实计算逻辑。

---

# 19. 骑行总结

页面末尾增加：

```text
骑行总结
RIDE SUMMARY
```

第一版可基于明确规则生成三项标签。

例如：

```text
长距离
LONG RIDE

爬升挑战
CLIMBING

有氧耐力
AEROBIC ENDURANCE
```

每项：

```text
Icon
Title
Short Description
```

注意：

如果当前没有成熟业务规则，不应生成夸张的智能结论。

可以先使用简单规则：

```text
distance threshold
ascent threshold
HR zone distribution
```

---

# 20. 起终点信息

页面底部可显示：

```text
起点
曲江

终点
秦岭

骑行类型
户外骑行
```

如果地名数据暂无：

先不显示地名。

不要硬编码虚假地点。

---

# 21. 页面交互

整页：

```text
Vertical Scroll
```

Touch：

```text
上下滚动
```

Encoder：

```text
CW
→ 向下滚动

CCW
→ 向上滚动
```

建议编码器按块滚动：

```text
Section Snap / Scroll By Fixed Distance
```

不要求严格一屏一卡。

---

# 22. Back

DETAIL_MODE：

```text
Back
→ History
```

SUMMARY_MODE：

```text
Back
→ 根据 Ride State / Product Flow 处理
```

推荐：

```text
Summary 完成后
→ Home
```

如果有“完成”按钮：

按下完成同样返回 Home。

---

# 23. Summary 与 Detail 页面合并

本轮核心要求：

不要继续维护：

```text
Ride Summary
+
Ride Detail
```

两套完全独立视觉。

建议实现：

```text
ui_ride_report_page
```

并通过：

```text
mode
```

决定：

```text
SUMMARY_MODE
DETAIL_MODE
```

---

# 24. 推荐数据模型

建议建立或扩展：

```c
typedef struct {
    uint64_t record_id;

    uint32_t start_timestamp;
    uint32_t duration_s;
    uint32_t moving_time_s;

    float distance_km;
    float avg_speed_kmh;
    float max_speed_kmh;

    uint32_t total_ascent_m;
    uint32_t total_descent_m;
    uint32_t max_elevation_m;

    bool has_hr;
    uint16_t avg_hr;
    uint16_t max_hr;
    uint16_t min_hr;

    bool has_power;
    uint16_t avg_power;
    uint16_t max_power;

    bool has_cadence;
    uint16_t avg_cadence;
    uint16_t max_cadence;

    uint32_t calories_kcal;

    bool has_temperature;
    float avg_temperature_c;

    // route summary
    // charts
    // zones
} ui_ride_report_model_t;
```

具体字段名按现有工程模型调整。

不要无意义重复已有 `ui_ride_summary_model` / history model。

---

# 25. 图表数据模型

图表不要直接加载全部原始采样。

建议为 UI 提供降采样数据：

```text
speed_points[]
heart_rate_points[]
power_points[]
cadence_points[]
elevation_points[]
```

建议每条图：

```text
约 120 ~ 300 points
```

第一版可固定：

```text
240 points
```

---

# 26. 原始数据降采样

例如：

3小时骑行：

```text
1Hz
≈ 10800 samples
```

UI 不绘制 10800 点。

Repository / Report Model 层提前降采样到：

```text
≈ 240 samples
```

再传给 LVGL。

---

# 27. LVGL 图表实现

优先：

```text
lv_chart
```

或轻量自绘 polyline。

禁止：

- 生成大尺寸 chart PNG
- 每次页面刷新重新生成图片
- 大量离屏图层
- 大面积实时透明动画

---

# 28. 图表更新策略

Ride Report 是静态结果页。

进入页面时：

```text
Load Model
↓
Populate Charts
```

之后不需要高频 Timer。

禁止 100ms / 1s 无意义刷新。

---

# 29. 内存要求

页面虽然内容很多，但必须控制对象数量。

建议：

- Section 延迟创建可选
- Chart 点数降采样
- Image 复用
- 不创建无限 History / Detail 对象
- 离开页面时正确释放临时 Chart 数据

如当前 Router 使用页面生命周期销毁对象，继续沿用。

---

# 30. 地图资源约束

真实地图未实现时：

PC Simulator：

```text
Mock Map
```

真机：

```text
Tile / Map Provider
```

Ride Report 不能直接依赖：

```text
SDL
FatFs
Map Engine
```

只能依赖：

```text
Map Provider Interface
```

---

# 31. 动效

Ride Report 页面保持克制。

建议：

## Summary Mode 首次进入

```text
Main Distance
Count-up

Main Card
Reveal

Performance
Stagger Reveal
```

## Detail Mode

不需要每次打开做完整 Count-up。

只需：

```text
Section Fade / Translate
```

---

# 32. 图表动画

第一版：

可选：

```text
Chart line reveal
```

但如果会增加大量 redraw：

不实现。

优先保证：

```text
FPS
Memory
Scroll
```

---

# 33. 硬件约束

所有实现继续面向：

```text
STM32
+ External SDRAM
+ LTDC
+ DMA2D
+ 480×854 RGB565
```

禁止：

- Blur
- Full-screen opacity animation
- Large transform
- Metaball
- 大图实时缩放
- 高成本 Chart Shader
- 复杂动态 Gradient

---

# 34. 页面内容自适应

根据数据存在性自动显示。

例如：

无 HR：

```text
隐藏 Heart Rate Section
```

无 Power：

```text
隐藏 Power Section
隐藏 Power Zones
```

无 Cadence：

```text
隐藏 Cadence Section
```

无 Temperature：

```text
隐藏 Avg Temperature
```

页面自动重新排版。

---

# 35. 正式 UI 禁止显示的开发信息

禁止：

```text
RECORD ID
Repository Status
Persisted
Simulator Preview
Mock Geometry
Internal File Path
Debug ID
```

如果需要：

仅在：

```text
DEBUG_BUILD
```

或 Debug Overlay 中显示。

---

# 36. 视觉参考结构

第一屏：

```text
Status Bar
Ride Report Header
Map
Ride Type / Date
Distance Hero Card
Performance Summary
```

第二段：

```text
Speed
Heart Rate
Heart Rate Zones
```

第三段：

```text
Power
Cadence
Power Zones
```

第四段：

```text
Elevation
Energy / Environment
Ride Summary
Start / End / Type
```

注意：

实际实现为一条 Vertical Scroll 页面。

这里的“第一段 / 第二段 / 第三段 / 第四段”只用于设计和测试截图分区。

---

# 37. 推荐文件组织

建议：

```text
src/ui/pages/
    ui_ride_report.c
    ui_ride_report.h

src/ui/components/
    ui_ride_report_map.c
    ui_ride_report_overview.c
    ui_ride_report_chart.c
    ui_ride_report_zones.c
    ui_ride_report_summary.c

src/ui/models/
    ui_ride_report_model.c
    ui_ride_report_model.h
```

如果当前已有对应模块：

优先重构并复用。

不要机械创建重复文件。

---

# 38. Router 建议

可保留现有两个 route：

```text
UI_ROUTE_RIDE_SUMMARY
UI_ROUTE_RIDE_DETAIL
```

但两个 route 最终都创建：

```text
ui_ride_report_page
```

传不同 mode。

如果 Router 支持参数：

推荐使用参数。

如果不支持：

可以通过 report context 设置 mode。

不要求本轮大改 Router。

---

# 39. History 联动

History row：

```text
Touch / Encoder Press
↓
load record_id
↓
build ui_ride_report_model
↓
open DETAIL_MODE
```

---

# 40. End Ride 联动

骑行结束：

```text
Finish
↓
Save
↓
Repository
↓
Build Ride Report
↓
SUMMARY_MODE
```

保存失败：

```text
不进入 Report
```

继续使用现有 Bottom Interaction Surface 显示错误与重试。

---

# 41. PC Mock 数据

PC Simulator 必须准备至少：

### Case A
完整数据：

- HR
- Power
- Cadence
- Elevation

### Case B
无 Power

### Case C
无 HR / Cadence

### Case D
短骑行

### Case E
长距离 / 大爬升

用于验证自适应布局。

---

# 42. 测试内容

新增 / 更新测试建议：

```text
ui_ride_report_model_test
ui_ride_report_page_test
ui_ride_report_optional_sections_test
ui_ride_report_chart_downsample_test
ui_ride_report_summary_mode_test
ui_ride_report_detail_mode_test
```

---

# 43. 手工验收

必须检查：

```text
History
→ Ride Report
→ Scroll
→ Back
→ History
```

以及：

```text
Ride Finish
→ Save
→ Ride Report
→ Complete
→ Home
```

---

# 44. 视觉验收

逐项检查：

- 地图是否为视觉主体
- Distance 是否为主数字
- Duration / Avg Speed 层级是否清晰
- Performance 是否整齐
- Charts 是否 readable
- Axis 是否裁切
- 单位是否对齐
- 无重复小卡片
- 无开发字段
- 黑黄风格一致
- 不出现手机白卡风格
- 480×854 真机可读

---

# 45. 性能验收

PC：

- 目标 59/60 FPS
- 快速滚动无明显卡顿
- Chart 不导致明显掉帧
- 页面反复进入/退出无 Heap 持续增长
- Timer 数不增长
- Animation 数不增长

真机阶段再重新验证：

- LTDC FPS
- SDRAM Bandwidth
- DMA2D
- Cache
- Touch Scroll Latency

---

# 46. 本轮明确不做

不在本轮加入：

- 训练负荷复杂算法
- VO2Max
- FTP 自动估计
- 完整 TSS / IF / NP
- 骑行评分
- AI 训练建议
- 社交分享
- 云同步
- 视频回放
- 3D 地图
- 高成本动态图表

---

# 47. Codex 执行顺序

## Step 1
只读检查当前：

- Ride Summary
- Ride Detail
- History
- Ride Summary Model
- History Repository
- Router
- Map Mock
- Chart Capability

输出重构计划。

## Step 2
建立统一 Ride Report Model。

## Step 3
将 Ride Summary / Ride Detail 统一到 Ride Report 页面。

## Step 4
完成顶部地图和核心概览。

## Step 5
完成 Performance。

## Step 6
完成 Speed / HR / HR Zones。

## Step 7
完成 Power / Cadence / Power Zones。

## Step 8
完成 Elevation / Energy / Summary。

## Step 9
完成数据缺失自适应。

## Step 10
完成 Encoder / Touch / Back。

## Step 11
Build + Test。

## Step 12
生成 PC Simulator 截图并核对 480×854。

---

# 48. Codex 当前执行指令

```text
你现在开始 UI V6：Ride Report 重构。

本轮目标：

把当前 Ride Summary 与 Ride Detail 重构为统一的“骑行报告（Ride Report）”页面。

必须遵守：

1. 不重构 Router / App State / Command Service 核心架构；
2. 不重新设计 Dashboard；
3. 不修改 Bottom Interaction Surface 架构；
4. History 与 Finish Flow 继续使用现有业务状态；
5. Ride Report 为纵向滚动长页面；
6. 顶部以地图和黄色路线作为视觉主体；
7. 核心数据按照“Distance Hero → Overview → Analysis”组织；
8. 删除 Record ID、Simulator Preview、Persisted 等开发字段；
9. Speed / HR / Power / Cadence / Elevation 使用 LVGL Chart 或轻量 Polyline；
10. 原始数据必须降采样后再绘制；
11. 根据 has_hr / has_power / has_cadence 等字段自动隐藏无数据 Section；
12. 所有设计必须考虑 STM32 + SDRAM + LTDC + DMA2D 的资源边界。

目标视觉：

黑色 Background
深灰 Surface
白色主数据
灰色辅助文字
黄色 Accent
MiSans
细 Border
小圆角
高信息密度
工业设备风格

最终结构：

Status Bar
→ Ride Report Header
→ Route Map
→ Ride Type / Date
→ Distance Hero
→ Performance
→ Speed
→ Heart Rate
→ Heart Rate Zones
→ Power
→ Cadence
→ Power Zones
→ Elevation
→ Energy / Environment
→ Ride Summary

完成后：

- Build
- 自动化测试
- 完整数据 Case
- 无 Power Case
- 无 HR Case
- 长骑行 Case
- Summary Mode
- Detail Mode
- PC Simulator 480×854 手工验收
- 输出 `UI_V6_RIDE_REPORT_REPORT.md`
```

---

# 49. 最终冻结结论

UI V6 完成后：

> **骑行结束总结与历史骑行详情使用同一套 Ride Report 信息架构和视觉系统。**

以后只通过：

```text
Mode
+
Model
```

区分显示场景。

页面不再回到：

```text
一个指标一个小方框
```

的工程调试式布局。

最终用户看到的是：

> **地图 → 核心结果 → 骑行表现 → 趋势分析 → 区间分析 → 海拔 → 总结**

形成完整、成熟、可继续接入真实 STM32 骑行数据的骑行报告系统。
