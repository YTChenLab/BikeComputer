# 更新记录

本项目遵循语义化版本编号，Git 标签格式为 `vMAJOR.MINOR.PATCH`。

## v1.4.0 - 2026-10-02

- 归档 v1.3.1 之后的当前 LVGL UI 基线，统一 `VERSION`、系统信息版本及仓库/工程 README 为 v1.4.0；保留候选阶段和旧版本的迭代记录。
- 固化纯黑画布、中性灰卡片、白色文字与黄色主题；首页和个人主页按确认的视觉基准重排，改善数字/单位基线、长内容、活动矩阵和卡片间距。
- 按最终产品布局统一 16 px 底边距与 72 px 底部操作行高；图标和文字组按实际字体对齐，性能信息作为半透明顶层浮层，不占用产品布局高度。
- 纳入现有 PRTS 两幕开机动画与首页错开入场；连续阻尼弹簧用于胶囊、焦点、控制面板和底部容器，支持打断、反向、减少动态效果与对象销毁清理。
- 骑行速度恢复按档位变色的平行四边形进度条，扩大心率/功率区间表达；新增巡航/训练/爬坡六格指标编辑、可视预览、滚动条与配置持久化。
- 纳入独立设备与传感器页面、收藏路线空态与 16 项累计成就；个人统计刷新、30 天活动与最近记录绑定真实历史模型，未接入功能显示真实反馈。
- 完善骑行开始/暂停/恢复/结束/保存/失败重试以及关屏/电源状态，增强异步命令 ACK、数据来源/质量/时间/有效期和真实零值处理。
- 纳入后台历史保存、幂等恢复、CRC/备份、累计统计与分页读取；保留 STM32 任务安全数据、命令、存储及五键/编码器输入接口。
- PC 模拟器增加约 3 英寸尺寸估算、50%/原始尺寸切换与校准，内部画布保持 480×854；纳入回归测试、实际 LVGL 预览和接口示例。
- 导航仍为原型和接口基线：完整目的地流程、真实道路地图、路线规划/偏航重算及 BLE 实际连接未实现。本次仅版本归档，不开展后续导航开发。
- 本次发布验证：CMake/Ninja 构建成功，既有 CTest 33/33 通过；6 个 PRTS 开机源文件在发布整理前后 SHA-256 一致。详细记录见 [v1.4.0 发布记录](docs/V1.4.0_RELEASE_NOTES.md)。

## v1.4.0-rc1 - 2026-10-01

- 2026-10-02：按最终产品布局统一页面底边距 16 px、底部操作行高 72 px；性能信息改为半透明最上层浮层，不分配专用底部空间。完善图标、按钮及动态字体下的双行文字组居中，重新分配个人页与布局预览空间；固定报告完成按钮并修正编码器滚动末端，保留电源取消的触摸与编码器操作。构建成功，CTest 33/33 通过，225 组整屏／分块刷新帧一致，PRTS 开机源文件保持不变。说明见 `docs/全局底部与对齐规范_20261002.md`。

- 2026-10-02：上一轮个人主页重新分配纵向空间，放大统计/活动卡片与底部入口，统一主卡片 10 px、入口行 8 px 间距；维持数字/单位基线、真实数据与已有动效。该轮底部方案已由上方的产品布局规则替代。构建成功，CTest 33/33 通过。

- 2026-10-02：PC 模拟器增加 3 英寸估算、50% / 原始尺寸和 +/- 校准；固定内部 480×854，统一本机点击与手势坐标转换，保留性能信息和偏好持久化。说明见 `docs/模拟器尺寸与校准_20261002.md`。

- 2026-10-02：完成最后一轮 UI 审查，统一个人页数值基线、长文本、只读诊断行、列表滚动条、报告/导航卡片和黄色保存主按钮；删除屏幕外兼容文本及无效个人身份跳转，报告总结只显示有依据的统计与实际保存状态。
- 修正 30 天活动误用终身累计、日期缺失状态和首页最近记录依赖历史焦点的问题；活动改为逐条读取，避免在 UI 栈复制 100 条含轨迹的记录。
- 首页采用共用中心按压/释放动画；完善减少动态效果策略，并修复报告快速连续旋转丢步、区间跨边界、触摸选行编辑和弹窗全局按键退出。
- 外设搜索在本页反馈真实未接入/蓝牙关闭状态，收藏路线保留独立空态；补充硬件接入接口与统计窗口限制。构建成功，CTest 32/32 通过，225 组整屏/分块刷新帧逐像素一致。详见 `docs/最后一轮UI审查与优化_20261002.md`。

- 按用户提供的 HOME／Profile 最终视觉基准重排两页：HOME 改为行动中心并恢复收藏路线入口，最近骑行在真实轨迹接入前使用中性路线图标；Profile 改为 2×2 总数据、成就进度、30 天活动矩阵和五个功能入口，保持真实数据绑定与触摸／编码器焦点。
- 恢复模拟器右下角默认显示的 FPS、CPU 占用估计与渲染／刷屏耗时；保留 `UI_SHOW_PERFORMANCE=0` 显式隐藏开关，直接运行和调试启动均生效。
- 根据用户 LOTUS.HMI 参考图收敛为纯黑／中性灰／黄色主题；默认不加载旧颗粒背景，保留可选背景资源端口，普通图标／开关／滚动条／成就均统一黄色，骑行区间与异常状态色保留。
- 重构个人主页为开放身份区、统计胶囊、单一成就模块和轻量记录／布局入口，减少卡片、英文装饰标签和重复标题；真实统计、焦点、可打断按压及开机动效保留。说明见 `docs/黑黄主题与个人主页重构_20261001.md`。

- 首页删除山形／道路线和固定假路线，改为淡色 A8 图标水印与保存摘要；增加最近距离／爬升可用性、数值与单位跟随、长姓名和数字处理。
- 个人主页移除固定等级、活跃标签及重复累计指标，改为真实骑行次数／平均用时、16 项成就目标、日志与布局偏好，覆盖无记录和累计／详情缓存分离状态。
- 设置增加右侧滚动条及范围／到底部提示，行高增至 78 px，采用功能色图标、确认状态开关弹簧和可读中文说明；统一深蓝黑背景，取消只读配置状态的假操作，补齐中文标点字形。
- 保留另一轮更新的 PRTS 启动／首页入场；本轮 32 项回归通过，215 组整屏／分块刷新帧逐像素一致，新增首页／个人页／设置真实 LVGL 预览。

- 修复模拟器脱离工具链 PATH 时缺少 SDL2.dll 的启动失败；Windows 构建自动复制 SDL2 运行库到 bin，并新增固定工程工作目录的启动脚本。
- 首页开机入场改为自上而下八组错开淡入和 12 px 上移：状态栏、身份卡、累计数据、骑行/导航按钮、最近骑行、历史、成就、设置；每组 360 ms、错开 60 ms，完成后开放交互，离页恢复布局和透明度。
- 开机演出完成后增加约 280 ms 淡出与首页入场过渡；过渡期间隔离按键和触摸，减少动效跳过，离页打断清理遮罩，补充真实渲染及中断回归。
- 接入用户提供 LOGO 的 5 秒 PRTS 两幕开机动画，移除转场中间横线；首次启动、重启和关机后开机统一等待动画完成，支持减少动效及离页暂停，补充全帧/部分缓冲启动流程回归。
- 骑行状态移至速度区居中，调整纯色背景、数字层级及语义区间色，默认隐藏模拟器性能覆盖。
- 速度进度条恢复七段平行四边形并按速度档位变色；心率/功率采用 10 px 区间条与淡色卡槽背景，Z1–Z5 改为 24 px MiSans Heavy。
- 新增巡航/训练/爬坡方案、六格指标编辑及后台持久化，配置 BKPF v2 兼容 v1；扩展数据减为四项一页、两页显示，支持触摸和编码器翻页。
- 修复设置初始中文按钮值、无数据占位符缺字和长数值裁切，新增布局配置/原生触摸/拖动误触/颜色跨档回归及实际 LVGL 预览。
- 加入原创终端启动序列和 16 项累计成就档案/详情；统一当前会话心率区间依据，隔离详情手势并验证返回顺序。
- 增补商用彩屏界面对照、后续阶段分配和真实 LVGL 启动/骑行/成就预览，见 `docs/UI精致度审查与商用对照_20261001.md`。
- 采用保留当前位置/速度的阻尼弹簧，改进胶囊焦点、控制中心展开和底部容器；增加骑行收敛及减少动态效果，处理取消/打断/对象销毁。
- 修复结束取消、保存失败、关屏继续记录及开始 ACK 尚未完成时的安全关机流程；未接入的上传服务不再假成功。
- 增加字段质量、时间/会话/来源、真实零值与过期处理、生产提交路径统计、真实时间轴/GPS 采样预览及持久化聚合可用性。
- 引入后台保存事务、幂等恢复、独立累计统计和旧历史异步分页；PC 历史格式 v3 兼容读取 v1/v2。
- 增加配置后台保存与状态反馈、个人区间编辑、自动息屏、五键/编码器去抖/重映射、设备诊断以及可编译硬件接口示例。
- 修正背景未加载时的部分刷新残影、动态字体回退、静止运动时间、负海拔基线及报告日期映射。
- CTest 32/32 通过，包含完整应用全帧/部分缓冲两种刷新模式；实际 LVGL 渲染帧用于视觉核对。未进行真机性能、驱动或商用发行验收。

## v1.3.1 - 2026-09-21

- 完成全局 UI、交互、路由、状态机、持久化和 Simulator 稳定性验收，形成 `docs/v1.3.1报告.md`。
- 修复 `cmake --build --clean-first` 删除生成字体源后，主程序因部分字体未固定加入目标而链接失败的问题。
- 将全部运行时生成字体源从条件性文件 Glob 中排除，并作为固定目标源显式声明，恢复干净构建可复现性。
- 完整 CTest 为 26/26 通过；连续 10 轮共 260 次测试执行全部通过。
- Simulator 在正确 UCRT64 运行环境下持续运行 30 秒未自行退出，未输出内存、缺字、断言或异常日志。
- 当前 LVGL/Simulator 功能通过自动验收；真实地图、GNSS、BLE、TF/FatFs、音频与 STM32 性能仍属于后端和真机阶段。

## v1.3.0 - 2026-09-21

- 将骑行结束 Summary 与历史详情 Detail 收敛为同一套 `Ride Report` 页面实现，两种模式共享视觉、布局和交互逻辑。
- 新增路线预览、距离 Hero、骑行表现、速度/心率/功率/踏频/海拔趋势、心率/功率区间、能量环境和规则化总结。
- 使用单页纵向 LVGL 滚动容器；Touch 可直接滑动，Encoder 按内容块平滑滚动，并依据实际内容高度限制边界。
- 新增统一 Ride Report Model 和最多 120 点趋势数据，支持无功率、无心率/踏频、短途和长距离爬升等 PC Mock 情形。
- 心率、功率、踏频和温度采用能力标志控制显示；传感器缺失时整段回流，不伪造数据。
- 扩展骑行完成聚合数据和 PC 历史文件格式至 v2，保留对旧 v1 记录的只读兼容。
- 页面离开时释放长报告内容对象，重新进入时从模型重建，避免 Summary 和 Detail 长期同时占用 LVGL 堆。
- 完成 MiSans 运行时子集再生成和中英文字体分工修正，子集覆盖 462 glyphs / 463 required codepoints。
- 新增 Ride Report 模型、可选区段回流、滚动边界、历史 v2 存储和渲染内存回归验证；CTest 为 26 项全部通过。
- 路线区域当前仍为 Simulator Mock 几何预览，真实地图、GNSS、TF/FatFs 与 STM32 后端不在本版本的已完成范围。

## v1.2.0 - 2026-09-21

- 将 Profile 从 Legacy 升级为正式页面，补齐个人资料、累计骑行指标、历史、成就和设置入口，并支持 Touch、Encoder 与统一 Back。
- 新增完整 Achievements 页面，基于持久化骑行历史计算 6 项里程碑、进度和解锁状态，不再显示固定演示数字。
- 新增 Ride Detail 路由与页面，History 的 Touch/Encoder 选择可进入单次记录详情，展示路线预览和完整骑行指标。
- Home 恢复正式 Profile、History、Achievements、Settings 入口，并将 Encoder 焦点顺序统一为视觉从上到下的 7 项。
- Dynamic Card 的 Extended Ride Data 改为订阅统一 Ride Data Model，新增平均/最大速度、海拔、爬升、卡路里、温度、心率区间和圈数。
- Weather Card 接入独立 Weather Model；Navigation Card 接入 Navigation Service，并增加剩余爬升数据。
- 将 Navigation 周期 Mock 从 UI 页面下沉到 PC 平台服务层，Simulator 与未来 STM32 共用同一页面和模型接口。
- 修复 History、Achievements、Settings、System 列表与全局上下滑手势冲突，列表触摸从 PRESS 到 RELEASE 全程拥有本次手势。
- 修复 Profile 数字专用字体误绘 `km`、Achievements 数字字体误绘百分号和 Montserrat 中点缺字问题。
- 页面渲染测试统一把缺字、LVGL 内存不足和图层缓冲分配失败视为测试失败；MiSans 运行时子集覆盖 443 glyph。
- 状态栏注册容量由 10 提升到 16，并增加 12 个并存状态栏实例的回归测试。
- 新增 Weather、Profile/Achievements、History/Detail、导航平台数据和滚动区域命中测试；自动化测试为 25 项全部通过。
- 完成 Navigation、Profile、Achievements 离屏视觉检查，以及静态/动态数据性能基准。
- 明确 Simulator 能力边界：真实 GNSS、BLE、TF/FatFs、地图瓦片、GPX/FIT、联网天气和音频后端仍需后续平台开发，不伪装成已完成能力。

## v1.1.2 - 2026-09-19

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
