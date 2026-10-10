# STM32 v1.5.2 接口适配与接入说明

适用源码：BikeComputer LVGL UI `v1.5.2`。本说明以当前 `src/platform/stm32` 和公共服务头文件为准。它描述已实现的软件端口及板端需要实现的职责，不表示完整 STM32 固件或实板验收已经完成。

本版 Git 归档 LVGL 工程；`Software/STM32_Project`、硬件原理图／数据手册和结构设计仍为单独维护的本地内容。历史指南中的这些路径是核验来源，不是本版已上传文件的承诺。

## 1. 构建与依赖

| 内容 | 当前实现 |
| --- | --- |
| `CMakeLists.txt` | `UI_PLATFORM=PC|STM32` 选择目标 |
| `cmake/ui_sources.cmake` | 共用 UI、服务及已提交字体源清单 |
| `cmake/stm32.cmake` | `bike_ui_stm32` 静态库，排除桌面驱动、演示和 PC HAL |
| `config/stm32/lv_conf.h` | 板端 LVGL 配置，独立于 PC |
| `cmake/toolchains/armclang.cmake` | 现有 AC6／Cortex-M7／hard-float 工具链 |
| `UI_BOARD_CONTRACT_TESTS=ON` | 使用 host mock 验证端口，不能作为实板驱动 |

从 LVGL 工程目录使用已安装且兼容的工具链构建：

```sh
cmake -S . -B build/stm32 -G Ninja -DUI_PLATFORM=STM32 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/armclang.cmake \
  -DUI_ARMCLANG_ROOT="<已有 ARM/ARMCLANG 安装目录>" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/stm32 --parallel 4
```

不在此静态库中启用 PC POSIX FreeRTOS。板端项目提供自己的 FreeRTOS、HAL、启动文件、链接脚本和最终链接；源码使用 C11／原子契约，不能直接当作旧 ARMCC5 工程。字体已提交，正常构建无需重制字体。

## 2. 启动顺序和必需端口

1. BSP 先完成时钟、SDRAM、显示、存储和资源的初始化。`resources_ready()` 只确认就绪，不负责初始化硬件。
2. 建立系统队列、服务 mutex、真实任务身份和单调时钟；准备显示 draw buffers、导航 raster buffers、字体与声音端口。
3. 在唯一 UiTask 填写 `ui_stm32_startup_t`，调用一次 `ui_stm32_startup()`。它注册后端、配置 raster、调用 `lv_init()`、设置 tick、创建显示／pointer indev 和 UI。
4. 初始化成功后启动生产者或允许其提交数据。UiTask 消费输入／电源通知，再周期调用 `ui_stm32_ui_task_handler()`，根据返回期限等待或休眠。
5. 部分初始化失败必须复位，不能再次调用启动入口或第二次 `ui_app_init()`。

启动参数必须提供 services 的 `dispatch_command`、`lock/unlock`、历史 worker 请求；history 的 load/store/now_utc；preferences 的 load/store/request_worker；navigation、maps、search 和 devices 的请求回调；字体、声音、显示与 raster 缓冲区。详细必需项及检查见 [`ui_stm32_startup.c`](../src/platform/stm32/ui_stm32_startup.c)。缺少能力应在正式后端拒绝请求，并只声明真实支持的 capability 位，不能用演示成功结果填补。

`ui_runtime_port_t` 的 `is_ui_task`／`is_isr` 必须查询真实执行上下文，不能固定返回允许；`now_ms` 与输入、骑行和导航质量时间戳共用单调时钟。启动设置 `lv_tick_set_cb(ui_runtime_now_ms)`，不要再叠加另一套 `lv_tick_inc()`。

## 3. 任务职责

| 执行位置 | 允许的工作 |
| --- | --- |
| UiTask | 初始化／LVGL、消费语义输入、应用电源显示通知、UI handler |
| InputTask／InputService | GPIO／编码器采样与全部手势识别；向系统队列复制事件；提交触摸缓存 |
| 传感器／融合任务 | 通过 submit 接口提交带单位、质量和时间的数据 |
| 后台 worker | 历史存储、配置保存、地图读取、搜索、算路及命令执行／完成 |
| ISR | DMA／GPIO／串口最小处理，使用系统 FromISR 通知；不调用 UI／LVGL／任务型 submit |

服务的 lock/unlock 必须使用可验证的板端互斥机制。UI 触发的 dispatch/request 只排队，不能在回调中同步访问 TF、算路或等待网络。后端完成按 request／command id 返回，晚到 ACK 不应覆盖新任务。

## 4. 实体输入和电源契约

标准头文件：[`ui_physical_input.h`](../src/ui/core/ui_physical_input.h)。

```c
typedef struct {
    uint8_t key;
    uint8_t action;
    uint16_t reserved;
    int32_t encoder_delta;
    uint32_t timestamp_ms;
} ui_physical_input_event_t;
```

固定大小 12 字节，delta／timestamp 偏移分别为 4／8；key/action 使用固定宽度字段，不依赖编译器 enum 大小。`reserved=0`，非编码器事件的 delta 为 0。编码器为 CW 正数、CCW 负数，单条 -64..64，超限由生产者拆分。InputService 保证同一手势不会同时产生相互排斥的 CLICK、DOUBLE_CLICK、LONG_START。

系统输入队列复制记录，UiTask 消费后调用：

```c
bool ui_stm32_services_handle_input_event(
    const ui_physical_input_event_t *event);
bool ui_stm32_services_apply_display_state(bool enabled);
```

不在 UI 中再次消抖或根据 timestamp 判定单双击。POWER 和生命周期事件可被接受而不产生 UI 动作；实体电源硬件由 PowerService 处理，UiTask 收到状态通知后 apply_display_state，不重新发送同一电源命令。

迁移时删除对 `sample_inputs`、`set_key_map`、`submit_input` 的旧调用。STM32 项目的旧 `AppInputEvent_t { type, value }` 需要升级为相同语义字段／标准记录，并重新编译全部生产者和消费者；不能混用旧队列 payload。本版没有替你完成 InputService／PowerService 驱动。

完整行为、映射、队列迁移与 PC 联调方式见 [2026-10-09 输入契约](输入架构重构_20261009.md)。

## 5. 显示、DMA 和触摸

[`ui_stm32_display_port_t`](../src/platform/stm32/ui_stm32_display.h) 使用 LVGL 9 PARTIAL、RGB565。两块 draw buffer 独立、32 字节对齐，每块小于完整 framebuffer；它们还不能与两个导航 raster 工作区重叠。

`start_flush()` 接收 area、pixels 和 stride_bytes，返回 DONE／ASYNC／FAILED。ASYNC 时 DMA 持有像素，UiTask 通过 `wait_flush_complete()` 确认完成；ISR 只发送系统通知，不调用 `lv_display_flush_ready()`。BSP 应提供截止时间和失败处理，避免 DMA 失败导致永久等待，并负责 Cache clean/invalidate 的正确范围。

触摸任务调用 `ui_stm32_services_submit_touch(x,y,pressed,cancelled)` 提交校准后的坐标。pointer indev 只读取缓存；触摸控制器的 I2C、INT、RST、坐标方向和标定由 BSP 实现。

## 6. 内存、地图和业务后端

`UI_LARGE_RAM` 将大型静态对象放到 32 字节对齐的 `.ui_sdram`。必须在最终链接脚本将该段落到真实 SDRAM，保证初始化先于 UI 使用；heap、栈、framebuffer、raster、DMA 描述符和 Cache 策略仍需整机预算。可用 `tools/audit_board_ready.py` 检查板端源清单、ARM ELF、大对象和桌面依赖，但它不能替代实板内存验收。

地图、路网、瓦片和地点读取通过既有 `read_at`／异步后端接入。FatFs、卡热拔、读错误、CRC、资源生命周期由存储／地图 worker 处理；不能在 UiTask 显示回调中阻塞读取大文件。提交 map frame、nav session、search result 时遵守请求匹配、缓冲区所有权和数据质量契约。

历史、preferences、devices、sound 和 command 后端遵循各自头文件；没有实现的能力返回失败／不支持，不能使用 PC mock。中文字体和板端字库存储另需预算，PC 的 TinyTTF 文件后端不自动变成 STM32 字库方案。

## 7. 文档优先级与验收

1. 当前源码头文件和本文为 v1.5.2 接入入口。
2. 2026-10-09 输入契约负责实体输入细节。
3. 2026-10-06 Board-Ready 和全流程接入指南保留架构／硬件核验来源；其旧输入接口、源码行号和 CSV 是历史快照。

历史指南引用的旧工程 `work` 日志、截图和备份已删除，本版不依赖该目录运行或构建。本版重新执行的验证结果见 [v1.5.2 发布记录](V1.5.2_RELEASE_NOTES.md)。

实板至少还需验证：完整固件链接、SDRAM／MPU／Cache、显示与触摸、输入队列识别和溢出、电源保存与休眠、TF／地图失败恢复、真实 GNSS／传感器、ESP32 通信及长期运行资源消耗。PC 测试、host mock 和 ARM 库编译分别只覆盖其软件范围。
