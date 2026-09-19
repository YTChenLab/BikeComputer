# Phase 6F 骑行历史与持久化报告

## 阶段结论

本阶段已把 Ride Summary 的“保存”从仅更新当前进程内 UI，升级为 PC Simulator 上可重启恢复的真实持久化闭环：

```text
结束骑行
→ 冻结 Summary 数据
→ Repository 生成待保存记录
→ PC Storage 写入临时文件并提交
→ 原文件进入备份
→ 临时文件替换正式文件
→ 更新 History Model
→ 刷新 Summary / Home / History
```

保存失败时不会更新 Home 累计数据，也不会把 Summary 标记为 `SAVED`；待保存记录会保留，允许后续重试。Dashboard 已冻结的顶部状态栏、实时速度、黄色速度条、2×3 数据区、背景和 MiSans 字体没有重构。

## 新增模块

### `ui_ride_history_model`

- 保存最近 100 条有效骑行记录；
- 按记录 ID 去重；
- 超出容量时淘汰最早记录；
- 提供按“最新优先”读取；
- 聚合总里程、总骑行时间、总爬升和骑行次数；
- 记录距离、时长、平均/最大速度、爬升、平均心率、平均功率和平均踏频。

### `ui_ride_history_repository`

- 隔离 UI 模型与具体文件系统；
- 管理 `load → prepare → commit` 生命周期；
- 先持久化后替换内存模型，避免写盘失败污染 UI；
- 保留失败的 pending 记录并支持重试；
- 对同一条记录进行幂等提交；
- 通过 `load/store/now_utc/context` 后端接口适配 PC 或 STM32。

### `ui_pc_ride_history_storage`

默认数据路径：

```text
runtime/ride_history.dat
```

文件使用显式二进制协议，不直接写入 C struct，避免编译器填充、字节序和 ABI 变化破坏兼容性。

```text
Header 24 bytes
├─ Magic            8 bytes: BKRIDE1\0
├─ Format Version   uint16 little-endian
├─ Header Size      uint16 little-endian
├─ Record Count     uint32 little-endian
├─ Payload Size     uint32 little-endian
└─ Payload CRC32    uint32 little-endian

Record 50 bytes
├─ id / start / finish
├─ distance / duration
├─ average speed / maximum speed
├─ ascent
└─ average HR / power / cadence
```

写入时先生成 `.tmp`，调用 `fflush` 和平台文件提交函数，再把原文件改名为 `.bak`，最后替换正式文件。启动加载时，如果正式文件缺失或损坏，会先完整校验 `.bak` 的版本、长度和 CRC，校验通过才恢复。未知格式版本不会被旧程序当作备份故障静默覆盖。

### `ui_history_page`

- 显示总骑行次数、累计距离和累计时间；
- 无记录时显示明确空状态；
- 列表按最新骑行优先排列；
- 每行显示日期、距离、时长、平均速度和爬升；
- 支持鼠标触摸滚动、整行点击和 Encoder 焦点滚动；
- Repository 保留最近 100 条，页面当前最多创建最近 50 条，控制 1 MiB LVGL Heap 下的对象数量。

## 应用集成

- Home 的 `RIDE HISTORY` 入口已连接 `UI_ROUTE_HISTORY`；
- 启动时加载 Repository，并用真实聚合值替代旧的虚构累计数据；
- 最新一条记录驱动 Home 的最近骑行日期、距离、时间和爬升；
- `UI_COMMAND_RIDE_SAVE` 在 PC 后端内提交 Repository，成功回执后才刷新页面模型；
- History 页面接入 Router 生命周期和语义 Encoder 输入；
- `runtime/` 已加入 `.gitignore`，用户数据不会进入源码版本库。

## STM32 接入边界

已提供：

```c
bool ui_stm32_services_register_history_storage(
    const ui_ride_history_storage_t *storage);
```

后续 STM32 工程可用 FatFs、littlefs 或其他介质实现相同的 `load/store/now_utc` 接口，UI、Router、Home 和 History 页面不需要知道底层来自 TF 卡还是内部 Flash。

本阶段没有完成以下内容，因此不能视为真机持久化已验收：

- FatFs/SDMMC/SPI-SD 驱动接入；
- TF 卡拔出、写保护、满盘和坏块错误映射；
- RTC 与时区来源；
- 写缓存一致性和突然掉电测试；
- 大量历史记录在目标 SDRAM/文件系统上的性能测试。

## 自动化验收

本阶段测试覆盖：

- 记录追加、重复 ID、非法数据和最近 100 条淘汰；
- 聚合距离、时间、爬升和次数；
- 写盘失败不修改内存模型，pending 保留，重试后成功；
- 重复提交同一记录不重复写入；
- Repository 重新初始化后从文件恢复；
- little-endian 文件正常读写和替换；
- 未知版本、CRC 损坏、文件截断；
- 主文件缺失时从有效 `.bak` 恢复；
- 主文件损坏时从有效 `.bak` 恢复；
- `UI_COMMAND_RIDE_SAVE` 经 PC 异步服务提交 Repository，覆盖成功、写盘失败保留 pending 和重试成功；
- History 空状态、50 条显示上限、Encoder 焦点和 LVGL `No memory` 回归。

验收结果：

```text
CMake configure/build: PASS
CTest: 19/19 PASS
git diff --check: PASS
Simulator executable: 可启动且进程持续响应
```

本次 Windows UI 自动化服务未返回可操作窗口，因此没有把鼠标点击端到端过程伪报为已自动验收。核心交互、存储和页面渲染由可重复自动化测试覆盖；发布前仍应在本机手动完成一次真实骑行保存与重启回归。

## 当前边界与下一阶段

Phase 6F 已完成 History V1 和 PC 持久化闭环，但尚未实现：

- 单次骑行详情页；
- 删除、导出或筛选骑行记录；
- 速度、功率、心率和海拔趋势图；
- GPX/FIT 轨迹文件关联；
- 云同步和冲突解决。

建议下一阶段进入 Phase 7A：先建立 `Ride Detail` 数据契约与页面，使用现有记录字段完成概览区，并为后续轨迹与分段数据预留按记录 ID 加载的扩展文件接口。不要在 100 条索引文件中直接嵌入高频传感器采样数据。
