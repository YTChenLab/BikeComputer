
#ifndef POWER_SERVICE_H
#define POWER_SERVICE_H

#include <stdbool.h>  // 提供bool、true、false类型
#include <stdint.h>   // 提供uint32_t等固定宽度整数类型

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================
 * 一、电源操作命令
 *
 * PowerAction_t表示系统收到哪一种电源请求。
 * 这是电源业务命令，不是GPIO原始电平。
 *==========================================================*/
typedef enum
{
    POWER_ACTION_TOGGLE_DISPLAY = 1,     // POWER短按：切换亮屏和息屏
    POWER_ACTION_REQUEST_SHUTDOWN = 2    // POWER长按：请求有序软件关机
} PowerAction_t;

/*===========================================================
 * 二、电源管理状态
 *
 * 状态表示软件已确认的操作结果。
 * 未注册真实硬件后端时，不允许假装显示已经切换。
 *==========================================================*/
typedef enum
{
    POWER_STATE_NOT_READY = 0,           // 电源硬件控制接口尚未准备好
    POWER_STATE_DISPLAY_ON,              // 当前已确认处于亮屏状态
    POWER_STATE_DISPLAY_OFF,             // 当前已确认处于息屏状态
    POWER_STATE_SHUTDOWN_PENDING         // 已接受关机请求，等待关机流程完成
} PowerState_t;

/*===========================================================
 * 三、PowerQueue传输的事件结构
 *
 * SystemTask从FreeRTOS PowerQueue取出此结构体。
 *==========================================================*/
typedef struct
{
    PowerAction_t action;                // 电源事件需要执行的操作
    uint32_t timestamp_ms;               // InputService产生事件时的毫秒时间戳
} PowerEvent_t;

/*===========================================================
 * 四、硬件操作后端
 *
 * PowerService负责判断需要执行什么操作。
 * Backend负责实际执行背光控制或提交关机请求。
 *
 * 通过函数指针解耦电源逻辑与底层硬件驱动。
 *==========================================================*/
typedef struct
{
    bool (*set_display_enabled)(         // 控制实际显示背光的函数指针
        bool enabled,                    // true：亮屏；false：息屏
        void *context                    // 传递给硬件驱动的上下文
    );

    bool (*begin_shutdown)(              // 提交异步关机请求的函数指针
        void *context                    // 传递给关机服务的上下文
    );

    void *context;                       // 硬件后端自定义上下文指针
} PowerBackend_t;

/*===========================================================
 * 五、对外公共接口
 *==========================================================*/

/* 初始化电源服务内部状态，在调度器启动前调用一次 */
void PowerService_Init(void);

/* 注册真实电源硬件控制后端，只允许成功注册一次 */
bool PowerService_RegisterBackend(
    const PowerBackend_t *backend,       // 实际硬件操作接口
    bool initial_display_enabled        // 注册时已经确认的显示状态
);

/* 由SystemTask处理PowerQueue中的电源请求 */
bool PowerService_HandleEvent(
    const PowerEvent_t *event            // 待处理的电源事件
);

/* 查询当前电源状态 */
PowerState_t PowerService_GetState(void);

/* 查询电源操作失败的累计次数 */
uint32_t PowerService_GetFailedRequests(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_SERVICE_H */
