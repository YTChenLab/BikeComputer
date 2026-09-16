# Phase 6B Quick Control Center 实现报告

## 阶段结论

Quick Control Center 已在既有 App State、Command Service 和语义输入架构上完成统一。它仍然是唯一的全屏系统快捷控制覆盖层，不承载开始、暂停、恢复或结束骑行等业务操作。

## UI 与布局

- 第一层：WiFi、Bluetooth 两张等尺寸连接卡；
- 第二层左侧：Sound、Display 两个纵向滑条；
- 第二层右侧：GPS、Power Save 两张等尺寸功能卡；
- 第三层：Touch Lock 与只读 Battery Status；
- 所有功能卡整块区域可点击，连接名称使用单行省略显示；
- 声音与亮度取消百分比文本，图标固定在滑条下半部；
- 使用统一 Design Token 管理颜色、圆角、边框、间距和焦点状态。

## 状态与命令闭环

- WiFi/Bluetooth 分别显示 Disabled、Enabled/Waiting、Connected 三种真实语义，不再把 Enabled 与 Connected 合并；
- GPS 根据卫星数显示无信号、一般或良好；
- 控制命令 Pending 和 Failed 直接显示在对应卡片中；
- Control Center 自己处理的命令不再生成关闭覆盖层后才出现的过期 Bottom Surface 提示；
- Touch Lock 已加入 App State、State Adapter、Command Service 和 PC Mock Backend，STM32 后端只需处理同一语义命令。

## Touch 与 Encoder

- 左键下拉继续使用原有 `y` 坐标跟手移动，不使用全屏透明或缩放动画；
- Control Center 内上滑收起并恢复之前页面；
- Sound/Display 拖动时只更新本地填充，释放时提交一次命令；
- Encoder 可循环移动焦点；在 Sound/Display 上按下进入调节，旋转按 5% 步长改变，再按下退出调节；
- Touch Lock 开启后普通触摸序列被阻止；PC 可用 `M` 重新打开 Control Center，实体平台应把专用菜单键映射为 `UI_INPUT_TOGGLE_CONTROL_CENTER`。

## 性能约束

Control Center 全屏对象只执行纵向位置动画。卡片反馈只改变边框、颜色和小型图标，不允许对 480×854 根对象使用透明度或缩放动画，避免 LVGL 创建大尺寸 ARGB8888 离屏层。

## 验收

- UCRT64 编译成功；
- CTest 11/11 通过；
- 新增 `ui_control_center_test`，覆盖整卡点击、Pending 屏蔽重复点击、Encoder 焦点、滑条调节、边界命中和真实刷新；
- 动态数据 10 秒基准：`fps_avg=59`、`fps_min=59`、`cpu_avg=0%`、`cpu_max=1%`；
- 未出现 `No memory` 或 `Allocating layer buffer failed`。

## 阶段边界

- PC 的 Touch Lock 是 UI 输入锁定演示，最终真机还需由实体按键提供可靠解锁入口；
- 电池状态目前来自 App State，真机电量、充电 IC 和温度数据留给 STM32 Backend；
- WiFi 和蓝牙名称由平台状态适配器提供，扫描列表与配对详情属于 Settings/Detail 阶段。
