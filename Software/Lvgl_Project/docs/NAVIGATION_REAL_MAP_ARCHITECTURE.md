# 导航真实地图与 STM32 落地架构

> 历史架构讨论。当前实现已采用九级 RGB565/LZ4 彩色瓦片、独立正向地名、464×540 地图与 464×226 交互坞。以下 Mock、JPEG、460×460、512 路线节点等描述属于较早阶段；当前工程接口、内存与边界以 [彩色离线地图与选点导航](彩色离线地图与选点导航_20261003.md) 为准。

## 1. 当前状态与本次内存故障

当前导航页中的道路和路线是 LVGL `DRAW_MAIN` 绘制的 Mock 几何，不是真实地图。

旧缩放实现直接对约 469×470 的 `map_content` 设置 transform。LVGL 9.3.0-dev 会为该
变换创建 Alpha 中间图层，日志中的一次申请为：

```text
469 × 470 × 4 = 881720 Byte
```

当前 `LV_MEM_SIZE` 为 1 MiB，扣除对象、样式和动画占用后无法满足该申请。渲染器持续重试，
表现为滚轮缩放后卡死。

当前修复不扩大 LVGL 堆，而是：

- 道路和路线在绘制前对点坐标做定点缩放/旋转；
- 位置标记使用相同坐标变换；
- 地图容器只平移，不再整体 transform；
- 48×48 HUD 按钮仍可保留轻量 scale 动画；
- 几何渲染独立在 `ui_nav_geometry.c`，`ui_nav_map.c` 保持在 500 行以内。

这既解决 PC Simulator，也避免把同一问题带到 STM32。

## 2. LVGL 能做什么、不能做什么

LVGL 负责显示、裁切、手势和控件，不提供地图数据、地名检索、路径规划或转向指令引擎。
真实地图必须由以下模块共同提供：

```text
离线地图数据 + Tile Provider + 坐标投影 + 路线规划结果 + GNSS 定位 + LVGL 显示
```

因此可以显示西安市真实地图，但不能仅调用一个 LVGL API 就出现地图。

## 3. 推荐方案：西安市离线栅格瓦片

第一版优先采用 256×256 的深色 JPEG 栅格瓦片：

```text
离线制图工具
  -> 西安市/路线走廊瓦片（多个离散 zoom）
  -> navpack 文件和索引
  -> microSD / eMMC / 大容量外部 Flash
  -> Tile Loader Task
  -> STM32 Hardware JPEG
  -> DMA2D RGB565
  -> SDRAM Tile Cache
  -> LVGL 3×3 Tile View
```

不应把整个西安市做成一张超大图片，也不应在 LVGL 中缩放一张超大地图对象。

### 3.1 运行期对象

- Viewport：460×460，负责裁切和触摸拖动；
- Tile Layer：固定复用 3×3 共 9 个 `lv_image`；
- Route Layer：路线折线；
- Marker Layer：当前位置和航向；
- HUD：指南针、缩放、定位、比例尺，始终固定；
- Tile Cache：LRU，缓存当前瓦片和相邻瓦片；
- Zoom：只在整数级切换瓦片，可用旧/新两组瓦片短暂交叉淡入淡出。

### 3.2 SDRAM 预算

单张 256×256 RGB565 解码图占用：

```text
256 × 256 × 2 = 131072 Byte = 128 KiB
```

- 9 张可见瓦片：约 1.125 MiB；
- 两套 9 张瓦片用于 zoom 交叉淡入淡出：约 2.25 MiB；
- 480×854 RGB565 单 framebuffer：约 800.6 KiB；
- 双 framebuffer：约 1.56 MiB。

以上应放在 32 MiB 外部 SDRAM，不占 LVGL 1 MiB 通用对象堆。

## 4. 存储选择

16 MiB W25Q128 适合字体、UI 图标、背景和小范围应急底图，不适合完整保存西安市多个
高细节 zoom 的栅格地图。实际地图包体积取决于边界、zoom 范围、道路样式和 JPEG 品质，
在生成样包前不应承诺固定容量。

推荐优先级：

1. microSD/TF：开发和更新地图最方便；
2. eMMC：量产一致性和连续读取更好；
3. 更大容量 OSPI NOR：仅用于固定区域、有限 zoom；
4. W25Q128：只存索引、配置和小范围 fallback。

地图包第一版建议使用简单顺序文件 `xian.navpack`，包含文件头、瓦片索引和 JPEG 数据块。
比在 MCU 上直接引入 SQLite/MBTiles 更容易控制 RAM、代码体积和随机读延迟。PC 端工具可从
合法来源的 MBTiles 转换为 `navpack`。

## 5. 坐标与路线

- GNSS 输出通常按 WGS-84；
- OSM/Web Mercator 瓦片需进行 WGS-84 经纬度到 `z/x/y` 和瓦片内像素转换；
- 若采用国内互联网地图服务，必须明确其坐标系、授权和离线使用条款，避免 WGS-84、
  GCJ-02 等坐标混用导致路线与位置错位；
- 当前 Mock 中心已改为西安市中心附近，但这不等于已加载西安真实地图。

推荐让手机或云端完成路线搜索，将以下紧凑数据经 BLE 下发给码表：

```text
路线 polyline + 转向点 + 道路名 + 剩余距离 + ETA
```

STM32 只做离线显示、GNSS 匹配和偏航检测。完整的本机离线路径规划需要道路图、空间索引
和寻路算法，存储与开发成本明显更高，建议放到后续阶段。

## 6. 线程与数据流

```text
GNSS Task ---------------------> Navigation Model Queue
Storage/Tile Loader Task ------> Compressed Tile Queue
JPEG + DMA2D Task -------------> RGB565 Tile Ready Queue
LVGL Task ---------------------> 替换 image descriptor / 更新路线和 HUD
```

约束：

- 文件读取、JPEG 解码和 `lv_obj_create/delete` 不进入 draw callback；
- LVGL 对象仅由 LVGL Task 修改；
- Tile 对象固定复用，平移跨过边界时回收对侧对象；
- 队列满时丢弃过期请求，优先保留当前视口和最新 zoom；
- JPEG、DMA2D、LTDC 共用 SDRAM 时，缓存区按 Cortex-M7 32-byte cache line 对齐；
- DMA 前后执行正确的 D-Cache Clean/Invalidate，或通过 MPU 配置专用 non-cacheable 区域；
- zoom 快速滚动时合并中间请求，只加载最终 zoom，避免 I/O 风暴。

## 7. 仿真阶段如何保证真机地图正确

真实地图不需要等到 STM32 才验证。正确做法是让 PC 和 STM32 共用地图核心，仅替换平台后端：

```text
共用：navpack 格式、瓦片索引、WGS-84/Web Mercator、Tile Cache、路线坐标、LVGL Map View
PC：  STDIO + 软件 JPEG
STM32：FatFs + SDMMC/TF + Hardware JPEG + DMA2D
```

PC Simulator 可以提前验收：

- 同一份 TF 卡目录和 `xian.navpack` 是否能读取；
- 经纬度到 zoom/x/y/瓦片内像素的投影结果；
- 西安已知地标坐标是否落在正确道路位置；
- 平移、整数 zoom、瓦片回收、缺瓦片占位和缓存命中；
- GNSS 轨迹回放、路线叠加、转向点、偏航后的视觉行为；
- 460×460 Viewport 的裁切、FPS、对象数和峰值 LVGL 堆；
- 对固定坐标生成截图并与 Golden Image 做像素回归。

STM32 真机仍必须单独验收：

- TF/SDMMC 在连续和随机读下的延迟峰值；
- 硬件 JPEG 与 PC 软件 JPEG 的输出差异；
- DMA2D/LTDC/SDRAM 并发带宽；
- Cortex-M7 D-Cache Clean/Invalidate 是否正确；
- 拔卡、坏块、掉电和低速卡情况下是否降级而不阻塞 LVGL Task；
- 实际 GNSS 更新频率、漂移和隧道/高楼环境下的地图匹配。

建议为投影模块建立固定测试向量：输入西安钟楼、城墙、大雁塔等已核验的 WGS-84 坐标，
断言 zoom/x/y 与瓦片内像素。PC 与 STM32 对同一输入必须得到完全相同的整数结果。地图包文件头、
索引区和每个压缩瓦片应带版本、尺寸和 CRC；这样才能区分“地图数据损坏”和“渲染错误”。

结论：仿真可以验证地图数据和绝大部分导航逻辑，但不能替代 SDMMC、硬件解码、缓存一致性和
实时 GNSS 的真机验证。两阶段都通过，才可以认为移植结果可靠。

## 8. 分阶段开发

### Phase A：当前已完成

- Mock 分层地图；
- 拖动、惯性、定位、North Up/Heading Up；
- 安全的坐标级 zoom/rotation；
- 模型队列与导航数据卡；
- PC 缩放内存故障修复。

### Phase B：离线西安地图 MVP

- 确定合法地图数据来源和署名方案；
- 生成一个小范围、3 个 zoom 的西安样包；
- 实现 `navpack` 索引和异步 Tile Provider；
- 实现 3×3 RGB565 Tile Cache；
- 接入真实 WGS-84/Web Mercator 投影；
- 完成平移回收、整数 zoom 和缺瓦片占位图；
- 在 About/地图页落实数据署名。

### Phase C：真机优化

- 接入 QSPI/microSD、Hardware JPEG、DMA2D；
- SDRAM 分区与 D-Cache 一致性测试；
- 统计 tile load latency、cache hit、FPS、CPU 和峰值内存；
- BLE 接收路线和转向指令；
- 弱 GNSS、丢瓦片、存储拔出、断电恢复等异常测试。

## 9. 地图数据合规边界

OpenStreetMap 数据可按 ODbL 使用并需要署名，但 OSM 官方标准 tile server 不允许批量下载
或离线预取。设备地图包必须自行渲染，或使用明确授权离线使用的供应商数据。

- OSM Copyright/ODbL：https://www.openstreetmap.org/copyright
- OSM Tile Usage Policy：https://operations.osmfoundation.org/policies/tiles/
- OSM Vector Tile Usage Policy：https://operations.osmfoundation.org/policies/vector/
