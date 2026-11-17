#include "bsp_resource_boot.h"
#include "stm32h7xx_hal.h"

/* 本工程此前未使用 BKP30/31。RTC 日历未启用也可经 APB 访问备份寄存器；
 * 不改变 RTC 时钟源、不复位备份域，也不修改任何资源包/metadata。 */
#define RESOURCE_MAINTENANCE_MAGIC 0x52494D31UL
static void backup_access(void)
{
    HAL_PWR_EnableBkUpAccess();
    /* 此版 HAL 无 RTCAPB CLK_ENABLE 宏，复用器件 CMSIS 正式位定义。 */
    SET_BIT(RCC->APB4ENR,RCC_APB4ENR_RTCAPBEN);
    (void)READ_BIT(RCC->APB4ENR,RCC_APB4ENR_RTCAPBEN);
    __DSB();
}
bool BSP_ResourceBoot_ConsumeMaintenance(void)
{
    backup_access();
    bool requested=RTC->BKP30R==RESOURCE_MAINTENANCE_MAGIC &&
                   RTC->BKP31R==~RESOURCE_MAINTENANCE_MAGIC;
    RTC->BKP30R=0; RTC->BKP31R=0; __DSB();
    return requested;
}
void BSP_ResourceBoot_Reset(bool maintenance)
{
    backup_access();
    RTC->BKP30R=maintenance?RESOURCE_MAINTENANCE_MAGIC:0U;
    RTC->BKP31R=maintenance?~RESOURCE_MAINTENANCE_MAGIC:0U;
    /* 标记完成且串口 ACK 已发送后才允许调用。资源映射保护不在这里解除。 */
    __DSB(); NVIC_SystemReset();
}
