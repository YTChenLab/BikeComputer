# 更新记录

本项目遵循语义化版本编号，Git 标签格式为 `vMAJOR.MINOR.PATCH`。

## 未发布（目标 v1.1.2）

- 新增固定四项快捷控制条 V1：WiFi、蓝牙、声音、亮度，使用真实 LVGL 对象和单一 Focus Capsule。
- 新增 `CLOSED → QUICK_BAR → FULL_CONTROL_CENTER` 系统控制帘状态机；Touch 下拉/上滑和 PC `M` 键统一使用同一状态来源。
- 快捷控制条支持整项 Touch、Encoder 循环 Focus、Press 执行，以及 WiFi/蓝牙 Pending、Failed 和权威 App State 状态显示。
- 声音与亮度从快捷控制条进入共享 Bottom Interaction Surface CONTROL，避免在顶部建立第二套数值调节逻辑。
- 增加 Bottom Surface Gesture Ownership，底部 CONTROL 拖动期间不会被第二次下拉手势抢占。
- 增加胶囊 Focus、按压和 Surface 切换 Motion Token；动画仅作用于局部小对象。
- 新增系统控制帘与快捷控制条自动化测试，当前测试总数提升为 22 项。
- 将 UI V5 作为 v1.1.2 后续开发交接文档纳入工程；V5 是文档修订号，不替代软件版本号。
- 新增系统控制任务反馈所有权模型：系统控制帘打开时由顶部原位反馈，关闭时将仍 Pending 的任务下沉到唯一 Bottom Interaction Surface。
- 重新打开快捷控制条时会回收底部 Pending 命令反馈，禁止同一任务同时出现在顶部和底部。
- P0 Critical 与 P1 Riding Safety 事件会先退出 Bottom CONTROL、自动收起系统控制帘，再由 Bottom Surface 抢占；P0/P1 活跃期间禁止重新打开控制帘。
- 下沉任务完成后，Success/Failed/Timeout/Rejected 只更新当前 owner；底部 owner 时顶部状态强制回到 Idle，避免重新打开后出现重复失败反馈。
- 电源关闭、重启和 PC 电源切换会同步清空反馈 ownership，防止重新开机后恢复过期命令状态。
- 新增顶部/底部 handoff、P1 抢占和 restore-stack 清理测试，当前测试总数提升为 23 项。
- 完整控制中心改为 462 px 紧凑面板，下半主页面保持可见，同时使用透明全屏输入层阻止点击穿透。
- 声音和亮度改为 4 px 横向 Track、12 px Thumb 和百分比读数；Touch 直接拖动，Encoder Press 进入共享 Bottom CONTROL 精调。
- 新增 Focus Capsule 拉伸吸附、Toggle 回弹、Pending 单次 Pulse、横条 Fill/Thumb 推进和统一 Control Expand Token；所有动画只作用于局部对象或位置属性。
- Home 隐藏尚未实现的 Achievements 入口；Legacy Profile 明确标记为 `LEGACY / DEPRECATED` 并移除无功能成就按钮。
- 数值命令失败时在原 Bottom CONTROL 中恢复 App State 权威值并显示失败信息，不创建第二张失败卡。
- 新增 250 次局部动画与控制中心状态压力循环，检查对象数不增长、动画归零和无大图层内存错误；自动化测试总数提升为 24 项。
- 将系统控制中心收敛为单一 `ui_control_center` Surface：`CLOSED → COMPACT → EXPANDED`，删除产品代码中的旧 Quick Bar/Full Overlay 语义。
- Compact 与 Expanded 共用同一组 LVGL 对象，通过 Panel 高度 Morph 切换；Expanded 上滑直接关闭，声音/亮度在控制中心 Inline Adjust，不再呼出 Bottom CONTROL。
- 修复 Pointer MOVE 期间重复重启滑条动画造成的抽搐；拖动时仅更新几何，状态变更时才启动数值动画。
- 增加 Compact/Morph/Expanded 对象稳定性、焦点和 50 次形态往返测试；当前 CTest 为 23 项全部通过。

## v1.1.1 - 2026-09-19

- 修复 History 空状态和声音/亮度交互坞新增文案的 MiSans 缺字方框，并重新生成完整运行时 UI 字体子集。
- 将 MiSans UI 子集生成接入 CMake 源码依赖；新增中文文案后会自动重新生成字体及字符清单，不再依赖手工补字。
- 新增全运行时字符覆盖测试，并将 History 空状态与 Interaction Dock 正文改为可换行固定高度布局，避免长中文被单行裁切。
- 新增 `ui_app_state`，统一管理页面路由、骑行状态、状态栏、连接、控制中心、媒体与导航状态。
- 为 `ui_router` 增加页面创建、进入、离开和销毁生命周期，并暂停隐藏页面的刷新定时器。
- 建立全局状态栏绑定注册表，消除各页面各自保存时间、电量和连接状态的问题。
- 将 PC 鼠标、滚轮和键盘输入转换为平台无关的 UI 语义事件，为 STM32 输入适配预留统一接口。
- Dynamic Card 音乐状态改由全局状态驱动，触摸和 Encoder 共用同一份播放状态。
- 新增 App State 与 Router Lifecycle 自动化测试。
- 新增平台状态适配器、导航数据邮箱和语义输入队列，所有生产者数据统一在 UI 线程落地。
- STM32 适配层新增状态、导航与输入提交接口，不再要求硬件任务直接调用页面或 LVGL API。
- Ride Data Model 移除重复的 WiFi/蓝牙字段，连接状态只由 App State 管理。
- SDL 键鼠改为写入语义输入队列，连续触摸移动会自动合并且 RELEASE/BACK 不会因队列满而丢失。
- Router 生命周期测试增加 250 次页面往返压力验证。
- 设置 `CMAKE_POLICY_DEFAULT_CMP0177`，消除内置 LVGL 子工程的 CMake policy 警告。
- 新增 `ui_command_service`，统一 UI 到 PC/STM32 的异步命令、执行结果、超时、队列合并和重复请求抑制。
- WiFi、蓝牙、GPS、省电模式、声音、亮度、音乐控制及骑行请求不再由 UI 直接伪造硬件成功状态。
- STM32 服务端口新增命令派发与带权威状态快照的完成回传接口。
- 固化 Bottom Interaction Surface 架构约束：Dynamic Card 与 Interaction Dock 共用唯一底部视觉容器，不允许叠加独立 Popup。
- 建立 `ui/design` 第一版语义 Token 与公共 Surface Style，保留旧颜色和尺寸宏作为迁移期兼容别名。
- 新增 `ui_bottom_surface`，统一管理底部位置、背景、边框、圆角、Mini/Normal/Expanded 尺寸、内容模式和优先级恢复栈。
- Dynamic Card 已迁入 Bottom Surface 的 Normal Content Host，四张卡不再分别控制底层视觉容器。
- 新增 Bottom Surface 对象树、尺寸状态、优先级抢占与恢复自动化测试。
- 将 INFO/ACTION/CONFIRM/CONTROL/NAVIGATION/MEDIA 全部迁入唯一 Bottom Surface，删除旧 Interaction Dock 独立 top-layer 视觉根对象。
- 建立 Mini/Normal 类型映射、P0/P1/P2 Surface 优先级视觉和原子 replace 同步。
- 将设备设置命令的 Pending/Success/Failed/Timeout/Rejected 映射为统一 Interaction 内容，结束后恢复此前 Dynamic Card。
- 扩展 Dock 自动化测试，覆盖单一 top-layer 根、命令状态反馈、自动关闭和 Dynamic Card 恢复。
- Phase 5.1 将 Bottom Surface 挂入 Dashboard 固定页面 Slot，取消 INFO/ACTION 的 Mini 位移和内容向上弹入动画。
- Dynamic Card 与 Interaction 内容改为在同一 464×226 容器中原位交叉淡入，并增加父对象、坐标、尺寸不变回归测试。
- 修复 Interaction Dock 类型标记误用纯数字状态栏字体导致 `INFO/ACTION/CONFIRM` 显示为方框的问题，并增加 Badge 字形覆盖测试。
- Phase 6 为 Navigation、Summary、Profile、Settings、System 和 Power 建立页面内 Bottom Surface Slot，并接入 Router 进入/离开生命周期。
- 删除 Bottom Surface 的 `lv_layer_top()` 备用挂载路径；无 Slot 页面保持隐藏，Interaction 状态在跨页面挂载时继续保留。
- Phase 6A 新增 UI V4 Home / Ride Hub：用户卡、累计骑行数据、开始骑行/导航入口、最近骑行和功能入口均由真实 LVGL 对象构成。
- 新增 `ui_home_data_model`，将 Home 展示数据与页面对象解耦，并增加更新、订阅和边界值自动化测试。
- Home 成为启动默认根路由；非骑行态主链为 Home 与 Navigation，骑行态仍保持 Dashboard 与 Navigation。
- 开始骑行流程修正为 Home → Dashboard → PRE_RIDE → GPS 搜索 → GPS Ready → GO → RIDING，异步命令成功后才进入正式骑行态。
- 删除控制中心内重复的开始、暂停和结束骑行按钮，骑行业务入口改由 Home 与 Bottom Interaction Surface 统一承载。
- 修复 Home 按压样式向 LVGL 传入空 transition descriptor 导致首次点击 SIGSEGV 的运行时故障。
- 扩展 MiSans UI 20 字号字形集，覆盖 Home 与 GPS 流程新增中文文案。
- 新增平台无关的返回键单击、双击、长按识别器：单击返回上一级、双击手动暂停/恢复、长按进入结束骑行确认。
- 新增自动暂停控制器：持续低速后自动暂停，恢复移动后自动恢复；手动暂停不会被自动恢复覆盖。
- 暂停、恢复、结束确认、保存与失败状态全部在同一个 Bottom Interaction Surface 内原位刷新，不创建独立弹窗。
- PC Simulator 增加 `S` 键移动/停止模拟，并补充返回键识别与自动暂停自动化测试。
- 修复暂停/恢复通用 Pending 卡未被骑行结果卡替换，导致交互坞跨页面残留的问题；骑行命令全流程统一复用 `DOCK_ID_RIDE`。
- 消除 `ui_dock` 与 `ui_bottom_surface` 的双恢复栈：业务抢占/恢复只由 Dock 管理，Surface 只渲染当前内容，结束保存后不再恢复旧卡或留下空黄框。
- Bottom Interaction Surface 采用位置与宽高几何形变实现收缩回弹；禁止对大尺寸 Surface/Host 使用整体缩放或透明动画，避免 LVGL 创建约 355 KiB 的 ARGB8888 离屏图层而耗尽 1 MiB UI 堆。
- 增加 NO 取消、YES 结束保存、暂停/恢复 Pending 生命周期和 Surface 强制复位回归测试。
- Phase 6B 重构 Quick Control Center：区分无线功能的开启与连接状态，增加 Touch Lock、Battery Status、Encoder 焦点/调节和卡片内 Pending/Failed 反馈。
- 声音与亮度滑条改为拖动时本地预览、松手后仅提交一次异步命令；Control Center 命令结果不再生成关闭覆盖层后才出现的过期 Dock 卡。
- Touch Lock 纳入 App State、Command Service、PC/STM32 状态适配契约；锁定后触摸序列被 UI 层阻止，物理返回键和平台快捷键仍保持可用。
- 新增 Control Center 真实渲染、Encoder、滑条、整卡点击和离屏图层内存回归测试。
- Phase 6C 保留既有 Navigation 页面结构，将地图、2×2 数据卡、控件边框、圆角和间距统一到 Design Token 与共享布局常量。
- 导航鼠标滚轮/Encoder 缩放及回中动作增加统一 Focus 反馈，触摸与语义输入复用同一控件回调。
- 导航方向变化现在发布 P1 安全优先级事件，由 Bottom Interaction Surface 原位抢占并在超时后恢复，不再叠加独立弹窗。
- 转向内容更新移除整块透明度动画，改为低内存纵向位移动画；新增导航布局、缩放、Focus、转向事件和 Surface 恢复回归测试。
- 修复 Quick Control Center 状态副文本误用 ASCII-only Montserrat 字体导致中文显示为方框的问题，并补充中文状态字形测试。
- 修复 Navigation Interaction 将中文“查看”写入 Montserrat 按钮字体导致方框的问题，导航操作按钮统一使用 MiSans Surface Action 字体。
- Home 的 Achievement 未开发入口不再发布占位 `FEATURE_INFO`，避免无实际页面时抢占并残留 Bottom Interaction Surface；History 已在 Phase 6F 接入真实页面。
- 修复 Navigation 等无 Dynamic Card 页面在 Interaction 退出时先切回 Dynamic Host、造成底层卡片闪现的问题；退出动画保持当前 Interaction 内容，完成后原子隐藏全部 Surface Host。
- Phase 6F 新增骑行历史模型与 Repository，统一管理最近 100 条骑行记录、ID 去重、容量淘汰、保存失败重试和总里程/时间/爬升聚合。
- PC 骑行记录改为真实文件持久化：显式 little-endian 编码、格式版本、CRC32、临时文件提交、备份替换与启动恢复，默认保存到 `runtime/ride_history.dat`。
- 结束骑行保存改为事务式流程：文件提交成功后才更新内存记录、Home 累计数据和 Summary `SAVED` 状态；失败时保留 pending 供重试，不提前污染统计。
- 新增 History 页面，提供总骑行次数、累计距离、累计时间、空状态和最近 50 条可滚动记录，并支持 Touch 与 Encoder 焦点浏览。
- Home 的历史入口已接入 `UI_ROUTE_HISTORY`，虚构累计骑行数据改为启动时从 Repository 聚合；保存成功和重新进入 Home 时同步刷新。
- STM32 服务层新增历史存储后端注册接口，为后续 FatFs/littlefs 适配保留平台边界；本阶段未宣称 TF 卡真机验证完成。
- 新增历史模型、Repository、PC 文件后端和 History 页面自动化测试，覆盖 100 条淘汰、幂等提交、失败重试、重启加载、格式版本、CRC 损坏、截断、备份恢复和 LVGL 内存回归。

## v1.0.0 - 2026-09-15

- 建立 BikeComputer GitHub 总仓库目录结构。
- 导入当前 LVGL PC Simulator 与 BikeComputer UI 完整源码快照。
- 将当前有本地修改的 LVGL `9.3.0-dev` 源码纳入仓库，保证当前 UI 可复现。
- 固化 Anaconda/MSYS2 UCRT64 的 CMake preset、VS Code 工作区和测试入口。
- 纳入当前 UI、字体、背景、图标、导航原型、Core Layer、Dock 和测试代码。
