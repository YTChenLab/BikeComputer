#ifndef RESOURCE_SYSTEM_H
#define RESOURCE_SYSTEM_H
#include <stdbool.h>
/* 原 CommTask 驱动安装系统；原 UiTask 只等待启动决定和报告结果。 */
void ResourceSystem_Start(void);
void ResourceSystem_Poll(void);
bool ResourceSystem_UiRequested(void);
void ResourceSystem_UiResult(bool success);
#endif
