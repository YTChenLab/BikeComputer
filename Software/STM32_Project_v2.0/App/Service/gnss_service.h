/**
 * @file    gnss_service.h
 * @brief   GNSS 数据快照发布服务：GNSSTask 独占写入，其他 Task 只读。
 */
#ifndef GNSS_SERVICE_H
#define GNSS_SERVICE_H
#include <stdbool.h>
#include "gnss.h"

void GnssService_Init(void);                                 // 初始化快照状态
void GnssService_Publish(const gnss_snapshot_t *snapshot);    // GNSS Task 发布一次完整数据
bool GnssService_GetSnapshot(gnss_snapshot_t *out);           // 其他任务读取一致快照
void GnssService_Invalidate(void);                            // 接收中断恢复时撤销定位有效性
#endif
