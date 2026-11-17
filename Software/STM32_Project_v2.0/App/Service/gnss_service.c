/**
 * @file    gnss_service.c
 * @brief   任务安全 GNSS 状态快照；不访问 UART、HAL 或 LVGL。
 *
 * GNSS 解析器属于 GNSSTask；本服务通过极短的 FreeRTOS 临界区
 * 将固定大小的结构体复制给 UI / 骑行计算等消费端。
 */
#include "gnss_service.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

static gnss_snapshot_t s_published;             // 当前已发布的完整状态
static bool s_initialized = false;              // GNSS Task 是否已初始化服务

void GnssService_Init(void)
{
    taskENTER_CRITICAL();                       // 防止其他任务观察中间状态
    memset(&s_published, 0, sizeof(s_published));
    s_initialized = true;
    taskEXIT_CRITICAL();
}

void GnssService_Publish(const gnss_snapshot_t *snapshot)
{
    if (snapshot == NULL) return;
    taskENTER_CRITICAL();                       // 保护跨任务结构体复制的一致性
    s_published = *snapshot;
    taskEXIT_CRITICAL();
}

bool GnssService_GetSnapshot(gnss_snapshot_t *out)
{
    bool ready;
    if (out == NULL) return false;
    taskENTER_CRITICAL();
    ready = s_initialized;
    if (ready) *out = s_published;               // 取得一次完整、无撕裂的快照
    taskEXIT_CRITICAL();
    return ready;
}

void GnssService_Invalidate(void)
{
    taskENTER_CRITICAL();
    s_published.fix_valid = false;               // 数据接收异常时清除定位有效性
    s_published.altitude_valid = false;
    taskEXIT_CRITICAL();
}
