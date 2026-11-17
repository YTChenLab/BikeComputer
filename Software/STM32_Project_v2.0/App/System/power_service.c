
#include "power_service.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stddef.h>

/* 已注册的硬件操作接口 */
static PowerBackend_t s_backend;

/* 硬件接口是否已经注册 */
static volatile uint8_t s_backendReady = 0U;

/* 当前系统电源状态 */
static volatile PowerState_t s_powerState =
    POWER_STATE_NOT_READY;

/* 电源请求执行失败次数 */
static volatile uint32_t s_powerFailures = 0U;

/* 初始化电源管理服务 */
void PowerService_Init(void)
{
    s_backend.set_display_enabled = NULL;
    s_backend.begin_shutdown = NULL;
    s_backend.context = NULL;

    s_backendReady = 0U;
    s_powerState = POWER_STATE_NOT_READY;
    s_powerFailures = 0U;
}

/*
 * 注册背光及关机业务接口。
 *
 * 允许在后续UI/显示硬件初始化完成后注册。
 * 使用临界区，避免SystemTask读到半更新的结构体。
 *
 * 注册成功不等于背光操作或关机已经发生。
 */
bool PowerService_RegisterBackend(
    const PowerBackend_t *backend,
    bool initial_display_enabled
)
{
    bool success = false;

    /* 必须提供有效的显示控制函数 */
    if ((backend == NULL) ||
        (backend->set_display_enabled == NULL))
    {
        return false;
    }

    /* 保护共享的接口注册状态 */
    taskENTER_CRITICAL();

    /* 禁止重复注册 */
    if (s_backendReady == 0U)
    {
        s_backend = *backend;

        /* 根据已确认的实际显示状态初始化 */
        s_powerState = initial_display_enabled ?
            POWER_STATE_DISPLAY_ON :
            POWER_STATE_DISPLAY_OFF;

        /* 最后才发布接口就绪标志 */
        s_backendReady = 1U;
        success = true;
    }

    taskEXIT_CRITICAL();

    return success;
}

/*
 * 处理一条电源事件。
 *
 * 只允许SystemTask调用。
 * 所有实际硬件操作通过已注册的Backend执行。
 */
bool PowerService_HandleEvent(
    const PowerEvent_t *event
)
{
    bool enableDisplay;

    /* 未注册真实硬件接口时不能假装成功 */
    if ((event == NULL) || (s_backendReady == 0U))
    {
        s_powerFailures++;
        return false;
    }

    /* 已进入关机请求阶段，不再响应普通切换 */
    if (s_powerState == POWER_STATE_SHUTDOWN_PENDING)
    {
        return false;
    }

    /* POWER短按：切换显示状态 */
    if (event->action == POWER_ACTION_TOGGLE_DISPLAY)
    {
        /* 当前关屏则准备开屏，否则准备关屏 */
        enableDisplay =
            (s_powerState == POWER_STATE_DISPLAY_OFF);

        /* 先执行真实硬件操作 */
        if (s_backend.set_display_enabled(
                enableDisplay,
                s_backend.context) == false)
        {
            s_powerFailures++;
            return false;
        }

        /* 硬件操作确认成功后才更新软件状态 */
        s_powerState = enableDisplay ?
            POWER_STATE_DISPLAY_ON :
            POWER_STATE_DISPLAY_OFF;

        return true;
    }

    /* POWER长按：提出关机请求 */
    if (event->action == POWER_ACTION_REQUEST_SHUTDOWN)
    {
        /*
         * 当前阶段必须有真实的关机流程后端。
         * 不允许直接声称整机已断电。
         */
        if ((s_backend.begin_shutdown == NULL) ||
            (s_backend.begin_shutdown(
                s_backend.context) == false))
        {
            s_powerFailures++;
            return false;
        }

        /*
         * 只表示关机流程已接受请求。
         * 后续存储保存、外设停用、低功耗进入
         * 仍必须由正式系统流程完成。
         */
        s_powerState = POWER_STATE_SHUTDOWN_PENDING;

        return true;
    }

    /* 未定义命令一律拒绝 */
    s_powerFailures++;
    return false;
}

/* 获取当前电源状态 */
PowerState_t PowerService_GetState(void)
{
    return s_powerState;
}

/* 获取电源请求执行失败次数 */
uint32_t PowerService_GetFailedRequests(void)
{
    return s_powerFailures;
}
