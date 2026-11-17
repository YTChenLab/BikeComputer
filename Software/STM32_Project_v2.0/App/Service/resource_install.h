#ifndef RESOURCE_INSTALL_H
#define RESOURCE_INSTALL_H
#include <stdbool.h>
#include <stdint.h>

/* 与 Resource Package V1 分层：这里仅定义传输会话，不定义存储格式。 */
enum { RI_HELLO=1, RI_STATUS, RI_BEGIN, RI_DATA, RI_COMMIT, RI_ABORT,
       RI_DIAG, RI_MAINTENANCE, RI_REBOOT };
enum { RI_OK=0, RI_FORMAT, RI_CRC, RI_STATE, RI_SESSION, RI_SEQUENCE,
       RI_LENGTH, RI_STORAGE, RI_LOCKED, RI_BUSY, RI_TIMEOUT, RI_CONFLICT,
       RI_UNSUPPORTED, RI_TRANSPORT, RI_NO_SAFE_AREA };
enum { RI_BOOT=0, RI_INSTALL=1, RI_UI_PENDING=2, RI_UI_ACTIVE=3 };
#define RI_HEADER 20U
#define RI_PAYLOAD_MAX 260U
#define RI_REPLY_SIZE 128U
#define RI_PARTIAL_MS 1000U
#define RI_SESSION_MS 30000U
#define RI_BOOT_GRACE_MS 5000U

typedef struct {
    bool (*send)(const uint8_t *data, uint32_t bytes, void *context);
    void (*diagnose)(uint32_t flags, uint32_t result[13], void *context);
    void (*reset)(bool maintenance, void *context);
    bool (*locked)(void *context);
    uint8_t uid[12];
    void *context;
} resource_install_port_t;

/* 所有 Feed/Poll/初始化/Flash 操作由原 CommTask 串行执行。
 * UiTask 仅查询原子发布的 mode，报告启动成功/失败，不执行安装命令。 */
void ResourceInstall_Init(const resource_install_port_t *port, bool maintenance, uint32_t now);
void ResourceInstall_Feed(const uint8_t *data, uint32_t bytes, uint32_t now);
void ResourceInstall_Poll(uint32_t now);
void ResourceInstall_TransportLost(void);
uint32_t ResourceInstall_Mode(void);
void ResourceInstall_UiResult(bool success);
#endif
