# 导航 UI v2 验收记录

日期：2026-10-03。分支：`feature/navigation-ui`。设计依据为用户提供的 7 张导航效果图与定版文字。

![实际 LVGL 页面](previews/navigation-v2/navigation-review.png)

| 检查 | 实际结果 |
| --- | --- |
| CMake/Ninja | 最终源码构建成功，包含 PC 模拟器、测试及 STM32 工作队列示例编译检查 |
| CTest | 34/34 通过；保留首页、骑行、设置、报告、字体、输入与数据接口回归 |
| 实际渲染 | 28 个导航场景，480×854；全帧与局部刷新逐像素一致，无缺字/大图层分配失败日志 |
| 菜单生命周期 | 60 次菜单开关，在弹簧运动中反向关闭；独立导航测试 LVGL 堆稳定为 37736 B、6 个定时器，活动动画数量稳定 |
| 异常数据 | 迟到 ACK、取消、超时/时钟回绕、旧会话复活、路线版本/坐标系、超限几何、NaN、非凸区域、未终止标签、存储容量异常被验证 |
| 输入与状态 | 跟随/浏览按压、方向与提示音开关、总览与回中、选点、准备取消、明确开始、地图目录切换、删除确认、重算与关闭菜单、到达后查看地图与结束均通过整流程验证 |
| PRTS | 6 个开机/PRTS 源码相对本轮开始时 SHA-256 一致 |
| 发布状态 | 本轮仅修改与验证导航 UI，保留 v1.4.0 标签，未创建新发布或推送 |

菜单堆数值来自独立 PC 测试，不代表完整工程或 STM32 总 RAM。PC 单个结构大小：地图帧 8624 B、会话 664 B、目录 2408 B。窗口和邮箱固定有界，实际板端还需测量静态内存、任务栈、字体缓存、DMA/LTDC 缓冲和 IO 峰值。

逐场景结果在 [validation.json](previews/navigation-v2/validation.json)。以下是部分原尺寸截图：

- [导航中心](previews/navigation-v2/nav_center.png)、[目的地](previews/navigation-v2/nav_destination.png)、[路线准备](previews/navigation-v2/nav_preparing.png)、[路线预览](previews/navigation-v2/nav_preview.png)。
- [行中导航](previews/navigation-v2/nav_live.png)、[路口引导](previews/navigation-v2/nav_imminent.png)、[路线总览](previews/navigation-v2/nav_overview.png)、[快捷菜单](previews/navigation-v2/nav_menu.png)。
- [重算中](previews/navigation-v2/nav_rerouting.png)、[重算成功](previews/navigation-v2/nav_reroute_success.png)、[重算失败](previews/navigation-v2/nav_reroute_failed.png)、[弱定位](previews/navigation-v2/nav_gps_weak.png)。
- [地图管理](previews/navigation-v2/nav_maps.png)、[空间管理](previews/navigation-v2/nav_storage.png)、[收藏路线](previews/navigation-v2/nav_saved_routes.png)、[到达](previews/navigation-v2/nav_arrival.png)、[到达后查看地图](previews/navigation-v2/nav_arrival_map.png)。

地图与指标是明确标注的 PC 合成演示；不代表西安真实道路、实际算路、真实车道/交通灯、BLE/GNSS/高德已接入，也不代表真机 60 FPS。真实数据替换接口、产品布局和后续边界见 [实现说明](NAVIGATION_UI_V2_FINAL.md)。
