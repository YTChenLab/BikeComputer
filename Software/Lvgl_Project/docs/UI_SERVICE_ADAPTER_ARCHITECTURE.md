# UI 服务适配层与线程边界

## 结论

PC Simulator 与 STM32 现在共用同一条数据路径：生产者任务只提交快照或语义事件，20 ms UI Tick 在 LVGL 线程统一取出并更新 App State、页面模型与事件总线。外设任务不得直接操作 LVGL 对象。

## 数据路径

```text
GNSS / Sensors / BLE / WiFi / Encoder / SDL
                    |
          producer-safe submit
                    |
     +--------------+----------------+
     |              |                |
Ride Data       Navigation       System State
 mailbox          mailbox           mailbox
     |              |                |
     +--------------+----------------+
                    |
               UI thread tick
                    |
       Dashboard / Navigation / App State
```

输入单独使用有序队列：

```text
SDL or STM32 input -> ui_input_queue -> ui_app_handle_input/pointer
```

连续 `UI_INPUT_TOUCH_MOVE` 会折叠为最新坐标，降低 PC 采样频率高于 UI Tick 时的积压。队列满时，`TOUCH_END`、`TOUCH_CANCEL` 和 `BACK` 会替换较旧输入，避免手势所有权或返回状态卡死。

## 状态适配器

文件：

- `src/services/ui_state_adapter.h`
- `src/services/ui_state_adapter.c`

可独立提交的字段组：

- `UI_STATE_ADAPTER_STATUS`：时间、电量与充电状态。
- `UI_STATE_ADAPTER_GNSS`：GNSS 开关状态与卫星数。
- `UI_STATE_ADAPTER_SATELLITES`：只更新卫星数，不覆盖用户当前 GNSS 开关。
- `UI_STATE_ADAPTER_CONNECTIVITY`：WiFi、蓝牙及设备名称。
- `UI_STATE_ADAPTER_CONTROL`：声音、亮度与省电模式。
- `UI_STATE_ADAPTER_MEDIA`：播放、曲目与播放模式。
- `UI_STATE_ADAPTER_NAVIGATION`：导航活动状态。

同一字段组在 UI Tick 前提交多次时，仅保留最新快照。PC 随机数据使用 `SATELLITES`，因此不会每 800 ms 把用户关闭的 GPS 自动重新打开；STM32 在获得真实 GNSS 电源状态时可以使用完整 `GNSS` 字段组。

## 导航服务

`ui_navigation_service` 是导航模型的唯一生产者入口。页面内部不再维护第二套并发队列：

- 生产者调用 `ui_navigation_service_submit()`。
- UI Tick 调用 `ui_navigation_service_process()`。
- Navigation 页面订阅最新模型。
- 页面隐藏时仅保存最新模型，不刷新地图对象；再次进入时立即渲染最新快照。

这保证 TF 卡离线地图任务、GNSS 任务或后续路线引擎不需要依赖 Navigation 页面实现。

## STM32 接口

`ui_stm32_services.h` 提供任务上下文 API：

- `ui_stm32_services_publish_event()`
- `ui_stm32_services_submit_ride_data()`
- `ui_stm32_services_submit_navigation()`
- `ui_stm32_services_submit_state()`
- `ui_stm32_services_submit_input()`

这些接口不是 ISR 入口。中断只应记录硬件状态或通知 FreeRTOS 任务，再由任务调用提交接口。注册 STM32 backend 时必须提供互斥锁或短临界区回调。

## 当前边界与风险

- 目前已完成“硬件状态上报到 UI”，尚未建立“UI 命令下发到硬件”的异步请求/确认通道。
- WiFi、蓝牙、GPS 和背光开关在 PC 上可直接模拟；移植 STM32 前必须增加命令成功、失败、处理中和超时状态，不能把 UI 显示变化当作外设操作成功。
- 导航模型已解耦，但真实地图瓦片读取、TF 卡缓存、坐标投影和路线计算仍属于后续地图服务阶段。
- App State 与 LVGL 更新只能在 UI 线程执行；生产者只允许调用本文列出的 submit/post API。
