# LVGL 1.5.1 STM32H743 Board-Ready Refactor 交付

> v1.5.2 归档说明：本目录保留 2026-10-06 的版本、行号和哈希快照；当前接入入口见 [STM32 v1.5.2 接入说明](../STM32_V1.5.2_INTEGRATION.md)。旧工程 work 原始证据目录已删除，本版发布前独立重新验证，不将历史路径作为当前可下载附件。

> 2026-10-09 输入契约变更：实体按键消抖、单双击、长按与编码器解码现由 STM32 InputService 负责；旧 sample_inputs/set_key_map/submit_input 已删除。本文输入部分及原行号属于历史快照，新接入须使用 [实体输入架构与队列契约](../输入架构重构_20261009.md)，触摸与其余端口不变。


最终工程：`D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project`。日期2026-10-06。应用仍为1.5.1，UI冻结；本轮是源码/端口/内存和构建准备，不是HAL驱动开发或实板完成宣告。

## 四项结果

1. `01_目录与平台架构.md`：共用UI/services，PC/STM32源选择与独立lv_conf，AC6构建入口。
2. `02_PC回归与板端编译验证.md`：最终PC44/44；762图逐字节一致；board host1/1；AC6真实源码编译通过，完整固件未链接。
3. `03_STM32待实现接口.md`：全部backend和数据入口、UiTask/worker/ISR分工、startup顺序、输入/显示/存储/导航/字体/BSP合同。
4. `04_依赖内存与线程审计.md`：PC-only、大内存、C11/atomic、潜在阻塞、跨线程LVGL、时间与fake路径审计。

## 十四项要求对应

| 要求 | 当前交付 | 板端待办 |
| --- | --- | --- |
| 1 PC-only隔离 | target清单排除PC入口/HAL/WinHTTP/桌面FS/POSIX内核 | BSP实际工程照同一清单集成 |
| 2 lv_conf分离 | config/pc与config/stm32；LVGL9、LV_OS_NONE | 配置必须一致传播到vendor与应用 |
| 3 AC6/C11与mutex | 保留原子；真实AC6编译；没有STM32 spinlock | 注入具优先级继承mutex |
| 4 唯一UiTask | runtime上下文检查、任务入口、异步mailbox | FromISR队列/通知；所有backend立即排队返回 |
| 5 正式服务边界 | 既有services扩展；startup聚合原backend类型 | 实现/注册真实BSP端口 |
| 6 Input双路径 | 原短长按+BACK手势；统一touch缓存与pointer/events | 四相解码、校准、单InputTask owner |
| 7 大内存 | UI_LARGE_RAM/.ui_sdram，ARM对象与caller清单 | scatter/清零/allocator/峰值验证 |
| 8 LVGL9显示 | PARTIAL双小buffer；DONE/ASYNC/FAILED、flush_wait | BSP单fb软件copy，后续DMA2D和cache |
| 9 时间 | runtime单调clock+lv_tick_set_cb | 所有producer同一毫秒域，禁止重复tick_inc |
| 10 无fake成功 | demo/mock/replay仅PC；validity/capability | 未实现命令FAILED/导航UNSUPPORTED |
| 11 字体 | ui_text_register_fonts保留，STM32无TinyTTF | QSPI/SDRAM长期字体、动态中文覆盖 |
| 12 离线与在线底层 | read_at算法；无PC FS/WinHTTP链接 | FatFs读句柄与ESP32 HTTPS/协议 |
| 13 启动契约 | startup一次，注册/资源校验后app_init | BSP先初始化全部资源；partial失败复位 |
| 14 诊断 | drop/timeout/ACK/mailbox/context/display/handler | BSP队列失败、RTOS栈、cache/TF/性能记录 |

## 可追溯附录

- `05_原指南差异.md`：原接入资料保留为基线，BACK/flush/字体/初始化等差异明确标注。
- `当前STM32接口变量与回调清单.md/.csv`：31头文件、62结构、428成员声明行，以及公开函数声明和当前行号。
- `大对象与工作区清单.md/.csv`：对象名、ARM尺寸/容量、生命周期、owner、推荐存储区、DMA。
- `接口结构变更.csv`、`C11原子使用点.csv`、`潜在阻塞调用点.csv`、`STM32显示与LVGL调用点.csv`。
- `审计数据.json`、`ARM大对象符号.csv`、`验证证据.json`、`PC截图逐张比对.csv`、`受保护版本与字体校验.csv`。

本轮未改原理图、CubeMX或STM32 HAL工程，未commit/push。当前源码准备已完成；实板成熟度必须由后续固件链接和硬件验收确定。
