#include "ui_app.h"

#include "ui_colors.h"
#include "ui_app_state.h"
#include "ui_background.h"
#include "ui_bottom_surface.h"
#include "ui_dock.h"
#include "ui_dock_controller.h"
#include "ui_event_bridge.h"
#include "ui_event_bus.h"
#include "ui_input_queue.h"
#include "ui_motion.h"
#include "ui_ride_controller.h"
#include "ui_router.h"
#include "ui_sizes.h"
#include "ui_sound.h"
#include "ui_state_machine.h"
#include "ui_theme.h"
#include "../../services/ui_command_service.h"
#include "../../services/ui_system_services.h"
#include "../../services/ui_state_adapter.h"
#include "../../services/ui_navigation_service.h"
#include "../components/ui_status_bar.h"
#include "../components/ui_dynamic_card_manager.h"
#include "../components/dock/ui_dock_view.h"
#include "../screens/ui_dashboard.h"
#include "../pages/navigation/ui_navigation.h"
#include "../pages/ui_boot_page.h"
#include "../pages/ui_home_page.h"
#include "../pages/ui_page_action.h"
#include "../pages/ui_page_common.h"
#include "../pages/ui_power_page.h"
#include "../pages/ui_profile_page.h"
#include "../pages/ui_settings_page.h"
#include "../pages/ui_summary_page.h"
#include "../pages/ui_system_page.h"
#include "../models/ui_ride_data_model.h"
#include "../models/ui_home_data_model.h"
#include "../overlays/ui_control_center.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef ui_route_id_t ui_page_t;
#define UI_PAGE_BOOT        UI_ROUTE_BOOT
#define UI_PAGE_HOME        UI_ROUTE_HOME
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
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *bottom_surface_slot;
    ui_status_bar_t status;
    bool show_dynamic_surface;
} ui_page_view_t;

static ui_page_view_t s_pages[UI_PAGE_COUNT];
static lv_timer_t *s_boot_timer;
static lv_timer_t *s_event_timer;
static bool s_powered_off;
static bool s_quick_visible, s_pointer_dragging, s_pointer_opening, s_slider_dragging;
static bool s_page_swiping, s_home_swiping;
static bool s_dynamic_card_gesture_owned;
static bool s_navigation_map_gesture_owned;
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
static void app_state_changed(const ui_app_state_t *state,
                              ui_app_state_change_mask_t changed,
                              void *user_data);
static void app_route_changed(ui_route_id_t previous,
                              ui_route_id_t current, void *user_data);
static void app_ride_state_changed(ui_ride_state_t previous,
                                   ui_ride_state_t current,
                                   ui_ride_event_t event, void *user_data);
static void app_command_changed(const ui_command_result_t *result,
                                void *user_data);
static void app_handle_key(uint32_t key);
static bool app_submit_command(ui_command_type_t type, int32_t value,
                               uint32_t timeout_ms);
static void app_publish_command_feedback(const ui_command_result_t *result);
static bool app_ride_domain_active(void);
static void app_show_feature_info(const char *title, const char *message);

static void app_input_received(const ui_input_message_t *message,
                               void *user_data)
{
    (void)user_data;
    if(message == NULL) return;
    if(message->type >= UI_INPUT_TOUCH_BEGIN &&
       message->type <= UI_INPUT_TOUCH_CANCEL) {
        ui_app_handle_pointer(message);
    }
    else {
        ui_app_handle_input(message->type);
    }
}

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

static bool app_ride_domain_active(void)
{
    ui_ride_state_t state = ui_state_machine_state();
    return state == UI_RIDE_STATE_PRE_RIDE ||
           state == UI_RIDE_STATE_GPS_READY ||
           state == UI_RIDE_STATE_RIDING ||
           state == UI_RIDE_STATE_PAUSED ||
           state == UI_RIDE_STATE_FINISHING;
}

static void app_show_feature_info(const char *title, const char *message)
{
    ui_event_t event = {0};
    event.type = UI_EVENT_FEATURE_INFO;
    event.priority = UI_EVENT_PRIORITY_P4_NORMAL;
    event.dedupe_key = 0xF001U;
    event.timeout_ms = 2200U;
    snprintf(event.title, sizeof(event.title), "%s",
             title != NULL ? title : "COMING SOON");
    snprintf(event.message, sizeof(event.message), "%s",
             message != NULL ? message : "FEATURE RESERVED");
    (void)ui_event_bus_publish(&event);
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
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
            if(!app_submit_command(UI_COMMAND_GNSS_ACQUIRE_FIX, 0, 3000U))
                (void)app_publish_event(UI_EVENT_GPS_FAILED,
                                        UI_EVENT_PRIORITY_P1_RIDING_SAFETY);
            break;
        case UI_EVENT_RIDE_GO_REQUESTED:
            if(!app_submit_command(UI_COMMAND_RIDE_START, 0, 2000U)) {
                ui_event_t failed = {0};
                failed.type = UI_EVENT_COMMAND_FAILED;
                failed.priority = UI_EVENT_PRIORITY_P2_RIDE_STATE;
                failed.timeout_ms = 4000U;
                snprintf(failed.title, sizeof(failed.title), "START FAILED");
                snprintf(failed.message, sizeof(failed.message),
                         "RIDE COMMAND QUEUE REJECTED");
                snprintf(failed.primary_action,
                         sizeof(failed.primary_action), "CLOSE");
                (void)ui_event_bus_publish(&failed);
            }
            break;
        case UI_EVENT_RIDE_FINISH_CONFIRMED:
            if(!app_submit_command(UI_COMMAND_RIDE_FINISH, 0, 2000U)) {
                (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED,
                                        UI_EVENT_PRIORITY_P0_CRITICAL);
            }
            break;
        case UI_EVENT_STORAGE_RETRY:
            if(ui_state_machine_state() == UI_RIDE_STATE_FINISHING &&
               !app_submit_command(UI_COMMAND_RIDE_SAVE, 0, 5000U)) {
                (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED,
                                        UI_EVENT_PRIORITY_P0_CRITICAL);
            }
            break;
        case UI_EVENT_MEDIA_PREVIOUS:
        case UI_EVENT_MEDIA_TOGGLE:
        case UI_EVENT_MEDIA_NEXT:
        case UI_EVENT_MEDIA_MODE: {
            if(event->type == UI_EVENT_MEDIA_PREVIOUS) {
                (void)app_submit_command(UI_COMMAND_MEDIA_PREVIOUS,
                                         0, 1500U);
            }
            else if(event->type == UI_EVENT_MEDIA_NEXT) {
                (void)app_submit_command(UI_COMMAND_MEDIA_NEXT, 0, 1500U);
            }
            else if(event->type == UI_EVENT_MEDIA_TOGGLE) {
                (void)app_submit_command(UI_COMMAND_MEDIA_TOGGLE_PLAYBACK,
                                         0, 1500U);
            }
            else {
                (void)app_submit_command(UI_COMMAND_MEDIA_CYCLE_MODE,
                                         0, 1500U);
            }
            break;
        }
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
    ui_command_service_process(20);
    (void)ui_state_adapter_process();
    (void)ui_ride_data_model_process();
    (void)ui_navigation_service_process();
    (void)ui_input_queue_drain(UI_INPUT_QUEUE_MAX,
                               app_input_received, NULL);
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
    ui_page_status_create(&s_pages[page].status, s_pages[page].screen);
}
static void app_ride_data_changed(const ui_ride_data_t *data,
                                  uint32_t version, void *user_data) {
    (void)version;
    (void)user_data;
    if(data == NULL) return;
    ui_dashboard_set_data(data);
}

static void app_state_changed(const ui_app_state_t *state,
                              ui_app_state_change_mask_t changed,
                              void *user_data)
{
    (void)user_data;
    if(state == NULL) return;
    if((changed & UI_APP_STATE_CHANGE_CONTROL) != 0U) {
        ui_sound_set_volume(state->sound_level);
        ui_background_set_opacity(
            (lv_opa_t)(70 + (185 * state->brightness_level / 100)));
    }
    if((changed & (UI_APP_STATE_CHANGE_STATUS |
                   UI_APP_STATE_CHANGE_CONNECTIVITY |
                   UI_APP_STATE_CHANGE_CONTROL |
                   UI_APP_STATE_CHANGE_RIDE)) != 0U) {
        app_update_quick_states();
    }
}

static void app_route_changed(ui_route_id_t previous,
                              ui_route_id_t current, void *user_data)
{
    (void)previous;
    (void)user_data;
    ui_app_state_set_route((uint8_t)current);
}

static void app_ride_state_changed(ui_ride_state_t previous,
                                   ui_ride_state_t current,
                                   ui_ride_event_t event, void *user_data)
{
    (void)previous;
    (void)event;
    (void)user_data;
    ui_app_state_set_ride_state(current);
}

static bool app_submit_command(ui_command_type_t type, int32_t value,
                               uint32_t timeout_ms)
{
    uint32_t command_id =
        ui_command_service_submit(type, value, timeout_ms);
    if(command_id == 0U)
        LV_LOG_WARN("UI command queue rejected type=%d", (int)type);
    return command_id != 0U;
}

static const char *app_command_label(ui_command_type_t type)
{
    switch(type) {
        case UI_COMMAND_WIFI_SET_ENABLED: return "WIFI";
        case UI_COMMAND_BLUETOOTH_SET_ENABLED: return "BLUETOOTH";
        case UI_COMMAND_GNSS_SET_ENABLED: return "GPS";
        case UI_COMMAND_POWER_SAVE_SET_ENABLED: return "POWER SAVE";
        case UI_COMMAND_SOUND_SET_LEVEL: return "VOLUME";
        case UI_COMMAND_BRIGHTNESS_SET_LEVEL: return "BRIGHTNESS";
        case UI_COMMAND_RIDE_START: return "START RIDE";
        case UI_COMMAND_RIDE_PAUSE: return "PAUSE RIDE";
        case UI_COMMAND_RIDE_RESUME: return "RESUME RIDE";
        case UI_COMMAND_RIDE_UPLOAD: return "UPLOAD RIDE DATA";
        case UI_COMMAND_MEDIA_SET_PLAYING:
        case UI_COMMAND_MEDIA_SET_TRACK:
        case UI_COMMAND_MEDIA_SET_MODE:
        case UI_COMMAND_MEDIA_PREVIOUS:
        case UI_COMMAND_MEDIA_TOGGLE_PLAYBACK:
        case UI_COMMAND_MEDIA_NEXT:
        case UI_COMMAND_MEDIA_CYCLE_MODE:
            return "MEDIA CONTROL";
        default: return "DEVICE REQUEST";
    }
}

static bool app_command_feedback_needed(const ui_command_result_t *result)
{
    if(result == NULL) return false;
    switch(result->type) {
        case UI_COMMAND_GNSS_ACQUIRE_FIX:
        case UI_COMMAND_RIDE_FINISH:
        case UI_COMMAND_RIDE_SAVE:
            return false;
        case UI_COMMAND_RIDE_START:
        case UI_COMMAND_RIDE_PAUSE:
        case UI_COMMAND_RIDE_RESUME:
        case UI_COMMAND_MEDIA_SET_PLAYING:
        case UI_COMMAND_MEDIA_SET_TRACK:
        case UI_COMMAND_MEDIA_SET_MODE:
        case UI_COMMAND_MEDIA_PREVIOUS:
        case UI_COMMAND_MEDIA_TOGGLE_PLAYBACK:
        case UI_COMMAND_MEDIA_NEXT:
        case UI_COMMAND_MEDIA_CYCLE_MODE:
            return result->status != UI_COMMAND_STATUS_PENDING &&
                   result->status != UI_COMMAND_STATUS_SUCCESS;
        default:
            return true;
    }
}

static void app_publish_command_feedback(const ui_command_result_t *result)
{
    if(!app_command_feedback_needed(result)) return;
    ui_event_t event = {0};
    event.priority = UI_EVENT_PRIORITY_P3_USER_ACTION;
    event.source_id = result->id;
    event.dedupe_key = 0xC001U;
    snprintf(event.message, sizeof(event.message), "%s",
             app_command_label(result->type));
    if(result->status == UI_COMMAND_STATUS_PENDING) {
        event.type = UI_EVENT_COMMAND_PENDING;
        snprintf(event.title, sizeof(event.title), "PROCESSING");
    }
    else if(result->status == UI_COMMAND_STATUS_SUCCESS) {
        event.type = UI_EVENT_COMMAND_SUCCESS;
        event.timeout_ms = 1200U;
        snprintf(event.title, sizeof(event.title), "DONE");
    }
    else {
        event.type = UI_EVENT_COMMAND_FAILED;
        event.timeout_ms = 6000U;
        snprintf(event.title, sizeof(event.title), "%s",
                 result->status == UI_COMMAND_STATUS_TIMEOUT ?
                 "TIMEOUT" :
                 (result->status == UI_COMMAND_STATUS_REJECTED ?
                  "REJECTED" : "FAILED"));
        snprintf(event.primary_action, sizeof(event.primary_action), "CLOSE");
    }
    (void)ui_event_bus_publish(&event);
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
}

static void app_command_changed(const ui_command_result_t *result,
                                 void *user_data)
{
    (void)user_data;
    if(result == NULL) return;
    app_publish_command_feedback(result);
    if(result->status == UI_COMMAND_STATUS_PENDING) return;

    if(result->status == UI_COMMAND_STATUS_SUCCESS) {
        switch(result->type) {
            case UI_COMMAND_GNSS_ACQUIRE_FIX:
                (void)app_publish_event(UI_EVENT_GPS_READY,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
                break;
            case UI_COMMAND_RIDE_START:
                app_hide_quick_immediate();
                (void)app_publish_event(UI_EVENT_RIDE_GO,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
                break;
            case UI_COMMAND_RIDE_PAUSE:
                app_hide_quick_immediate();
                (void)app_publish_event(UI_EVENT_RIDE_PAUSE_REQUESTED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
                break;
            case UI_COMMAND_RIDE_RESUME:
                app_hide_quick_immediate();
                (void)app_publish_event(UI_EVENT_RIDE_RESUME_REQUESTED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
                break;
            case UI_COMMAND_RIDE_FINISH:
                if(!app_submit_command(UI_COMMAND_RIDE_SAVE, 0, 5000U))
                    (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED,
                                            UI_EVENT_PRIORITY_P0_CRITICAL);
                break;
            case UI_COMMAND_RIDE_SAVE:
                if(ui_state_machine_state() == UI_RIDE_STATE_FINISHING) {
                    (void)app_publish_event(UI_EVENT_RIDE_SAVE_SUCCESS,
                                            UI_EVENT_PRIORITY_P2_RIDE_STATE);
                }
                else {
                    ui_sound_play(UI_SFX_SUCCESS);
                }
                break;
            case UI_COMMAND_RIDE_UPLOAD:
                ui_sound_play(UI_SFX_SUCCESS);
                break;
            default:
                break;
        }
        return;
    }

    LV_LOG_WARN("UI command failed: id=%lu type=%d status=%d error=%ld",
                (unsigned long)result->id, (int)result->type,
                (int)result->status, (long)result->error_code);
    if(result->type == UI_COMMAND_GNSS_ACQUIRE_FIX) {
        (void)app_publish_event(UI_EVENT_GPS_FAILED,
                                UI_EVENT_PRIORITY_P1_RIDING_SAFETY);
    }
    else if(result->type == UI_COMMAND_RIDE_SAVE) {
        (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED,
                                UI_EVENT_PRIORITY_P0_CRITICAL);
    }
    else if(result->type == UI_COMMAND_RIDE_FINISH) {
        (void)app_publish_event(UI_EVENT_RIDE_FINISH_CANCELLED,
                                UI_EVENT_PRIORITY_P2_RIDE_STATE);
    }
    app_update_quick_states();
}

static void app_home_enter(ui_route_id_t route, lv_obj_t *screen,
                           void *user_data)
{
    (void)screen;
    (void)user_data;
    ui_page_view_t *page = &s_pages[route];
    if(!ui_bottom_surface_mount(page->bottom_surface_slot, false))
        LV_LOG_ERROR("Home Bottom Surface mount failed");
    ui_home_page_set_active(true);
}

static void app_home_leave(ui_route_id_t route, lv_obj_t *screen,
                           void *user_data)
{
    (void)route;
    (void)screen;
    (void)user_data;
    ui_home_page_set_active(false);
    ui_bottom_surface_unmount();
}

static void app_dashboard_enter(ui_route_id_t route, lv_obj_t *screen,
                                void *user_data)
{
    (void)screen;
    (void)user_data;
    ui_page_view_t *page = &s_pages[route];
    if(!ui_bottom_surface_mount(page->bottom_surface_slot,
                                page->show_dynamic_surface))
        LV_LOG_ERROR("Dashboard Bottom Surface mount failed");
    ui_dashboard_set_active(true);
}

static void app_dashboard_leave(ui_route_id_t route, lv_obj_t *screen,
                                void *user_data)
{
    (void)screen;
    (void)user_data;
    ui_dashboard_set_active(false);
    ui_bottom_surface_unmount();
}

static void app_navigation_enter(ui_route_id_t route, lv_obj_t *screen,
                                 void *user_data)
{
    (void)screen;
    (void)user_data;
    ui_page_view_t *page = &s_pages[route];
    if(!ui_bottom_surface_mount(page->bottom_surface_slot,
                                page->show_dynamic_surface))
        LV_LOG_ERROR("Navigation Bottom Surface mount failed");
    ui_navigation_set_active(true);
    ui_app_state_set_navigation_active(true);
}

static void app_navigation_leave(ui_route_id_t route, lv_obj_t *screen,
                                 void *user_data)
{
    (void)screen;
    (void)user_data;
    ui_navigation_set_active(false);
    ui_app_state_set_navigation_active(false);
    ui_bottom_surface_unmount();
}

static void app_surface_page_enter(ui_route_id_t route, lv_obj_t *screen,
                                   void *user_data)
{
    (void)screen;
    (void)user_data;
    if(route >= UI_PAGE_COUNT) return;
    ui_page_view_t *page = &s_pages[route];
    if(!ui_bottom_surface_mount(page->bottom_surface_slot,
                                page->show_dynamic_surface))
        LV_LOG_ERROR("Page Bottom Surface mount failed route=%d", (int)route);
}

static void app_surface_page_leave(ui_route_id_t route, lv_obj_t *screen,
                                   void *user_data)
{
    (void)route;
    (void)screen;
    (void)user_data;
    ui_bottom_surface_unmount();
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
        (void)ui_router_replace(UI_PAGE_HOME, UI_MOTION_PAGE_NONE);
        return;
    }
    if(s_quick_visible) {
        app_close_quick();
        return;
    }
    if(app_ride_domain_active()) {
        if(ui_router_current() == UI_PAGE_NAVIGATION)
            (void)ui_router_replace(UI_PAGE_DASHBOARD,
                                    UI_MOTION_PAGE_BACKWARD);
        return;
    }
    ui_sound_play(UI_SFX_BACK);
    (void)ui_router_back();
}
static void app_create_navigation(void) {
    lv_obj_t *screen = app_make_screen();
    s_pages[UI_PAGE_NAVIGATION].screen = screen;
    app_add_status(UI_PAGE_NAVIGATION);
    ui_navigation_create(screen);
    s_pages[UI_PAGE_NAVIGATION].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(screen);
}
static void app_page_action(ui_page_action_t action, void *user_data) {
    (void)user_data;
    switch(action) {
        case UI_PAGE_ACTION_HOME: app_action(UI_ACTION_HOME); break;
        case UI_PAGE_ACTION_START_RIDE:
            app_action(UI_ACTION_START_RIDE);
            break;
        case UI_PAGE_ACTION_START_NAVIGATION:
            app_action(UI_ACTION_NAVIGATION);
            break;
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
        default: break;
    }
}
static void app_create_screens(void) {
    s_pages[UI_PAGE_BOOT].screen = ui_boot_page_create();
    s_pages[UI_PAGE_HOME].screen = ui_home_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_HOME].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_HOME].screen);
    ui_dashboard_create();
    s_pages[UI_PAGE_DASHBOARD].screen = ui_dashboard_get_screen();
    s_pages[UI_PAGE_DASHBOARD].bottom_surface_slot =
        ui_dashboard_get_bottom_surface_slot();
    s_pages[UI_PAGE_DASHBOARD].show_dynamic_surface = true;
    app_create_navigation();
    s_pages[UI_PAGE_SUMMARY].screen = ui_summary_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_SUMMARY].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_SUMMARY].screen);
    s_pages[UI_PAGE_PROFILE].screen = ui_profile_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_PROFILE].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_PROFILE].screen);
    s_pages[UI_PAGE_SETTINGS].screen = ui_settings_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_SETTINGS].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_SETTINGS].screen);
    s_pages[UI_PAGE_SYSTEM].screen = ui_system_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_SYSTEM].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_SYSTEM].screen);
    ui_control_center_create(app_control_center_action, NULL);
    s_quick_overlay = ui_control_center_root();
    s_pages[UI_PAGE_POWER].screen = ui_power_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_POWER].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_POWER].screen);
    s_off_screen = app_make_screen();
    lv_obj_set_style_bg_image_src(s_off_screen, NULL, 0);
    lv_obj_set_style_bg_color(s_off_screen, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_off_screen, LV_OPA_COVER, 0);
}
static void app_register_routes(void) {
    (void)ui_router_register(UI_ROUTE_BOOT, s_pages[UI_PAGE_BOOT].screen);
    const ui_router_page_t home = {
        .screen = s_pages[UI_PAGE_HOME].screen,
        .on_enter = app_home_enter,
        .on_leave = app_home_leave
    };
    const ui_router_page_t dashboard = {
        .screen = s_pages[UI_PAGE_DASHBOARD].screen,
        .on_enter = app_dashboard_enter,
        .on_leave = app_dashboard_leave
    };
    const ui_router_page_t navigation = {
        .screen = s_pages[UI_PAGE_NAVIGATION].screen,
        .on_enter = app_navigation_enter,
        .on_leave = app_navigation_leave
    };
    const ui_router_page_t summary = {
        .screen = s_pages[UI_PAGE_SUMMARY].screen,
        .on_enter = app_surface_page_enter,
        .on_leave = app_surface_page_leave
    };
    const ui_router_page_t profile = {
        .screen = s_pages[UI_PAGE_PROFILE].screen,
        .on_enter = app_surface_page_enter,
        .on_leave = app_surface_page_leave
    };
    const ui_router_page_t settings = {
        .screen = s_pages[UI_PAGE_SETTINGS].screen,
        .on_enter = app_surface_page_enter,
        .on_leave = app_surface_page_leave
    };
    const ui_router_page_t system = {
        .screen = s_pages[UI_PAGE_SYSTEM].screen,
        .on_enter = app_surface_page_enter,
        .on_leave = app_surface_page_leave
    };
    const ui_router_page_t power = {
        .screen = s_pages[UI_PAGE_POWER].screen,
        .on_enter = app_surface_page_enter,
        .on_leave = app_surface_page_leave
    };
    (void)ui_router_register_page(UI_ROUTE_HOME, &home);
    (void)ui_router_register_page(UI_ROUTE_RIDE_DASHBOARD, &dashboard);
    (void)ui_router_register_page(UI_ROUTE_NAVIGATION, &navigation);
    (void)ui_router_register_page(UI_ROUTE_RIDE_SUMMARY, &summary);
    (void)ui_router_register_page(UI_ROUTE_PROFILE, &profile);
    (void)ui_router_register_page(UI_ROUTE_SETTINGS, &settings);
    (void)ui_router_register_page(UI_ROUTE_SYSTEM, &system);
    (void)ui_router_register_page(UI_ROUTE_POWER, &power);
    (void)ui_router_register(UI_ROUTE_POWER_OFF, s_off_screen);
}
static void app_boot_timer_cb(lv_timer_t *timer) {
    (void)app_publish_event(UI_EVENT_BOOT_COMPLETE,
                            UI_EVENT_PRIORITY_P2_RIDE_STATE);
    (void)ui_router_replace(UI_PAGE_HOME, UI_MOTION_PAGE_NONE);
    s_boot_timer = NULL;
    lv_timer_delete(timer);
}
static void app_update_quick_states(void) {
    if(!s_quick_overlay) return;
    ui_app_state_t app_state;
    if(!ui_app_state_get(&app_state)) return;
    const ui_control_center_state_t center_state = {
        .wifi = app_state.wifi_enabled && app_state.wifi_connected,
        .bluetooth = app_state.bluetooth_enabled &&
                     app_state.bluetooth_connected,
        .gps = app_state.gps_enabled,
        .power_save = app_state.power_save_enabled,
        .satellites = app_state.satellites,
        .sound_level = app_state.sound_level,
        .brightness_level = app_state.brightness_level
    };
    ui_control_center_set_state(&center_state);
}
static void app_update_slider_visuals(void) {
    ui_app_state_t state;
    if(!ui_app_state_get(&state)) return;
    ui_sound_set_volume(state.sound_level);
    ui_background_set_opacity(
        (lv_opa_t)(70 + (185 * state.brightness_level / 100)));
    app_update_quick_states();
}
static void app_action(ui_action_t action) {
    ui_app_state_t state;
    if(!ui_app_state_get(&state)) return;
    switch(action) {
        case UI_ACTION_HOME:
            ui_router_clear_history();
            if(ui_state_machine_state() == UI_RIDE_STATE_SUMMARY) {
                (void)app_publish_event(UI_EVENT_RIDE_SUMMARY_DISMISSED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
            }
            (void)ui_router_replace(app_ride_domain_active() ?
                                    UI_PAGE_DASHBOARD : UI_PAGE_HOME,
                                    UI_MOTION_PAGE_NONE);
            break;
        case UI_ACTION_NAVIGATION: app_show(UI_PAGE_NAVIGATION); break;
        case UI_ACTION_SUMMARY: app_show(UI_PAGE_SUMMARY); break;
        case UI_ACTION_PROFILE: app_show(UI_PAGE_PROFILE); break;
        case UI_ACTION_SETTINGS: app_show(UI_PAGE_SETTINGS); break;
        case UI_ACTION_SYSTEM: app_show(UI_PAGE_SYSTEM); break;
        case UI_ACTION_QUICK: app_open_quick(); break;
        case UI_ACTION_SAVE_RIDE:
            (void)app_submit_command(UI_COMMAND_RIDE_SAVE, 0, 5000U);
            break;
        case UI_ACTION_UPLOAD_RIDE:
            (void)app_submit_command(UI_COMMAND_RIDE_UPLOAD, 0, 5000U);
            break;
        case UI_ACTION_START_RIDE:
            if(ui_state_machine_state() == UI_RIDE_STATE_HOME) {
                app_hide_quick_immediate();
                ui_router_clear_history();
                (void)ui_router_replace(UI_PAGE_DASHBOARD,
                                        UI_MOTION_PAGE_FORWARD);
                (void)app_publish_event(UI_EVENT_RIDE_START_REQUESTED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
            }
            break;
        case UI_ACTION_PAUSE_RIDE:
            if(ui_state_machine_state() == UI_RIDE_STATE_RIDING) {
                (void)app_submit_command(UI_COMMAND_RIDE_PAUSE, 0, 2000U);
            }
            else if(ui_state_machine_state() == UI_RIDE_STATE_PAUSED) {
                (void)app_submit_command(UI_COMMAND_RIDE_RESUME, 0, 2000U);
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
            (void)app_submit_command(UI_COMMAND_WIFI_SET_ENABLED,
                                     !state.wifi_enabled, 2000U);
            break;
        case UI_ACTION_BT:
            (void)app_submit_command(UI_COMMAND_BLUETOOTH_SET_ENABLED,
                                     !state.bluetooth_enabled, 2000U);
            break;
        case UI_ACTION_GPS:
            (void)app_submit_command(UI_COMMAND_GNSS_SET_ENABLED,
                                     !state.gps_enabled, 2000U);
            break;
        case UI_ACTION_POWER_SAVE:
            (void)app_submit_command(UI_COMMAND_POWER_SAVE_SET_ENABLED,
                                     !state.power_save_enabled, 2000U);
            break;
        case UI_ACTION_BRIGHTNESS:
        case UI_ACTION_SOUND:
            break;
        case UI_ACTION_HISTORY:
            app_show_feature_info("RIDE HISTORY",
                                  "PHASE 6F · HISTORY LIST RESERVED");
            break;
        case UI_ACTION_ACHIEVEMENT:
            app_show_feature_info("ACHIEVEMENTS",
                                  "LIGHTWEIGHT PAGE RESERVED");
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
    ui_bottom_surface_init();
    ui_app_state_init();
    ui_state_adapter_init();
    ui_input_queue_init();
    ui_navigation_service_init();
    ui_page_status_registry_init();
    ui_event_bridge_init();
    ui_event_bus_init();
    ui_dock_init();
    ui_state_machine_init();
    ui_state_machine_set_changed_callback(app_ride_state_changed, NULL);
    ui_home_data_model_init();
    ui_ride_data_model_init();
    ui_router_init(UI_ROUTE_HOME);
    ui_router_set_changed_callback(app_route_changed, NULL);
    app_create_screens();
    app_register_routes();
    ui_dock_view_init();
    ui_event_bus_set_overflow_callback(app_event_overflow, NULL);
    ui_dock_controller_init();
    ui_ride_controller_init();
    (void)ui_event_bus_subscribe(UI_EVENT_MASK_ALL, app_event_received, NULL);
    (void)ui_command_service_subscribe(app_command_changed, NULL);
    (void)ui_app_state_subscribe(UI_APP_STATE_CHANGE_ALL,
                                 app_state_changed, NULL);
    (void)ui_ride_data_model_subscribe(app_ride_data_changed, NULL);
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
    if(input == UI_INPUT_PAGE_NEXT || input == UI_INPUT_PAGE_PREV) {
        if(s_powered_off || s_quick_visible) return;
        bool next = input == UI_INPUT_PAGE_NEXT;
        ui_route_id_t route = ui_router_current();
        if(app_ride_domain_active()) {
            if(next && route == UI_PAGE_DASHBOARD)
                app_show_slide(UI_PAGE_NAVIGATION, true);
            else if(!next && route == UI_PAGE_NAVIGATION)
                app_show_slide(UI_PAGE_DASHBOARD, false);
        }
        else {
            if(next && route == UI_PAGE_HOME)
                app_show_slide(UI_PAGE_NAVIGATION, true);
            else if(!next && route == UI_PAGE_NAVIGATION)
                app_show_slide(UI_PAGE_HOME, false);
        }
        return;
    }
    if(input == UI_INPUT_OPEN_POWER) {
        app_handle_key('o');
        return;
    }
    if(input == UI_INPUT_TOGGLE_CONTROL_CENTER) {
        app_handle_key('m');
        return;
    }
    if(input == UI_INPUT_POWER_TOGGLE) {
        app_handle_key('p');
        return;
    }
    if(input == UI_INPUT_OPEN_PROFILE) {
        if(!s_powered_off && !app_ride_domain_active()) app_show(UI_PAGE_HOME);
        return;
    }
    if(input == UI_INPUT_OPEN_SETTINGS) {
        if(!s_powered_off) app_show(UI_PAGE_SETTINGS);
        return;
    }
    if(!s_powered_off && !s_quick_visible && ui_router_current() == UI_PAGE_DASHBOARD &&
       ui_dynamic_card_manager_handle_input(input)) return;
    if(!s_powered_off && !s_quick_visible && ui_router_current() == UI_PAGE_HOME &&
       ui_home_page_handle_input(input)) return;
    if(!s_powered_off && !s_quick_visible && ui_router_current() == UI_PAGE_NAVIGATION &&
       ui_navigation_handle_input(input)) return;

    if(input == UI_INPUT_ENCODER_CW) app_handle_key(LV_KEY_DOWN);
    else if(input == UI_INPUT_ENCODER_CCW) app_handle_key(LV_KEY_UP);
    else if(input == UI_INPUT_ENCODER_PRESS) app_handle_key(LV_KEY_ENTER);
}

static void app_handle_key(uint32_t key) {
    if(s_powered_off) {
        if(key == 'p' || key == 'P' || key == LV_KEY_ENTER) {
            s_powered_off = false;
            (void)app_publish_event(UI_EVENT_BOOT_COMPLETE,
                                    UI_EVENT_PRIORITY_P2_RIDE_STATE);
            (void)ui_router_replace(UI_PAGE_HOME, UI_MOTION_PAGE_NONE);
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
    if(key == '[') { app_show(UI_PAGE_HOME); return; }
    if(key == ']') { app_show(UI_PAGE_SETTINGS); return; }
    if(key == LV_KEY_ESC || key == 8) { app_back(); return; }
    if(s_quick_visible) return;
    if(ui_router_current() == UI_PAGE_NAVIGATION) {
        if(key == LV_KEY_UP) { ui_navigation_handle_input(UI_INPUT_ENCODER_CCW); return; }
        if(key == LV_KEY_DOWN) { ui_navigation_handle_input(UI_INPUT_ENCODER_CW); return; }
        if(key == LV_KEY_ENTER) { ui_navigation_handle_input(UI_INPUT_ENCODER_PRESS); return; }
    }
    if(ui_router_current() == UI_PAGE_HOME) {
        if(key == LV_KEY_UP) {
            ui_home_page_handle_input(UI_INPUT_ENCODER_CCW);
            return;
        }
        if(key == LV_KEY_DOWN) {
            ui_home_page_handle_input(UI_INPUT_ENCODER_CW);
            return;
        }
        if(key == LV_KEY_ENTER) {
            ui_home_page_handle_input(UI_INPUT_ENCODER_PRESS);
            return;
        }
    }
    if(ui_router_current() == UI_PAGE_SETTINGS &&
       ui_settings_page_handle_key(key)) return;
    if(key == LV_KEY_LEFT) ui_app_handle_input(UI_INPUT_PAGE_PREV);
    else if(key == LV_KEY_RIGHT) ui_app_handle_input(UI_INPUT_PAGE_NEXT);
    else if(key == LV_KEY_ENTER) app_open_quick();
}

static uint8_t app_slider_level_from_y(int32_t screen_y) {
    return ui_control_center_level_from_y(screen_y);
}

static void app_update_active_slider(int32_t screen_y) {
    uint8_t value = app_slider_level_from_y(screen_y);
    if(s_active_slider == UI_CONTROL_CENTER_SLIDER_SOUND) {
        (void)app_submit_command(UI_COMMAND_SOUND_SET_LEVEL, value, 1500U);
    } else if(s_active_slider == UI_CONTROL_CENTER_SLIDER_BRIGHTNESS) {
        (void)app_submit_command(UI_COMMAND_BRIGHTNESS_SET_LEVEL,
                                 value, 1500U);
    }
}

void ui_app_handle_pointer(const ui_input_pointer_event_t *event) {
    if(event == NULL) return;
    if(s_quick_overlay == NULL || s_powered_off) return;

    lv_point_t point = { .x = event->x, .y = event->y };

    if(event->type == UI_INPUT_TOUCH_BEGIN) {
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

    if(event->type == UI_INPUT_TOUCH_MOVE) {
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
               (ui_router_current() == UI_PAGE_HOME ||
                ui_router_current() == UI_PAGE_DASHBOARD ||
                ui_router_current() == UI_PAGE_NAVIGATION)) {
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

    if(event->type == UI_INPUT_TOUCH_END ||
       event->type == UI_INPUT_TOUCH_CANCEL) {
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
            if(delta_x <= -72)
                ui_app_handle_input(UI_INPUT_PAGE_NEXT);
            else if(delta_x >= 72)
                ui_app_handle_input(UI_INPUT_PAGE_PREV);
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
