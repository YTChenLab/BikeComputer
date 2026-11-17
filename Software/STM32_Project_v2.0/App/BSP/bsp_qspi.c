
#include "bsp_qspi.h"                 /* 当前 BSP 驱动接口 */
#include "quadspi.h"                  /* CubeMX 生成的 QSPI 句柄 */
#include "qspi_partition.h"          /* 固定 Flash 分区布局 */
#include "stm32h7xx_hal.h"           /* STM32 HAL 驱动 */

#include "FreeRTOS.h"                 /* FreeRTOS 基础类型 */
#include "task.h"                     /* 调度器运行状态 */
#include "semphr.h"                   /* 静态互斥锁 */

#include <stdbool.h>
#include <stddef.h>                   /* NULL */
#include <string.h>                   /* memcmp */

/*============================================================
 * W25Q128JV 指令定义
 *============================================================*/
#define QSPI_CMD_JEDEC         0x9FU  /* 读取 JEDEC ID */
#define QSPI_CMD_READ          0x03U  /* 普通读取 */
#define QSPI_CMD_QUAD_READ     0x6BU  /* 四线快速读取 */
#define QSPI_CMD_WREN          0x06U  /* 写使能 */
#define QSPI_CMD_SR1           0x05U  /* 读状态寄存器 1 */
#define QSPI_CMD_SR2           0x35U  /* 读状态寄存器 2 */
#define QSPI_CMD_WRITE_SR2     0x31U  /* 写状态寄存器 2 */
#define QSPI_CMD_PROGRAM       0x02U  /* 页编程 */
#define QSPI_CMD_ERASE_4K      0x20U  /* 4 KiB 擦除 */

/* 状态寄存器标志 */
#define QSPI_SR1_BUSY          0x01U  /* 内部写入/擦除忙 */
#define QSPI_SR1_WEL           0x02U  /* 写使能锁存 */
#define QSPI_SR2_QE            0x02U  /* 四线通信使能 */

/* 超时配置，单位 ms */
#define QSPI_IO_TIMEOUT        1000U  /* 单次 HAL 通信 */
#define QSPI_PROGRAM_TIMEOUT   1000U  /* 单次页编程 */
#define QSPI_ERASE_TIMEOUT     5000U  /* 单次扇区擦除 */

/* 任务等待互斥锁的最长时间 */
#define QSPI_LOCK_TIMEOUT      1000U

/* 驱动是否完成初始化 */
static volatile uint8_t s_ready = 0U;

/* 当前是否处于 QSPI 内存映射模式 */
static volatile uint8_t s_mapped = 0U;

/* Q4-2：保护 LVGL 长期持有的资源指针，只有 MCU 复位才能清除。 */
static volatile uint8_t s_mappingPinned = 0U;

/* 当前获得写入授权的分区 */
static BSP_QSPI_Partition_t s_writePartition =
    BSP_QSPI_PART_NONE;

/* FreeRTOS 静态互斥锁存储空间 */
static StaticSemaphore_t s_mutexMemory;

/* FreeRTOS 互斥锁句柄 */
static SemaphoreHandle_t s_mutex = NULL;

/*============================================================
 * 将 HAL 返回值转换为 BSP 统一返回值
 *============================================================*/
static BSP_QSPI_Status_t QSPI_FromHAL(HAL_StatusTypeDef status)
{
    if (status == HAL_OK)
        return BSP_QSPI_OK;

    if (status == HAL_TIMEOUT)
        return BSP_QSPI_ERROR_TIMEOUT;

    if (status == HAL_BUSY)
        return BSP_QSPI_ERROR_BUSY;

    return BSP_QSPI_ERROR_HAL;
}

/*============================================================
 * QSPI 操作加锁
 *
 * 在 FreeRTOS 启动前，只有主启动流程访问 QSPI，
 * 此时不需要等待互斥锁。
 *
 * 调度器启动之后，使用静态互斥锁串行访问。
 *
 * 禁止从中断中调用这些阻塞式驱动接口。
 *============================================================*/
static BSP_QSPI_Status_t QSPI_Lock(void)
{
    BaseType_t state;

    /* 不能在中断上下文执行阻塞 Flash 操作 */
    if (__get_IPSR() != 0U)
        return BSP_QSPI_ERROR_CONTEXT;

    /* 必须先通过 BSP_QSPI_Init 创建互斥锁 */
    if (s_mutex == NULL)
        return BSP_QSPI_ERROR_STATE;

    /* 获取 FreeRTOS 调度器状态 */
    state = xTaskGetSchedulerState();

    /* 调度器尚未启动时为单线程启动环境 */
    if (state == taskSCHEDULER_NOT_STARTED)
        return BSP_QSPI_OK;

    /* 不允许在调度器暂停状态下执行阻塞操作 */
    if (state != taskSCHEDULER_RUNNING)
        return BSP_QSPI_ERROR_CONTEXT;

    /* 等待获得独占访问权 */
    if (xSemaphoreTake(
            s_mutex,
            pdMS_TO_TICKS(QSPI_LOCK_TIMEOUT)) != pdTRUE)
    {
        return BSP_QSPI_ERROR_BUSY;
    }

    return BSP_QSPI_OK;
}

/*============================================================
 * 释放互斥锁
 *============================================================*/
static void QSPI_Unlock(void)
{
    /* 启动阶段没有实际获取 FreeRTOS 互斥锁 */
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        (void)xSemaphoreGive(s_mutex);
    }
}

/*============================================================
 * 检查 Flash 内部地址是否越界
 *
 * 使用减法形式避免 address + size 溢出。
 *============================================================*/
static uint8_t QSPI_RangeValid(
    uint32_t address,
    uint32_t size
)
{
    if (size == 0U)
        return 0U;

    if (address >= BSP_QSPI_FLASH_SIZE)
        return 0U;

    if (size > BSP_QSPI_FLASH_SIZE - address)
        return 0U;

    return 1U;
}

/*============================================================
 * 统一构建 QSPI 指令
 *
 * 默认采用单线指令阶段。
 * 地址、数据阶段由具体操作再配置。
 *============================================================*/
static QSPI_CommandTypeDef QSPI_CommandNew(uint32_t instruction)
{
    QSPI_CommandTypeDef cmd = {0};

    cmd.Instruction = instruction;
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;

    cmd.AddressMode = QSPI_ADDRESS_NONE;
    cmd.AddressSize = QSPI_ADDRESS_24_BITS;

    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode = QSPI_DATA_NONE;

    cmd.DummyCycles = 0U;
    cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;

    cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    return cmd;
}

/*============================================================
 * 发送 QSPI 命令
 *============================================================*/
static BSP_QSPI_Status_t QSPI_Send(
    QSPI_CommandTypeDef *cmd
)
{
    return QSPI_FromHAL(
        HAL_QSPI_Command(&hqspi, cmd, QSPI_IO_TIMEOUT)
    );
}

/*============================================================
 * 读取状态寄存器
 *
 * 当前使用 1-0-1 通信：
 * 指令 1 线，地址无，数据 1 线。
 *============================================================*/
static BSP_QSPI_Status_t QSPI_ReadRegister(
    uint8_t opcode,
    uint8_t *value
)
{
    QSPI_CommandTypeDef cmd;
    BSP_QSPI_Status_t result;

    if (value == NULL)
        return BSP_QSPI_ERROR_PARAM;

    cmd = QSPI_CommandNew(opcode);

    cmd.DataMode = QSPI_DATA_1_LINE;
    cmd.NbData = 1U;

    result = QSPI_Send(&cmd);

    if (result != BSP_QSPI_OK)
        return result;

    return QSPI_FromHAL(
        HAL_QSPI_Receive(&hqspi, value, QSPI_IO_TIMEOUT)
    );
}

/*============================================================
 * 等待 Flash 内部操作完成
 *
 * 擦除和页编程开始后，Flash 会置位 BUSY。
 * BUSY 清零后才能发送后续命令。
 *============================================================*/
static BSP_QSPI_Status_t QSPI_WaitReady(uint32_t timeoutMs)
{
    uint32_t startTick = HAL_GetTick();
    uint8_t sr1;
    BSP_QSPI_Status_t result;

    for (;;)
    {
        /* 读取状态寄存器 1 */
        result = QSPI_ReadRegister(QSPI_CMD_SR1, &sr1);

        if (result != BSP_QSPI_OK)
            return result;

        /* BUSY=0，说明内部操作结束 */
        if ((sr1 & QSPI_SR1_BUSY) == 0U)
            return BSP_QSPI_OK;

        /* 超过等待时间则返回超时 */
        if ((uint32_t)(HAL_GetTick() - startTick) >= timeoutMs)
            return BSP_QSPI_ERROR_TIMEOUT;

        /* RTOS 运行后，让出一个 Tick 避免无意义空转 */
        if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        {
            vTaskDelay(1U);
        }
    }
}

/*============================================================
 * 发送写使能命令 0x06
 *
 * 每一次编程、擦除、状态寄存器写入之前，
 * 都必须重新执行写使能。
 *============================================================*/
static BSP_QSPI_Status_t QSPI_WriteEnable(void)
{
    QSPI_CommandTypeDef cmd;
    BSP_QSPI_Status_t result;
    uint8_t sr1;

    cmd = QSPI_CommandNew(QSPI_CMD_WREN);

    result = QSPI_Send(&cmd);

    if (result != BSP_QSPI_OK)
        return result;

    result = QSPI_ReadRegister(QSPI_CMD_SR1, &sr1);

    if (result != BSP_QSPI_OK)
        return result;

    if ((sr1 & QSPI_SR1_WEL) == 0U)
        return BSP_QSPI_ERROR_STATE;

    return BSP_QSPI_OK;
}

/*============================================================
 * 内部 JEDEC ID 读取
 *
 * 这个内部函数只在已经获得 QSPI 所有权后调用，
 * 避免公共函数之间重复获取互斥锁。
 *============================================================*/
static BSP_QSPI_Status_t QSPI_ReadID(
    BSP_QSPI_JedecID_t *id
)
{
    QSPI_CommandTypeDef cmd;
    BSP_QSPI_Status_t result;
    uint8_t data[3] = {0};

    if (id == NULL)
        return BSP_QSPI_ERROR_PARAM;

    cmd = QSPI_CommandNew(QSPI_CMD_JEDEC);

    cmd.DataMode = QSPI_DATA_1_LINE;
    cmd.NbData = 3U;

    result = QSPI_Send(&cmd);

    if (result != BSP_QSPI_OK)
        return result;

    result = QSPI_FromHAL(
        HAL_QSPI_Receive(&hqspi, data, QSPI_IO_TIMEOUT)
    );

    if (result != BSP_QSPI_OK)
        return result;

    id->manufacturer = data[0];
    id->memoryType = data[1];
    id->capacity = data[2];

    return BSP_QSPI_OK;
}

/*============================================================
 * 配置 QE 四线使能位
 *
 * 首先读取 SR2：
 * 如果 QE 已经是 1，直接返回。
 *
 * 如果 QE=0：
 * 1. 等待 Flash 空闲。
 * 2. 写使能。
 * 3. 使用 0x31 写入 SR2。
 * 4. 保留原 SR2 的其他位。
 * 5. 等待写入完成并回读。
 *
 * 注意：
 * 只在需要时修改状态寄存器，
 * 不在每次上电时重复擦写。
 *============================================================*/
static BSP_QSPI_Status_t QSPI_EnsureQuad(void)
{
    QSPI_CommandTypeDef cmd;
    BSP_QSPI_Status_t result;
    uint8_t sr2 = 0U;
    uint8_t updated;

    result = QSPI_ReadRegister(QSPI_CMD_SR2, &sr2);

    if (result != BSP_QSPI_OK)
        return result;

    /* QE 已经启用，不执行任何 Flash 写操作 */
    if ((sr2 & QSPI_SR2_QE) != 0U)
        return BSP_QSPI_OK;

    /* 设置 QE，同时保留其他状态位 */
    updated = (uint8_t)(sr2 | QSPI_SR2_QE);

    result = QSPI_WriteEnable();

    if (result != BSP_QSPI_OK)
        return result;

    /* 构建写状态寄存器 2 指令 */
    cmd = QSPI_CommandNew(QSPI_CMD_WRITE_SR2);

    cmd.DataMode = QSPI_DATA_1_LINE;
    cmd.NbData = 1U;

    result = QSPI_Send(&cmd);

    if (result != BSP_QSPI_OK)
        return result;

    /* 写入新的 SR2 值 */
    result = QSPI_FromHAL(
        HAL_QSPI_Transmit(
            &hqspi,
            &updated,
            QSPI_IO_TIMEOUT)
    );

    if (result != BSP_QSPI_OK)
        return result;

    /* 等待状态寄存器写入完成 */
    result = QSPI_WaitReady(QSPI_PROGRAM_TIMEOUT);

    if (result != BSP_QSPI_OK)
        return result;

    /* 回读状态寄存器 2 */
    result = QSPI_ReadRegister(QSPI_CMD_SR2, &sr2);

    if (result != BSP_QSPI_OK)
        return result;

    /* 只有 QE 真正变成 1 才算完成 */
    if ((sr2 & QSPI_SR2_QE) == 0U)
        return BSP_QSPI_ERROR_QUAD;

    return BSP_QSPI_OK;
}

/*============================================================
 * 探测芯片并准备四线通信
 *
 * 不读取或改写用户数据区。
 *============================================================*/
static BSP_QSPI_Status_t QSPI_Probe(
    BSP_QSPI_JedecID_t *id
)
{
    BSP_QSPI_Status_t result;

    result = QSPI_WaitReady(QSPI_IO_TIMEOUT);

    if (result != BSP_QSPI_OK)
        return result;

    result = QSPI_ReadID(id);

    if (result != BSP_QSPI_OK)
        return result;

    /* W25Q128JV 预期 JEDEC ID：EF 40 18 */
    if ((id->manufacturer != 0xEFU) ||
        (id->memoryType != 0x40U) ||
        (id->capacity != 0x18U))
    {
        return BSP_QSPI_ERROR_ID;
    }

    /* 确保后续 0x6B 四线读取能够工作 */
    return QSPI_EnsureQuad();
}

/*============================================================
 * BSP_QSPI_Init
 *
 * 必须在 MX_QUADSPI_Init() 之后调用。
 * 建议在 FreeRTOS 调度器启动前执行一次。
 *============================================================*/
BSP_QSPI_Status_t BSP_QSPI_Init(
    BSP_QSPI_JedecID_t *id
)
{
    BSP_QSPI_Status_t result;

    if (id == NULL)
        return BSP_QSPI_ERROR_PARAM;

    /* 确保 CubeMX 的 HAL 初始化已经执行 */
    if ((hqspi.Instance != QUADSPI) ||
        (HAL_QSPI_GetState(&hqspi) != HAL_QSPI_STATE_READY))
    {
        return BSP_QSPI_ERROR_STATE;
    }

    /* 第一次启动时创建静态互斥锁 */
    if (s_mutex == NULL)
    {
        s_mutex = xSemaphoreCreateMutexStatic(&s_mutexMemory);

        if (s_mutex == NULL)
            return BSP_QSPI_ERROR_STATE;
    }

    result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    /* 不允许在内存映射期间重新初始化驱动 */
    if (s_mapped != 0U)
    {
        QSPI_Unlock();
        return BSP_QSPI_ERROR_STATE;
    }

    /* 每次重新初始化都撤销写入授权 */
    s_ready = 0U;
    s_writePartition = BSP_QSPI_PART_NONE;

    /* 探测真实芯片并检查 QE */
    result = QSPI_Probe(id);

    if (result == BSP_QSPI_OK)
        s_ready = 1U;

    QSPI_Unlock();

    return result;
}

/*============================================================
 * BSP_QSPI_Recover
 *
 * 在 HAL 操作异常后重新建立外设状态。
 *
 * 必须先确保没有任务正在使用内存映射指针。
 * 不会自动擦除或恢复用户数据。
 *============================================================*/
BSP_QSPI_Status_t BSP_QSPI_Recover(void)
{
    BSP_QSPI_Status_t result;
    BSP_QSPI_JedecID_t id = {0};

    result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    /* 映射仍在使用时不允许强行恢复 */
    if (s_mapped != 0U)
    {
        QSPI_Unlock();
        return BSP_QSPI_ERROR_STATE;
    }

    s_ready = 0U;
    s_writePartition = BSP_QSPI_PART_NONE;

    /* 中止可能残留的 HAL 传输状态 */
    result = QSPI_FromHAL(HAL_QSPI_Abort(&hqspi));

    if (result != BSP_QSPI_OK)
    {
        QSPI_Unlock();
        return result;
    }

    /* 重新初始化现有 QUADSPI 外设 */
    result = QSPI_FromHAL(HAL_QSPI_Init(&hqspi));

    if (result == BSP_QSPI_OK)
    {
        /* 重新确认 JEDEC 和 QE 状态 */
        result = QSPI_Probe(&id);
    }

    if (result == BSP_QSPI_OK)
        s_ready = 1U;

    QSPI_Unlock();

    return result;
}

/*============================================================
 * BSP_QSPI_ReadJEDEC
 *============================================================*/
BSP_QSPI_Status_t BSP_QSPI_ReadJEDEC(
    BSP_QSPI_JedecID_t *id
)
{
    BSP_QSPI_Status_t result;

    if (id == NULL)
        return BSP_QSPI_ERROR_PARAM;

    result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    if ((s_ready == 0U) || (s_mapped != 0U))
        result = BSP_QSPI_ERROR_STATE;
    else
        result = QSPI_ReadID(id);

    QSPI_Unlock();

    return result;
}

/*============================================================
 * 公共状态寄存器读取
 *============================================================*/
static BSP_QSPI_Status_t QSPI_PublicReadRegister(
    uint8_t opcode,
    uint8_t *status
)
{
    BSP_QSPI_Status_t result;

    if (status == NULL)
        return BSP_QSPI_ERROR_PARAM;

    result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    if ((s_ready == 0U) || (s_mapped != 0U))
        result = BSP_QSPI_ERROR_STATE;
    else
        result = QSPI_ReadRegister(opcode, status);

    QSPI_Unlock();

    return result;
}

BSP_QSPI_Status_t BSP_QSPI_ReadStatus1(uint8_t *status)
{
    return QSPI_PublicReadRegister(QSPI_CMD_SR1, status);
}

BSP_QSPI_Status_t BSP_QSPI_ReadStatus2(uint8_t *status)
{
    return QSPI_PublicReadRegister(QSPI_CMD_SR2, status);
}

/*============================================================
 * 内部数据读取
 *
 * quad=0：普通 0x03，1-1-1 模式。
 * quad=1：快速 0x6B，1-1-4 模式。
 *
 * 该函数由已经获得 QSPI 锁的调用者使用。
 *============================================================*/
static BSP_QSPI_Status_t QSPI_ReadInternal(
    uint32_t address,
    uint8_t *buffer,
    uint32_t size,
    uint8_t quad
)
{
    QSPI_CommandTypeDef cmd;
    BSP_QSPI_Status_t result;

    cmd = QSPI_CommandNew(
        (quad != 0U) ? QSPI_CMD_QUAD_READ : QSPI_CMD_READ
    );

    cmd.AddressMode = QSPI_ADDRESS_1_LINE;
    cmd.Address = address;

    cmd.DataMode =
        (quad != 0U) ? QSPI_DATA_4_LINES : QSPI_DATA_1_LINE;

    cmd.DummyCycles = (quad != 0U) ? 8U : 0U;
    cmd.NbData = size;

    result = QSPI_Send(&cmd);

    if (result != BSP_QSPI_OK)
        return result;

    return QSPI_FromHAL(
        HAL_QSPI_Receive(&hqspi, buffer, QSPI_IO_TIMEOUT)
    );
}

/*============================================================
 * 公共普通读取和四线读取
 *============================================================*/
static BSP_QSPI_Status_t QSPI_PublicRead(
    uint32_t address,
    uint8_t *buffer,
    uint32_t size,
    uint8_t quad
)
{
    BSP_QSPI_Status_t result;

    if (buffer == NULL)
        return BSP_QSPI_ERROR_PARAM;

    if (QSPI_RangeValid(address, size) == 0U)
        return BSP_QSPI_ERROR_RANGE;

    result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    if ((s_ready == 0U) || (s_mapped != 0U))
    {
        QSPI_Unlock();
        return BSP_QSPI_ERROR_STATE;
    }

    result = QSPI_WaitReady(QSPI_IO_TIMEOUT);

    if (result == BSP_QSPI_OK)
    {
        result = QSPI_ReadInternal(
            address, buffer, size, quad
        );
    }

    QSPI_Unlock();

    return result;
}

BSP_QSPI_Status_t BSP_QSPI_Read(
    uint32_t address,
    uint8_t *buffer,
    uint32_t size
)
{
    return QSPI_PublicRead(address, buffer, size, 0U);
}

BSP_QSPI_Status_t BSP_QSPI_ReadQuad(
    uint32_t address,
    uint8_t *buffer,
    uint32_t size
)
{
    return QSPI_PublicRead(address, buffer, size, 1U);
}

/*============================================================
 * 查询固定分区的实际地址范围
 *
 * 分区数据直接来自 qspi_partition.h。
 * 不允许在 BSP 中重新定义另一套分区地址。
 *============================================================*/
static uint8_t QSPI_PartitionBounds(
    BSP_QSPI_Partition_t partition,
    uint32_t *offset,
    uint32_t *size
)
{
    switch (partition)
    {
        case BSP_QSPI_PART_METADATA:
            *offset = QSPI_METADATA_OFFSET;
            *size = QSPI_METADATA_SIZE;
            return 1U;

        case BSP_QSPI_PART_STAGING:
            *offset = QSPI_STAGING_OFFSET;
            *size = QSPI_STAGING_SIZE;
            return 1U;

        case BSP_QSPI_PART_BACKUP:
            *offset = QSPI_BACKUP_OFFSET;
            *size = QSPI_BACKUP_SIZE;
            return 1U;

        case BSP_QSPI_PART_RESOURCE_A:
            *offset = QSPI_RESOURCE_A_OFFSET;
            *size = QSPI_RESOURCE_A_SIZE;
            return 1U;

        case BSP_QSPI_PART_RESOURCE_B:
            *offset = QSPI_RESOURCE_B_OFFSET;
            *size = QSPI_RESOURCE_B_SIZE;
            return 1U;

        case BSP_QSPI_PART_RESERVED:
            *offset = QSPI_RESERVED_OFFSET;
            *size = QSPI_RESERVED_SIZE;
            return 1U;

        default:
            return 0U;
    }
}


/*============================================================
 * 检查本次擦写是否完全位于授权分区内部
 *
 * 同时检查：
 * 1. 分区是否已授权。
 * 2. 起始地址不能低于分区起点。
 * 3. 起始地址不能超出分区终点。
 * 4. 本次操作不能跨越分区末尾。
 *============================================================*/
static uint8_t QSPI_WriteAllowed(
    uint32_t address,
    uint32_t size
)
{
    uint32_t start;               /* 当前分区起始地址 */
    uint32_t length;              /* 当前分区总长度 */
    uint32_t relative;            /* 地址相对分区起点的偏移 */

    /* 确认存在有效的授权分区 */
    if (QSPI_PartitionBounds(
            s_writePartition, &start, &length) == 0U)
    {
        return 0U;
    }

    /* 不允许访问分区起点之前的位置 */
    if (address < start)
    {
        return 0U;
    }

    /* 计算相对偏移，此时减法不会下溢 */
    relative = address - start;

    /* 不允许从分区终点或终点之后开始操作 */
    if (relative >= length)
    {
        return 0U;
    }

    /* 不允许本次操作跨越分区的末尾 */
    if ((size == 0U) || (size > length - relative))
    {
        return 0U;
    }

    return 1U;
}


/*============================================================
 * BSP_QSPI_AllowWritePartition
 *
 * 仅授权一个固定分区。
 * 并不会立即发送 Flash 写入指令。
 *============================================================*/
BSP_QSPI_Status_t BSP_QSPI_AllowWritePartition(
    BSP_QSPI_Partition_t partition
)
{
    BSP_QSPI_Status_t result;
    uint32_t offset;
    uint32_t size;

    if (QSPI_PartitionBounds(
            partition, &offset, &size) == 0U)
    {
        return BSP_QSPI_ERROR_PARAM;
    }

    result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    if ((s_ready == 0U) || (s_mapped != 0U))
        result = BSP_QSPI_ERROR_STATE;
    else
    {
        s_writePartition = partition;
        result = BSP_QSPI_OK;
    }

    QSPI_Unlock();

    return result;
}

/*============================================================
 * BSP_QSPI_LockWrite
 *
 * 撤销分区写入授权。
 *============================================================*/
BSP_QSPI_Status_t BSP_QSPI_LockWrite(void)
{
    BSP_QSPI_Status_t result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    s_writePartition = BSP_QSPI_PART_NONE;

    QSPI_Unlock();

    return BSP_QSPI_OK;
}

/*============================================================
 * BSP_QSPI_Program
 *
 * 功能：
 * 自动分页写入，写前检查 0xFF，写后回读。
 *
 * 注意：
 * 本函数不自动擦除扇区。
 * 只能写入已擦除的目标字节。
 *============================================================*/
BSP_QSPI_Status_t BSP_QSPI_Program(
    uint32_t address,
    const uint8_t *buffer,
    uint32_t size
)
{
    BSP_QSPI_Status_t result;
    QSPI_CommandTypeDef cmd;
    uint8_t verify[BSP_QSPI_PAGE_SIZE];
    uint32_t chunk;
    uint32_t remaining;
    uint32_t i;

    if (buffer == NULL)
        return BSP_QSPI_ERROR_PARAM;

    if (QSPI_RangeValid(address, size) == 0U)
        return BSP_QSPI_ERROR_RANGE;

    result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    if ((s_ready == 0U) || (s_mapped != 0U))
    {
        result = BSP_QSPI_ERROR_STATE;
        goto finish;
    }

    if (QSPI_WriteAllowed(address, size) == 0U)
    {
        result = BSP_QSPI_ERROR_PROTECTED;
        goto finish;
    }

    while (size > 0U)
    {
        /* 计算当前地址所在页还剩多少字节 */
        remaining =
            BSP_QSPI_PAGE_SIZE -
            (address % BSP_QSPI_PAGE_SIZE);

        /* 本次编程不能跨越 256 字节页边界 */
        chunk = (size < remaining) ? size : remaining;

        /* 确保上一条 Flash 命令已经完成 */
        result = QSPI_WaitReady(QSPI_PROGRAM_TIMEOUT);

        if (result != BSP_QSPI_OK)
            break;

        /* 读取当前目标字节，检查是否已经擦除 */
        result = QSPI_ReadInternal(
            address, verify, chunk, 0U
        );

        if (result != BSP_QSPI_OK)
            break;

        for (i = 0U; i < chunk; i++)
        {
            if (verify[i] != 0xFFU)
            {
                result = BSP_QSPI_ERROR_NOT_ERASED;
                break;
            }
        }

        if (result != BSP_QSPI_OK)
            break;

        /* 每一页编程之前都需要重新写使能 */
        result = QSPI_WriteEnable();

        if (result != BSP_QSPI_OK)
            break;

        /* 构建 Page Program 0x02 指令 */
        cmd = QSPI_CommandNew(QSPI_CMD_PROGRAM);

        cmd.AddressMode = QSPI_ADDRESS_1_LINE;
        cmd.Address = address;

        cmd.DataMode = QSPI_DATA_1_LINE;
        cmd.NbData = chunk;

        result = QSPI_Send(&cmd);

        if (result != BSP_QSPI_OK)
            break;

        /* 发送需要编程的数据 */
        result = QSPI_FromHAL(
            HAL_QSPI_Transmit(
                &hqspi,
                (uint8_t *)(uintptr_t)buffer,
                QSPI_IO_TIMEOUT)
        );

        if (result != BSP_QSPI_OK)
            break;

        /* 等待 Flash 真正完成本页编程 */
        result = QSPI_WaitReady(QSPI_PROGRAM_TIMEOUT);

        if (result != BSP_QSPI_OK)
            break;

        /* 回读刚刚写入的实际数据 */
        result = QSPI_ReadInternal(
            address, verify, chunk, 0U
        );

        if (result != BSP_QSPI_OK)
            break;

        /* 数据不一致则拒绝继续后面的页编程 */
        if (memcmp(verify, buffer, chunk) != 0)
        {
            result = BSP_QSPI_ERROR_VERIFY;
            break;
        }

        /* 继续处理后面的数据 */
        address += chunk;
        buffer += chunk;
        size -= chunk;
    }

finish:
    /* 所有退出路径统一释放互斥锁 */
    QSPI_Unlock();

    return result;
}

/*============================================================
 * BSP_QSPI_EraseSector4K
 *
 * 每次只擦除一个 4 KiB 扇区。
 * 擦除之前必须已授权所在分区。
 * 擦除后回读检查是否全部为 0xFF。
 *============================================================*/
BSP_QSPI_Status_t BSP_QSPI_EraseSector4K(
    uint32_t address
)
{
    BSP_QSPI_Status_t result;
    QSPI_CommandTypeDef cmd;
    uint8_t verify[256];
    uint32_t offset;
    uint32_t i;

    /* 扇区必须按 4096 字节对齐 */
    if ((address % BSP_QSPI_SECTOR_SIZE) != 0U)
        return BSP_QSPI_ERROR_PARAM;

    if (QSPI_RangeValid(
            address, BSP_QSPI_SECTOR_SIZE) == 0U)
    {
        return BSP_QSPI_ERROR_RANGE;
    }

    result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    if ((s_ready == 0U) || (s_mapped != 0U))
    {
        result = BSP_QSPI_ERROR_STATE;
        goto finish;
    }

    if (QSPI_WriteAllowed(
            address, BSP_QSPI_SECTOR_SIZE) == 0U)
    {
        result = BSP_QSPI_ERROR_PROTECTED;
        goto finish;
    }

    /* 等待芯片空闲 */
    result = QSPI_WaitReady(QSPI_ERASE_TIMEOUT);

    if (result != BSP_QSPI_OK)
        goto finish;

    /* 执行写使能 */
    result = QSPI_WriteEnable();

    if (result != BSP_QSPI_OK)
        goto finish;

    /* 构建 4 KiB 擦除指令 */
    cmd = QSPI_CommandNew(QSPI_CMD_ERASE_4K);

    cmd.AddressMode = QSPI_ADDRESS_1_LINE;
    cmd.Address = address;

    result = QSPI_Send(&cmd);

    if (result != BSP_QSPI_OK)
        goto finish;

    /* 等待内部擦除周期完成 */
    result = QSPI_WaitReady(QSPI_ERASE_TIMEOUT);

    if (result != BSP_QSPI_OK)
        goto finish;

    /* 分批回读整个 4 KiB 扇区 */
    for (offset = 0U;
         offset < BSP_QSPI_SECTOR_SIZE;
         offset += sizeof(verify))
    {
        result = QSPI_ReadInternal(
            address + offset,
            verify,
            sizeof(verify),
            0U
        );

        if (result != BSP_QSPI_OK)
            break;

        /* 擦除成功后的每一字节都应当为 FF */
        for (i = 0U; i < sizeof(verify); i++)
        {
            if (verify[i] != 0xFFU)
            {
                result = BSP_QSPI_ERROR_VERIFY;
                break;
            }
        }

        if (result != BSP_QSPI_OK)
            break;
    }

finish:
    QSPI_Unlock();

    return result;
}

/*============================================================
 * BSP_QSPI_EnterMemoryMapped
 *
 * 使用 W25Q128 的 0x6B 四线快速读指令。
 *
 * STM32 通过 0x90000000～0x90FFFFFF
 * 读取外部 Flash 内容。
 *
 * 一旦映射成功，不得继续执行间接擦写。
 *============================================================*/
BSP_QSPI_Status_t BSP_QSPI_EnterMemoryMapped(void)
{
    BSP_QSPI_Status_t result;
    QSPI_CommandTypeDef cmd;
    QSPI_MemoryMappedTypeDef mapped = {0};

    result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    if (s_ready == 0U)
    {
        QSPI_Unlock();
        return BSP_QSPI_ERROR_STATE;
    }

    /* 已经映射时不需要重复配置 */
    if (s_mapped != 0U)
    {
        QSPI_Unlock();
        return BSP_QSPI_OK;
    }

    /* 存在写入授权时不允许切换到映射模式 */
    if (s_writePartition != BSP_QSPI_PART_NONE)
    {
        QSPI_Unlock();
        return BSP_QSPI_ERROR_STATE;
    }

    result = QSPI_WaitReady(QSPI_IO_TIMEOUT);

    if (result != BSP_QSPI_OK)
    {
        QSPI_Unlock();
        return result;
    }

    /* 配置 Quad Read 0x6B */
    cmd = QSPI_CommandNew(QSPI_CMD_QUAD_READ);

    cmd.AddressMode = QSPI_ADDRESS_1_LINE;
    cmd.Address = 0U;

    cmd.DataMode = QSPI_DATA_4_LINES;
    cmd.DummyCycles = 8U;

    /* 禁用 Memory-Mapped 超时计数器 */
    mapped.TimeOutActivation =
        QSPI_TIMEOUT_COUNTER_DISABLE;

    mapped.TimeOutPeriod = 0U;

    result = QSPI_FromHAL(
        HAL_QSPI_MemoryMapped(&hqspi, &cmd, &mapped)
    );

    if (result == BSP_QSPI_OK)
    {
        /* 确保映射配置完成后再允许应用访问 */
        __DSB();
        __ISB();

        s_mapped = 1U;
    }

    QSPI_Unlock();

    return result;
}

/*============================================================
 * BSP_QSPI_ExitMemoryMapped
 *
 * 注意：
 * 必须由上层资源管理器确保没有活动读取者。
 *
 * 映射指针在此函数执行后立即失效。
 *============================================================*/
BSP_QSPI_Status_t BSP_QSPI_ExitMemoryMapped(void)
{
    BSP_QSPI_Status_t result;

    result = QSPI_Lock();

    if (result != BSP_QSPI_OK)
        return result;

    /* UI 已固定映射，禁止 HAL Abort 使外部资源指针失效。 */
    if (s_mappingPinned != 0U)
    {
        QSPI_Unlock();
        return BSP_QSPI_ERROR_PROTECTED;
    }

    if (s_mapped == 0U)
    {
        QSPI_Unlock();
        return BSP_QSPI_OK;
    }

    /* HAL Abort 退出外部 Flash 内存映射模式 */
    result = QSPI_FromHAL(HAL_QSPI_Abort(&hqspi));

    if (result == BSP_QSPI_OK)
    {
        s_mapped = 0U;

        __DSB();
        __ISB();
    }

    QSPI_Unlock();

    return result;
}

/*============================================================
 * BSP_QSPI_GetMappedPointer
 *
 * address：W25Q128 内部地址偏移。
 * size：读取范围的字节数。
 *
 * 返回的是只读指针，不能通过它写入 Flash。
 *
 * 返回指针后必须保证映射模式保持有效。
 *============================================================*/
/* 与退出映射使用同一 BSP 锁，原子地确认映射状态并禁止退出。
 * 擦写、Init 和 Recover 已由原有入口的 mapped 检查拒绝。 */
BSP_QSPI_Status_t BSP_QSPI_PinMemoryMapped(void)
{
    BSP_QSPI_Status_t result = QSPI_Lock();
    if (result != BSP_QSPI_OK)
        return result;
    if ((s_ready == 0U) || (s_mapped == 0U) ||
        (s_writePartition != BSP_QSPI_PART_NONE))
    {
        QSPI_Unlock();
        return BSP_QSPI_ERROR_STATE;
    }
    __DSB();
    s_mappingPinned = 1U;
    __DMB();
    QSPI_Unlock();
    return BSP_QSPI_OK;
}

uint8_t BSP_QSPI_IsMemoryMappedPinned(void)
{
    return s_mappingPinned;
}

const uint8_t *BSP_QSPI_GetMappedPointer(
    uint32_t address,
    uint32_t size
)
{
    if ((s_ready == 0U) || (s_mapped == 0U))
        return NULL;

    if (QSPI_RangeValid(address, size) == 0U)
        return NULL;

    return (const uint8_t *)(uintptr_t)(
        BSP_QSPI_MAPPED_BASE + address
    );
}

/* Q4-3：只读诊断和可撤销的 WREN/WRDI 自检，绝不自动编程或擦除。
 * Reserved 没有持久化所有权声明，不能推断为空闲，故没有破坏性目标地址接口。 */
/* 在映射仍有效时验证 getter 的正、负边界，不解引用非法地址。 */
static bool QSPI_DiagnosticBounds(void)
{
    return BSP_QSPI_GetMappedPointer(QSPI_FLASH_TOTAL_SIZE-1U,1U)!=NULL &&
           BSP_QSPI_GetMappedPointer(QSPI_FLASH_TOTAL_SIZE,1U)==NULL &&
           BSP_QSPI_GetMappedPointer(UINT32_MAX,2U)==NULL;
}
void BSP_QSPI_Diagnose(uint32_t flags,uint32_t result[13])
{
    uint8_t before[32],sr1=0U,sr2=0U;
    BSP_QSPI_JedecID_t id={0};
    BSP_QSPI_Status_t status;
    bool boundary_ok=false;
    memset(result,0,13U*sizeof(uint32_t));
    result[0]=(flags&1U)?14U:0U; /* RI_NO_SAFE_AREA */
    result[9]=0U; /* 不存在可证明安全的 scratch 区。 */
    result[12]=(flags&1U)?14U:0U;
    /* 检查配置的 MPU region 3 具有只读/XN/16MiB 权限；保留 RNR。 */
    uint32_t rnr=MPU->RNR;
    MPU->RNR=3U; __DSB();
    uint32_t rasr=MPU->RASR,rbar=MPU->RBAR;
    MPU->RNR=rnr;
    result[8]=((rbar&0xFFFFFFE0UL)==BSP_QSPI_MAPPED_BASE &&
        (rasr&MPU_RASR_ENABLE_Msk)!=0U && (rasr&MPU_RASR_XN_Msk)!=0U &&
        ((rasr>>24)&7U)==6U && ((rasr>>1)&31U)==23U &&
        (MPU->CTRL&MPU_CTRL_ENABLE_Msk)!=0U && ((rasr>>19)&7U)==1U &&
        (rasr&((3UL<<16)|(255UL<<8)))==0U)?0U:1U;
    if(result[8]) { result[0]=7U; result[12]=BSP_QSPI_ERROR_PROTECTED; return; }
    const uint8_t *mapped=BSP_QSPI_GetMappedPointer(QSPI_RESOURCE_A_OFFSET,sizeof(before));
    if(mapped!=NULL) {
        /* UI 已固定映射时不可切回间接模式；只验证窗口/权限及可读性。 */
        for(unsigned i=0;i<sizeof(before);++i) result[11]^=((volatile const uint8_t *)mapped)[i];
        result[1]|=8U;
        boundary_ok=QSPI_DiagnosticBounds();
        if(BSP_QSPI_IsMemoryMappedPinned()==0U && BSP_QSPI_ExitMemoryMapped()!=BSP_QSPI_OK) {
            result[0]=7U; return;
        }
        result[2]|=7U; /* JEDEC、SR、WEL 的间接命令在映射期间跳过。 */
    } else {
        status=BSP_QSPI_ReadJEDEC(&id);
        if(status==BSP_QSPI_ERROR_STATE) status=BSP_QSPI_Init(&id);
        result[3]=(uint32_t)id.manufacturer|
            ((uint32_t)id.memoryType<<8)|((uint32_t)id.capacity<<16);
        if(status==BSP_QSPI_OK && (id.manufacturer!=0xEFU || id.memoryType!=0x40U || id.capacity!=0x18U))
            status=BSP_QSPI_ERROR_ID;
        if(status!=BSP_QSPI_OK) { result[0]=7U; result[12]=status; return; }
        result[1]|=1U;
        status=BSP_QSPI_ReadStatus1(&sr1);
        if(status==BSP_QSPI_OK) status=BSP_QSPI_ReadStatus2(&sr2);
        result[4]=sr1; result[5]=sr2;
        if(status!=BSP_QSPI_OK || (sr1&QSPI_SR1_BUSY)!=0U) { result[0]=7U; result[12]=status?status:BSP_QSPI_ERROR_BUSY; return; }
        result[1]|=2U;
        status=QSPI_Lock();
        if(status!=BSP_QSPI_OK) { result[0]=7U; result[12]=status; return; }
        status=QSPI_WaitReady(QSPI_IO_TIMEOUT);
        if(status==BSP_QSPI_OK) status=QSPI_WriteEnable();
        /* 即使 WREN 检查失败也发 WRDI，不能将写使能状态留给后续调用者。 */
        QSPI_CommandTypeDef cmd=QSPI_CommandNew(0x04U);
        BSP_QSPI_Status_t disable=QSPI_Send(&cmd);
        if(disable==BSP_QSPI_OK) disable=QSPI_ReadRegister(QSPI_CMD_SR1,&sr1);
        QSPI_Unlock();
        if(status!=BSP_QSPI_OK || disable!=BSP_QSPI_OK || (sr1&QSPI_SR1_WEL)) {
            result[0]=7U; result[12]=status?status:(disable?disable:BSP_QSPI_ERROR_STATE); return;
        }
        result[1]|=4U;
        status=BSP_QSPI_Read(QSPI_RESOURCE_A_OFFSET,before,sizeof(before)); result[6]=status;
        if(status!=BSP_QSPI_OK) { result[0]=7U; result[12]=status; return; }
        status=BSP_QSPI_EnterMemoryMapped(); result[7]=status;
        if(status!=BSP_QSPI_OK) { result[0]=7U; result[12]=status; return; }
        mapped=BSP_QSPI_GetMappedPointer(QSPI_RESOURCE_A_OFFSET,sizeof(before));
        bool same=(mapped!=NULL && memcmp(before,mapped,sizeof(before))==0);
        boundary_ok=QSPI_DiagnosticBounds();
        status=BSP_QSPI_ExitMemoryMapped();
        if(!same || status!=BSP_QSPI_OK) { result[0]=7U; result[12]=status?status:BSP_QSPI_ERROR_VERIFY; return; }
        result[1]|=8U;
    }
    result[10]=sizeof(before);
    /* 负边界只查询 getter，不实际读取非法地址。 */
    if(boundary_ok) result[1]|=16U;
    else { result[0]=7U; result[12]=BSP_QSPI_ERROR_RANGE; }
    if(!result[8]) result[1]|=32U;
}
