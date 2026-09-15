#ifndef UI_STATUS_BAR_H
#define UI_STATUS_BAR_H

#include "lvgl.h"

#include <stdint.h>
#include <stdbool.h>


/*
 * ============================================================
 * 顶部状态栏
 * ============================================================
 */

typedef struct
{
    /*
     * 状态栏根对象
     */
    lv_obj_t *root;


    /*
     * GNSS
     */
    lv_obj_t *sat_icon;
    lv_obj_t *sat_label;


    /*
     * 当前系统时间
     */
    lv_obj_t *time_label;


    /*
     * 网络状态
     */
    lv_obj_t *wifi_icon;
    lv_obj_t *bt_icon;


    /*
     * 电池
     */
    lv_obj_t *battery_label;
    lv_obj_t *battery_icon;
    lv_obj_t *battery_fill;
    uint8_t battery_percent;

    bool wifi_connected;
    bool bluetooth_connected;
    bool wifi_state_valid;
    bool bluetooth_state_valid;

} ui_status_bar_t;


/*
 * 创建状态栏
 */
void ui_status_bar_create(
    ui_status_bar_t *bar,
    lv_obj_t *parent
);


/*
 * 修改卫星数量
 */
void ui_status_bar_set_satellites(
    ui_status_bar_t *bar,
    uint8_t count
);


/*
 * 修改当前时间
 */
void ui_status_bar_set_time(
    ui_status_bar_t *bar,
    uint8_t hour,
    uint8_t minute
);


/*
 * 修改电量
 */
void ui_status_bar_set_battery(
    ui_status_bar_t *bar,
    uint8_t percent
);


/*
 * 修改 WiFi 状态
 */
void ui_status_bar_set_wifi(
    ui_status_bar_t *bar,
    bool connected
);


/*
 * 修改蓝牙状态
 */
void ui_status_bar_set_bluetooth(
    ui_status_bar_t *bar,
    bool connected
);


#endif
