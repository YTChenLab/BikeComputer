# STM32 正式源码提交范围审计

## 审计记录

| 项目 | 本次核对结果 |
| --- | --- |
| 日期 | 2026-11-16，Asia/Shanghai |
| 工程目录 | `D:\Postgraduate_Program\111AAALearn_Lab\111AAABikeComputer\Software\STM32_Project_v2.0` |
| 父仓库 | `YTChenLab/BikeComputer` |
| 当前分支 | `feature/navigation-ui` |
| 父仓库当前 HEAD | `6ab8ee6708214b418b371619b61d56d16ef62caa` |
| 源码版本 | `2.0.0`，待审阅源码基线 |
| 本工程拟纳入文件 | 462 个，含本文 |
| 本工程排除文件 | 1471 个 |
| 清单文件大小 | 约 60.65 MiB，统计剔除本文 |
| Keil 显式文件引用 | 116 项，文件均存在且均纳入清单 |
| 已有文件一致性 | 本轮修改前后对 1923 个既有文件进行字节摘要核对，结果一致 |
| Git 暂存区 | 本轮核对前后均为空，索引文件 SHA-256 一致 |

当前结论为正式文件范围核对通过。完整交付还需要配套审阅下文的 Lvgl_Project 正式源码变更，并完成干净检出后的构建验证。

## 本次建立的版本记录

版本号按现有 STM32_Project_v2.0 目录命名确定为 2.0.0。当前目录此前缺少独立固件版本文件与版本日志，本次建立 VERSION 与 CHANGELOG.md，并更新 README。

VERSION 用于记录本目录源码版本。当前 UI_BOARD_RESOURCE_FW_VERSION、资源格式版本和 UI ABI 版本均为 1，各字段按现有兼容含义维护。

本次清单同时包含必要的构建配置、许可证、正式资源安装工具、通信协议和版本文档。

## 本工程拟纳入范围

以下清单路径均相对本文顶部的工程目录。

| 分组 | 文件数 | 范围与理由 |
| --- | ---: | --- |
| 根目录文件 | 6 | .gitignore、VERSION、CHANGELOG.md、README.md、.mxproject、BikeComputer_STM32.ioc |
| App | 62 | 当前 BSP、设备接口、服务、RTOS、应用适配及配置源码 |
| Core | 30 | 当前外设初始化、中断、系统和 FreeRTOS 配置源码 |
| Drivers | 312 | HAL 的 Inc、Src 与许可证，CMSIS Include、芯片 Include 与相关许可证 |
| Middlewares | 44 | 当前 FreeRTOS 运行库、移植层和许可证 |
| MDK-ARM | 5 | Keil 工程、启动汇编、自定义链接脚本、RTE 头文件和共享目标调试配置 |
| tools | 1 | 正式资源安装客户端 resource_installer.py |
| docs | 2 | 当前审计及 USART1 资源安装协议 V1 |

### 需要保留的正式运行逻辑

- 当前 app_main.c、bsp_sdram.c、bsp_qspi.c、resource_install.c 等正式源文件继续整体纳入。
- 启动状态、硬件初始化失败处理、资源包校验与维护诊断具有当前运行路径调用关系。
- LCD 显示自检开关 APP_LCD_DISPLAY_SELF_CHECK 当前为 0U，同文件中的现有可选逻辑随正式源码保留。
- 正式资源安装客户端通过 load_package 导入 Lvgl_Project/tools/validate_resource_package.py，该校验器属于正式安装依赖。
- HAL 与 FreeRTOS 保留当前厂商运行库源码集，CMSIS 保留当前头文件集，内部按芯片或函数继续裁剪的工作留待具体需求确定。

### 需要单独保留的链接脚本

Keil 当前 ScatterFile 指向以下文件，已作为输出目录中的显式例外保留。

```text
MDK-ARM/BikeComputer_STM32/BikeComputer_Memory.sct
```

## 排除范围

| 路径或内容 | 本次处置依据 |
| --- | --- |
| backup/、Core/Inc/Backup/、Core/Src/Backup/、*.bak | 历史源码副本 |
| tests/ | 独立主机测试及测试模型 |
| tools/audit_q4_2.py、tools/audit_q4_3.py | 历史构建与资源验收脚本 |
| 输出目录中的三份 sdram_*host_verify.c | 历史 SDRAM 主机诊断程序 |
| .o、.d、.crf、.axf、.hex、.map、.lnp、.dep、.lst、构建 .htm、.exe、.dll | 构建或测试产物 |
| .vscode/、.uvoptx、.uvguix.*、EventRecorderStub.scvd | 本机配置、会话状态或当前未引用的事件记录占位 |
| CMSIS 的 DSP、NN、Core、Core_A、RTOS、RTOS2、docs | 当前 Keil 引用与正式应用源码中未发现依赖 |
| CMSIS Device 的 Source/Templates | 当前使用的系统源文件与启动汇编已分别位于 Core 和 MDK-ARM |
| 旧学习笔记、面试总结、Q4 阶段交付报告、基线及验收 JSON | 历史过程资料 |
| docs/Q4-3.zip、日志、缓存 | 重复归档或运行生成文件 |

这些文件继续保留在本机。本目录 .gitignore 通过允许列表管理当前提交范围。

## Lvgl_Project 配套正式源码

当前 Keil 链接参数依赖以下构建输出。

```text
Lvgl_Project/build-stm32-host/q4-arm/libbike_ui_stm32.a
Lvgl_Project/build-stm32-host/q4-arm/lib/liblvgl.a
Lvgl_Project/build-stm32-host/q4-arm/lib/liblvgl_thorvg.a
```

当前正式 UI 库的来源包含下列尚未提交的生产源码变更。建议在后续实际交付时一并审阅并纳入匹配版本。路径以同级 Lvgl_Project 为根。

```text
cmake/stm32.cmake
config/stm32/lv_conf.h
src/platform/stm32/ui_stm32_startup.c
src/platform/stm32/ui_stm32_startup.h
src/platform/stm32/ui_stm32_resources.c
src/platform/stm32/ui_stm32_resources.h
src/platform/stm32/ui_stm32_resources_internal.h
src/ui/assets/ui_home_watermark.h
src/ui/core/ui_app.c
src/ui/core/ui_memory.h
src/ui/pages/ui_boot_prts_assets.h
tools/build_resource_package.py
tools/generate_stm32_resources.py
tools/validate_resource_package.py
```

其中 ui_app.c 包含骑行菜单交互变更，属于共享 UI 行为，审阅时需要确认其符合当前固件版本目标。src/main.c 当前差异仅为文件末尾空白，本次配套清单排除该项。

Lvgl_Project 中的新增主机测试、测试夹具、audit_stm32_resources.py、历史 Q4-1 报告和 build-stm32-host 产物均排除在本次配套清单之外。正式 ARM 构建使用 UI_BOARD_CONTRACT_TESTS=OFF，该选项的当前默认值为 OFF。主机契约测试另有对应的测试源码依赖。

本目录的 .gitignore 作用范围止于 STM32_Project_v2.0。后续暂存时按本节明确的生产源码路径选择 Lvgl_Project 配套文件。

## 构建顺序与资源包来源

1. 保留 Software/Lvgl_Project 与 Software/STM32_Project_v2.0 的同级关系。
2. 在干净检出的 Lvgl_Project 中使用 tools/build_resource_package.py 生成 V1 的 BIN、JSON 和资源 ID 头文件，再使用 tools/validate_resource_package.py 校验。现有工作区资源包摘要见下节。
3. 使用 cmake/toolchains/armclang.cmake 配置 ARM 构建，UI_PLATFORM 设为 STM32，并提供本机 UI_ARMCLANG_ROOT、Ninja 路径。输出目录与当前 Keil 参数保持为 build-stm32-host/q4-arm。
4. 构建 bike_ui_stm32 目标，生成上述三个静态库。
5. 使用 Keil ARM Compiler 6 构建 MDK-ARM/BikeComputer_STM32.uvprojx 中的 BikeComputer_STM32 目标。
6. 使用正式资源安装客户端校验资源包，随后按实际设备接口完成资源安装和功能验证。

当前工程记录 ARM Compiler 6.7 与 Keil.STM32H7xx_DFP.3.1.1。本机工具安装路径由执行环境提供。

### 当前工作区 V1 资源包摘要

以下摘要用于核对当前工作区资源来源。重新生成后的字节一致性和完整构建需要实际执行验证。

| 相对 Lvgl_Project 的路径 | SHA-256 |
| --- | --- |
| `build/resource_package/bike_ui_resources_v1.bin` | `9c5c6fedbca69b1306aa7938a441247ea038221dacc24e64235b31d032feda94` |
| `build/resource_package/bike_ui_resources_v1.json` | `e4f31ed4a699ed321761ca0fab000fa9bb0b664707a5fd718d7ed127ec05df9c` |
| `build/resource_package/resource_ids_generated.h` | `0d4c9fdb9e5a06186e6f9db6f2d8f47ee1fff4130d5718ea9303c25fca2bae35` |

## 当前完成程度与待验证项

- 本次执行静态目录、Git 跟踪状态、Keil 显式引用、忽略规则和文件字节一致性检查。
- 完整重建、干净检出构建、固件烧录、资源安装与板级功能仍待执行验证。
- 源码中的 SPL06、传感器融合与骑行记录存在空文件和任务占位。版本日志按源码现状记录完成程度。
- 历史报告中的测试或硬件结论需要沿原始证据追溯，本次审计结果仅覆盖本轮实际核对内容。
- VERSION 当前用于源码交付记录。后续若需要串口或设备界面显示该版本，需要另行接入运行时版本接口。

### 当前空文件

```
App/Common/app_types.h
App/Config/app_config.h
App/Config/board_config.h
App/Device/spl06.c
App/Device/spl06.h
App/Service/ride_recorder.c
App/Service/ride_recorder.h
App/Service/sensor_fusion.c
App/Service/sensor_fusion.h
```

## 本轮文件整理记录

- 新建 .gitignore。
- 新建 VERSION。
- 新建 CHANGELOG.md。
- 新建 docs/GIT_SUBMISSION_AUDIT.md。
- 更新 README.md。

既有固件源码、厂商运行库、工程配置和外部资源包已通过修改前后的字节摘要核对。Git 暂存、提交、标签与推送操作留待用户审阅后安排。

## 完整拟纳入文件清单

<details>
<summary>展开当前工程完整文件清单</summary>

```text
.gitignore
.mxproject
App/BSP/bsp_adc.c
App/BSP/bsp_adc.h
App/BSP/bsp_dma2d.c
App/BSP/bsp_dma2d.h
App/BSP/bsp_gnss.c
App/BSP/bsp_gnss.h
App/BSP/bsp_input.c
App/BSP/bsp_input.h
App/BSP/bsp_lcd.c
App/BSP/bsp_lcd.h
App/BSP/bsp_qspi.c
App/BSP/bsp_qspi.h
App/BSP/bsp_resource_boot.c
App/BSP/bsp_resource_boot.h
App/BSP/bsp_sdram.c
App/BSP/bsp_sdram.h
App/BSP/bsp_uart.c
App/BSP/bsp_uart.h
App/Common/app_events.h
App/Common/app_types.h
App/Config/app_config.h
App/Config/board_config.h
App/Config/qspi_partition.h
App/Config/resource_package_format.h
App/Device/gnss.c
App/Device/gnss.h
App/Device/spl06.c
App/Device/spl06.h
App/RTOS/app_rtos.c
App/RTOS/app_rtos.h
App/Service/battery_service.c
App/Service/battery_service.h
App/Service/gnss_service.c
App/Service/gnss_service.h
App/Service/input_router.c
App/Service/input_router.h
App/Service/input_service.c
App/Service/input_service.h
App/Service/resource_install.c
App/Service/resource_install.h
App/Service/resource_manager.c
App/Service/resource_manager.h
App/Service/resource_package.c
App/Service/resource_package.h
App/Service/resource_storage.c
App/Service/resource_storage.h
App/Service/ride_recorder.c
App/Service/ride_recorder.h
App/Service/sensor_fusion.c
App/Service/sensor_fusion.h
App/System/app_main.c
App/System/app_main.h
App/System/power_service.c
App/System/power_service.h
App/System/resource_system.c
App/System/resource_system.h
App/System/ui_board.c
App/System/ui_board.h
App/System/ui_display.c
App/System/ui_display.h
App/System/ui_gnss_bridge.c
App/System/ui_gnss_bridge.h
BikeComputer_STM32.ioc
CHANGELOG.md
Core/Inc/FreeRTOSConfig.h
Core/Inc/adc.h
Core/Inc/dma.h
Core/Inc/dma2d.h
Core/Inc/fmc.h
Core/Inc/gpio.h
Core/Inc/i2c.h
Core/Inc/ltdc.h
Core/Inc/main.h
Core/Inc/quadspi.h
Core/Inc/stm32h7xx_hal_conf.h
Core/Inc/stm32h7xx_it.h
Core/Inc/tim.h
Core/Inc/usart.h
Core/Src/adc.c
Core/Src/dma.c
Core/Src/dma2d.c
Core/Src/fmc.c
Core/Src/freertos.c
Core/Src/gpio.c
Core/Src/i2c.c
Core/Src/ltdc.c
Core/Src/main.c
Core/Src/quadspi.c
Core/Src/stm32h7xx_hal_msp.c
Core/Src/stm32h7xx_hal_timebase_tim.c
Core/Src/stm32h7xx_it.c
Core/Src/system_stm32h7xx.c
Core/Src/tim.c
Core/Src/usart.c
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h723xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h725xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h730xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h730xxq.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h733xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h735xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h742xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h743xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h745xg.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h745xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h747xg.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h747xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h750xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h753xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h755xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h757xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h7a3xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h7a3xxq.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h7b0xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h7b0xxq.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h7b3xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h7b3xxq.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h7xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/system_stm32h7xx.h
Drivers/CMSIS/Device/ST/STM32H7xx/LICENSE.txt
Drivers/CMSIS/Include/cmsis_armcc.h
Drivers/CMSIS/Include/cmsis_armclang.h
Drivers/CMSIS/Include/cmsis_armclang_ltm.h
Drivers/CMSIS/Include/cmsis_compiler.h
Drivers/CMSIS/Include/cmsis_gcc.h
Drivers/CMSIS/Include/cmsis_iccarm.h
Drivers/CMSIS/Include/cmsis_version.h
Drivers/CMSIS/Include/core_armv81mml.h
Drivers/CMSIS/Include/core_armv8mbl.h
Drivers/CMSIS/Include/core_armv8mml.h
Drivers/CMSIS/Include/core_cm0.h
Drivers/CMSIS/Include/core_cm0plus.h
Drivers/CMSIS/Include/core_cm1.h
Drivers/CMSIS/Include/core_cm23.h
Drivers/CMSIS/Include/core_cm3.h
Drivers/CMSIS/Include/core_cm33.h
Drivers/CMSIS/Include/core_cm35p.h
Drivers/CMSIS/Include/core_cm4.h
Drivers/CMSIS/Include/core_cm7.h
Drivers/CMSIS/Include/core_sc000.h
Drivers/CMSIS/Include/core_sc300.h
Drivers/CMSIS/Include/mpu_armv7.h
Drivers/CMSIS/Include/mpu_armv8.h
Drivers/CMSIS/Include/tz_context.h
Drivers/CMSIS/LICENSE.txt
Drivers/STM32H7xx_HAL_Driver/Inc/Legacy/stm32_hal_legacy.h
Drivers/STM32H7xx_HAL_Driver/Inc/Legacy/stm32h7xx_hal_eth_ex_legacy.h
Drivers/STM32H7xx_HAL_Driver/Inc/Legacy/stm32h7xx_hal_eth_legacy.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32_assert_template.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_adc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_adc_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_cec.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_comp.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_conf_template.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_cordic.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_cortex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_crc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_crc_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_cryp.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_cryp_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_dac.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_dac_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_dcmi.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_def.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_dfsdm.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_dfsdm_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_dma.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_dma2d.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_dma_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_dsi.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_dts.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_eth.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_eth_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_exti.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_fdcan.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_flash.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_flash_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_fmac.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_gfxmmu.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_gpio.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_gpio_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_hash.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_hash_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_hcd.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_hrtim.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_hsem.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_i2c.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_i2c_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_i2s.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_i2s_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_irda.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_irda_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_iwdg.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_jpeg.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_lptim.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_ltdc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_ltdc_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_mdios.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_mdma.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_mmc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_mmc_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_nand.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_nor.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_opamp.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_opamp_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_ospi.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_otfdec.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_pcd.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_pcd_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_pssi.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_pwr.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_pwr_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_qspi.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_ramecc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_rcc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_rcc_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_rng.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_rng_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_rtc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_rtc_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_sai.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_sai_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_sd.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_sd_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_sdio.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_sdram.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_smartcard.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_smartcard_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_smbus.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_smbus_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_spdifrx.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_spi.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_spi_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_sram.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_swpmi.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_tim.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_tim_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_uart.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_uart_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_usart.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_usart_ex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_wwdg.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_adc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_bdma.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_bus.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_comp.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_cordic.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_cortex.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_crc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_crs.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_dac.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_delayblock.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_dma.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_dma2d.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_dmamux.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_exti.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_fmac.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_fmc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_gpio.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_hrtim.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_hsem.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_i2c.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_iwdg.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_lptim.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_lpuart.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_mdma.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_opamp.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_pwr.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_rcc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_rng.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_rtc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_sdmmc.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_spi.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_swpmi.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_system.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_tim.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_usart.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_usb.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_utils.h
Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_wwdg.h
Drivers/STM32H7xx_HAL_Driver/LICENSE.txt
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_adc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_adc_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cec.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_comp.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cordic.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_crc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_crc_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cryp.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cryp_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dac.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dac_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dcmi.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dfsdm.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dfsdm_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma2d.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dsi.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dts.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_eth.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_eth_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_exti.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_fdcan.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_fmac.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gfxmmu.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hash.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hash_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hcd.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hrtim.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hsem.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2s.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2s_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_irda.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_iwdg.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_jpeg.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_lptim.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_ltdc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_ltdc_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdios.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mmc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mmc_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_msp_template.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_nand.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_nor.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_opamp.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_opamp_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_ospi.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_otfdec.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pssi.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_qspi.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_ramecc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rng.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rng_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rtc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rtc_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sai.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sai_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sd.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sd_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sdio.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sdram.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_smartcard.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_smartcard_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_smbus.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_smbus_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_spdifrx.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_spi.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_spi_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sram.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_swpmi.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_timebase_rtc_alarm_template.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_timebase_rtc_wakeup_template.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_timebase_tim_template.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_usart.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_usart_ex.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_wwdg.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_adc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_bdma.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_comp.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_cordic.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_crc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_crs.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_dac.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_delayblock.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_dma.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_dma2d.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_exti.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_fmac.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_fmc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_gpio.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_hrtim.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_i2c.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_lptim.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_lpuart.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_mdma.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_opamp.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_pwr.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_rcc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_rng.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_rtc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_sdmmc.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_spi.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_swpmi.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_tim.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_usart.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_usb.c
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_utils.c
MDK-ARM/BikeComputer_STM32.uvprojx
MDK-ARM/BikeComputer_STM32/BikeComputer_Memory.sct
MDK-ARM/DebugConfig/BikeComputer_STM32_STM32H743IITx_1.1.0.dbgconf
MDK-ARM/RTE/_BikeComputer_STM32/RTE_Components.h
MDK-ARM/startup_stm32h743xx.s
Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os.h
Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.h
Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/freertos_mpool.h
Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/freertos_os2.h
Middlewares/Third_Party/FreeRTOS/Source/History.txt
Middlewares/Third_Party/FreeRTOS/Source/LICENSE
Middlewares/Third_Party/FreeRTOS/Source/croutine.c
Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
Middlewares/Third_Party/FreeRTOS/Source/include/FreeRTOS.h
Middlewares/Third_Party/FreeRTOS/Source/include/FreeRTOSConfig_template.h
Middlewares/Third_Party/FreeRTOS/Source/include/StackMacros.h
Middlewares/Third_Party/FreeRTOS/Source/include/atomic.h
Middlewares/Third_Party/FreeRTOS/Source/include/croutine.h
Middlewares/Third_Party/FreeRTOS/Source/include/deprecated_definitions.h
Middlewares/Third_Party/FreeRTOS/Source/include/event_groups.h
Middlewares/Third_Party/FreeRTOS/Source/include/list.h
Middlewares/Third_Party/FreeRTOS/Source/include/message_buffer.h
Middlewares/Third_Party/FreeRTOS/Source/include/mpu_prototypes.h
Middlewares/Third_Party/FreeRTOS/Source/include/mpu_wrappers.h
Middlewares/Third_Party/FreeRTOS/Source/include/portable.h
Middlewares/Third_Party/FreeRTOS/Source/include/projdefs.h
Middlewares/Third_Party/FreeRTOS/Source/include/queue.h
Middlewares/Third_Party/FreeRTOS/Source/include/semphr.h
Middlewares/Third_Party/FreeRTOS/Source/include/stack_macros.h
Middlewares/Third_Party/FreeRTOS/Source/include/stream_buffer.h
Middlewares/Third_Party/FreeRTOS/Source/include/task.h
Middlewares/Third_Party/FreeRTOS/Source/include/timers.h
Middlewares/Third_Party/FreeRTOS/Source/list.c
Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/portmacro.h
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_1.c
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_2.c
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_3.c
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_5.c
Middlewares/Third_Party/FreeRTOS/Source/portable/RVDS/ARM_CM4F/port.c
Middlewares/Third_Party/FreeRTOS/Source/portable/RVDS/ARM_CM4F/portmacro.h
Middlewares/Third_Party/FreeRTOS/Source/queue.c
Middlewares/Third_Party/FreeRTOS/Source/readme.txt
Middlewares/Third_Party/FreeRTOS/Source/st_readme.txt
Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
Middlewares/Third_Party/FreeRTOS/Source/tasks.c
Middlewares/Third_Party/FreeRTOS/Source/timers.c
README.md
VERSION
docs/GIT_SUBMISSION_AUDIT.md
docs/Q4-3/资源安装协议V1.md
tools/resource_installer.py
```

</details>
