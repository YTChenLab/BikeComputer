#include "ui_board.h"       /* UI 板级适配模块对外接口 */
#include "app_main.h"       /* 新增：显示硬件初始化接口 */
#include "ui_display.h"     /* LVGL 显示驱动适配接口 */

#include "bsp_qspi.h"
#include "resource_storage.h"
#include "qspi_partition.h"

#include "battery_service.h"                 // 电池服务任务安全快照接口
#include "ui_gnss_bridge.h"                 // PU-252 GNSS 数据接入 LVGL UI
#include "ui_altitude_bridge.h"              // SPL06 气压高度与 LVGL 数据模型桥接

#include "ui/core/ui_app_state.h"             // LVGL UI 已有的统一状态模型

#include "bsp_lcd.h"                       /* LCD 就绪检查 */
#include "bsp_dma2d.h"                     /* DMA2D 就绪检查 */

#include "platform/stm32/ui_stm32_startup.h"
#include "platform/stm32/ui_stm32_services.h"

#include "stm32h7xx_hal.h"                 /* HAL 时间和 CMSIS */
#include "FreeRTOS.h"                      /* FreeRTOS 类型 */
#include "task.h"                          /* 任务身份 */
#include "semphr.h"                        /* 互斥锁 */
#include "main.h"  /* CubeMX 声明 Error_Handler() 的头文件 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/*============================================================
 * 文件：ui_board.c
 *
 * 功能：
 * 将 BikeComputer v1.5.3 完整 UI
 * 接入 STM32H743 的 FreeRTOS 运行环境。
 *
 * 只允许 UI Task 执行 LVGL API。
 *
 * 当前尚未完成的硬件服务明确报告不可用。
 * 不生成虚假的骑行、导航或连接状态。
 *============================================================*/

/* 已有 UI 静态库中导出的完整 MiSans 字体 */
extern const lv_font_t ui_font_misans_ui_20;
extern const lv_font_t ui_font_misans_ui_24;

/* 导航地图栅格尺寸：768 × 768 RGB565 */
#define UI_BOARD_RASTER_PIXELS  (768U * 768U)

/*
 * 地图缓存 A：
 * 0xC0700000 至 0xC081FFFF。
 *
 * 地图缓存 B：
 * 0xC0820000 至 0xC093FFFF。
 *
 * 位置处于 SDRAM 可缓存应用区域。
 * 避开：
 * - 2 MiB LCD 显示区
 * - 4 MiB UI 静态变量区域
 * - 1 MiB LVGL 内存池
 */
#define UI_BOARD_RASTER_A  (0xC0700000UL)
#define UI_BOARD_RASTER_B  (0xC0820000UL)

/* 记录真正拥有 LVGL 的 FreeRTOS 任务 */
static TaskHandle_t s_uiOwner = NULL;

/* UI 服务系统的静态互斥锁对象 */
static StaticSemaphore_t s_mutexStorage;

/* 互斥锁句柄 */
static SemaphoreHandle_t s_mutex = NULL;

/* UI 是否已经成功启动 */
static bool s_started = false;

/* Q4-2：资源兼容性版本，对应固定包 min/max_firmware_version=1。
 * 与工程目录 v2.0、UI 版本号无关；后续版本统一维护兼容策略。 */
#define UI_BOARD_RESOURCE_FW_VERSION (1UL)

/* reader_context、描述符和完整性校验对象均具有静态生命周期。 */
static resource_manager_t s_resourceManager;
static ui_stm32_resource_port_t s_resourcePort;
static uint8_t s_resourceScratch[512];
static bool s_resourceAttempted = false;

/* 仅查询，不读索引或改变 QSPI 模式，供原 UiTask 持续检查。 */
static bool UI_ResourceMappingValid(void *context)
{
    const ui_stm32_resource_port_t *port = context;
    return port != NULL && port->base != NULL &&
           BSP_QSPI_IsMemoryMappedPinned() != 0U &&
           BSP_QSPI_GetMappedPointer(port->flash_offset, port->bytes) == port->base;
}

/* 间接读取活动槽 -> 建立映射 -> 固定生命周期 -> 映射 reader 整包校验。
 * 只在启动时执行一次，失败保留原安全等待路径；任何固定映射都保持至复位，
 * 即使后续验证失败也不恢复 Flash 写入，避免长期指针意外失效。 */
static bool UI_ResourceStart(void)
{
    resource_package_slot_t slot;
    resource_package_slot_t pinnedSlot;
    if (s_resourceAttempted)
        return false;
    s_resourceAttempted = true;
    if ((ResourceStorage_GetActiveSlot(&slot) != RESOURCE_STORAGE_OK &&
         ResourceStorage_Init(UI_BOARD_RESOURCE_FW_VERSION) != RESOURCE_STORAGE_OK) ||
        ResourceStorage_GetActiveSlot(&slot) != RESOURCE_STORAGE_OK)
        return false;

    if (slot == RESOURCE_PACKAGE_SLOT_A)
    {
        s_resourcePort.flash_offset = QSPI_RESOURCE_A_OFFSET;
        s_resourcePort.bytes = QSPI_RESOURCE_A_SIZE;
    }
    else if (slot == RESOURCE_PACKAGE_SLOT_B)
    {
        s_resourcePort.flash_offset = QSPI_RESOURCE_B_OFFSET;
        s_resourcePort.bytes = QSPI_RESOURCE_B_SIZE;
    }
    else
        return false;

    if (BSP_QSPI_EnterMemoryMapped() != BSP_QSPI_OK ||
        BSP_QSPI_PinMemoryMapped() != BSP_QSPI_OK)
        return false;
    if (ResourceStorage_GetActiveSlot(&pinnedSlot) != RESOURCE_STORAGE_OK || pinnedSlot != slot)
        return false;

    s_resourcePort.base = BSP_QSPI_GetMappedPointer(
        s_resourcePort.flash_offset, s_resourcePort.bytes);
    s_resourcePort.manager = &s_resourceManager;
    s_resourcePort.valid = UI_ResourceMappingValid;
    s_resourcePort.context = &s_resourcePort;
    if (!UI_ResourceMappingValid(&s_resourcePort))
        return false;

    /* mapped 模式禁止 BSP 间接 Read；复用原 manager 执行格式、版本、SHA256
     * 及全部 CRC32 验证，只替换 reader，不复制包解析或校验算法。 */
    return ResourceManager_Open(&s_resourceManager, slot, UI_BOARD_RESOURCE_FW_VERSION,
        ui_stm32_resources_read_mapped, &s_resourcePort,
        s_resourceScratch, sizeof(s_resourceScratch)) == RESOURCE_MANAGER_OK;
}

/*============================================================
 * 运行时接口
 *============================================================*/

/* 获取系统毫秒时间 */
static uint32_t UI_NowMs(void *context)
{
    (void)context;

    return HAL_GetTick();
}

/* 判断当前是否为 UI Task */
static bool UI_IsTask(void *context)
{
    (void)context;

    return (xTaskGetCurrentTaskHandle() == s_uiOwner);
}

/* 判断当前是否处于中断上下文 */
static bool UI_IsISR(void *context)
{
    (void)context;

    return (__get_IPSR() != 0U);
}

/* UI 运行发生不可恢复错误 */
static void UI_Fault(const char *reason, void *context)
{
    (void)reason;
    (void)context;

    Error_Handler();
}

/* 检查当前必需的硬件资源是否就绪 */
static bool UI_ResourcesReady(void *context)
{
    (void)context;

    return (BSP_LCD_IsReady() != 0U) &&
           (BSP_DMA2D_IsReady() != 0U);
}

/*============================================================
 * 服务层互斥锁
 *
 * UI 本身只能由 UI Task 操作。
 * 但部分 UI 状态邮箱会被其他任务访问，
 * 因此需要 FreeRTOS 互斥锁保护。
 *============================================================*/

/* 获取互斥锁 */
static void UI_Lock(void *context)
{
    (void)context;

    configASSERT(s_mutex != NULL);

    configASSERT(
        xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE
    );
}

/* 释放互斥锁 */
static void UI_Unlock(void *context)
{
    (void)context;

    configASSERT(s_mutex != NULL);

    configASSERT(
        xSemaphoreGive(s_mutex) == pdTRUE
    );
}

/*============================================================
 * 尚未接入的系统服务
 *
 * 不可用的服务返回 false。
 * 这只表示请求无法执行，不代表成功。
 *============================================================*/

/* 当前不接受尚未实现的系统命令 */
static bool UI_Command(
    const ui_command_t *command,
    void *context
)
{
    (void)command;
    (void)context;

    return false;
}

/* TF 卡历史记录工作任务尚未接入 */
static bool UI_HistoryRequest(void *context)
{
    (void)context;

    return false;
}

/* 读取骑行历史：当前没有可用存储 */
static ui_ride_history_result_t UI_HistoryLoad(
    ui_ride_record_t *records,
    size_t capacity,
    size_t *count,
    void *context
)
{
    (void)records;
    (void)capacity;
    (void)context;

    if (count != NULL)
    {
        *count = 0U;
    }

    return UI_RIDE_HISTORY_NOT_CONFIGURED;
}

/* 保存骑行历史：存储未接入，不报告成功 */
static ui_ride_history_result_t UI_HistoryStore(
    const ui_ride_record_t *records,
    size_t count,
    void *context
)
{
    (void)records;
    (void)count;
    (void)context;

    return UI_RIDE_HISTORY_NOT_CONFIGURED;
}

/* 当前 RTC 时间尚未正式接入 UI */
static int64_t UI_NowUTC(void *context)
{
    (void)context;

    return 0;
}

/* 读取用户偏好：交由 UI 使用内置默认值 */
static bool UI_PreferencesLoad(
    uint8_t blob[UI_PREFERENCES_BLOB_SIZE],
    void *context
)
{
    (void)blob;
    (void)context;

    return false;
}

/* 偏好持久化尚未接入 */
static bool UI_PreferencesStore(
    const uint8_t blob[UI_PREFERENCES_BLOB_SIZE],
    void *context
)
{
    (void)blob;
    (void)context;

    return false;
}

/* 偏好存储工作任务尚未接入 */
static bool UI_PreferencesRequest(void *context)
{
    (void)context;

    return false;
}

/* 导航后端尚未接入 */
static bool UI_NavigationDispatch(
    const ui_nav_request_t *request,
    void *context
)
{
    (void)request;
    (void)context;

    return false;
}

/* 地图后端尚未接入 */
static bool UI_MapRequest(
    const ui_nav_map_request_t *request,
    void *context
)
{
    (void)request;
    (void)context;

    return false;
}

/* 地点搜索后端尚未接入 */
static bool UI_SearchRequest(
    const ui_nav_search_request_t *request,
    void *context
)
{
    (void)request;
    (void)context;

    return false;
}

/* BLE 设备扫描尚未接入 */
static bool UI_DeviceScan(void *context)
{
    (void)context;

    return false;
}

/* BLE 设备连接尚未接入 */
static bool UI_DeviceConnect(
    ui_device_kind_t kind,
    void *context
)
{
    (void)kind;
    (void)context;

    return false;
}

/* 声音驱动尚未接入，不操作喇叭 */
static void UI_Sound(
    ui_sound_event_t event,
    uint8_t volume,
    void *context
)
{
    (void)event;
    (void)volume;
    (void)context;
}

/*============================================================
 * 服务接口实例
 *
 * 使用 static const 保证接口在运行期间有效。
 *============================================================*/

static const ui_stm32_service_port_t s_services =
{
    .dispatch_command = UI_Command,
    .request_history_page_worker = UI_HistoryRequest,
    .lock = UI_Lock,
    .unlock = UI_Unlock,
    .context = NULL
};

static const ui_ride_history_storage_t s_history =
{
    .load = UI_HistoryLoad,
    .store = UI_HistoryStore,
    .now_utc = UI_NowUTC,
    .context = NULL
};

static const ui_preferences_port_t s_preferences =
{
    .load = UI_PreferencesLoad,
    .store = UI_PreferencesStore,
    .request_worker = UI_PreferencesRequest,
    .context = NULL
};

static const ui_nav_backend_t s_navigation =
{
    .dispatch = UI_NavigationDispatch,
    .context = NULL
};

static const ui_nav_map_backend_t s_maps =
{
    .request = UI_MapRequest,
    .context = NULL
};

static const ui_nav_search_backend_t s_search =
{
    .request = UI_SearchRequest,
    .context = NULL
};

static const ui_device_connection_port_t s_devices =
{
    .scan = UI_DeviceScan,
    .connect = UI_DeviceConnect,
    .context = NULL
};

/*============================================================
 * UI_BoardStart
 *
 * 由真正的 FreeRTOS UI Task 调用一次。
 *
 * 执行顺序：
 * 1. 检查硬件状态。
 * 2. 创建服务互斥锁。
 * 3. 建立运行时接口。
 * 4. 建立显示接口。
 * 5. 配置地图缓存和字体。
 * 6. 启动已有的完整码表 UI。
 *============================================================*/

/**
 * @brief  启动公路车码表正式 LVGL UI
 *
 * @details
 * 本函数只能由 FreeRTOS 的 UITask 调用。
 *
 * 执行顺序：
 *
 * 1. 检查是否重复启动。
 * 2. 确认 FreeRTOS 调度器已启动。
 * 3. 初始化 SDRAM、DMA2D 和 LCD。
 * 4. 检查显示硬件是否就绪。
 * 5. 记录 UI Task 身份。
 * 6. 创建 UI 服务互斥锁。
 * 7. 配置 LVGL 运行时回调。
 * 8. 配置显示、导航和设备接口。
 * 9. 验证并绑定外部 QSPI 资源。
 * 10. 启动完整 BikeComputer UI。
 *
 * @retval true  UI 成功启动。
 * @retval false UI 启动失败。
 */
bool UI_BoardStart(void)
{
    /*
     * 创建 UI 启动配置结构体。
     *
     * ui_stm32_startup_t 是 UI 静态库定义的
     * STM32 平台启动参数结构体。
     *
     * {0} 表示所有成员先初始化为 0 或 NULL。
     *
     * 这样可以避免未初始化成员带来随机行为。
     */
    ui_stm32_startup_t startup = {0};

    /*========================================================
     * 第一步：防止 UI 重复启动
     *========================================================*/

    /* 检查完整 UI 是否已经成功启动 */
    if (s_started)
    {
        return false;  /* UI 已运行，不允许重复初始化 */
    }

    /*========================================================
     * 第二步：确认 FreeRTOS 已经运行
     *========================================================*/

    /*
     * xTaskGetSchedulerState() 用于获取调度器状态。
     *
     * taskSCHEDULER_RUNNING：
     * 表示 FreeRTOS 调度器正在运行。
     *
     * 显示硬件初始化现在运行在 UI Task 中，
     * 因此必须先确认调度器已经启动。
     */
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
    {
        return false;  /* 调度器尚未启动，禁止启动 UI */
    }

    /*========================================================
     * 第三步：初始化显示硬件
     *
     * 这是本轮新增的核心调用。
     *========================================================*/

    /*
     * App_DisplayHardwareInit() 执行：
     *
     * 1. BSP_SDRAM_Init()
     * 2. BSP_SDRAM_ClearUILargeRam()
     * 3. BSP_DMA2D_Init()
     * 4. BSP_LCD_Init()
     * 5. 可选显示自检
     * 6. 背光初始化
     *
     * 当前函数已处于 UI Task 上下文。
     *
     * 因此初始化过程中，
     * FreeRTOS 仍然可以调度更高优先级的任务。
     *
     * 注意：
     * 这里不会主动调用 Error_Handler()。
     * 但是底层 BSP/HAL 自身的致命错误路径仍需单独处理。
     */
    if (App_DisplayHardwareInit() == 0U)
    {
        /*
         * 显示硬件初始化失败。
         *
         * 禁止启动 LVGL，
         * 避免访问尚未就绪的 SDRAM 或 LCD。
         */
        return false;
    }

    /*========================================================
     * 第四步：再次验证显示硬件就绪状态
     *========================================================*/

    /*
     * UI_ResourcesReady() 会检查：
     *
     * BSP_LCD_IsReady()
     * BSP_DMA2D_IsReady()
     *
     * 只有两个驱动都报告就绪，
     * 才允许进入后续 UI 初始化。
     *
     * 这里沿用原来的正式检查接口。
     */
    if (!UI_ResourcesReady(NULL))
    {
        return false;  /* 显示依赖没有全部准备完成 */
    }

    /*========================================================
     * 第五步：记录 UI Task 身份
     *========================================================*/

    /*
     * 获取当前任务句柄。
     *
     * 当前执行本函数的任务应该是 UITask。
     *
     * 之后 UI_IsTask() 可以通过这个句柄，
     * 判断 LVGL API 是否由正确的任务调用。
     */
    s_uiOwner = xTaskGetCurrentTaskHandle();

    /*========================================================
     * 第六步：创建 UI 服务互斥锁
     *========================================================*/

    /*
     * 使用静态方式创建 FreeRTOS Mutex。
     *
     * s_mutexStorage：
     * 预先定义好的互斥锁控制块。
     *
     * 与动态创建不同：
     * xSemaphoreCreateMutexStatic() 不需要
     * 从 FreeRTOS Heap 动态申请控制块内存。
     */
    s_mutex = xSemaphoreCreateMutexStatic(&s_mutexStorage);

    /* 判断互斥锁是否成功创建 */
    if (s_mutex == NULL)
    {
        return false;  /* 互斥锁创建失败，禁止启动 UI */
    }

    /*========================================================
     * 第七步：配置 LVGL 运行时接口
     *========================================================*/

    /*
     * 提供当前系统毫秒时间。
     *
     * UI 内部可通过此回调获取 HAL 时间。
     */
    startup.runtime.now_ms = UI_NowMs;

    /*
     * 当前没有实现独立的微秒级时间接口。
     *
     * 不提供虚假的时间来源。
     */
    startup.runtime.now_us = NULL;

    /* 提供 UI Task 身份检查回调 */
    startup.runtime.is_ui_task = UI_IsTask;

    /* 提供中断上下文检查回调 */
    startup.runtime.is_isr = UI_IsISR;

    /* 提供 UI 严重错误处理回调 */
    startup.runtime.fault = UI_Fault;

    /* 当前运行时回调不需要额外上下文 */
    startup.runtime.context = NULL;

    /*========================================================
     * 第八步：配置显示资源就绪检查
     *========================================================*/

    /*
     * UI 静态库在初始化和运行过程中，
     * 可以通过回调判断显示依赖是否有效。
     */
    startup.resources_ready = UI_ResourcesReady;

    /* 资源就绪检查不需要额外上下文 */
    startup.resources_context = NULL;

    /*========================================================
     * 第九步：建立 STM32 显示驱动接口
     *========================================================*/

    /*
     * App_UI_DisplayBuildPort() 会把已经实现的
     * STM32 显示操作接口注册给 UI 启动结构体。
     *
     * 这里继续使用原来的显示适配层。
     *
     * 不重新创建另一套 LVGL 显示驱动。
     */
    if (App_UI_DisplayBuildPort(&startup.display) == 0U)
    {
        return false;  /* 显示驱动接口构建失败 */
    }

    /*========================================================
     * 第十步：配置 UI 服务接口
     *========================================================*/

    /* 系统服务接口，例如骑行数据和状态等 */
    startup.services = &s_services;

    /* 骑行历史数据服务接口 */
    startup.history = &s_history;

    /* 用户偏好设置接口 */
    startup.preferences = &s_preferences;

    /*========================================================
     * 第十一步：配置导航系统接口
     *========================================================*/

    /* 导航服务接口 */
    startup.navigation = &s_navigation;

    /* 地图服务接口 */
    startup.maps = &s_maps;

    /* 地点搜索服务接口 */
    startup.search = &s_search;

    /*
     * 当前没有实现的导航能力不提前声明。
     *
     * 避免 UI 认为某个硬件或服务已经可用。
     */
    startup.navigation_capabilities = 0U;

    /*========================================================
     * 第十二步：配置设备连接接口
     *========================================================*/

    /*
     * 继续使用已有的设备扫描和连接服务。
     *
     * 后续 ESP32-S3 设备管理接入时，
     * 仍沿用这一接口扩展。
     */
    startup.devices = &s_devices;

    /*========================================================
     * 第十三步：配置背景和中文字体
     *========================================================*/

    /*
     * 当前不提供独立的背景资源回调。
     *
     * 正式 UI 图片资源通过下面的
     * QSPI 资源包接口完成绑定。
     */
    startup.background = NULL;

    /*
     * 注册已有的 MiSans 20px 字体对象。
     *
     * 字体位图数据由外部资源包提供，
     * 这里保留 UI 静态库导出的字体接口。
     */
    startup.font20 = &ui_font_misans_ui_20;

    /* 注册已有的 MiSans 24px 字体对象 */
    startup.font24 = &ui_font_misans_ui_24;

    /*========================================================
     * 第十四步：配置声音接口
     *========================================================*/

    /* 注册 UI 声音播放接口 */
    startup.sound = UI_Sound;

    /* 声音接口当前不需要额外上下文 */
    startup.sound_context = NULL;

    /*========================================================
     * 第十五步：配置地图 SDRAM 缓存
     *========================================================*/

    /*
     * 地图缓存 A 地址：
     * UI_BOARD_RASTER_A = 0xC0700000
     *
     * 通过 uintptr_t 转换，
     * 再转换为 uint16_t 指针。
     *
     * 地图像素采用 RGB565，
     * 因此每个像素占 16 bit。
     */
    startup.raster_a =
        (uint16_t *)(uintptr_t)UI_BOARD_RASTER_A;

    /* 地图缓存 B 地址 */
    startup.raster_b =
        (uint16_t *)(uintptr_t)UI_BOARD_RASTER_B;

    /*
     * 每块地图缓存的像素数量。
     *
     * 当前原有配置为：
     * 768 × 768 个 RGB565 像素。
     */
    startup.raster_pixel_count = UI_BOARD_RASTER_PIXELS;

    /*========================================================
     * 第十六步：打开并校验 W25Q128 UI 资源包
     *========================================================*/

    /*
     * UI_ResourceStart() 属于 Q4-2 的正式资源接入逻辑。
     *
     * 它会处理：
     *
     * 1. 获取活动资源槽 A 或 B。
     * 2. 进入 QSPI Memory-Mapped 模式。
     * 3. 固定当前映射生命周期。
     * 4. 验证 Resource Package。
     * 5. 建立 UI 资源读取接口。
     *
     * 如果资源包无效：
     * 不允许创建正式 LVGL 页面。
     *
     * 当前仍保留原来的校验逻辑，
     * 不绕过资源包完整性检查。
     */
    if (!UI_ResourceStart())
    {
        return false;  /* QSPI 资源验证或映射失败 */
    }

    /*
     * 将已经完成校验的资源接口
     * 传递给 UI 静态库。
     *
     * 之后字体、图片等资源，
     * 才能通过该接口读取。
     */
    startup.resources = &s_resourcePort;

    /*========================================================
     * 第十七步：启动完整 BikeComputer UI
     *========================================================*/

    /*
     * ui_stm32_startup() 是现有 UI 静态库提供的
     * STM32 平台启动入口。
     *
     * 它会根据此前配置的所有接口，
     * 创建正式 LVGL 显示系统和 UI。
     *
     * 这里不重新设计页面，
     * 不替换原有 BikeComputer UI。
     */
    if (!ui_stm32_startup(&startup))
    {
        return false;  /* UI 静态库启动失败 */
    }

    /*========================================================
     * 第十八步：标记 UI 已经成功启动
     *========================================================*/

    /*
     * 只有上述步骤全部成功，
     * 才允许设置 s_started = true。
     *
     * 之后 UI_BoardPoll() 可以正常执行 LVGL，
     * UI_BoardInput() 可以正常接收实体输入。
     */
    s_started = true;

    /* 告诉 UITask：整个正式 UI 启动成功 */
    return true;
}


/**
 * @brief  公路车码表 LVGL UI 周期处理
 *
 * @details
 * 本函数由 FreeRTOS UITask 周期调用。
 *
 * 主要功能：
 * 1. 检查正式 UI 是否已经完成初始化。
 * 2. 将 SPL06 气压高度提交给 UI 数据模型。
 * 3. 将 PU-252 GNSS 数据提交给 UI 数据模型。
 * 4. 同步现有 BatteryService 电池状态。
 * 5. 执行一次 LVGL UI 调度。
 * 6. 返回下次 UI 处理建议间隔。
 *
 * 注意事项：
 * 1. 只允许 UITask 调用本函数。
 * 2. 不直接读取 SPL06 或 GNSS 硬件寄存器。
 * 3. 不重复调用 GNSS 和 LVGL 处理函数。
 * 4. 不改变现有 UI 页面结构及输入系统。
 *
 * @return 下一次 UI 调度建议间隔，单位 ms
 */
uint32_t UI_BoardPoll(void)
{
    uint32_t nextDelay;                         // 保存 LVGL 返回的下次调度间隔

    uint32_t nowMs;                            // 当前系统运行时间，单位毫秒

    battery_service_snapshot_t battery = {0};  // 保存 BatteryService 的最新电池快照

    ui_app_state_t uiState = {0};              // 保存当前 UI 系统状态


    /* ---------- 1. 检查 LVGL UI 是否已经完成初始化 ---------- */

    if (!s_started)                            // 正式 UI 还没有成功启动
    {
        return 10U;                            // 暂时等待 10ms，禁止访问未初始化 UI
    }


    /* ---------- 2. 获取当前系统时间 ---------- */

    nowMs = HAL_GetTick();                     // 统一获取本次 UI 周期使用的时间戳


    /* ---------- 3. 更新 SPL06 气压高度数据 ---------- */

    UI_AltitudeBridge_Poll(
        nowMs                                  // 将最新气压高度提交到 UI 数据模型
    );


    /* ---------- 4. 更新 PU-252 GNSS 数据 ---------- */

    UI_GnssBridge_Poll(
        nowMs                                  // 更新速度、经纬度、卫星数量和时间
    );


    /* ---------- 5. 读取现有电池状态 ---------- */

    if (BatteryService_GetSnapshot(&battery) && // 成功获取电池服务快照
        ui_app_state_get(&uiState))              // 成功获取当前 UI 系统状态
    {
        /* ---------- 5.1 电池测量有效时更新 UI ---------- */

        if (battery.voltage_valid)             // 检查 ADC 电池测量数据是否有效
        {
            if (!uiState.battery_valid ||      // 当前 UI 电池数据尚未有效
                uiState.battery_percent !=
                    battery.percent ||        // 电池百分比发生变化
                uiState.battery_charging !=
                    battery.charging)         // 电池充电状态发生变化
            {
                ui_app_state_set_battery(
                    battery.percent,           // 更新真实电池百分比
                    battery.charging           // 更新真实电池充电状态
                );
            }
        }

        /* ---------- 5.2 电池测量无效时撤销有效标志 ---------- */

        else                                   // 本次电池电压测量无效
        {
            if (uiState.battery_valid)         // UI 之前认为电池数据有效
            {
                ui_app_state_set_status_validity(
                    uiState.clock_valid,       // 保留原有时钟有效性
                    false                      // 仅将电池数据标记为无效
                );
            }
        }
    }


    /* ---------- 6. 执行一次正式 LVGL UI 调度 ---------- */

    nextDelay = ui_stm32_ui_task_handler();    // 统一处理 LVGL 事件、模型及绘制


    /* ---------- 7. 返回下一次 UI 调度间隔 ---------- */

    return nextDelay;                         // 将 LVGL 建议间隔返回 UITask
}

/*============================================================
 * UI_BoardInput
 *
 * 将已经转换好的 12 字节物理输入事件
 * 交给现有 UI 输入适配器。
 *============================================================*/
bool UI_BoardInput(
    const ui_physical_input_event_t *event
)
{
    if (!s_started || event == NULL || !ui_stm32_resources_ready())
    {
        return false;
    }

    return ui_stm32_services_handle_input_event(event);
}
