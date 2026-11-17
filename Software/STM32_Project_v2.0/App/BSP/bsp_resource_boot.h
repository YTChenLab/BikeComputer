#ifndef BSP_RESOURCE_BOOT_H
#define BSP_RESOURCE_BOOT_H
#include <stdbool.h>
/* 单次维护请求保存在 BKP30/31，不占用任何 QSPI 分区；复位后立即消费。 */
bool BSP_ResourceBoot_ConsumeMaintenance(void);
void BSP_ResourceBoot_Reset(bool maintenance);
#endif
