# UI 全局背景资源架构

## 当前资源与生成链路

当前 UI 使用 `bg_6`；旧的 `bg_5_*`、`bg_2_*` 与 `bg_rhodes_main_*` 资源保留为 legacy，不参与任何运行时路径。

```text
assets/background/bg_6_master.jpg
    └─ tools/prepare_ui_assets.py
       ├─ assets/generated/bg_6_480x854.rgb565  → PC Simulator
       └─ assets/generated/bg_6_q75.jpg          → STM32/W25Q128
```

`bg_6_master.jpg` 实测为 1144×2544、RGB JPEG。脚本先按 cover 规则等比例缩放，再从上下
两端等量裁切至 480×854，不进行非等比拉伸；输出图像不继承源文件的 EXIF/ICC 元数据。

运行下列命令重新生成资源：

```powershell
& 'C:\Users\cyt\AppData\Local\Programs\Python\Python311\python.exe' tools\prepare_ui_assets.py
```

输出属性：

- PC RAW：480×854、`LV_COLOR_FORMAT_RGB565`、无 Alpha、无压缩、819,840 bytes。
- PC byte order：小端；每个像素的低字节在前、高字节在后。UCRT64/x86 为小端，当前
  LVGL 9.3 的 `LV_COLOR_DEPTH=16` 将 native display format 定义为 `RGB565`；工程中
  不存在 `LV_COLOR_16_SWAP` 配置。
- STM32 JPEG：480×854、baseline JPEG、YCbCr 4:2:0、quality 75、非 progressive，且
  不含 EXIF/ICC。当前样本约 29.8 KB，大小可随图像内容变化。

## PC Simulator 运行时链路

```text
bg_6_480x854.rgb565
    ↓ fopen/fread（仅 ui_background_init 时一次）
819,840-byte C runtime heap buffer（不占用 LVGL 内置 heap）
    ↓ lv_image_dsc_t (RGB565, stride = 960)
lv_layer_bottom() 上唯一的 lv_image
    ↓
透明 Dashboard / 后续透明页面
```

`ui_background_init()` 是幂等的；背景对象不属于任何 Screen，并且挂在
`lv_layer_bottom()`。普通页面切换不能调用背景创建或销毁。`ui_background_deinit()` 只在
受控 UI 关闭时释放对象和 PC RAM 缓冲。`ui_background_get_obj()` 可用于回归测试其地址
在整个 UI 生命周期中保持不变。

PC backend 在加载 RAW 前验证默认 display format 必须为 `LV_COLOR_FORMAT_RGB565`，不匹配
时拒绝加载。背景没有缩放、zoom、rotation、transform、recolor 或 alpha blending，图像
opacity 为 `LV_OPA_COVER`。

`LV_USE_TJPGD` 与 STDIO 文件系统仍然保留，供未来小 JPEG 或其他资源使用；但 bg_6 的
PC 全局背景没有文件系统 LVGL decoder 路径，也不会产生 TJPGD runtime decode。

所有 Screen 根容器必须设置为 `LV_OPA_TRANSP`。如未来有局部可读性问题，只在局部增加
小面积遮罩，禁止为整屏背景添加半透明黑层。

## 性能优化依据

旧 PC 链路为 JPEG 文件 → LVGL filesystem → TJPGD → `lv_layer_bottom()`。当底层图像的
脏区域需要重绘时，软件 JPEG decode 会参与绘制。旧 Dashboard 还存在一个 16 ms 速度
timer：即使速度已到目标值，仍重复 `lv_label_set_text`、重排读数和设置全部七段条颜色。

第二阶段的改动为：

1. 全屏背景改为启动时一次 RAW 读取，运行期没有背景 JPEG decode、重复文件读取或转换。
2. 速度数字仅在一位小数显示文本变化时更新；速度分段仅在点亮数量变化时更新；速度动画
   到达目标后暂停 timer，新的目标速度到来时再恢复。
3. 指标卡、状态栏和底部状态条仅在目标文本/状态变化时调用 LVGL setter。
4. LVGL 性能叠加层由 300 ms 调整为 500 ms；没有改变 `LV_DEF_REFR_PERIOD=16 ms`、窗口
   分辨率或主循环 sleep 策略。

工程审计确认：没有业务侧 full-screen invalidate、`lv_refr_now`、背景重建、图像 scale/
zoom/transform；也没有大面积半透明 panel。状态栏断开图标的 `LV_OPA_30` 是小图标局部效果，
不是全屏混合。

## 可复现性能测试

`src/perf_benchmark.c` 默认不启用，仅在命令行指定时运行 10 秒并退出：

```powershell
.\bin\main.exe --perf-benchmark=static
.\bin\main.exe --perf-benchmark=data
```

`static` 测试无数据变化；`data` 每 100 ms 生成一组骑行数据，验证持续变更路径。输出的
FPS 是 LVGL `LV_EVENT_REFR_READY` 计数；CPU 是与屏幕性能叠加层相同的
`100 - lv_timer_get_idle()`，即 LVGL handler 忙碌度，不等同于操作系统全部进程 CPU。

## STM32H743IIT6 后续接口

上层 API 不变，`ui_background_load_platform()` 将在 STM32 backend 接入：

```text
W25Q128 (bg_6_q75.jpg)
    ↓ QSPI memory-mapped / DMA
STM32H743 Hardware JPEG decoder
    ↓
DMA2D RGB565 conversion
    ↓
32 MB SDRAM 的 lv_image_dsc_t
    ↓
LVGL / LTDC
```

RGB565 背景缓冲为约 800.6 KiB；双缓冲为约 1.56 MiB，必须放在外部 SDRAM，绝不能放入
STM32 内部 SRAM 或内部 Flash。JPEG 继续存于 16 MB W25Q128，仅在启动或背景切换时解码
一次，不在每帧或切页解码。

DMA2D/JPEG/LTDC 共用 SDRAM 时，优先以 MPU 将 framebuffer 和 DMA 写入区配置为
non-cacheable；若必须 cache，则 DMA 读取 CPU 写入数据前 Clean，CPU 读取 DMA 写入数据前
Invalidate，并按 Cortex-M7 32-byte cache line 对齐范围。LTDC framebuffer 切换必须在
垂直消隐或驱动规定的安全时机完成，避免撕裂。

当前 PC 工程不引入 HAL、QSPI、JPEG peripheral 或 DMA2D 实现。
