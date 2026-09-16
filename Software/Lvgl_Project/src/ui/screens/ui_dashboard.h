#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H

#include "lvgl.h"
#include "../models/ui_ride_data_model.h"

#include <stdint.h>
#include <stdbool.h>


/*
 * ============================================================
 * Dashboard 数据模型
 * ============================================================
 */

/* Compatibility alias. The source of truth now lives in the Ride Data Model. */
typedef ui_ride_data_t ui_dashboard_data_t;


/*
 * ============================================================
 * 对外接口
 * ============================================================
 */


/**
 * @brief 创建 Dashboard
 */
void ui_dashboard_create(void);


/**
 * @brief 获取 Dashboard LVGL Screen
 */
lv_obj_t *ui_dashboard_get_screen(void);

/** Page-local mount point for the shared Bottom Interaction Surface. */
lv_obj_t *ui_dashboard_get_bottom_surface_slot(void);


/**
 * @brief 更新整个 Dashboard
 */
void ui_dashboard_set_data(
    const ui_dashboard_data_t *data
);


/**
 * @brief 单独设置实时速度
 */
void ui_dashboard_set_speed(
    float speed
);

/* Router lifecycle: hidden pages must not keep animation timers running. */
void ui_dashboard_set_active(bool active);


#endif /* UI_DASHBOARD_H */
