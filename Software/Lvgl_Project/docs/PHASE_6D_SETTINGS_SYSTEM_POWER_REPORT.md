# Phase 6D — Settings / System / Power

## 本阶段结果

Phase 6D 已把旧的静态 Settings、System 和 Power 页面迁移到统一的设置交互框架，同时保持既有状态栏、背景资源、MiSans 字体和 Bottom Interaction Surface 架构不变。

## 新增共享组件

- `ui_settings_list`：统一承载 List、Action、Toggle、Slider、Selection 和 Info Row 的视觉与输入行为。
- 整行均为触摸热区，并带按压缩放反馈。
- 编码器顺时针/逆时针移动焦点，按下执行当前行；选中行会自动滚入可视区域。
- Toggle、Slider、Info 和 Action 使用同一尺寸、圆角、边框与字体规范。

## Settings

- WiFi、蓝牙、GPS、省电模式与触摸锁定接入既有异步 Command Service。
- Command 的 Pending / Success / Failed 状态会同步到设置行和统一 Bottom Surface，不在页面上叠加独立弹窗。
- 声音和亮度使用 Bottom Surface `CONTROL` 内容，通过触摸滑条或编码器以 5% 步进调整。
- 页面值只读取 App State；页面不保存第二份系统状态。
- 系统信息与电源设置进入独立 Detail Page。

## System

- 设备型号、UI 版本、LVGL 版本、存储设备、目标平台和系统健康统一为只读 Info Row。
- 当前 PC Simulator 没有真实 TF 卡容量、序列号和 STM32 固件数据源，因此存储状态明确显示为 `PC MOCK`，不伪装为真机数据。
- 预留 `ui_system_page_set_info()`，STM32 平台后续可注入真实设备信息。

## Power

- 关机、重启和取消改为统一 Action Row。
- 关机和重启先在共享 Bottom Surface 中显示 `CONFIRM`，YES 才执行，NO/BACK 取消并恢复页面。
- 鼠标、编码器和返回语义共用同一套 Dock 交互路径。

## Bottom Surface 输入补全

- `CONTROL`：编码器旋转调节，按下关闭。
- `CONFIRM`：编码器切换 NO/YES，按下执行。
- `ACTION`、`NAVIGATION`、`MEDIA`：统一焦点与按下动作。
- BACK 优先关闭当前可关闭的 Interaction 内容，不会误返回页面并把旧 Dock 带到下一页。

## 字体与稳定性

- 新增 Settings/System/Power 全部中文文案纳入字体字形覆盖测试。
- 对 MiSans 当前未包含的字符采用已有字形或英文语义替代，避免再次出现方框字。
- Dock 动画继续只使用位置变化，不创建大尺寸离屏图层。

## 验证

- Debug 构建成功。
- Simulator 连续运行检查无启动崩溃、无新增内存错误日志。
- CTest：13/13 通过。
- 新增 `ui_settings_list_test`，覆盖 Toggle、Slider、反馈状态、编码器选择和执行。

## 后续阶段

按 V4 路线，下一阶段进入 Phase 6E：Ride Summary 商用视觉与保存状态闭环；随后是 Phase 6F History。

