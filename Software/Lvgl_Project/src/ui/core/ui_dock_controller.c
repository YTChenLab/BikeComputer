#include "ui_dock_controller.h"

#include "ui_dock.h"
#include "ui_event_bus.h"
#include "../../services/ui_command_service.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define UI_DOCK_TIMEOUT_MAX 12

enum
{
    DOCK_ID_GPS = 0x1001,
    DOCK_ID_RIDE = 0x1002,
    DOCK_ID_SENSOR = 0x1003,
    DOCK_ID_BATTERY = 0x1004,
    DOCK_ID_NAVIGATION = 0x1005,
    DOCK_ID_MEDIA = 0x1006,
    DOCK_ID_STORAGE = 0x1007,
    DOCK_ID_CONTROL = 0x1008,
    DOCK_ID_COMMAND = 0x1009
};

typedef struct
{
    uint32_t card_id;
    uint32_t remaining_ms;
    bool active;
} ui_dock_timeout_t;

typedef struct
{
    int32_t subscription_id;
    ui_dock_timeout_t timeouts[UI_DOCK_TIMEOUT_MAX];
    uint32_t mapping_failures;
} ui_dock_controller_context_t;

static ui_dock_controller_context_t s_controller;

static bool is_ride_toggle_command(int32_t value)
{
    return value == (int32_t)UI_COMMAND_RIDE_PAUSE ||
           value == (int32_t)UI_COMMAND_RIDE_RESUME;
}

static bool is_pause_command(int32_t value)
{
    return value == (int32_t)UI_COMMAND_RIDE_PAUSE;
}

static void copy_text(char *destination, size_t capacity,
                      const char *preferred, const char *fallback)
{
    const char *source = preferred != NULL && preferred[0] != '\0' ?
                         preferred : fallback;
    snprintf(destination, capacity, "%s", source != NULL ? source : "");
}

static void clear_timeout(uint32_t card_id)
{
    for(uint8_t i = 0; i < UI_DOCK_TIMEOUT_MAX; i++) {
        if(s_controller.timeouts[i].active &&
           s_controller.timeouts[i].card_id == card_id)
            s_controller.timeouts[i].active = false;
    }
}

static void schedule_timeout(uint32_t card_id, uint32_t timeout_ms)
{
    clear_timeout(card_id);
    if(timeout_ms == 0) return;
    for(uint8_t i = 0; i < UI_DOCK_TIMEOUT_MAX; i++) {
        if(s_controller.timeouts[i].active) continue;
        s_controller.timeouts[i].active = true;
        s_controller.timeouts[i].card_id = card_id;
        s_controller.timeouts[i].remaining_ms = timeout_ms;
        return;
    }
    s_controller.mapping_failures++;
}

static void publish_action(ui_event_type_t type, ui_event_priority_t priority,
                           uint32_t source_id, int32_t value)
{
    ui_event_t event = {0};
    event.type = type;
    event.priority = priority;
    event.source_id = source_id;
    event.dedupe_key = source_id;
    event.value = value;
    (void)ui_event_bus_publish(&event);
}

static void dock_action(uint32_t card_id, ui_dock_interaction_t interaction,
                        int32_t value, void *context)
{
    ui_event_type_t origin = (ui_event_type_t)(uintptr_t)context;
    if(card_id == DOCK_ID_GPS && interaction == UI_DOCK_INTERACTION_PRIMARY) {
        if(origin != UI_EVENT_GPS_READY) (void)ui_dock_hide(card_id);
        publish_action(origin == UI_EVENT_GPS_READY ?
                       UI_EVENT_RIDE_GO_REQUESTED : UI_EVENT_GPS_SEARCHING,
                       UI_EVENT_PRIORITY_P2_RIDE_STATE, card_id, 0);
    }
    else if(card_id==DOCK_ID_GPS && interaction==UI_DOCK_INTERACTION_SECONDARY)
        publish_action(UI_EVENT_RIDE_PREPARE_CANCELLED,UI_EVENT_PRIORITY_P2_RIDE_STATE,card_id,0);
    else if(card_id == DOCK_ID_RIDE && origin == UI_EVENT_RIDE_FINISH_REQUESTED) {
        if(interaction == UI_DOCK_INTERACTION_PRIMARY) {
            publish_action(UI_EVENT_RIDE_FINISH_CONFIRMED,
                           UI_EVENT_PRIORITY_P2_RIDE_STATE, card_id, 0);
        }
        else if(interaction == UI_DOCK_INTERACTION_SECONDARY ||
                interaction == UI_DOCK_INTERACTION_DISMISS) {
            if(interaction == UI_DOCK_INTERACTION_SECONDARY)
                (void)ui_dock_hide(card_id);
            publish_action(UI_EVENT_RIDE_FINISH_CANCELLED,
                           UI_EVENT_PRIORITY_P2_RIDE_STATE, card_id, 0);
        }
    }
    else if(card_id == DOCK_ID_NAVIGATION &&
            interaction == UI_DOCK_INTERACTION_PRIMARY) {
        publish_action(UI_EVENT_NAVIGATION_OPEN_REQUESTED,
                       UI_EVENT_PRIORITY_P3_USER_ACTION, card_id, 0);
    }
    else if(card_id == DOCK_ID_MEDIA) {
        ui_event_type_t type = UI_EVENT_MEDIA_TOGGLE;
        if(interaction == UI_DOCK_INTERACTION_DECREMENT) type = UI_EVENT_MEDIA_PREVIOUS;
        else if(interaction == UI_DOCK_INTERACTION_INCREMENT) type = UI_EVENT_MEDIA_NEXT;
        else if(interaction == UI_DOCK_INTERACTION_SECONDARY) type = UI_EVENT_MEDIA_MODE;
        else if(interaction != UI_DOCK_INTERACTION_TOGGLE) return;
        publish_action(type, UI_EVENT_PRIORITY_P4_NORMAL, card_id, value);
    }
    else if(card_id == DOCK_ID_STORAGE) {
        if(interaction == UI_DOCK_INTERACTION_PRIMARY) {
            publish_action(UI_EVENT_STORAGE_RETRY,
                           UI_EVENT_PRIORITY_P0_CRITICAL, card_id, 0);
        }
        else if(interaction == UI_DOCK_INTERACTION_SECONDARY)
            (void)ui_dock_hide(card_id);
    }
    else if(card_id == DOCK_ID_CONTROL &&
            interaction == UI_DOCK_INTERACTION_VALUE_CHANGED) {
        ui_event_t event = {0};
        const ui_dock_card_t *current = ui_dock_current();
        event.type = UI_EVENT_CONTROL_CHANGED;
        event.priority = UI_EVENT_PRIORITY_P3_USER_ACTION;
        event.source_id = card_id;
        event.dedupe_key = card_id;
        event.value = value;
        if(current != NULL) {
            event.minimum = current->minimum;
            event.maximum = current->maximum;
            copy_text(event.title, sizeof(event.title), current->title, NULL);
            copy_text(event.message, sizeof(event.message),
                      current->message, NULL);
        }
        (void)ui_event_bus_publish(&event);
    }
    else if(card_id == DOCK_ID_COMMAND &&
            interaction == UI_DOCK_INTERACTION_PRIMARY) {
        (void)ui_dock_hide(card_id);
    }
    else if(card_id == DOCK_ID_RIDE &&
            origin == UI_EVENT_COMMAND_FAILED &&
            interaction == UI_DOCK_INTERACTION_PRIMARY) {
        (void)ui_dock_hide(card_id);
    }
}

static ui_dock_card_t base_card(const ui_event_t *event, uint32_t id,
                                ui_dock_type_t type)
{
    ui_dock_card_t card = {0};
    card.id = id;
    card.type = type;
    card.priority = (ui_dock_priority_t)event->priority;
    card.value = event->value;
    card.minimum = event->minimum;
    card.maximum = event->maximum;
    card.dismissible = true;
    card.action_cb = dock_action;
    card.context = (void *)(uintptr_t)event->type;
    return card;
}

static bool present(ui_dock_card_t *card, uint32_t timeout_ms)
{
    bool shown = ui_dock_update(card);
    if(!shown) shown = ui_dock_show(card);
    if(shown) schedule_timeout(card->id, timeout_ms);
    else s_controller.mapping_failures++;
    return shown;
}

static void map_event(const ui_event_t *event, void *user_data)
{
    (void)user_data;
    ui_dock_card_t card;
    uint32_t timeout = event->timeout_ms;
    switch(event->type) {
        case UI_EVENT_GPS_SEARCHING:
            card = base_card(event, DOCK_ID_GPS, UI_DOCK_TYPE_ACTION);
            card.priority = UI_DOCK_PRIORITY_P2_RIDE_STATE;
            copy_text(card.title, sizeof(card.title), event->title, "GPS 正在定位");
            copy_text(card.message, sizeof(card.message), event->message, "正在搜索卫星信号");
            card.dismissible = false;
            copy_text(card.primary_action,sizeof(card.primary_action),NULL,"定位中");
            copy_text(card.secondary_action,sizeof(card.secondary_action),NULL,"取消");
            (void)present(&card, timeout);
            break;
        case UI_EVENT_GPS_READY:
            card = base_card(event, DOCK_ID_GPS, UI_DOCK_TYPE_ACTION);
            card.priority = UI_DOCK_PRIORITY_P2_RIDE_STATE;
            copy_text(card.title, sizeof(card.title), event->title, "GPS 准备完成");
            copy_text(card.message, sizeof(card.message), event->message, "定位良好，可以开始骑行");
            copy_text(card.primary_action, sizeof(card.primary_action),
                      event->primary_action, "GO");
            copy_text(card.secondary_action,sizeof(card.secondary_action),NULL,"取消");
            card.dismissible = false;
            (void)present(&card, timeout);
            break;
        case UI_EVENT_GPS_FAILED:
            card = base_card(event, DOCK_ID_GPS, UI_DOCK_TYPE_ACTION);
            card.priority = UI_DOCK_PRIORITY_P1_RIDING_SAFETY;
            copy_text(card.title, sizeof(card.title), event->title, "GPS 定位失败");
            copy_text(card.message, sizeof(card.message), event->message, "请移动到开阔区域后重试");
            copy_text(card.primary_action, sizeof(card.primary_action),
                      event->primary_action, "重试");
            copy_text(card.secondary_action,sizeof(card.secondary_action),NULL,"取消");
            (void)present(&card, timeout);
            break;
        case UI_EVENT_SENSOR_DISCONNECTED:
            card = base_card(event, DOCK_ID_SENSOR, UI_DOCK_TYPE_ACTION);
            card.priority = UI_DOCK_PRIORITY_P1_RIDING_SAFETY;
            copy_text(card.title, sizeof(card.title), event->title, "传感器已断开");
            copy_text(card.message, sizeof(card.message), event->message, "请检查蓝牙与传感器电量");
            copy_text(card.primary_action, sizeof(card.primary_action),
                      event->primary_action, "检查");
            (void)present(&card, timeout ? timeout : 6000);
            break;
        case UI_EVENT_LOW_BATTERY:
            card = base_card(event, DOCK_ID_BATTERY, UI_DOCK_TYPE_INFO);
            card.priority = event->priority == UI_EVENT_PRIORITY_P0_CRITICAL ?
                            UI_DOCK_PRIORITY_P0_CRITICAL :
                            UI_DOCK_PRIORITY_P1_RIDING_SAFETY;
            copy_text(card.title, sizeof(card.title), event->title, "电量过低");
            copy_text(card.message, sizeof(card.message), event->message, "请尽快充电或开启省电模式");
            (void)present(&card, timeout);
            break;
        case UI_EVENT_RIDE_STARTED:
        case UI_EVENT_RIDE_PAUSED:
        case UI_EVENT_RIDE_RESUMED:
            card = base_card(event, DOCK_ID_RIDE, UI_DOCK_TYPE_INFO);
            card.priority = UI_DOCK_PRIORITY_P2_RIDE_STATE;
            bool automatic = event->value == UI_RIDE_PAUSE_REASON_AUTO;
            const char *default_title = "骑行已开始";
            const char *default_message = "骑行数据正在记录";
            if(event->type == UI_EVENT_RIDE_PAUSED) {
                default_title = automatic ? "自动暂停" : "骑行已暂停";
                default_message = automatic ?
                    "检测到停止，计时与里程已暂停" :
                    "双击返回键可恢复骑行";
            }
            else if(event->type == UI_EVENT_RIDE_RESUMED) {
                default_title = automatic ? "自动恢复" : "骑行已恢复";
                default_message = automatic ?
                    "检测到继续移动，骑行数据恢复记录" :
                    "骑行数据正在记录";
            }
            copy_text(card.title, sizeof(card.title), event->title,
                      default_title);
            copy_text(card.message, sizeof(card.message), event->message,
                      default_message);
            (void)present(&card, timeout ? timeout : 1800);
            break;
        case UI_EVENT_RIDE_FINISH_REQUESTED:
            card = base_card(event, DOCK_ID_RIDE, UI_DOCK_TYPE_CONFIRM);
            card.priority = UI_DOCK_PRIORITY_P2_RIDE_STATE;
            copy_text(card.title, sizeof(card.title), event->title, "结束骑行");
            copy_text(card.message, sizeof(card.message), event->message, "是否结束并保存本次骑行");
            copy_text(card.primary_action, sizeof(card.primary_action),
                      event->primary_action, "YES");
            copy_text(card.secondary_action, sizeof(card.secondary_action),
                      event->secondary_action, "NO");
            (void)present(&card, timeout);
            break;
        case UI_EVENT_NAVIGATION_TURN:
            card = base_card(event, DOCK_ID_NAVIGATION, UI_DOCK_TYPE_NAVIGATION);
            card.priority = UI_DOCK_PRIORITY_P1_RIDING_SAFETY;
            copy_text(card.title, sizeof(card.title), event->title, "前方转向");
            copy_text(card.message, sizeof(card.message), event->message, "继续沿当前道路骑行");
            copy_text(card.primary_action, sizeof(card.primary_action),
                      event->primary_action, "打开");
            (void)present(&card, timeout ? timeout : 6000);
            break;
        case UI_EVENT_NAVIGATION_OFF_ROUTE:
            card = base_card(event, DOCK_ID_NAVIGATION, UI_DOCK_TYPE_ACTION);
            card.priority = UI_DOCK_PRIORITY_P1_RIDING_SAFETY;
            copy_text(card.title, sizeof(card.title), event->title, "已偏离路线");
            copy_text(card.message, sizeof(card.message), event->message, "正在重新规划路线");
            copy_text(card.primary_action, sizeof(card.primary_action),
                      event->primary_action, "查看");
            (void)present(&card, timeout);
            break;
        case UI_EVENT_MEDIA_CHANGED:
            card = base_card(event, DOCK_ID_MEDIA, UI_DOCK_TYPE_MEDIA);
            card.priority = UI_DOCK_PRIORITY_P4_NORMAL;
            copy_text(card.title, sizeof(card.title), event->title, "音乐播放");
            copy_text(card.message, sizeof(card.message), event->message, "未提供曲目信息");
            copy_text(card.secondary_action, sizeof(card.secondary_action),
                      event->secondary_action, "模式");
            (void)present(&card, timeout ? timeout : 6000);
            break;
        case UI_EVENT_STORAGE_ERROR:
        case UI_EVENT_RIDE_SAVE_FAILED:
            clear_timeout(DOCK_ID_RIDE);
            (void)ui_dock_hide(DOCK_ID_RIDE);
            card = base_card(event, DOCK_ID_STORAGE, UI_DOCK_TYPE_CONFIRM);
            card.priority = UI_DOCK_PRIORITY_P0_CRITICAL;
            copy_text(card.title, sizeof(card.title), event->title, "存储异常");
            copy_text(card.message, sizeof(card.message), event->message, "骑行数据保存失败");
            copy_text(card.primary_action, sizeof(card.primary_action),
                      event->primary_action, "重试");
            copy_text(card.secondary_action, sizeof(card.secondary_action),
                      event->secondary_action, "稍后");
            card.dismissible = false;
            (void)present(&card, timeout);
            break;
        case UI_EVENT_RIDE_SAVE_STARTED:
            clear_timeout(DOCK_ID_RIDE);
            (void)ui_dock_hide(DOCK_ID_RIDE);
            card = base_card(event, DOCK_ID_STORAGE, UI_DOCK_TYPE_INFO);
            card.priority = UI_DOCK_PRIORITY_P2_RIDE_STATE;
            copy_text(card.title, sizeof(card.title), event->title,
                      "正在保存骑行数据");
            copy_text(card.message, sizeof(card.message), event->message,
                      "保存完成后自动进入骑行总结");
            card.dismissible = false;
            (void)present(&card, timeout);
            break;
        case UI_EVENT_CONTROL_CHANGED:
            card = base_card(event, DOCK_ID_CONTROL, UI_DOCK_TYPE_CONTROL);
            card.priority = UI_DOCK_PRIORITY_P3_USER_ACTION;
            copy_text(card.title, sizeof(card.title), event->title, "参数调节");
            copy_text(card.message, sizeof(card.message), event->message, "拖动滑条进行调整");
            if(card.maximum <= card.minimum) { card.minimum = 0; card.maximum = 100; }
            (void)present(&card, timeout ? timeout : 2500);
            break;
        case UI_EVENT_FEATURE_INFO:
            card = base_card(event, DOCK_ID_COMMAND, UI_DOCK_TYPE_INFO);
            card.priority = UI_DOCK_PRIORITY_P4_NORMAL;
            copy_text(card.title, sizeof(card.title), event->title,
                      "COMING SOON");
            copy_text(card.message, sizeof(card.message), event->message,
                      "FEATURE RESERVED FOR A LATER PHASE");
            (void)present(&card, timeout ? timeout : 2200);
            break;
        case UI_EVENT_COMMAND_PENDING:
            if(is_ride_toggle_command(event->value)) {
                card = base_card(event, DOCK_ID_RIDE, UI_DOCK_TYPE_INFO);
                card.priority = UI_DOCK_PRIORITY_P2_RIDE_STATE;
                copy_text(card.title, sizeof(card.title), NULL,
                          is_pause_command(event->value) ?
                          "正在暂停骑行" : "正在恢复骑行");
                copy_text(card.message, sizeof(card.message), NULL,
                          is_pause_command(event->value) ?
                          "正在暂停计时与里程记录" :
                          "正在恢复骑行数据记录");
            }
            else {
                card = base_card(event, DOCK_ID_COMMAND, UI_DOCK_TYPE_INFO);
                card.priority = UI_DOCK_PRIORITY_P3_USER_ACTION;
                copy_text(card.title, sizeof(card.title), event->title,
                          "PROCESSING");
                copy_text(card.message, sizeof(card.message), event->message,
                          "DEVICE REQUEST");
            }
            card.dismissible = false;
            (void)present(&card, 0);
            break;
        case UI_EVENT_COMMAND_SUCCESS:
            card = base_card(event,
                             is_ride_toggle_command(event->value) ?
                             DOCK_ID_RIDE : DOCK_ID_COMMAND,
                             UI_DOCK_TYPE_INFO);
            card.priority = is_ride_toggle_command(event->value) ?
                            UI_DOCK_PRIORITY_P2_RIDE_STATE :
                            UI_DOCK_PRIORITY_P3_USER_ACTION;
            copy_text(card.title, sizeof(card.title), event->title,
                      "DONE");
            copy_text(card.message, sizeof(card.message), event->message,
                      "DEVICE STATE UPDATED");
            (void)present(&card, timeout ? timeout : 1200);
            break;
        case UI_EVENT_COMMAND_FAILED:
            if(is_ride_toggle_command(event->value)) {
                card = base_card(event, DOCK_ID_RIDE,
                                 UI_DOCK_TYPE_ACTION);
                card.priority = UI_DOCK_PRIORITY_P2_RIDE_STATE;
                copy_text(card.title, sizeof(card.title), NULL,
                          is_pause_command(event->value) ?
                          "暂停失败" : "恢复失败");
                copy_text(card.message, sizeof(card.message), NULL,
                          is_pause_command(event->value) ?
                          "骑行仍在记录，请重试" :
                          "骑行保持暂停，请重试");
            }
            else {
                card = base_card(event, DOCK_ID_COMMAND,
                                 UI_DOCK_TYPE_ACTION);
                card.priority = UI_DOCK_PRIORITY_P3_USER_ACTION;
                copy_text(card.title, sizeof(card.title), event->title,
                          "FAILED");
                copy_text(card.message, sizeof(card.message), event->message,
                          "DEVICE REQUEST FAILED");
            }
            copy_text(card.primary_action, sizeof(card.primary_action),
                      event->primary_action, "CLOSE");
            (void)present(&card, timeout ? timeout : 6000);
            break;
        case UI_EVENT_RIDE_GO_REQUESTED:
            card=base_card(event,DOCK_ID_GPS,UI_DOCK_TYPE_INFO);
            card.priority=UI_DOCK_PRIORITY_P2_RIDE_STATE;card.dismissible=false;
            copy_text(card.title,sizeof(card.title),NULL,"正在开始骑行");
            copy_text(card.message,sizeof(card.message),NULL,"正在确认记录启动");
            (void)present(&card,0);
            break;
        case UI_EVENT_RIDE_PREPARE_CANCELLED:
            clear_timeout(DOCK_ID_GPS);(void)ui_dock_hide(DOCK_ID_GPS);
            break;
        case UI_EVENT_RIDE_GO:
            clear_timeout(DOCK_ID_GPS);
            (void)ui_dock_hide(DOCK_ID_GPS);
            break;
        case UI_EVENT_RIDE_FINISH_CONFIRMED:
            card = base_card(event, DOCK_ID_RIDE, UI_DOCK_TYPE_INFO);
            card.priority = UI_DOCK_PRIORITY_P2_RIDE_STATE;
            copy_text(card.title, sizeof(card.title), event->title,
                      "正在结束骑行");
            copy_text(card.message, sizeof(card.message), event->message,
                      "正在停止计时并准备保存");
            card.dismissible = false;
            (void)present(&card, 0);
            break;
        case UI_EVENT_RIDE_FINISH_CANCELLED:
            clear_timeout(DOCK_ID_RIDE);
            (void)ui_dock_hide(DOCK_ID_RIDE);
            break;
        case UI_EVENT_RIDE_SAVE_SUCCESS:
            clear_timeout(DOCK_ID_RIDE);
            clear_timeout(DOCK_ID_STORAGE);
            (void)ui_dock_hide(DOCK_ID_RIDE);
            (void)ui_dock_hide(DOCK_ID_STORAGE);
            break;
        case UI_EVENT_STORAGE_RETRY:
            clear_timeout(DOCK_ID_STORAGE);
            (void)ui_dock_hide(DOCK_ID_STORAGE);
            card = base_card(event, DOCK_ID_STORAGE, UI_DOCK_TYPE_INFO);
            card.priority = UI_DOCK_PRIORITY_P0_CRITICAL;
            copy_text(card.title, sizeof(card.title), NULL, "正在重试保存");
            copy_text(card.message, sizeof(card.message), NULL, "请稍候，骑行已停止记录");
            card.dismissible = false;
            (void)present(&card, 0);
            break;
        default:
            break;
    }
}

void ui_dock_controller_init(void)
{
    memset(&s_controller, 0, sizeof(s_controller));
    s_controller.subscription_id =
        ui_event_bus_subscribe(UI_EVENT_MASK_ALL, map_event, NULL);
}

void ui_dock_controller_deinit(void)
{
    if(s_controller.subscription_id > 0)
        (void)ui_event_bus_unsubscribe(s_controller.subscription_id);
    memset(&s_controller, 0, sizeof(s_controller));
}

void ui_dock_controller_tick(uint32_t elapsed_ms)
{
    if(elapsed_ms == 0) return;
    for(uint8_t i = 0; i < UI_DOCK_TIMEOUT_MAX; i++) {
        ui_dock_timeout_t *timeout = &s_controller.timeouts[i];
        if(!timeout->active) continue;
        if(elapsed_ms >= timeout->remaining_ms) {
            uint32_t card_id = timeout->card_id;
            timeout->active = false;
            (void)ui_dock_hide(card_id);
        }
        else timeout->remaining_ms -= elapsed_ms;
    }
}

uint32_t ui_dock_controller_mapping_failures(void)
{
    return s_controller.mapping_failures;
}

bool ui_dock_controller_hide_command_feedback(void)
{
    clear_timeout(DOCK_ID_COMMAND);
    return ui_dock_hide(DOCK_ID_COMMAND);
}
void ui_dock_controller_hide_navigation(void)
{ clear_timeout(DOCK_ID_NAVIGATION); (void)ui_dock_hide(DOCK_ID_NAVIGATION); }
