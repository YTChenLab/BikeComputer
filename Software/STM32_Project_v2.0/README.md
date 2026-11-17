# BikeComputer STM32 固件

当前源码版本为 **2.0.0**，状态为初始源码交付基线，记录日期为 2026-11-16。

## 工程内容

- Core 保存 CubeMX 生成的启动、时钟、外设初始化和中断接口。
- App 保存 BSP、设备接口、业务服务、FreeRTOS 任务和 STM32 LVGL 适配。
- Drivers 与 Middlewares 保存 STM32 HAL、CMSIS 头文件和 FreeRTOS 运行库。
- tools/resource_installer.py 提供正式 PC 串口资源安装客户端。

SPL06 驱动、传感器融合与骑行记录相关文件中仍有占位，功能实现待完成。

## 版本与审计入口

- [源码版本](./VERSION)
- [版本日志](./CHANGELOG.md)
- [正式提交范围审计](./docs/GIT_SUBMISSION_AUDIT.md)
- [USART1 资源安装协议](./docs/Q4-3/资源安装协议V1.md)

VERSION 记录本目录源码版本。资源格式、UI ABI 和资源兼容字段分别维护，当前相关字段值均为 1。

## 构建依赖

目标芯片为 STM32H743IITx。当前 Keil 工程记录 ARM Compiler 6.7，以及 Keil.STM32H7xx_DFP.3.1.1。

工作区需要保留以下同级目录结构。

```text
Software/
    Lvgl_Project/
    STM32_Project_v2.0/
```

先用 Lvgl_Project 的资源打包脚本生成 V1 资源包，再通过其 CMake ARM 工具链构建三个静态库。

```text
Lvgl_Project/build-stm32-host/q4-arm/libbike_ui_stm32.a
Lvgl_Project/build-stm32-host/q4-arm/lib/liblvgl.a
Lvgl_Project/build-stm32-host/q4-arm/lib/liblvgl_thorvg.a
```

随后用 MDK-ARM/BikeComputer_STM32.uvprojx 构建 BikeComputer_STM32 目标。所需 CMake、Python、编译器和 Ninja 路径由当前机器环境提供。详细构建顺序与配套文件见提交范围审计。

当前自定义链接脚本为 MDK-ARM/BikeComputer_STM32/BikeComputer_Memory.sct，已在忽略规则中单独保留。

## 交付状态

本次整理完成静态提交范围核对。完整构建、资源安装及实机功能需要后续验证记录。

