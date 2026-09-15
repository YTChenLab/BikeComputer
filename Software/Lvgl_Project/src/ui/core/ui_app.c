#include "ui_app.h"

#include "ui_colors.h"
#include "ui_background.h"
#include "ui_dock.h"
#include "ui_dock_controller.h"
#include "ui_event_bridge.h"
#include "ui_event_bus.h"
#include "ui_motion.h"
#include "ui_ride_controller.h"
#include "ui_router.h"
#include "ui_sizes.h"
#include "ui_sound.h"
#include "ui_state_machine.h"
#include "ui_theme.h"
#include "../../services/ui_system_services.h"
#include "../components/ui_status_bar.h"
#include "../components/ui_dynamic_card_manager.h"
#include "../components/dock/ui_dock_view.h"
#include "../screens/ui_dashboard.h"
#include "../pages/navigation/ui_navigation.h"
#include "../pages/ui_boot_page.h"
#include "../pages/ui_page_action.h"
#include "../pages/ui_power_page.h"
#include "../pages/ui_profile_page.h"
#include "../pages/ui_settings_page.h"
#include "../pages/ui_summary_page.h"
#include "../pages/ui_system_page.h"
#include "../models/ui_ride_data_model.h"
#include "../overlays/ui_control_center.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef ui_route_id_t ui_page_t;
#define UI_PAGE_BOOT        UI_ROUTE_BOOT
#define UI_PAGE_DASHBOARD   UI_ROUTE_RIDE_DASHBOARD
#define UI_PAGE_NAVIGATION  UI_ROUTE_NAVIGATION
#define UI_PAGE_SUMMARY     UI_ROUTE_RIDE_SUMMARY
#define UI_PAGE_PROFILE     UI_ROUTE_PROFILE
#define UI_PAGE_SETTINGS    UI_ROUTE_SETTINGS
#define UI_PAGE_SYSTEM      UI_ROUTE_SYSTEM
#define UI_PAGE_POWER       UI_ROUTE_POWER
#define UI_PAGE_COUNT       UI_ROUTE_COUNT
typedef enum { UI_ACTION_NONE, UI_ACTION_HOME, UI_ACTION_NAVIGATION, UI_ACTION_SUMMARY,
               UI_ACTION_PROFILE, UI_ACTION_SETTINGS, UI_ACTION_SYSTEM, UI_ACTION_QUICK,
               UI_ACTION_SAVE_RIDE, UI_ACTION_UPLOAD_RIDE, UI_ACTION_START_RIDE,
               UI_ACTION_PAUSE_RIDE, UI_ACTION_END_RIDE, UI_ACTION_WIFI, UI_ACTION_BT,
               UI_ACTION_GPS, UI_ACTION_POWER_SAVE, UI_ACTION_BRIGHTNESS, UI_ACTION_SOUND, UI_ACTION_HISTORY,
               UI_ACTION_ACHIEVEMENT, UI_ACTION_POWER_OFF, UI_ACTION_POWER_RESTART,
               UI_ACTION_POWER_CANCEL } ui_action_t;
typedef struct { lv_obj_t *screen; ui_status_bar_t status; } ui_page_view_t;

static ui_page_view_t s_pages[UI_PAGE_COUNT];
static lv_timer_t *s_boot_timer;
static lv_timer_t *s_event_timer;
static bool s_powered_off, s_wifi = true, s_bluetooth = true, s_gps = true, s_power_save;
static bool s_quick_visible, s_pointer_dragging, s_pointer_opening, s_slider_dragging;
static bool s_page_swiping, s_home_swiping;
static bool s_dynamic_card_gesture_owned;
static bool s_navigation_map_gesture_owned;
static uint8_t s_sound_level = 62, s_brightness_level = 75, s_satellites = 12;
static ui_control_center_slider_t s_active_slider;
static lv_point_t s_pointer_start;
static int32_t s_overlay_start_y;
static lv_obj_t *s_off_screen;
static lv_obj_t *s_quick_overlay;
static void app_action(ui_action_t action);
static void app_page_action(ui_page_action_t action, void *user_data);
static void app_show(ui_page_t page);
static void app_show_slide(ui_page_t page, bool move_left);
static void app_back(void);
static void app_close_quick(void);
static void app_open_quick(void);
static void app_hide_quick_immediate(void);
static void app_update_quick_states(void);
static void app_update_slider_visuals(void);
static void app_control_center_action(ui_control_center_action_t action,
                                      void *user_data);
static void app_ride_data_changed(const ui_ride_data_t *data,
                                  uint32_t version, void *user_data);

static bool app_publish_event(ui_event_type_t type,
                              ui_event_priority_t priority)
{
    ui_event_t event = {0};
    event.type = type;
    event.priority = priority;
    event.dedupe_key = 0xA000U + (uint32_t)type;
    bool published = ui_event_bus_publish(&event);
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
    return published;
}

static void app_event_overflow(const ui_event_t *incoming,
                               const ui_event_t *displaced,
                               ui_event_overflow_reason_t reason,
                               void *user_data)
{
    (void)displaced;
    (void)user_data;
    LV_LOG_ERROR("UI event queue overflow: type=%d reason=%d",
                 incoming != NULL ? (int)incoming->type : -1, (int)reason);
}

static void app_event_received(const ui_event_t *event, void *user_data)
{
    (void)user_data;
    switch(event->type) {
        case UI_EVENT_GPS_SEARCHING:
            if(!ui_system_services_request_gnss_fix()) {
                (void)app_publish_event(UI_EVENT_GPS_FAILED,
                                        UI_EVENT_PRIORITY_P1_RIDING_SAFETY);
            }
            break;
        case UI_EVENT_RIDE_FINISH_CONFIRMED:
            if(!ui_system_services_save_ride()) {
                (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED,
                                        UI_EVENT_PRIORITY_P0_CRITICAL);
            }
            break;
        case UI_EVENT_STORAGE_RETRY:
            if(ui_state_machine_state() == UI_RIDE_STATE_FINISHING &&
               !ui_system_services_retry_save()) {
                (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED,
                                        UI_EVENT_PRIORITY_P0_CRITICAL);
            }
            break;
        case UI_EVENT_RIDE_SUMMARY_READY:
            app_hide_quick_immediate();
            app_show(UI_PAGE_SUMMARY);
            ui_sound_play(UI_SFX_SUCCESS);
            break;
        case UI_EVENT_NAVIGATION_OPEN_REQUESTED:
            app_show(UI_PAGE_NAVIGATION);
            break;
        case UI_EVENT_RIDE_STARTED:
        case UI_EVENT_RIDE_PAUSED:
        case UI_EVENT_RIDE_RESUMED:
        case UI_EVENT_RIDE_FINISH_CANCELLED:
            app_update_quick_states();
            break;
        default:
            break;
    }
}

static void app_event_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_system_services_tick(20);
    (void)ui_ride_data_model_process();
    (void)ui_event_bridge_drain(UI_EVENT_BRIDGE_QUEUE_MAX);
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
    ui_dock_controller_tick(20);
}

#define QUICK_DRAG_THRESHOLD   8

static lv_obj_t *app_make_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen); lv_obj_set_size(screen, UI_SCREEN_W, UI_SCREEN_H);
    ui_background_apply(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}
static void app_add_status(ui_page_t page) {
    ui_status_bar_create(&s_pages[page].status, s_pages[page].screen);
    ui_status_bar_set_satellites(&s_pages[page].status, 12); ui_status_bar_set_time(&s_pages[page].status, 10, 24);
    ui_status_bar_set_battery(&s_pages[page].status, 82); ui_status_bar_set_wifi(&s_pages[page].status, s_wifi); ui_status_bar_set_bluetooth(&s_pages[page].status, s_bluetooth);
}
static void app_ride_data_changed(const ui_ride_data_t *data,
                                  uint32_t version, void *user_data) {
    (void)version;
    (void)user_data;
    if(data == NULL) return;
    s_satellites = data->satellites;
    ui_dashboard_set_data(data);
}
static void app_quick_close_done(lv_anim_t *animation) {
    (void)animation;
    s_quick_visible = false;
    if(s_quick_overlay) lv_obj_add_flag(s_quick_overlay, LV_OBJ_FLAG_HIDDEN);
}
static void app_animate_overlay_to(int32_t target_y, bool hide_when_done) {
    if(s_quick_overlay == NULL) return;
    ui_motion_move_y(s_quick_overlay, target_y, UI_MOTION_DURATION_DOCK,
                     target_y == 0 ? lv_anim_path_ease_out : lv_anim_path_ease_in,
                     hide_when_done ? app_quick_close_done : NULL, NULL);
}
static void app_open_quick(void) {
    if(s_quick_overlay == NULL || s_powered_off) return;
    lv_obj_clear_flag(s_quick_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_quick_overlay);
    s_quick_visible = true;
    app_update_quick_states();
    app_animate_overlay_to(0, false);
}
static void app_close_quick(void) {
    if(!s_quick_visible || s_quick_overlay == NULL) return;
    app_animate_overlay_to(-UI_SCREEN_H, true);
}
static void app_hide_quick_immediate(void) {
    if(s_quick_overlay == NULL) return;
    ui_motion_cancel_y(s_quick_overlay);
    lv_obj_set_y(s_quick_overlay, -UI_SCREEN_H);
    lv_obj_add_flag(s_quick_overlay, LV_OBJ_FLAG_HIDDEN);
    s_quick_visible = false;
    s_pointer_dragging = false;
    s_pointer_opening = false;
    s_slider_dragging = false;
    s_page_swiping = false;
    s_home_swiping = false;
    s_active_slider = UI_CONTROL_CENTER_SLIDER_NONE;
}
static void app_show(ui_page_t page) {
    if(page >= UI_PAGE_COUNT) return;
    s_powered_off = false;
    (void)ui_router_navigate(page, UI_MOTION_PAGE_NONE);
}
static void app_show_slide(ui_page_t page, bool move_left) {
    if(page >= UI_PAGE_COUNT) return;
    s_powered_off = false;
    (void)ui_router_navigate(page, move_left ? UI_MOTION_PAGE_FORWARD : UI_MOTION_PAGE_BACKWARD);
}
static void app_back(void) {
    if(s_powered_off) {
        s_powered_off = false;
        (void)ui_state_machine_dispatch(UI_RIDE_EVENT_BOOT_COMPLETE);
        (void)ui_router_replace(UI_PAGE_DASHBOARD, UI_MOTION_PAGE_NONE);
        return;
    }
    if(s_quick_visible) {
        app_close_quick();
        return;
    }
    ui_sound_play(UI_SFX_BACK);
    (void)ui_router_back();
}
static void app_update_status(void) {
    if(s_pages[UI_PAGE_NAVIGATION].screen) {
        ui_status_bar_set_wifi(&s_pages[UI_PAGE_NAVIGATION].status, s_wifi);
        ui_status_bar_set_bluetooth(&s_pages[UI_PAGE_NAVIGATION].status,
                                    s_bluetooth);
    }
    ui_summary_page_set_connectivity(s_wifi, s_bluetooth);
    ui_profile_page_set_connectivity(s_wifi, s_bluetooth);
    ui_settings_page_set_connectivity(s_wifi, s_bluetooth);
    ui_system_page_set_connectivity(s_wifi, s_bluetooth);
    ui_ride_data_model_set_connectivity(s_wifi, s_bluetooth);
    (void)ui_ride_data_model_process();
}
static void app_create_navigation(void) {
    lv_obj_t *screen = app_make_screen();
    s_pages[UI_PAGE_NAVIGATION].screen = screen;
    app_add_status(UI_PAGE_NAVIGATION);
    ui_navigation_create(screen);
}
static void app_page_action(ui_page_action_t action, void *user_data) {
    (void)user_data;
    switch(action) {
        case UI_PAGE_ACTION_HOME: app_action(UI_ACTION_HOME); break;
        case UI_PAGE_ACTION_SAVE_RIDE: app_action(UI_ACTION_SAVE_RIDE); break;
        case UI_PAGE_ACTION_UPLOAD_RIDE: app_action(UI_ACTION_UPLOAD_RIDE); break;
        case UI_PAGE_ACTION_HISTORY: app_action(UI_ACTION_HISTORY); break;
        case UI_PAGE_ACTION_ACHIEVEMENT: app_action(UI_ACTION_ACHIEVEMENT); break;
        case UI_PAGE_ACTION_SETTINGS: app_action(UI_ACTION_SETTINGS); break;
        case UI_PAGE_ACTION_SYSTEM: app_action(UI_ACTION_SYSTEM); break;
        case UI_PAGE_ACTION_POWER_OFF: app_action(UI_ACTION_POWER_OFF); break;
        case UI_PAGE_ACTION_POWER_RESTART: app_action(UI_ACTION_POWER_RESTART); break;
        case UI_PAGE_ACTION_POWER_CANCEL: app_action(UI_ACTION_POWER_CANCEL); break;
        default: break;
    }
}
static void app_control_center_action(ui_control_center_action_t action,
                                      void *user_data) {
    (void)user_data;
    ui_sound_play(UI_SFX_PRESS);
    switch(action) {
        case UI_CONTROL_CENTER_ACTION_WIFI: app_action(UI_ACTION_WIFI); break;
        case UI_CONTROL_CENTER_ACTION_BLUETOOTH: app_action(UI_ACTION_BT); break;
        case UI_CONTROL_CENTER_ACTION_GPS: app_action(UI_ACTION_GPS); break;
        case UI_CONTROL_CENTER_ACTION_POWER_SAVE:
            app_action(UI_ACTION_POWER_SAVE);
            break;
        case UI_CONTROL_CENTER_ACTION_START_RIDE:
            app_action(UI_ACTION_START_RIDE);
            break;
        case UI_CONTROL_CENTER_ACTION_PAUSE_RIDE:
            app_action(UI_ACTION_PAUSE_RIDE);
            break;
        case UI_CONTROL_CENTER_ACTION_END_RIDE:
            app_action(UI_ACTION_END_RIDE);
            break;
        default: break;
    }
}
static void app_create_screens(void) {
    s_pages[UI_PAGE_BOOT].screen = ui_boot_page_create();
    ui_dashboard_create();
    s_pages[UI_PAGE_DASHBOARD].screen = ui_dashboard_get_screen();
    app_create_navigation();
    s_pages[UI_PAGE_SUMMARY].screen = ui_summary_page_create(app_page_action, NULL, s_wifi, s_bluetooth);
    s_pages[UI_PAGE_PROFILE].screen = ui_profile_page_create(app_page_action, NULL, s_wifi, s_bluetooth);
    s_pages[UI_PAGE_SETTINGS].screen = ui_settings_page_create(app_page_action, NULL, s_wifi, s_bluetooth);
    s_pages[UI_PAGE_SYSTEM].screen = ui_system_page_create(app_page_action, NULL, s_wifi, s_bluetooth);
    ui_control_center_create(app_control_center_action, NULL);
    s_quick_overlay = ui_control_center_root();
    s_pages[UI_PAGE_POWER].screen = ui_power_page_create(app_page_action, NULL);
    s_off_screen = app_make_screen();
    lv_obj_set_style_bg_image_src(s_off_screen, NULL, 0);
    lv_obj_set_style_bg_color(s_off_screen, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_off_screen, LV_OPA_COVER, 0);
}
static void app_register_routes(void) {
    (void)ui_router_register(UI_ROUTE_BOOT, s_pages[UI_PAGE_BOOT].screen);
    (void)ui_router_register(UI_ROUTE_RIDE_DASHBOARD, s_pages[UI_PAGE_DASHBOARD].screen);
    (void)ui_router_register(UI_ROUTE_NAVIGATION, s_pages[UI_PAGE_NAVIGATION].screen);
    (void)ui_router_register(UI_ROUTE_RIDE_SUMMARY, s_pages[UI_PAGE_SUMMARY].screen);
    (void)ui_router_register(UI_ROUTE_PROFILE, s_pages[UI_PAGE_PROFILE].screen);
    (void)ui_router_register(UI_ROUTE_SETTINGS, s_pages[UI_PAGE_SETTINGS].screen);
    (void)ui_router_register(UI_ROUTE_SYSTEM, s_pages[UI_PAGE_SYSTEM].screen);
    (void)ui_router_register(UI_ROUTE_POWER, s_pages[UI_PAGE_POWER].screen);
    (void)ui_router_register(UI_ROUTE_POWER_OFF, s_off_screen);
}
static void app_boot_timer_cb(lv_timer_t *timer) {
    (void)app_publish_event(UI_EVENT_BOOT_COMPLETE,
                            UI_EVENT_PRIORITY_P2_RIDE_STATE);
    (void)ui_router_replace(UI_PAGE_DASHBOARD, UI_MOTION_PAGE_NONE);
    s_boot_timer = NULL;
    lv_timer_delete(timer);
}
static void app_update_quick_states(void) {
    if(!s_quick_overlay) return;
    const ui_control_center_state_t state = {
        .wifi = s_wifi,
        .bluetooth = s_bluetooth,
        .gps = s_gps,
        .power_save = s_power_save,
        .ride_controls_enabled = ui_state_machine_is_riding(),
        .ride_paused = ui_state_machine_is_paused(),
        .satellites = s_satellites,
        .sound_level = s_sound_level,
        .brightness_level = s_brightness_level
    };
    ui_control_center_set_state(&state);
}
static void app_update_slider_visuals(void) {
    app_update_quick_states();
    ui_sound_set_volume(s_sound_level);
    ui_background_set_opacity((lv_opa_t)(70 + (185 * s_brightness_level / 100)));
}
static void app_action(ui_action_t action) {
    switch(action) {
        case UI_ACTION_HOME:
            ui_router_clear_history();
            if(ui_state_machine_state() == UI_RIDE_STATE_SUMMARY) {
                (void)app_publish_event(UI_EVENT_RIDE_SUMMARY_DISMISSED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
            }
            (void)ui_router_replace(UI_PAGE_DASHBOARD, UI_MOTION_PAGE_NONE);
            break;
        case UI_ACTION_NAVIGATION: app_show(UI_PAGE_NAVIGATION); break;
        case UI_ACTION_SUMMARY: app_show(UI_PAGE_SUMMARY); break;
        case UI_ACTION_PROFILE: app_show(UI_PAGE_PROFILE); break;
        case UI_ACTION_SETTINGS: app_show(UI_PAGE_SETTINGS); break;
        case UI_ACTION_SYSTEM: app_show(UI_PAGE_SYSTEM); break;
        case UI_ACTION_QUICK: app_open_quick(); break;
        case UI_ACTION_SAVE_RIDE:
        case UI_ACTION_UPLOAD_RIDE:
            ui_sound_play(UI_SFX_SUCCESS);
            break;
        case UI_ACTION_START_RIDE:
            if(ui_state_machine_state() == UI_RIDE_STATE_HOME) {
                app_hide_quick_immediate();
                (void)app_publish_event(UI_EVENT_RIDE_START_REQUESTED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
            }
            break;
        case UI_ACTION_PAUSE_RIDE:
            if(ui_state_machine_state() == UI_RIDE_STATE_RIDING) {
                app_hide_quick_immediate();
                (void)app_publish_event(UI_EVENT_RIDE_PAUSE_REQUESTED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
            }
            else if(ui_state_machine_state() == UI_RIDE_STATE_PAUSED) {
                app_hide_quick_immediate();
                (void)app_publish_event(UI_EVENT_RIDE_RESUME_REQUESTED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
            }
            break;
        case UI_ACTION_END_RIDE:
            if(ui_state_machine_is_riding()) {
                app_hide_quick_immediate();
                (void)app_publish_event(UI_EVENT_RIDE_FINISH_REQUESTED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
            }
            break;
        case UI_ACTION_WIFI:
            s_wifi = !s_wifi;
            app_update_status();
            break;
        case UI_ACTION_BT:
            s_bluetooth = !s_bluetooth;
            app_update_status();
            break;
        case UI_ACTION_GPS:
            s_gps = !s_gps;
            break;
        case UI_ACTION_POWER_SAVE:
            s_power_save = !s_power_save;
            break;
        case UI_ACTION_BRIGHTNESS:
        case UI_ACTION_SOUND:
        case UI_ACTION_HISTORY:
        case UI_ACTION_ACHIEVEMENT:
            break;
        case UI_ACTION_POWER_OFF:
            app_hide_quick_immediate();
            ui_dock_clear();
            ui_event_bus_clear();
            s_powered_off = true;
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_RESET);
            (void)ui_router_replace(UI_ROUTE_POWER_OFF, UI_MOTION_PAGE_NONE);
            break;
        case UI_ACTION_POWER_RESTART:
            app_hide_quick_immediate();
            ui_dock_clear();
            ui_event_bus_clear();
            ui_router_clear_history();
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_RESET);
            (void)ui_router_replace(UI_PAGE_BOOT, UI_MOTION_PAGE_NONE);
            if(!s_boot_timer) {
                s_boot_timer = lv_timer_create(app_boot_timer_cb, 1300, NULL);
                lv_timer_set_repeat_count(s_boot_timer, 1);
            }
            break;
        case UI_ACTION_POWER_CANCEL: app_back(); break;
        default: break;
    }
    app_update_quick_states();
}
void ui_app_init(void) {
    ui_motion_init();
    ui_sound_init();
    ui_event_bridge_init();
    ui_event_bus_init();
    ui_dock_init();
    ui_state_machine_init();
    ui_ride_data_model_init();
    ui_router_init(UI_ROUTE_RIDE_DASHBOARD);
    app_create_screens();
    app_register_routes();
    ui_dock_view_init();
    ui_event_bus_set_overflow_callback(app_event_overflow, NULL);
    ui_dock_controller_init();
    ui_ride_controller_init();
    (void)ui_event_bus_subscribe(UI_EVENT_MASK_ALL, app_event_received, NULL);
    (void)ui_ride_data_model_subscribe(app_ride_data_changed, NULL);
    ui_ride_data_model_set_connectivity(s_wifi, s_bluetooth);
    (void)ui_ride_data_model_process();
    app_update_slider_visuals();
    (void)ui_router_replace(UI_PAGE_BOOT, UI_MOTION_PAGE_NONE);
    s_boot_timer = lv_timer_create(app_boot_timer_cb, 1600, NULL);
    lv_timer_set_repeat_count(s_boot_timer, 1);
    s_event_timer = lv_timer_create(app_event_timer_cb, 20, NULL);
}
void ui_app_handle_back(void) {
    if(!s_quick_visible && ui_router_current() == UI_PAGE_DASHBOARD &&
       ui_dynamic_card_manager_back()) return;
    app_back();
}

void ui_app_handle_input(ui_input_event_t input) {
    if(input == UI_INPUT_BACK) {
        ui_app_handle_back();
        return;
    }
    if(!s_powered_off && !s_quick_visible && ui_router_current() == UI_PAGE_DASHBOARD &&
       ui_dynamic_card_manager_handle_input(input)) return;
    if(!s_powered_off && !s_quick_visible && ui_router_current() == UI_PAGE_NAVIGATION &&
       ui_navigation_handle_input(input)) return;

    if(input == UI_INPUT_ENCODER_CW) ui_app_handle_key(LV_KEY_DOWN);
    else if(input == UI_INPUT_ENCODER_CCW) ui_app_handle_key(LV_KEY_UP);
    else if(input == UI_INPUT_ENCODER_PRESS) ui_app_handle_key(LV_KEY_ENTER);
}

void ui_app_handle_key(uint32_t key) {
    if(s_powered_off) {
        if(key == 'p' || key == 'P' || key == LV_KEY_ENTER) {
            s_powered_off = false;
            (void)app_publish_event(UI_EVENT_BOOT_COMPLETE,
                                    UI_EVENT_PRIORITY_P2_RIDE_STATE);
            (void)ui_router_replace(UI_PAGE_DASHBOARD, UI_MOTION_PAGE_NONE);
        }
        return;
    }
    if(key == 'p' || key == 'P') {
        app_hide_quick_immediate();
        ui_dock_clear();
        ui_event_bus_clear();
        s_powered_off = true;
        (void)ui_state_machine_dispatch(UI_RIDE_EVENT_RESET);
        (void)ui_router_replace(UI_ROUTE_POWER_OFF, UI_MOTION_PAGE_NONE);
        return;
    }
    if(key == 'o' || key == 'O') { app_show(UI_PAGE_POWER); return; }
    if(key == 'm' || key == 'M') { if(s_quick_visible) app_close_quick(); else app_open_quick(); return; }
    if(key == '[') { app_show(UI_PAGE_PROFILE); return; }
    if(key == ']') { app_show(UI_PAGE_SETTINGS); return; }
    if(key == LV_KEY_ESC || key == 8) { app_back(); return; }
    if(s_quick_visible) return;
    if(ui_router_current() == UI_PAGE_NAVIGATION) {
        if(key == LV_KEY_UP) { ui_navigation_handle_input(UI_INPUT_ENCODER_CCW); return; }
        if(key == LV_KEY_DOWN) { ui_navigation_handle_input(UI_INPUT_ENCODER_CW); return; }
        if(key == LV_KEY_ENTER) { ui_navigation_handle_input(UI_INPUT_ENCODER_PRESS); return; }
    }
    if(ui_router_current() == UI_PAGE_SETTINGS &&
       ui_settings_page_handle_key(key)) return;
    if(key == LV_KEY_LEFT) app_show(UI_PAGE_NAVIGATION); else if(key == LV_KEY_RIGHT) app_show(UI_PAGE_PROFILE); else if(key == LV_KEY_ENTER) app_open_quick();
}

static uint8_t app_slider_level_from_y(int32_t screen_y) {
    return ui_control_center_level_from_y(screen_y);
}

static void app_update_active_slider(int32_t screen_y) {
    uint8_t value = app_slider_level_from_y(screen_y);
    if(s_active_slider == UI_CONTROL_CENTER_SLIDER_SOUND) {
        s_sound_level = value;
        ui_sound_set_volume(s_sound_level);
    } else if(s_active_slider == UI_CONTROL_CENTER_SLIDER_BRIGHTNESS) {
        s_brightness_level = value;
    }
    app_update_slider_visuals();
}

void ui_app_handle_pointer(lv_event_code_t code, int32_t x, int32_t y) {
    if(s_quick_overlay == NULL || s_powered_off) return;

    lv_point_t point = { .x = x, .y = y };

    if(code == LV_EVENT_PRESSED) {
        s_pointer_start = point;
        s_overlay_start_y = s_quick_visible ? lv_obj_get_y(s_quick_overlay) : -UI_SCREEN_H;
        s_pointer_dragging = false;
        s_pointer_opening = false;
        s_slider_dragging = false;
        s_page_swiping = false;
        s_home_swiping = false;
        s_active_slider = UI_CONTROL_CENTER_SLIDER_NONE;
        s_dynamic_card_gesture_owned =
            !s_quick_visible && ui_router_current() == UI_PAGE_DASHBOARD &&
            ui_dynamic_card_manager_contains_point(point.x, point.y);
        s_navigation_map_gesture_owned =
            !s_quick_visible && ui_router_current() == UI_PAGE_NAVIGATION &&
            ui_navigation_contains_point(point.x, point.y);

        if(s_dynamic_card_gesture_owned || s_navigation_map_gesture_owned) return;

        if(s_quick_visible && s_overlay_start_y == 0) {
            s_active_slider = ui_control_center_slider_at(point.x, point.y);
            if(s_active_slider != UI_CONTROL_CENTER_SLIDER_NONE) {
                s_slider_dragging = true;
                app_update_active_slider(point.y);
            }
        }
        return;
    }

    if(code == LV_EVENT_PRESSING) {
        if(s_dynamic_card_gesture_owned || s_navigation_map_gesture_owned) return;

        if(s_slider_dragging) {
            app_update_active_slider(point.y);
            return;
        }

        int32_t delta_x = point.x - s_pointer_start.x;
        int32_t delta_y = point.y - s_pointer_start.y;
        int32_t abs_x = delta_x < 0 ? -delta_x : delta_x;
        int32_t abs_y = delta_y < 0 ? -delta_y : delta_y;

        if(!s_pointer_dragging && !s_page_swiping && !s_home_swiping) {
            if(abs_x > QUICK_DRAG_THRESHOLD && abs_x > abs_y && !s_quick_visible &&
               (ui_router_current() == UI_PAGE_DASHBOARD || ui_router_current() == UI_PAGE_NAVIGATION ||
                ui_router_current() == UI_PAGE_PROFILE)) {
                s_page_swiping = true;
                return;
            }
            if(abs_y <= QUICK_DRAG_THRESHOLD || abs_y <= abs_x) return;
            if(!s_quick_visible && delta_y > 0) {
                ui_motion_cancel_y(s_quick_overlay);
                lv_obj_clear_flag(s_quick_overlay, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(s_quick_overlay);
                s_quick_visible = true;
                s_pointer_opening = true;
                s_pointer_dragging = true;
                app_update_quick_states();
            } else if(s_quick_visible && delta_y < 0) {
                ui_motion_cancel_y(s_quick_overlay);
                s_pointer_opening = false;
                s_pointer_dragging = true;
            } else if(!s_quick_visible && delta_y < 0) {
                s_home_swiping = true;
                return;
            } else {
                return;
            }
        }

        if(s_page_swiping || s_home_swiping) return;

        int32_t target_y;
        if(s_pointer_opening) {
            target_y = -UI_SCREEN_H + delta_y;
        } else {
            target_y = s_overlay_start_y + delta_y;
        }
        if(target_y < -UI_SCREEN_H) target_y = -UI_SCREEN_H;
        if(target_y > 0) target_y = 0;
        lv_obj_set_y(s_quick_overlay, target_y);
        return;
    }

    if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if(s_dynamic_card_gesture_owned) {
            s_dynamic_card_gesture_owned = false;
            s_pointer_dragging = false;
            s_pointer_opening = false;
            s_page_swiping = false;
            s_home_swiping = false;
            return;
        }
        if(s_navigation_map_gesture_owned) {
            s_navigation_map_gesture_owned = false;
            s_pointer_dragging = false;
            s_pointer_opening = false;
            s_page_swiping = false;
            s_home_swiping = false;
            return;
        }

        if(s_slider_dragging) {
            app_update_active_slider(point.y);
            s_slider_dragging = false;
            s_active_slider = UI_CONTROL_CENTER_SLIDER_NONE;
            return;
        }

        int32_t delta_x = point.x - s_pointer_start.x;
        int32_t delta_y = point.y - s_pointer_start.y;
        if(s_page_swiping) {
            if(delta_x <= -72) {
                if(ui_router_current() == UI_PAGE_DASHBOARD) app_show_slide(UI_PAGE_NAVIGATION, true);
                else if(ui_router_current() == UI_PAGE_NAVIGATION) app_show_slide(UI_PAGE_PROFILE, true);
            } else if(delta_x >= 72) {
                if(ui_router_current() == UI_PAGE_PROFILE) app_show_slide(UI_PAGE_NAVIGATION, false);
                else if(ui_router_current() == UI_PAGE_NAVIGATION) app_show_slide(UI_PAGE_DASHBOARD, false);
            }
            s_page_swiping = false;
            return;
        }
        if(s_home_swiping) {
            if(delta_y <= -72) app_action(UI_ACTION_HOME);
            s_home_swiping = false;
            return;
        }

        if(s_pointer_dragging) {
            if(s_pointer_opening) {
                if(delta_y >= 72) app_animate_overlay_to(0, false);
                else app_animate_overlay_to(-UI_SCREEN_H, true);
            } else {
                if(delta_y <= -72 || lv_obj_get_y(s_quick_overlay) <= -UI_SCREEN_H / 4) app_close_quick();
                else app_animate_overlay_to(0, false);
            }
        }

        s_pointer_dragging = false;
        s_pointer_opening = false;
        s_page_swiping = false;
        s_home_swiping = false;
    }
}
