#include "ui_app.h"

#include "ui_colors.h"
#include "ui_app_state.h"
#include "ui_auto_pause.h"
#include "ui_background.h"
#include "ui_version.h"
#include "ui_bottom_surface.h"
#include "ui_control_feedback_ownership.h"
#include "ui_dock.h"
#include "ui_dock_controller.h"
#include "ui_event_bridge.h"
#include "ui_event_bus.h"
#include "ui_input_queue.h"
#include "ui_motion.h"
#include "ui_memory.h"
#include "ui_ride_controller.h"
#include "ui_router.h"
#include "ui_sizes.h"
#include "ui_sound.h"
#include "ui_state_machine.h"
#include "ui_system_control_shade.h"
#include "ui_theme.h"
#include "../../services/ui_command_service.h"
#include "../../services/ui_system_services.h"
#include "../../services/ui_state_adapter.h"
#include "../../services/ui_navigation_service.h"
#include "../../services/ui_ride_history_repository.h"
#include "../../services/ui_preferences.h"
#include "../../services/ui_device_info.h"
#include "../components/ui_status_bar.h"
#include "../components/ui_dynamic_card_manager.h"
#include "../components/dock/ui_dock_view.h"
#include "../design/ui_design_tokens.h"
#include "../screens/ui_dashboard.h"
#include "../pages/navigation/ui_navigation.h"
#include "../pages/ui_boot_page.h"
#include "../pages/ui_home_page.h"
#include "../pages/ui_history_page.h"
#include "../pages/ui_achievements_page.h"
#include "../pages/ui_ride_detail_page.h"
#include "../pages/ui_page_action.h"
#include "../pages/ui_page_common.h"
#include "../pages/ui_power_page.h"
#include "../pages/ui_profile_page.h"
#include "../pages/ui_devices_page.h"
#include "../pages/ui_saved_routes_page.h"
#include "../pages/ui_settings_page.h"
#include "../pages/ui_summary_page.h"
#include "../pages/ui_system_page.h"
#include "../models/ui_ride_data_model.h"
#include "../models/ui_ride_summary_model.h"
#include "../models/ui_home_data_model.h"
#include "../models/ui_ride_history_model.h"
#include "../models/ui_weather_model.h"
#include "../overlays/ui_control_center.h"
#include "../overlays/ui_zone_editor.h"
#include "../overlays/ui_dashboard_editor.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

typedef ui_route_id_t ui_page_t;
#define UI_PAGE_BOOT        UI_ROUTE_BOOT
#define UI_PAGE_HOME        UI_ROUTE_HOME
#define UI_PAGE_DASHBOARD   UI_ROUTE_RIDE_DASHBOARD
#define UI_PAGE_NAVIGATION  UI_ROUTE_NAVIGATION
#define UI_PAGE_SUMMARY     UI_ROUTE_RIDE_SUMMARY
#define UI_PAGE_HISTORY     UI_ROUTE_HISTORY
#define UI_PAGE_RIDE_DETAIL UI_ROUTE_RIDE_DETAIL
#define UI_PAGE_PROFILE     UI_ROUTE_PROFILE
#define UI_PAGE_ACHIEVEMENTS UI_ROUTE_ACHIEVEMENTS
#define UI_PAGE_SETTINGS    UI_ROUTE_SETTINGS
#define UI_PAGE_SYSTEM      UI_ROUTE_SYSTEM
#define UI_PAGE_DEVICES     UI_ROUTE_DEVICES
#define UI_PAGE_SAVED_ROUTES UI_ROUTE_SAVED_ROUTES
#define UI_PAGE_POWER       UI_ROUTE_POWER
#define UI_PAGE_COUNT       UI_ROUTE_COUNT
typedef enum { UI_ACTION_NONE, UI_ACTION_HOME, UI_ACTION_NAVIGATION, UI_ACTION_SUMMARY,
               UI_ACTION_PROFILE, UI_ACTION_SETTINGS, UI_ACTION_SYSTEM, UI_ACTION_QUICK,
               UI_ACTION_SAVE_RIDE, UI_ACTION_UPLOAD_RIDE, UI_ACTION_START_RIDE,
               UI_ACTION_PAUSE_RIDE, UI_ACTION_END_RIDE, UI_ACTION_WIFI, UI_ACTION_BT,
               UI_ACTION_GPS, UI_ACTION_POWER_SAVE, UI_ACTION_TOUCH_LOCK,
               UI_ACTION_BRIGHTNESS, UI_ACTION_SOUND, UI_ACTION_HISTORY,
               UI_ACTION_ACHIEVEMENT, UI_ACTION_DEVICES, UI_ACTION_POWER_OFF, UI_ACTION_POWER_RESTART,
               UI_ACTION_POWER_CANCEL } ui_action_t;
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *bottom_surface_slot;
    ui_status_bar_t status;
    bool show_dynamic_surface;
} ui_page_view_t;

static ui_page_view_t s_pages[UI_PAGE_COUNT];
static lv_timer_t *s_boot_timer;
typedef enum {
    APP_BOOT_FADE_IDLE,
    APP_BOOT_FADE_OUT,
    APP_BOOT_FADE_WAIT,
    APP_BOOT_FADE_IN
} app_boot_fade_state_t;
static app_boot_fade_state_t s_boot_fade_state;
static lv_obj_t *s_boot_fade_overlay;
static lv_timer_t *s_event_timer;
static bool s_powered_off;
static bool s_pointer_dragging, s_slider_dragging, s_slider_candidate;
static bool s_page_swiping, s_home_swiping;
static bool s_dynamic_card_gesture_owned;
static bool s_navigation_map_gesture_owned;
static bool s_bottom_surface_gesture_owned;
static bool s_page_scroll_gesture_owned;
static bool s_touch_sequence_blocked;
static ui_control_center_slider_t s_active_slider;
static lv_point_t s_pointer_start;
static int32_t s_overlay_start_y;
static int32_t s_panel_start_height;
static uint32_t s_pointer_start_tick;
static lv_obj_t *s_off_screen;
static lv_obj_t *s_blank_overlay;
static bool s_display_blank;
static ui_action_t s_pending_power_action;
static bool s_start_command_pending, s_system_command_pending;
static uint32_t s_last_event_tick;
static uint32_t s_last_interaction_tick;
static ui_preferences_t s_preferences;
static uint32_t s_seen_storage_commits;
static ui_ride_data_t s_latest_ride_data;
static lv_obj_t *s_ride_badge;
static uint32_t s_display_valid_flags;
static lv_obj_t *s_control_surface;
typedef enum {
    APP_SHADE_DRAG_NONE = 0,
    APP_SHADE_DRAG_OPEN_COMPACT,
    APP_SHADE_DRAG_CLOSE_COMPACT,
    APP_SHADE_DRAG_EXPAND,
    APP_SHADE_DRAG_CLOSE_EXPANDED
} app_shade_drag_t;
static app_shade_drag_t s_shade_drag;
typedef enum {
    APP_CONTROL_SURFACE_IDLE = 0,
    APP_CONTROL_SURFACE_DRAGGING,
    APP_CONTROL_SURFACE_SETTLING
} app_control_surface_interaction_t;
static app_control_surface_interaction_t s_control_interaction;
static ui_auto_pause_t s_auto_pause;
static ui_ride_pause_reason_t s_pending_pause_reason;
static ui_auto_pause_action_t s_pending_auto_pause_action;
static bool s_pause_command_pending;
typedef enum {
    APP_SETTINGS_CONTROL_NONE = 0,
    APP_SETTINGS_CONTROL_SOUND,
    APP_SETTINGS_CONTROL_BRIGHTNESS
} app_settings_control_t;
static app_settings_control_t s_settings_control;
#define APP_DOCK_ID_CONTROL       0x1008U
#define APP_DOCK_ID_POWER_CONFIRM 0x2101U
static void app_action(ui_action_t action);
static void app_page_action(ui_page_action_t action, void *user_data);
static void app_show(ui_page_t page);
static void app_show_slide(ui_page_t page, bool move_left);
static void app_back(void);
static void app_close_quick(void);
static void app_open_quick(void);
static void app_expand_full_control(void);
static void app_hide_quick_immediate(void);
static bool app_control_shade_visible(void);
static void app_update_quick_states(void);
static void app_update_slider_visuals(void);
static void app_control_center_action(ui_control_center_action_t action,
                                      int32_t value,
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
/* Detail scratch lives off the MCU task stack and is only used by LVGL. */
static UI_LARGE_RAM ui_ride_record_t s_detail_record;
static bool app_submit_command(ui_command_type_t type, int32_t value,
                               uint32_t timeout_ms);
static void app_publish_command_feedback(const ui_command_result_t *result);
static bool app_ride_domain_active(void);
static void app_show_feature_info(const char *title, const char *message);
static void app_auto_pause_tick(uint32_t elapsed_ms);
static bool app_request_pause_toggle(ui_ride_pause_reason_t reason);
static void app_show_settings_control(app_settings_control_t control);
static void app_refresh_settings_control(bool failed);
static void app_show_power_confirmation(bool restart);
static void app_refresh_history_models(void);
static bool app_prepare_ride_save(void);
static void app_dismiss_settings_control(void);
static void app_continue_power(void);
static bool app_publish_event(ui_event_type_t type, ui_event_priority_t priority);
static void app_boot_timer_cb(lv_timer_t *timer);
static void app_boot_fade_reset(void);
static void app_boot_fade_in(void);

static void app_apply_preferences(const ui_preferences_t *preferences)
{
    s_preferences = *preferences;
    ui_motion_set_reduced(preferences->reduced_motion);
    (void)ui_ride_summary_model_set_zones(preferences->heart_zone_bounds, preferences->power_zone_bounds);
    ui_home_data_t home;
    if(ui_home_data_model_get(&home)) {
        snprintf(home.user_name,sizeof(home.user_name),"%s",preferences->user_name);
        snprintf(home.tagline,sizeof(home.tagline),"%s",preferences->tagline);
        (void)ui_home_data_model_update(&home);
    }
    ui_settings_page_set_preferences(preferences->auto_pause, preferences->reduced_motion, preferences->idle_sleep_ms);
    (void)ui_dashboard_set_layout(preferences->dashboard_preset, preferences->dashboard_fields);
    ui_settings_page_set_dashboard_preset(preferences->dashboard_preset);
    ui_profile_page_set_preset(preferences->dashboard_preset);
}

static void app_set_display_blank(bool blank)
{
    if(!app_submit_command(UI_COMMAND_DISPLAY_SET_ENABLED, !blank, 1500U)) return;
    s_display_blank = blank;
    if(blank) {
        lv_obj_remove_flag(s_blank_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_blank_overlay);
    }
    else lv_obj_add_flag(s_blank_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void app_continue_power(void)
{
    if(s_pending_power_action == UI_ACTION_HOME || s_start_command_pending ||
       s_system_command_pending || ui_ride_history_repository_busy()) return;
    ui_ride_state_t state = ui_state_machine_state();
    if(state == UI_RIDE_STATE_STOPPING || state == UI_RIDE_STATE_SAVING) return;
    if(ui_ride_history_repository_has_pending()) {
        (void)app_publish_event(UI_EVENT_STORAGE_RETRY, UI_EVENT_PRIORITY_P0_CRITICAL);
        return;
    }
    if(ui_ride_summary_model_active()) {
        if(state != UI_RIDE_STATE_FINISHING)
            (void)app_publish_event(UI_EVENT_RIDE_FINISH_REQUESTED, UI_EVENT_PRIORITY_P2_RIDE_STATE);
        (void)app_publish_event(UI_EVENT_RIDE_FINISH_CONFIRMED, UI_EVENT_PRIORITY_P2_RIDE_STATE);
        return;
    }
    s_system_command_pending = app_submit_command(
        s_pending_power_action == UI_ACTION_POWER_RESTART ? UI_COMMAND_SYSTEM_RESTART :
        UI_COMMAND_SYSTEM_SHUTDOWN, 0, 3000U);
    if(!s_system_command_pending) s_pending_power_action = UI_ACTION_HOME;
}


static void app_update_ride_badge(void)
{
    if(s_ride_badge == NULL) return;
    ui_ride_state_t state = ui_state_machine_state();
    const char *text = NULL;
    if(state == UI_RIDE_STATE_STOPPING) text = "正在结束";
    else if(state == UI_RIDE_STATE_SAVING) text = "正在保存";
    else if(ui_ride_history_repository_has_pending()) text = "骑行待保存";
    else if(state == UI_RIDE_STATE_PAUSED) text = "骑行已暂停";
    else if(state == UI_RIDE_STATE_RIDING || state == UI_RIDE_STATE_FINISHING) text = "骑行记录中";
    if(text != NULL && !s_powered_off) {
        if(strcmp(lv_label_get_text(s_ride_badge), text) != 0) lv_label_set_text(s_ride_badge, text);
        lv_color_t color = UI_TOKEN_COLOR_ACCENT;
        if(state == UI_RIDE_STATE_PAUSED) color = lv_color_hex(0xFFB85A);
        if(state == UI_RIDE_STATE_STOPPING || state == UI_RIDE_STATE_SAVING)
            color = lv_color_hex(0x59C8E8);
        if(ui_ride_history_repository_has_pending()) color = lv_color_hex(0xFF6B70);
        if(ui_ride_history_repository_has_pending())
            lv_obj_add_flag(s_ride_badge, LV_OBJ_FLAG_CLICKABLE);
        else lv_obj_clear_flag(s_ride_badge, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(s_ride_badge, color, 0);
        lv_obj_clear_flag(s_ride_badge, LV_OBJ_FLAG_HIDDEN);
    }
    else lv_obj_add_flag(s_ride_badge, LV_OBJ_FLAG_HIDDEN);
}

static void app_ride_badge_clicked(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED || s_display_blank) return;
    if(ui_ride_history_repository_has_pending()) app_action(UI_ACTION_SUMMARY);
    else if(ui_state_machine_is_riding()) app_show(UI_PAGE_DASHBOARD);
}

static void app_restore_pending_summary(void)
{
    ui_ride_record_t record;
    if(!ui_ride_history_repository_get_pending(&record)) return;
    ui_ride_summary_t summary = { .saved = false, .version = 1,
        .started_at_utc = record.started_at_utc,
        .finished_at_utc = record.finished_at_utc };
#define RESTORE(member) summary.member = record.member
    RESTORE(distance_km); RESTORE(duration_seconds); RESTORE(moving_time_seconds);
    RESTORE(average_speed_kmh); RESTORE(maximum_speed_kmh); RESTORE(ascent_m);
    RESTORE(descent_m); RESTORE(maximum_elevation_m); RESTORE(calories_kcal);
    RESTORE(has_heart_rate); RESTORE(average_heart_rate); RESTORE(maximum_heart_rate); RESTORE(minimum_heart_rate);
    RESTORE(has_power); RESTORE(average_power); RESTORE(maximum_power);
    RESTORE(has_cadence); RESTORE(average_cadence); RESTORE(maximum_cadence);
    RESTORE(has_temperature); RESTORE(average_temperature_tenths_c); RESTORE(metric_flags); RESTORE(trace);
#undef RESTORE
    (void)ui_ride_summary_model_restore(&summary);
}
static void app_handoff_control_feedback_to_bottom(void);
static void app_reclaim_control_feedback_to_top(void);
static void app_surface_page_enter(ui_route_id_t route, lv_obj_t *screen,
                                   void *user_data);
static void app_surface_page_leave(ui_route_id_t route, lv_obj_t *screen,
                                   void *user_data);
static void app_profile_enter(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data);
static void app_profile_leave(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data);
static void app_devices_enter(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data);
static void app_devices_leave(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data);
static void app_system_enter(ui_route_id_t route, lv_obj_t *screen,
                             void *user_data);
static void app_system_leave(ui_route_id_t route, lv_obj_t *screen,
                             void *user_data);
static void app_achievements_enter(ui_route_id_t route, lv_obj_t *screen,
                                   void *user_data);
static void app_achievements_leave(ui_route_id_t route, lv_obj_t *screen,
                                   void *user_data);
static void app_ride_detail_enter(ui_route_id_t route, lv_obj_t *screen,
                                  void *user_data);
static void app_ride_detail_leave(ui_route_id_t route, lv_obj_t *screen,
                                  void *user_data);

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

static bool app_publish_event_value(ui_event_type_t type,
                                    ui_event_priority_t priority,
                                    int32_t value)
{
    ui_event_t event = {0};
    event.type = type;
    event.priority = priority;
    event.value = value;
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
           state == UI_RIDE_STATE_FINISHING || state == UI_RIDE_STATE_STOPPING ||
           state == UI_RIDE_STATE_SAVING;
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
    if(event->priority <= UI_EVENT_PRIORITY_P1_RIDING_SAFETY &&
       app_control_shade_visible()) {
        app_dismiss_settings_control();
        app_hide_quick_immediate();
    }
    switch(event->type) {
        case UI_EVENT_GPS_SEARCHING:
            if(!app_submit_command(UI_COMMAND_GNSS_ACQUIRE_FIX, 0, 3000U))
                (void)app_publish_event(UI_EVENT_GPS_FAILED,
                                        UI_EVENT_PRIORITY_P1_RIDING_SAFETY);
            break;
        case UI_EVENT_RIDE_GO_REQUESTED:
            if(s_start_command_pending || s_pending_power_action != UI_ACTION_HOME) break;
            s_start_command_pending = true;
            if(!app_submit_command(UI_COMMAND_RIDE_START,
                                   (int32_t)ui_ride_summary_model_reserve_session(), 2000U)) {
                s_start_command_pending = false;
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
                (void)app_publish_event(UI_EVENT_RIDE_FINISH_CANCELLED,
                                        UI_EVENT_PRIORITY_P0_CRITICAL);
            }
            break;
        case UI_EVENT_STORAGE_RETRY:
            if(ui_ride_history_repository_has_pending() &&
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
        case UI_EVENT_CONTROL_CHANGED:
            /* The first CONTROL_CHANGED event only presents the shared
             * Bottom Surface. Slider changes are republished by Dock using
             * APP_DOCK_ID_CONTROL and are the only events that reach the
             * command backend. */
            if(event->source_id == APP_DOCK_ID_CONTROL) {
                bool submitted = true;
                if(s_settings_control == APP_SETTINGS_CONTROL_SOUND)
                    submitted = app_submit_command(
                        UI_COMMAND_SOUND_SET_LEVEL, event->value, 1500U);
                else if(s_settings_control ==
                        APP_SETTINGS_CONTROL_BRIGHTNESS)
                    submitted = app_submit_command(
                        UI_COMMAND_BRIGHTNESS_SET_LEVEL,
                        event->value, 1500U);
                if(!submitted) app_refresh_settings_control(true);
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
    uint32_t now = lv_tick_get();
    uint32_t elapsed = now - s_last_event_tick;
    s_last_event_tick = now;
    (void)ui_ride_history_repository_process_page();
    ui_history_page_tick();
    ui_system_services_tick(elapsed);
    ui_command_service_process(elapsed);
    uint32_t commits = ui_ride_history_repository_commits();
    if(commits != s_seen_storage_commits) {
        s_seen_storage_commits = commits;
        ui_ride_summary_model_finish(true);
        ui_summary_page_set_saved(true);
        app_refresh_history_models();
        if(ui_state_machine_state() == UI_RIDE_STATE_UNSAVED)
            (void)app_publish_event(UI_EVENT_RIDE_SAVE_SUCCESS, UI_EVENT_PRIORITY_P2_RIDE_STATE);
        app_continue_power();
    }
    (void)ui_state_adapter_process();
    (void)ui_ride_data_model_process();
    (void)ui_weather_model_process(now);
    ui_preferences_t preferences;
    if(ui_preferences_process(now, &preferences)) app_apply_preferences(&preferences);
    ui_settings_page_set_storage_state(ui_preferences_storage_state());
    ui_device_info_t device;
    if(ui_device_info_process(&device)) {
        char storage[40], lvgl_version[24];
        snprintf(storage,sizeof(storage),device.storage_ready ? "%llu MiB FREE" : "UNAVAILABLE",
            (unsigned long long)(device.storage_free_bytes / (1024U*1024U)));
        snprintf(lvgl_version,sizeof(lvgl_version),"%u.%u.%u",LVGL_VERSION_MAJOR,LVGL_VERSION_MINOR,LVGL_VERSION_PATCH);
        ui_system_info_t info={.device_model=device.device_model,.ui_version=UI_SOFTWARE_VERSION,
            .lvgl_version=lvgl_version,.storage_status=storage,.hardware_target=device.hardware_target,
            .system_health=device.health,.firmware_version=device.firmware_version};
        ui_system_page_set_info(&info);
    }
    if(!s_display_blank && !s_powered_off && ui_router_current() != UI_PAGE_BOOT &&
       s_boot_fade_state == APP_BOOT_FADE_IDLE && s_preferences.idle_sleep_ms &&
       (uint32_t)(now - s_last_interaction_tick) >= s_preferences.idle_sleep_ms &&
       !s_pointer_dragging && !s_slider_dragging) app_set_display_blank(true);
    s_auto_pause.speed_valid = ui_ride_data_field_valid(
        &s_latest_ride_data, UI_RIDE_FIELD_SPEED, now);
    app_auto_pause_tick(elapsed);
    (void)ui_navigation_service_process();
    (void)ui_navigation_service_expire(now);
    (void)ui_input_queue_drain(UI_INPUT_QUEUE_MAX,
                               app_input_received, NULL);
    (void)ui_event_bridge_drain(UI_EVENT_BRIDGE_QUEUE_MAX);
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
    ui_dock_controller_tick(elapsed);
    uint32_t valid = 0;
    for(unsigned i = 0; i < UI_RIDE_FIELD_COUNT; ++i)
        if(ui_ride_data_field_valid(&s_latest_ride_data, (ui_ride_field_t)i, now))
            valid |= UI_RIDE_FIELD_BIT(i);
    if(valid != s_display_valid_flags) {
        s_display_valid_flags = valid;
        ui_dashboard_set_data(&s_latest_ride_data);
        ui_dynamic_card_manager_refresh_data(&s_latest_ride_data);
    }
    app_update_ride_badge();
}

#define QUICK_DRAG_THRESHOLD          5
#define CONTROL_OPEN_DISTANCE        24
#define CONTROL_EXPAND_DISTANCE      36
#define CONTROL_CLOSE_DISTANCE       48
#define CONTROL_FAST_VELOCITY       400

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
    s_latest_ride_data = *data;
    ui_auto_pause_set_speed(&s_auto_pause, data->speed);
    ui_dashboard_set_data(data);
}

static bool app_request_pause_toggle(ui_ride_pause_reason_t reason)
{
    if(s_pause_command_pending) return false;
    ui_ride_state_t state = ui_state_machine_state();
    ui_command_type_t command;
    if(state == UI_RIDE_STATE_RIDING)
        command = UI_COMMAND_RIDE_PAUSE;
    else if(state == UI_RIDE_STATE_PAUSED)
        command = UI_COMMAND_RIDE_RESUME;
    else return false;

    if(reason == UI_RIDE_PAUSE_REASON_MANUAL)
        ui_auto_pause_manual_override(&s_auto_pause);
    s_pending_pause_reason = reason;
    s_pending_auto_pause_action = reason == UI_RIDE_PAUSE_REASON_AUTO ?
        (command == UI_COMMAND_RIDE_PAUSE ? UI_AUTO_PAUSE_ACTION_PAUSE :
                                            UI_AUTO_PAUSE_ACTION_RESUME) :
        UI_AUTO_PAUSE_ACTION_NONE;
    s_pause_command_pending = app_submit_command(command, 0, 2000U);
    if(!s_pause_command_pending &&
       s_pending_auto_pause_action != UI_AUTO_PAUSE_ACTION_NONE) {
        ui_auto_pause_complete(&s_auto_pause,
                               s_pending_auto_pause_action, false);
    }
    return s_pause_command_pending;
}

static void app_auto_pause_tick(uint32_t elapsed_ms)
{
    if(!s_preferences.auto_pause) return;
    if(s_pause_command_pending) return;
    ui_auto_pause_action_t action = ui_auto_pause_tick(
        &s_auto_pause, ui_state_machine_state(), elapsed_ms);
    if(action == UI_AUTO_PAUSE_ACTION_PAUSE ||
       action == UI_AUTO_PAUSE_ACTION_RESUME) {
        if(!app_request_pause_toggle(UI_RIDE_PAUSE_REASON_AUTO))
            ui_auto_pause_complete(&s_auto_pause, action, false);
    }
}

static void app_format_history_date(int64_t timestamp,
                                    char *text, size_t capacity)
{
    if(timestamp <= 0) { snprintf(text,capacity,"DATE UNAVAILABLE"); return; }
    time_t value = (time_t)timestamp;
    struct tm local_time;
#if defined(_WIN32)
    if(localtime_s(&local_time, &value) != 0) {
#else
    if(localtime_r(&value, &local_time) == NULL) {
#endif
        snprintf(text, capacity, "DATE UNAVAILABLE");
        return;
    }
    snprintf(text, capacity, "%04d.%02d.%02d  %02d:%02d",
             local_time.tm_year + 1900, local_time.tm_mon + 1,
             local_time.tm_mday, local_time.tm_hour, local_time.tm_min);
}

static void app_refresh_history_models(void)
{
    ui_home_data_t home;
    if(!ui_home_data_model_get(&home)) return;
    ui_ride_history_totals_t totals;
    ui_ride_history_model_totals(&totals);
    home.total_distance_km =
        (uint32_t)(totals.total_distance_km + 0.5);
    uint64_t total_minutes = totals.total_duration_seconds / 60U;
    home.total_time_hours = (uint32_t)(total_minutes / 60U);
    home.total_time_minutes = (uint8_t)(total_minutes % 60U);
    home.total_ascent_m = totals.total_ascent_m > UINT32_MAX ? UINT32_MAX :
                          (uint32_t)totals.total_ascent_m;

    ui_ride_record_t latest;
    if(ui_ride_history_model_get_newest(0U, &latest)) {
        home.last_ride_available = true;
        home.last_distance_available = !(latest.metric_flags & UI_REPORT_METRICS_KNOWN) ||
            (latest.metric_flags & UI_REPORT_HAS_DISTANCE);
        home.last_ascent_available = !(latest.metric_flags & UI_REPORT_METRICS_KNOWN) ||
            (latest.metric_flags & UI_REPORT_HAS_ASCENT);
        app_format_history_date(latest.finished_at_utc,
                                home.last_ride_date,
                                sizeof(home.last_ride_date));
        home.last_distance_tenths_km =
            (uint32_t)(latest.distance_km * 10.0f + 0.5f);
        home.last_duration_minutes =
            (latest.duration_seconds + 59U) / 60U;
        home.last_ascent_m = latest.ascent_m;
        snprintf(home.last_ride_caption, sizeof(home.last_ride_caption),
                 "RIDE COMPLETE · SAVED");
    }
    else {
        home.last_ride_available = home.last_distance_available = home.last_ascent_available = false;
        snprintf(home.last_ride_date, sizeof(home.last_ride_date),
                 "NO RIDES YET");
        home.last_distance_tenths_km = 0U;
        home.last_duration_minutes = 0U;
        home.last_ascent_m = 0U;
        snprintf(home.last_ride_caption, sizeof(home.last_ride_caption),
                 "START YOUR FIRST RIDE");
    }
    (void)ui_home_data_model_update(&home);
    ui_history_page_refresh();
}

static bool app_prepare_ride_save(void)
{
    if(ui_ride_history_repository_has_pending()) return true;
    ui_ride_summary_t summary;
    if(!ui_ride_summary_model_get(&summary)) return false;
    if(summary.saved) return false;
    ui_ride_summary_model_finish(false);
    if(!ui_ride_summary_model_get(&summary)) return false;
    if(ui_ride_history_repository_stage(&summary) != UI_RIDE_HISTORY_OK) return false;
    ui_ride_record_t staged;
    if(ui_ride_history_repository_get_pending(&staged)) {
        summary.started_at_utc = staged.started_at_utc;
        summary.finished_at_utc = staged.finished_at_utc;
        (void)ui_ride_summary_model_restore(&summary);
    }
    return true;
}

static void app_state_changed(const ui_app_state_t *state,
                              ui_app_state_change_mask_t changed,
                              void *user_data)
{
    (void)user_data;
    if(state == NULL) return;
    if((changed & UI_APP_STATE_CHANGE_CONTROL) != 0U) {
        ui_preferences_t preferences;
        if(ui_preferences_get(&preferences) &&
           (preferences.sound != state->sound_level || preferences.brightness != state->brightness_level)) {
            preferences.sound = state->sound_level;
            preferences.brightness = state->brightness_level == 0 ? 1 : state->brightness_level;
            (void)ui_preferences_submit(&preferences);
        }
        ui_sound_set_volume(state->sound_level);
        ui_background_set_opacity(
            (lv_opa_t)(70 + (185 * state->brightness_level / 100)));
        if(s_settings_control != APP_SETTINGS_CONTROL_NONE)
            app_refresh_settings_control(false);
    }
    if((changed & (UI_APP_STATE_CHANGE_STATUS |
                   UI_APP_STATE_CHANGE_CONNECTIVITY |
                   UI_APP_STATE_CHANGE_CONTROL |
                   UI_APP_STATE_CHANGE_RIDE)) != 0U) {
        app_update_quick_states();
    }
    if((changed & (UI_APP_STATE_CHANGE_CONNECTIVITY |
                   UI_APP_STATE_CHANGE_CONTROL |
                   UI_APP_STATE_CHANGE_STATUS)) != 0U)
        ui_settings_page_set_state(state);
}

static void app_route_changed(ui_route_id_t previous,
                              ui_route_id_t current, void *user_data)
{
    (void)user_data;
    if(s_boot_fade_state != APP_BOOT_FADE_IDLE &&
       !(previous == UI_PAGE_BOOT && current == UI_PAGE_HOME &&
         s_boot_fade_state == APP_BOOT_FADE_WAIT)) app_boot_fade_reset();
    if(previous == UI_PAGE_BOOT) {
        ui_boot_page_set_active(false);
        if(s_boot_timer != NULL) lv_timer_delete(s_boot_timer);
        s_boot_timer = NULL;
        s_last_interaction_tick = lv_tick_get();
    }
    if(current == UI_PAGE_BOOT) {
        app_boot_fade_reset();
        ui_boot_page_set_active(true);
        s_last_interaction_tick = lv_tick_get();
        s_boot_timer = lv_timer_create(app_boot_timer_cb, 20, NULL);
        lv_timer_ready(s_boot_timer);
    }
    else if(previous == UI_PAGE_BOOT && current == UI_PAGE_HOME &&
            s_boot_fade_state == APP_BOOT_FADE_WAIT) app_boot_fade_in();
    ui_app_state_set_route((uint8_t)current);
}

static void app_ride_state_changed(ui_ride_state_t previous,
                                   ui_ride_state_t current,
                                   ui_ride_event_t event, void *user_data)
{
    (void)user_data;
    ui_app_state_set_ride_state(current);
    app_update_ride_badge();
    ui_motion_set_riding(current == UI_RIDE_STATE_RIDING);
    ui_ride_summary_model_set_paused(current == UI_RIDE_STATE_PAUSED);
    if(event == UI_RIDE_EVENT_GO && current == UI_RIDE_STATE_RIDING) {
        ui_ride_data_t data;
        if(ui_ride_data_model_get(&data, NULL))
            ui_ride_summary_model_begin(&data);
    }
    else if(current == UI_RIDE_STATE_SUMMARY &&
            previous == UI_RIDE_STATE_SAVING) {
        ui_ride_summary_model_finish(true);
        ui_summary_page_set_saved(true);
    }
    if(current != UI_RIDE_STATE_RIDING &&
       current != UI_RIDE_STATE_PAUSED &&
       current != UI_RIDE_STATE_FINISHING) {
        ui_auto_pause_manual_override(&s_auto_pause);
        s_pause_command_pending = false;
        s_pending_auto_pause_action = UI_AUTO_PAUSE_ACTION_NONE;
    }
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
        case UI_COMMAND_TOUCH_LOCK_SET_ENABLED: return "TOUCH LOCK";
        case UI_COMMAND_SOUND_SET_LEVEL: return "VOLUME";
        case UI_COMMAND_BRIGHTNESS_SET_LEVEL: return "BRIGHTNESS";
        case UI_COMMAND_DISPLAY_SET_ENABLED: return "DISPLAY";
        case UI_COMMAND_SYSTEM_SHUTDOWN: return "POWER OFF";
        case UI_COMMAND_SYSTEM_RESTART: return "RESTART";
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
        case UI_COMMAND_DISPLAY_SET_ENABLED:
        case UI_COMMAND_SYSTEM_SHUTDOWN:
        case UI_COMMAND_SYSTEM_RESTART:
            return result->status != UI_COMMAND_STATUS_PENDING && result->status != UI_COMMAND_STATUS_SUCCESS;
        case UI_COMMAND_WIFI_SET_ENABLED:
        case UI_COMMAND_BLUETOOTH_SET_ENABLED:
        case UI_COMMAND_GNSS_SET_ENABLED:
        case UI_COMMAND_POWER_SAVE_SET_ENABLED:
        case UI_COMMAND_TOUCH_LOCK_SET_ENABLED:
            /* Control Center owns inline feedback. Settings uses the same
             * command path but receives feedback through Bottom Surface. */
            return ui_router_current() == UI_PAGE_SETTINGS;
        case UI_COMMAND_SOUND_SET_LEVEL:
        case UI_COMMAND_BRIGHTNESS_SET_LEVEL:
            /* The CONTROL card itself remains visible while adjusting. */
            return false;
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
            if(result->type == UI_COMMAND_RIDE_PAUSE ||
               result->type == UI_COMMAND_RIDE_RESUME)
                return result->status != UI_COMMAND_STATUS_SUCCESS;
            return result->status != UI_COMMAND_STATUS_PENDING &&
                   result->status != UI_COMMAND_STATUS_SUCCESS;
        default:
            return true;
    }
}

static bool app_settings_item_for_command(ui_command_type_t type,
                                          ui_settings_item_t *item)
{
    if(item == NULL) return false;
    switch(type) {
        case UI_COMMAND_WIFI_SET_ENABLED:
            *item = UI_SETTINGS_ITEM_WIFI; return true;
        case UI_COMMAND_BLUETOOTH_SET_ENABLED:
            *item = UI_SETTINGS_ITEM_BLUETOOTH; return true;
        case UI_COMMAND_GNSS_SET_ENABLED:
            *item = UI_SETTINGS_ITEM_GPS; return true;
        case UI_COMMAND_POWER_SAVE_SET_ENABLED:
            *item = UI_SETTINGS_ITEM_POWER_SAVE; return true;
        case UI_COMMAND_TOUCH_LOCK_SET_ENABLED:
            *item = UI_SETTINGS_ITEM_TOUCH_LOCK; return true;
        case UI_COMMAND_SOUND_SET_LEVEL:
            *item = UI_SETTINGS_ITEM_SOUND; return true;
        case UI_COMMAND_BRIGHTNESS_SET_LEVEL:
            *item = UI_SETTINGS_ITEM_BRIGHTNESS; return true;
        default: return false;
    }
}

static void app_publish_command_feedback(const ui_command_result_t *result)
{
    if(result == NULL) return;
    ui_event_t event = {0};
    event.priority = UI_EVENT_PRIORITY_P3_USER_ACTION;
    event.source_id = result->id;
    event.dedupe_key = 0xC001U;
    /* Let Dock map ride pause/resume command feedback back to the same
     * DOCK_ID_RIDE owner used by the resulting ride-state event. */
    event.value = (int32_t)result->type;
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
    ui_settings_item_t settings_item;
    if(app_settings_item_for_command(result->type, &settings_item)) {
        ui_settings_feedback_t feedback = UI_SETTINGS_FEEDBACK_IDLE;
        if(result->status == UI_COMMAND_STATUS_PENDING)
            feedback = UI_SETTINGS_FEEDBACK_PENDING;
        else if(result->status != UI_COMMAND_STATUS_SUCCESS)
            feedback = UI_SETTINGS_FEEDBACK_FAILED;
        ui_settings_page_set_feedback(settings_item, feedback);
    }
    ui_control_center_action_t control_action =
        UI_CONTROL_CENTER_ACTION_COUNT;
    switch(result->type) {
        case UI_COMMAND_WIFI_SET_ENABLED:
            control_action = UI_CONTROL_CENTER_ACTION_WIFI; break;
        case UI_COMMAND_BLUETOOTH_SET_ENABLED:
            control_action = UI_CONTROL_CENTER_ACTION_BLUETOOTH; break;
        case UI_COMMAND_SOUND_SET_LEVEL:
            control_action = UI_CONTROL_CENTER_ACTION_SOUND; break;
        case UI_COMMAND_BRIGHTNESS_SET_LEVEL:
            control_action = UI_CONTROL_CENTER_ACTION_BRIGHTNESS; break;
        case UI_COMMAND_GNSS_SET_ENABLED:
            control_action = UI_CONTROL_CENTER_ACTION_GPS; break;
        case UI_COMMAND_POWER_SAVE_SET_ENABLED:
            control_action = UI_CONTROL_CENTER_ACTION_POWER_SAVE; break;
        case UI_COMMAND_TOUCH_LOCK_SET_ENABLED:
            control_action = UI_CONTROL_CENTER_ACTION_TOUCH_LOCK; break;
        default: break;
    }
    bool numeric_control_owner =
        (result->type == UI_COMMAND_SOUND_SET_LEVEL &&
         s_settings_control == APP_SETTINGS_CONTROL_SOUND) ||
        (result->type == UI_COMMAND_BRIGHTNESS_SET_LEVEL &&
         s_settings_control == APP_SETTINGS_CONTROL_BRIGHTNESS);
    bool managed_feedback =
        ui_control_feedback_ownership_manages(result->type);
    ui_control_feedback_owner_t owner = UI_CONTROL_FEEDBACK_OWNER_NONE;
    if(managed_feedback) {
        if(result->status == UI_COMMAND_STATUS_PENDING) {
            owner = ui_control_feedback_ownership_begin(
                result, !numeric_control_owner &&
                        app_control_shade_visible());
        }
        else {
            owner = ui_control_feedback_ownership_owner_for(result);
            if(owner == UI_CONTROL_FEEDBACK_OWNER_NONE) {
                owner = app_control_shade_visible() ?
                        UI_CONTROL_FEEDBACK_OWNER_TOP :
                        UI_CONTROL_FEEDBACK_OWNER_BOTTOM;
            }
        }
    }
    if(control_action < UI_CONTROL_CENTER_ACTION_COUNT) {
        ui_control_center_feedback_t feedback =
            result->status == UI_COMMAND_STATUS_PENDING ?
            UI_CONTROL_CENTER_FEEDBACK_PENDING :
            (result->status == UI_COMMAND_STATUS_SUCCESS ?
             UI_CONTROL_CENTER_FEEDBACK_IDLE :
             UI_CONTROL_CENTER_FEEDBACK_FAILED);
        if(managed_feedback && owner != UI_CONTROL_FEEDBACK_OWNER_TOP)
            feedback = UI_CONTROL_CENTER_FEEDBACK_IDLE;
        if(numeric_control_owner)
            feedback = UI_CONTROL_CENTER_FEEDBACK_IDLE;
        ui_control_center_set_feedback(control_action, feedback);
    }
    if(managed_feedback) {
        if(owner == UI_CONTROL_FEEDBACK_OWNER_BOTTOM &&
           !numeric_control_owner)
            app_publish_command_feedback(result);
        if(result->status != UI_COMMAND_STATUS_PENDING)
            ui_control_feedback_ownership_complete(result);
    }
    else if(app_command_feedback_needed(result)) {
        app_publish_command_feedback(result);
    }
    if(result->status != UI_COMMAND_STATUS_PENDING &&
       result->status != UI_COMMAND_STATUS_SUCCESS &&
       ((result->type == UI_COMMAND_SOUND_SET_LEVEL &&
         s_settings_control == APP_SETTINGS_CONTROL_SOUND) ||
        (result->type == UI_COMMAND_BRIGHTNESS_SET_LEVEL &&
         s_settings_control == APP_SETTINGS_CONTROL_BRIGHTNESS)))
        app_refresh_settings_control(true);
    if(result->status == UI_COMMAND_STATUS_PENDING) return;
    if(result->type == UI_COMMAND_RIDE_START) s_start_command_pending = false;
    if(result->type == UI_COMMAND_DISPLAY_SET_ENABLED && result->status != UI_COMMAND_STATUS_SUCCESS &&
       (result->value == 0) == s_display_blank) {
        s_display_blank = !s_display_blank;
        if(s_display_blank) {
            lv_obj_remove_flag(s_blank_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_blank_overlay);
        }
        else lv_obj_add_flag(s_blank_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if(result->type == UI_COMMAND_SYSTEM_SHUTDOWN || result->type == UI_COMMAND_SYSTEM_RESTART) {
        s_system_command_pending = false;
        s_pending_power_action = UI_ACTION_HOME;
        if(result->status == UI_COMMAND_STATUS_SUCCESS) {
            app_hide_quick_immediate();
            ui_dock_clear();
            ui_control_feedback_ownership_init();
            ui_event_bus_clear();
            ui_router_clear_history();
            (void)ui_state_machine_dispatch(UI_RIDE_EVENT_RESET);
            if(result->type == UI_COMMAND_SYSTEM_SHUTDOWN) {
                s_powered_off = true;
                (void)ui_router_replace(UI_ROUTE_POWER_OFF, UI_MOTION_PAGE_NONE);
            }
            else {
                (void)ui_router_replace(UI_PAGE_BOOT, UI_MOTION_PAGE_NONE);
            }
        }
        return;
    }

    if(result->type == UI_COMMAND_RIDE_PAUSE ||
       result->type == UI_COMMAND_RIDE_RESUME) {
        bool success = result->status == UI_COMMAND_STATUS_SUCCESS;
        if(s_pending_auto_pause_action != UI_AUTO_PAUSE_ACTION_NONE) {
            ui_auto_pause_complete(&s_auto_pause,
                                   s_pending_auto_pause_action, success);
        }
        s_pause_command_pending = false;
    }

    if(result->status == UI_COMMAND_STATUS_SUCCESS) {
        switch(result->type) {
            case UI_COMMAND_GNSS_ACQUIRE_FIX:
                (void)app_publish_event(UI_EVENT_GPS_READY,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
                break;
            case UI_COMMAND_RIDE_START:
                (void)ui_ride_data_model_process();
                (void)ui_ride_summary_model_set_zones(s_preferences.heart_zone_bounds, s_preferences.power_zone_bounds);
                app_hide_quick_immediate();
                (void)app_publish_event(UI_EVENT_RIDE_GO,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
                app_continue_power();
                break;
            case UI_COMMAND_RIDE_PAUSE:
                app_hide_quick_immediate();
                (void)app_publish_event_value(
                    UI_EVENT_RIDE_PAUSE_REQUESTED,
                    UI_EVENT_PRIORITY_P2_RIDE_STATE,
                    (int32_t)s_pending_pause_reason);
                s_pending_auto_pause_action = UI_AUTO_PAUSE_ACTION_NONE;
                break;
            case UI_COMMAND_RIDE_RESUME:
                app_hide_quick_immediate();
                (void)app_publish_event_value(
                    UI_EVENT_RIDE_RESUME_REQUESTED,
                    UI_EVENT_PRIORITY_P2_RIDE_STATE,
                    (int32_t)s_pending_pause_reason);
                s_pending_auto_pause_action = UI_AUTO_PAUSE_ACTION_NONE;
                break;
            case UI_COMMAND_RIDE_FINISH:
                (void)app_publish_event(UI_EVENT_RIDE_SAVE_STARTED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
                if(!app_prepare_ride_save() ||
                   !app_submit_command(UI_COMMAND_RIDE_SAVE, 0, 5000U))
                    (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED,
                                            UI_EVENT_PRIORITY_P0_CRITICAL);
                break;
            case UI_COMMAND_RIDE_SAVE:
                if(ui_ride_history_repository_has_pending()) {
                    (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED, UI_EVENT_PRIORITY_P0_CRITICAL);
                    break;
                }
                ui_ride_summary_model_finish(true);
                ui_summary_page_set_saved(true);
                app_refresh_history_models();
                if(ui_state_machine_state() == UI_RIDE_STATE_SAVING ||
                   ui_state_machine_state() == UI_RIDE_STATE_UNSAVED) {
                    (void)app_publish_event(UI_EVENT_RIDE_SAVE_SUCCESS,
                                            UI_EVENT_PRIORITY_P2_RIDE_STATE);
                }
                else {
                    ui_sound_play(UI_SFX_SUCCESS);
                }
                app_continue_power();
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
    if(result->type == UI_COMMAND_RIDE_START) app_continue_power();
    if(result->type == UI_COMMAND_GNSS_ACQUIRE_FIX) {
        (void)app_publish_event(UI_EVENT_GPS_FAILED,
                                UI_EVENT_PRIORITY_P1_RIDING_SAFETY);
    }
    else if(result->type == UI_COMMAND_RIDE_SAVE) {
        (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED,
                                UI_EVENT_PRIORITY_P0_CRITICAL);
    }
    else if(result->type == UI_COMMAND_RIDE_FINISH) {
        s_pending_power_action = UI_ACTION_HOME;
        (void)app_publish_event(UI_EVENT_RIDE_FINISH_CANCELLED,
                                UI_EVENT_PRIORITY_P2_RIDE_STATE);
    }
    if(result->type == UI_COMMAND_RIDE_PAUSE ||
       result->type == UI_COMMAND_RIDE_RESUME)
        s_pending_auto_pause_action = UI_AUTO_PAUSE_ACTION_NONE;
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
    app_refresh_history_models();
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

static void app_summary_enter(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data)
{
    (void)screen;
    (void)user_data;
    ui_page_view_t *page = &s_pages[route];
    if(!ui_bottom_surface_mount(page->bottom_surface_slot, false))
        LV_LOG_ERROR("Summary Bottom Surface mount failed");
    ui_ride_summary_t summary;
    if(ui_ride_summary_model_get(&summary))
        ui_summary_page_set_data(&summary);
    ui_summary_page_set_active(true);
}

static void app_summary_leave(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data)
{
    (void)route;
    (void)screen;
    (void)user_data;
    ui_summary_page_set_active(false);
    ui_bottom_surface_unmount();
}

static void app_history_enter(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data)
{
    (void)screen;
    (void)user_data;
    ui_page_view_t *page = &s_pages[route];
    if(!ui_bottom_surface_mount(page->bottom_surface_slot, false))
        LV_LOG_ERROR("History Bottom Surface mount failed");
    ui_history_page_set_active(true);
}

static void app_history_leave(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data)
{
    (void)route;
    (void)screen;
    (void)user_data;
    ui_history_page_set_active(false);
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

static void app_profile_enter(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data)
{
    app_surface_page_enter(route, screen, user_data);
    ui_profile_page_set_activity_time(ui_ride_history_repository_now_utc());
    ui_profile_page_set_active(true);
}

static void app_profile_leave(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data)
{
    ui_profile_page_set_active(false);
    app_surface_page_leave(route, screen, user_data);
}

static void app_devices_enter(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data)
{
    (void)route;
    (void)screen;
    (void)user_data;
    /* The device page intentionally owns the full 480x854 surface.  A
     * bottom dock would hide the final pairing row and make the page look
     * like it had an empty, unowned region. */
    ui_devices_page_set_active(true);
}

static void app_devices_leave(ui_route_id_t route, lv_obj_t *screen,
                              void *user_data)
{
    (void)route;
    (void)screen;
    (void)user_data;
    ui_devices_page_set_active(false);
    ui_bottom_surface_unmount();
}

static void app_system_enter(ui_route_id_t route, lv_obj_t *screen,
                             void *user_data)
{
    (void)route;
    (void)screen;
    (void)user_data;
    /* System diagnostics need the complete viewport so the scroll list and
     * its scrollbar remain visible down to the last row. */
}

static void app_system_leave(ui_route_id_t route, lv_obj_t *screen,
                             void *user_data)
{
    (void)route;
    (void)screen;
    (void)user_data;
    ui_bottom_surface_unmount();
}

static void app_achievements_enter(ui_route_id_t route, lv_obj_t *screen,
                                   void *user_data)
{
    app_surface_page_enter(route, screen, user_data);
    ui_achievements_page_set_active(true);
}

static void app_achievements_leave(ui_route_id_t route, lv_obj_t *screen,
                                   void *user_data)
{
    ui_achievements_page_set_active(false);
    app_surface_page_leave(route, screen, user_data);
}

static void app_ride_detail_enter(ui_route_id_t route, lv_obj_t *screen,
                                  void *user_data)
{
    app_surface_page_enter(route, screen, user_data);
    ui_ride_detail_page_set_active(true);
}

static void app_ride_detail_leave(ui_route_id_t route, lv_obj_t *screen,
                                  void *user_data)
{
    ui_ride_detail_page_set_active(false);
    app_surface_page_leave(route, screen, user_data);
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
static bool app_control_shade_visible(void)
{
    return ui_system_control_shade_state() !=
           UI_SYSTEM_CONTROL_SHADE_CLOSED;
}

static bool app_point_inside(lv_obj_t *object, int32_t x, int32_t y)
{
    if(object == NULL || lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN))
        return false;
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    return x >= area.x1 && x <= area.x2 && y >= area.y1 && y <= area.y2;
}

static bool app_safety_interaction_active(void)
{
    ui_bottom_surface_state_t state;
    return ui_bottom_surface_get_state(&state) &&
           state.mode == UI_BOTTOM_SURFACE_INTERACTION_MODE &&
           state.priority <= UI_BOTTOM_SURFACE_PRIORITY_P1_RIDING_SAFETY;
}

static void app_dismiss_settings_control(void)
{
    if(s_settings_control == APP_SETTINGS_CONTROL_NONE) return;
    if(ui_dock_view_is_visible())
        (void)ui_dock_view_handle_input(UI_INPUT_BACK);
    ui_control_feedback_state_t feedback;
    if(ui_control_feedback_ownership_get(&feedback) &&
       feedback.owner == UI_CONTROL_FEEDBACK_OWNER_BOTTOM &&
       (feedback.command_type == UI_COMMAND_SOUND_SET_LEVEL ||
        feedback.command_type == UI_COMMAND_BRIGHTNESS_SET_LEVEL)) {
        const ui_command_result_t pending = {
            .id = feedback.command_id,
            .type = feedback.command_type,
            .status = UI_COMMAND_STATUS_PENDING,
            .error_code = 0
        };
        app_publish_command_feedback(&pending);
    }
    s_settings_control = APP_SETTINGS_CONTROL_NONE;
}

static ui_control_center_action_t app_control_action_for_command(
    ui_command_type_t type)
{
    switch(type) {
        case UI_COMMAND_WIFI_SET_ENABLED:
            return UI_CONTROL_CENTER_ACTION_WIFI;
        case UI_COMMAND_BLUETOOTH_SET_ENABLED:
            return UI_CONTROL_CENTER_ACTION_BLUETOOTH;
        case UI_COMMAND_GNSS_SET_ENABLED:
            return UI_CONTROL_CENTER_ACTION_GPS;
        case UI_COMMAND_POWER_SAVE_SET_ENABLED:
            return UI_CONTROL_CENTER_ACTION_POWER_SAVE;
        case UI_COMMAND_TOUCH_LOCK_SET_ENABLED:
            return UI_CONTROL_CENTER_ACTION_TOUCH_LOCK;
        case UI_COMMAND_SOUND_SET_LEVEL:
            return UI_CONTROL_CENTER_ACTION_SOUND;
        case UI_COMMAND_BRIGHTNESS_SET_LEVEL:
            return UI_CONTROL_CENTER_ACTION_BRIGHTNESS;
        default:
            return UI_CONTROL_CENTER_ACTION_COUNT;
    }
}

static void app_set_top_control_feedback(ui_command_type_t type,
                                         ui_control_center_feedback_t feedback)
{
    ui_control_center_action_t action =
        app_control_action_for_command(type);
    if(action >= UI_CONTROL_CENTER_ACTION_COUNT) return;
    ui_control_center_set_feedback(action, feedback);
}

static void app_handoff_control_feedback_to_bottom(void)
{
    ui_control_feedback_state_t state;
    if(!ui_control_feedback_ownership_handoff_to_bottom(&state)) return;
    app_set_top_control_feedback(state.command_type,
                                 UI_CONTROL_CENTER_FEEDBACK_IDLE);
    const ui_command_result_t pending = {
        .id = state.command_id,
        .type = state.command_type,
        .status = UI_COMMAND_STATUS_PENDING,
        .error_code = 0
    };
    app_publish_command_feedback(&pending);
}

static void app_reclaim_control_feedback_to_top(void)
{
    ui_control_feedback_state_t state;
    if(!ui_control_feedback_ownership_reclaim_to_top(&state)) return;
    app_set_top_control_feedback(state.command_type,
                                 UI_CONTROL_CENTER_FEEDBACK_PENDING);
    (void)ui_dock_controller_hide_command_feedback();
}

static void app_set_panel_height(void *target, int32_t value)
{
    (void)target;
    ui_control_center_set_panel_height(value);
}

static void app_control_settle_done(lv_anim_t *animation)
{
    (void)animation;
    s_control_interaction = APP_CONTROL_SURFACE_IDLE;
}

static void app_control_compact_done(lv_anim_t *animation)
{
    (void)animation;
    ui_control_center_set_mode(UI_CONTROL_CENTER_MODE_COMPACT);
    s_control_interaction = APP_CONTROL_SURFACE_IDLE;
}

static void app_control_expanded_done(lv_anim_t *animation)
{
    (void)animation;
    ui_control_center_set_mode(UI_CONTROL_CENTER_MODE_EXPANDED);
    s_control_interaction = APP_CONTROL_SURFACE_IDLE;
}

static void app_control_close_done(lv_anim_t *animation)
{
    (void)animation;
    if(s_control_surface != NULL)
        lv_obj_add_flag(s_control_surface, LV_OBJ_FLAG_HIDDEN);
    ui_motion_cancel(s_control_surface, app_set_panel_height);
    ui_control_center_set_mode(UI_CONTROL_CENTER_MODE_COMPACT);
    s_control_interaction = APP_CONTROL_SURFACE_IDLE;
}

static void app_animate_panel_height(int32_t target)
{
    if(s_control_surface == NULL) return;
    ui_motion_spring(s_control_surface, app_set_panel_height,
                    ui_control_center_panel_height(), target, true,
                    target == UI_CONTROL_CENTER_PANEL_HEIGHT ?
                    app_control_expanded_done : app_control_compact_done,
                    NULL);
}

static void app_open_quick(void) {
    if(s_control_surface == NULL || s_powered_off ||
       app_safety_interaction_active() ||
       ui_system_control_shade_state() != UI_SYSTEM_CONTROL_SHADE_CLOSED)
        return;
    (void)ui_system_control_shade_open_compact();
    app_reclaim_control_feedback_to_top();
    ui_motion_cancel_y(s_control_surface);
    ui_motion_cancel(s_control_surface, app_set_panel_height);
    ui_control_center_set_mode(UI_CONTROL_CENTER_MODE_COMPACT);
    lv_obj_set_y(s_control_surface, -UI_CONTROL_CENTER_COMPACT_HEIGHT);
    lv_obj_clear_flag(s_control_surface, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_control_surface);
    ui_control_center_prepare_show();
    app_update_quick_states();
    s_control_interaction = APP_CONTROL_SURFACE_SETTLING;
    ui_motion_move_y(s_control_surface, 0, UI_MOTION_DURATION_DOCK,
                     lv_anim_path_ease_out, app_control_settle_done, NULL);
}

static void app_expand_full_control(void) {
    if(s_control_surface == NULL || s_powered_off ||
       ui_system_control_shade_state() !=
            UI_SYSTEM_CONTROL_SHADE_COMPACT) return;
    if(!ui_system_control_shade_expand()) return;
    app_reclaim_control_feedback_to_top();
    ui_motion_cancel_y(s_control_surface);
    lv_obj_set_y(s_control_surface, 0);
    app_update_quick_states();
    s_control_interaction = APP_CONTROL_SURFACE_SETTLING;
    app_animate_panel_height(UI_CONTROL_CENTER_PANEL_HEIGHT);
}

static void app_close_quick(void) {
    ui_system_control_shade_state_t state =
        ui_system_control_shade_state();
    if(state == UI_SYSTEM_CONTROL_SHADE_CLOSED) return;
    if(ui_control_center_exit_adjustment(true)) return;
    app_handoff_control_feedback_to_bottom();
    (void)ui_system_control_shade_close();
    ui_motion_cancel_y(s_control_surface);
    s_control_interaction = APP_CONTROL_SURFACE_SETTLING;
    ui_motion_spring(s_control_surface, app_set_panel_height,
                    ui_control_center_panel_height(),
                    UI_CONTROL_CENTER_COMPACT_HEIGHT, false, NULL, NULL);
    ui_motion_move_y(s_control_surface, -UI_CONTROL_CENTER_COMPACT_HEIGHT,
                     UI_TOKEN_MOTION_CONTROL_EXPAND_MS,
                     lv_anim_path_ease_in, app_control_close_done, NULL);
}

static void app_hide_quick_immediate(void) {
    if(s_control_surface == NULL) return;
    app_handoff_control_feedback_to_bottom();
    ui_motion_cancel_y(s_control_surface);
    ui_motion_cancel(s_control_surface, app_set_panel_height);
    lv_obj_set_y(s_control_surface, -UI_CONTROL_CENTER_COMPACT_HEIGHT);
    ui_control_center_set_mode(UI_CONTROL_CENTER_MODE_COMPACT);
    lv_obj_add_flag(s_control_surface, LV_OBJ_FLAG_HIDDEN);
    (void)ui_system_control_shade_close();
    s_pointer_dragging = false;
    s_shade_drag = APP_SHADE_DRAG_NONE;
    s_control_interaction = APP_CONTROL_SURFACE_IDLE;
    s_bottom_surface_gesture_owned = false;
    s_slider_dragging = false;
    s_page_swiping = false;
    s_home_swiping = false;
    s_active_slider = UI_CONTROL_CENTER_SLIDER_NONE;
    s_touch_sequence_blocked = false;
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
        (void)ui_router_replace(UI_PAGE_BOOT, UI_MOTION_PAGE_NONE);
        return;
    }
    if(app_control_shade_visible()) {
        app_close_quick();
        return;
    }
    if(ui_state_machine_state() == UI_RIDE_STATE_FINISHING) {
        (void)app_publish_event(UI_EVENT_RIDE_FINISH_CANCELLED,
                                UI_EVENT_PRIORITY_P2_RIDE_STATE);
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
        case UI_PAGE_ACTION_BACK: ui_app_handle_back(); break;
        case UI_PAGE_ACTION_START_RIDE:
            app_action(UI_ACTION_START_RIDE);
            break;
        case UI_PAGE_ACTION_START_NAVIGATION:
            app_action(UI_ACTION_NAVIGATION);
            break;
        case UI_PAGE_ACTION_SAVE_RIDE: app_action(UI_ACTION_SAVE_RIDE); break;
        case UI_PAGE_ACTION_UPLOAD_RIDE: app_action(UI_ACTION_UPLOAD_RIDE); break;
        case UI_PAGE_ACTION_HISTORY: app_action(UI_ACTION_HISTORY); break;
        case UI_PAGE_ACTION_SAVED_ROUTES: app_show(UI_PAGE_SAVED_ROUTES); break;
        case UI_PAGE_ACTION_RECENT_RIDE_DETAIL: {
            if(ui_ride_history_model_get_newest(0U, &s_detail_record)) {
                ui_ride_detail_page_set_record(&s_detail_record);
                app_show(UI_PAGE_RIDE_DETAIL);
            } else app_show_feature_info("最近一次骑行", "暂无可读取的骑行详情");
            break;
        }
        case UI_PAGE_ACTION_RIDE_DETAIL: {
            if(ui_history_page_selected_record(&s_detail_record)) {
                ui_ride_detail_page_set_record(&s_detail_record);
                app_show(UI_PAGE_RIDE_DETAIL);
            }
            break;
        }
        case UI_PAGE_ACTION_PROFILE: app_action(UI_ACTION_PROFILE); break;
        case UI_PAGE_ACTION_ACHIEVEMENT: app_action(UI_ACTION_ACHIEVEMENT); break;
        case UI_PAGE_ACTION_SETTINGS: app_action(UI_ACTION_SETTINGS); break;
        case UI_PAGE_ACTION_SYSTEM: app_action(UI_ACTION_SYSTEM); break;
        case UI_PAGE_ACTION_DEVICES: app_action(UI_ACTION_DEVICES); break;
        case UI_PAGE_ACTION_PROFILE_REFRESH:
            app_refresh_history_models();
            ui_profile_page_set_activity_time(ui_ride_history_repository_now_utc());
            ui_profile_page_refresh();
            break;
        case UI_PAGE_ACTION_DEVICE_SCAN: {
            ui_app_state_t state;
            bool enabled = ui_app_state_get(&state) && state.bluetooth_enabled;
            /* Full-height devices has no mounted dock. Put unavailable
             * feedback inside the page so taps never appear to do nothing. */
            ui_devices_page_set_feedback(enabled ?
                "暂无法扫描，蓝牙服务待接入" : "请先在系统设置中开启蓝牙");
            break;
        }
        case UI_PAGE_ACTION_POWER_MENU: app_show(UI_PAGE_POWER); break;
        case UI_PAGE_ACTION_WIFI_TOGGLE: app_action(UI_ACTION_WIFI); break;
        case UI_PAGE_ACTION_BLUETOOTH_TOGGLE: app_action(UI_ACTION_BT); break;
        case UI_PAGE_ACTION_GPS_TOGGLE: app_action(UI_ACTION_GPS); break;
        case UI_PAGE_ACTION_POWER_SAVE_TOGGLE:
            app_action(UI_ACTION_POWER_SAVE); break;
        case UI_PAGE_ACTION_TOUCH_LOCK_TOGGLE:
            app_action(UI_ACTION_TOUCH_LOCK); break;
        case UI_PAGE_ACTION_SOUND_CONTROL:
            app_show_settings_control(APP_SETTINGS_CONTROL_SOUND); break;
        case UI_PAGE_ACTION_BRIGHTNESS_CONTROL:
            app_show_settings_control(APP_SETTINGS_CONTROL_BRIGHTNESS); break;
        case UI_PAGE_ACTION_POWER_OFF:
            app_show_power_confirmation(false); break;
        case UI_PAGE_ACTION_POWER_RESTART:
            app_show_power_confirmation(true); break;
        case UI_PAGE_ACTION_POWER_CANCEL: app_action(UI_ACTION_POWER_CANCEL); break;
        case UI_PAGE_ACTION_ZONE_EDITOR: app_hide_quick_immediate(); ui_zone_editor_open(); break;
        case UI_PAGE_ACTION_DASHBOARD_LAYOUT: app_hide_quick_immediate(); ui_dashboard_editor_open(); break;
        case UI_PAGE_ACTION_AUTO_PAUSE_TOGGLE:
        case UI_PAGE_ACTION_REDUCED_MOTION_TOGGLE:
        case UI_PAGE_ACTION_IDLE_SLEEP_CYCLE: {
            ui_preferences_t preferences;
            if(!ui_preferences_get(&preferences)) break;
            if(action == UI_PAGE_ACTION_AUTO_PAUSE_TOGGLE) {
                preferences.auto_pause = !preferences.auto_pause;
                ui_auto_pause_manual_override(&s_auto_pause);
            }
            else if(action == UI_PAGE_ACTION_REDUCED_MOTION_TOGGLE)
                preferences.reduced_motion = !preferences.reduced_motion;
            else preferences.idle_sleep_ms = preferences.idle_sleep_ms == 0 ? 30000 :
                preferences.idle_sleep_ms == 30000 ? 60000 : preferences.idle_sleep_ms == 60000 ? 120000 : 0;
            (void)ui_preferences_submit(&preferences);
            break;
        }
        default: break;
    }
}
static void app_control_center_action(ui_control_center_action_t action,
                                      int32_t value,
                                      void *user_data) {
    (void)user_data;
    ui_sound_play(UI_SFX_PRESS);
    switch(action) {
        case UI_CONTROL_CENTER_ACTION_WIFI: app_action(UI_ACTION_WIFI); break;
        case UI_CONTROL_CENTER_ACTION_BLUETOOTH: app_action(UI_ACTION_BT); break;
        case UI_CONTROL_CENTER_ACTION_SOUND:
            if(value >= 0) {
                if(!app_submit_command(UI_COMMAND_SOUND_SET_LEVEL,
                                       value, 1500U)) {
                    app_update_slider_visuals();
                    ui_control_center_set_feedback(
                        action, UI_CONTROL_CENTER_FEEDBACK_FAILED);
                }
            }
            else {
                app_expand_full_control();
                ui_control_center_focus_action(
                    UI_CONTROL_CENTER_ACTION_SOUND);
            }
            break;
        case UI_CONTROL_CENTER_ACTION_BRIGHTNESS:
            if(value >= 0) {
                if(!app_submit_command(UI_COMMAND_BRIGHTNESS_SET_LEVEL,
                                       value, 1500U)) {
                    app_update_slider_visuals();
                    ui_control_center_set_feedback(
                        action, UI_CONTROL_CENTER_FEEDBACK_FAILED);
                }
            }
            else {
                app_expand_full_control();
                ui_control_center_focus_action(
                    UI_CONTROL_CENTER_ACTION_BRIGHTNESS);
            }
            break;
        case UI_CONTROL_CENTER_ACTION_GPS: app_action(UI_ACTION_GPS); break;
        case UI_CONTROL_CENTER_ACTION_POWER_SAVE:
            app_action(UI_ACTION_POWER_SAVE);
            break;
        case UI_CONTROL_CENTER_ACTION_TOUCH_LOCK:
            app_action(UI_ACTION_TOUCH_LOCK);
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
    s_pages[UI_PAGE_HISTORY].screen =
        ui_history_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_HISTORY].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_HISTORY].screen);
    s_pages[UI_PAGE_RIDE_DETAIL].screen =
        ui_ride_detail_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_RIDE_DETAIL].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_RIDE_DETAIL].screen);
    s_pages[UI_PAGE_PROFILE].screen = ui_profile_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_PROFILE].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_PROFILE].screen);
    s_pages[UI_PAGE_ACHIEVEMENTS].screen = ui_achievements_page_create();
    s_pages[UI_PAGE_ACHIEVEMENTS].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_ACHIEVEMENTS].screen);
    s_pages[UI_PAGE_SETTINGS].screen = ui_settings_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_SETTINGS].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_SETTINGS].screen);
    s_pages[UI_PAGE_SYSTEM].screen = ui_system_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_DEVICES].screen = ui_devices_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_SAVED_ROUTES].screen =
        ui_saved_routes_page_create(app_page_action, NULL);
    s_pages[UI_PAGE_SAVED_ROUTES].bottom_surface_slot =
        ui_page_bottom_surface_slot_create(s_pages[UI_PAGE_SAVED_ROUTES].screen);
    ui_control_center_create(app_control_center_action, NULL);
    s_control_surface = ui_control_center_root();
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
        .on_enter = app_summary_enter,
        .on_leave = app_summary_leave
    };
    const ui_router_page_t history = {
        .screen = s_pages[UI_PAGE_HISTORY].screen,
        .on_enter = app_history_enter,
        .on_leave = app_history_leave
    };
    const ui_router_page_t ride_detail = {
        .screen = s_pages[UI_PAGE_RIDE_DETAIL].screen,
        .on_enter = app_ride_detail_enter,
        .on_leave = app_ride_detail_leave
    };
    const ui_router_page_t profile = {
        .screen = s_pages[UI_PAGE_PROFILE].screen,
        .on_enter = app_profile_enter,
        .on_leave = app_profile_leave
    };
    const ui_router_page_t achievements = {
        .screen = s_pages[UI_PAGE_ACHIEVEMENTS].screen,
        .on_enter = app_achievements_enter,
        .on_leave = app_achievements_leave
    };
    const ui_router_page_t settings = {
        .screen = s_pages[UI_PAGE_SETTINGS].screen,
        .on_enter = app_surface_page_enter,
        .on_leave = app_surface_page_leave
    };
    const ui_router_page_t system = {
        .screen = s_pages[UI_PAGE_SYSTEM].screen,
        .on_enter = app_system_enter,
        .on_leave = app_system_leave
    };
    const ui_router_page_t devices = {
        .screen = s_pages[UI_PAGE_DEVICES].screen,
        .on_enter = app_devices_enter,
        .on_leave = app_devices_leave
    };
    const ui_router_page_t saved_routes = {
        .screen = s_pages[UI_PAGE_SAVED_ROUTES].screen,
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
    (void)ui_router_register_page(UI_ROUTE_HISTORY, &history);
    (void)ui_router_register_page(UI_ROUTE_RIDE_DETAIL, &ride_detail);
    (void)ui_router_register_page(UI_ROUTE_PROFILE, &profile);
    (void)ui_router_register_page(UI_ROUTE_ACHIEVEMENTS, &achievements);
    (void)ui_router_register_page(UI_ROUTE_SETTINGS, &settings);
    (void)ui_router_register_page(UI_ROUTE_SYSTEM, &system);
    (void)ui_router_register_page(UI_ROUTE_DEVICES, &devices);
    (void)ui_router_register_page(UI_ROUTE_SAVED_ROUTES, &saved_routes);
    (void)ui_router_register_page(UI_ROUTE_POWER, &power);
    (void)ui_router_register(UI_ROUTE_POWER_OFF, s_off_screen);
}
static void app_boot_fade_opacity(void *target, int32_t opacity)
{
    /* Blend one black rectangle instead of allocating a screen-sized
     * offscreen layer for every fading component. */
    lv_obj_set_style_bg_opa((lv_obj_t *)target, (lv_opa_t)opacity, 0);
}

static void app_boot_fade_reset(void)
{
    ui_home_page_cancel_enter();
    if(s_boot_fade_overlay != NULL) {
        ui_motion_cancel(s_boot_fade_overlay, app_boot_fade_opacity);
        lv_obj_add_flag(s_boot_fade_overlay, LV_OBJ_FLAG_HIDDEN);
        app_boot_fade_opacity(s_boot_fade_overlay, LV_OPA_TRANSP);
    }
    s_boot_fade_state = APP_BOOT_FADE_IDLE;
    s_last_interaction_tick = lv_tick_get();
}

static void app_boot_fade_out_done(lv_anim_t *animation)
{
    (void)animation;
    if(s_boot_fade_state == APP_BOOT_FADE_OUT)
        s_boot_fade_state = APP_BOOT_FADE_WAIT;
}

static void app_boot_fade_in_done(lv_anim_t *animation)
{
    (void)animation;
    if(s_boot_fade_state == APP_BOOT_FADE_IN) app_boot_fade_reset();
}

static void app_boot_fade_in(void)
{
    s_boot_fade_state = APP_BOOT_FADE_IN;
    lv_obj_move_foreground(s_boot_fade_overlay);
    /* Prepare every group while the screen is black, then retain the
     * transparent overlay as a touch shield until the last group settles. */
    ui_home_page_animate_enter(s_preferences.reduced_motion,
                               app_boot_fade_in_done, NULL);
    app_boot_fade_opacity(s_boot_fade_overlay, LV_OPA_TRANSP);
}

static void app_boot_timer_cb(lv_timer_t *timer) {
    (void)timer;
    if(ui_router_current() != UI_PAGE_BOOT || !ui_boot_page_is_complete()) return;
    if(s_boot_fade_state == APP_BOOT_FADE_IDLE && !s_preferences.reduced_motion) {
        s_boot_fade_state = APP_BOOT_FADE_OUT;
        lv_obj_remove_flag(s_boot_fade_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_boot_fade_overlay);
        ui_motion_value(s_boot_fade_overlay, app_boot_fade_opacity,
                        LV_OPA_TRANSP, LV_OPA_COVER, 280,
                        lv_anim_path_ease_in_out, app_boot_fade_out_done, NULL);
        return;
    }
    if(s_boot_fade_state == APP_BOOT_FADE_OUT) return;
    /* Initialization is completed before entering this route. Retry a full
     * event queue instead of leaving the UI and ride state out of sync. */
    if(!app_publish_event(UI_EVENT_BOOT_COMPLETE,
                          UI_EVENT_PRIORITY_P2_RIDE_STATE)) return;
    /* Route leave pauses the animation and deletes the completion timer. */
    (void)ui_router_replace(UI_PAGE_HOME, UI_MOTION_PAGE_NONE);
}
static void app_update_quick_states(void) {
    if(s_control_surface == NULL) return;
    ui_app_state_t app_state;
    if(!ui_app_state_get(&app_state)) return;
    const ui_control_center_state_t center_state = {
        .wifi_enabled = app_state.wifi_enabled,
        .wifi_connected = app_state.wifi_connected,
        .bluetooth_enabled = app_state.bluetooth_enabled,
        .bluetooth_connected = app_state.bluetooth_connected,
        .gps = app_state.gps_enabled,
        .power_save = app_state.power_save_enabled,
        .touch_locked = app_state.touch_locked,
        .battery_charging = app_state.battery_charging,
        .satellites = app_state.satellites,
        .sound_level = app_state.sound_level,
        .brightness_level = app_state.brightness_level,
        .battery_valid = app_state.battery_valid,
        .battery_percent = app_state.battery_percent
    };
    ui_control_center_state_t named_state = center_state;
    snprintf(named_state.wifi_name, sizeof(named_state.wifi_name), "%s",
             app_state.wifi_name);
    snprintf(named_state.bluetooth_name,
             sizeof(named_state.bluetooth_name), "%s",
             app_state.bluetooth_name);
    ui_control_center_set_state(&named_state);
}
static void app_update_slider_visuals(void) {
    ui_app_state_t state;
    if(!ui_app_state_get(&state)) return;
    ui_sound_set_volume(state.sound_level);
    ui_background_set_opacity(
        (lv_opa_t)(70 + (185 * state.brightness_level / 100)));
    app_update_quick_states();
}

static void app_show_settings_control(app_settings_control_t control)
{
    ui_app_state_t state;
    if(control == APP_SETTINGS_CONTROL_NONE || !ui_app_state_get(&state))
        return;
    s_settings_control = control;
    ui_event_t event = {0};
    event.type = UI_EVENT_CONTROL_CHANGED;
    event.priority = UI_EVENT_PRIORITY_P3_USER_ACTION;
    event.source_id = control == APP_SETTINGS_CONTROL_SOUND ?
                      0x5301U : 0x5302U;
    event.dedupe_key = APP_DOCK_ID_CONTROL;
    event.minimum = 0;
    event.maximum = 100;
    event.value = control == APP_SETTINGS_CONTROL_SOUND ?
                  state.sound_level : state.brightness_level;
    event.timeout_ms = 0U;
    snprintf(event.title, sizeof(event.title), "%s",
             control == APP_SETTINGS_CONTROL_SOUND ? "声音" : "亮度");
    snprintf(event.message, sizeof(event.message), "%s",
             "拖动滑条或旋转编码器进行调节");
    (void)ui_event_bus_publish(&event);
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
}

static void app_refresh_settings_control(bool failed)
{
    ui_app_state_t state;
    if(s_settings_control == APP_SETTINGS_CONTROL_NONE ||
       !ui_app_state_get(&state)) return;
    ui_event_t event = {0};
    event.type = UI_EVENT_CONTROL_CHANGED;
    event.priority = UI_EVENT_PRIORITY_P3_USER_ACTION;
    event.source_id = s_settings_control == APP_SETTINGS_CONTROL_SOUND ?
                      0x5311U : 0x5312U;
    event.dedupe_key = APP_DOCK_ID_CONTROL;
    event.minimum = 0;
    event.maximum = 100;
    event.value = s_settings_control == APP_SETTINGS_CONTROL_SOUND ?
                  state.sound_level : state.brightness_level;
    snprintf(event.title, sizeof(event.title), "%s",
             s_settings_control == APP_SETTINGS_CONTROL_SOUND ?
             "声音" : "亮度");
    snprintf(event.message, sizeof(event.message), "%s",
             failed ? "设置失败，已恢复" :
                      "拖动滑条或旋转编码器进行调节");
    (void)ui_event_bus_publish(&event);
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
}

static void app_power_confirm_action(uint32_t card_id,
                                     ui_dock_interaction_t interaction,
                                     int32_t value, void *context)
{
    (void)value;
    bool restart = (bool)(uintptr_t)context;
    if(interaction == UI_DOCK_INTERACTION_PRIMARY) {
        (void)ui_dock_hide(card_id);
        app_action(restart ? UI_ACTION_POWER_RESTART : UI_ACTION_POWER_OFF);
    }
    else if(interaction == UI_DOCK_INTERACTION_SECONDARY ||
            interaction == UI_DOCK_INTERACTION_DISMISS) {
        (void)ui_dock_hide(card_id);
    }
}

static void app_show_power_confirmation(bool restart)
{
    ui_dock_card_t card = {0};
    card.id = APP_DOCK_ID_POWER_CONFIRM;
    card.type = UI_DOCK_TYPE_CONFIRM;
    card.priority = UI_DOCK_PRIORITY_P3_USER_ACTION;
    card.dismissible = true;
    snprintf(card.title, sizeof(card.title), "%s",
             restart ? "重新启动" : "关闭设备");
    snprintf(card.message, sizeof(card.message), "%s",
             restart ? "是否重新启动系统" : "是否关闭设备");
    snprintf(card.primary_action, sizeof(card.primary_action), "YES");
    snprintf(card.secondary_action, sizeof(card.secondary_action), "NO");
    card.action_cb = app_power_confirm_action;
    card.context = (void *)(uintptr_t)restart;
    if(ui_dock_is_visible()) (void)ui_dock_replace(&card);
    else (void)ui_dock_show(&card);
}

static void app_show_feature_info(const char *title, const char *message)
{
    ui_dock_card_t card = {0};
    card.id = 0x2102U;
    card.type = UI_DOCK_TYPE_INFO;
    card.priority = UI_DOCK_PRIORITY_P3_USER_ACTION;
    card.dismissible = true;
    snprintf(card.title, sizeof(card.title), "%s", title != NULL ? title : "提示");
    snprintf(card.message, sizeof(card.message), "%s", message != NULL ? message : "");
    if(ui_dock_is_visible()) (void)ui_dock_replace(&card);
    else (void)ui_dock_show(&card);
}

static void app_action(ui_action_t action) {
    ui_app_state_t state;
    if(!ui_app_state_get(&state)) return;
    switch(action) {
        case UI_ACTION_HOME:
            ui_router_clear_history();
            if(ui_state_machine_state() == UI_RIDE_STATE_SUMMARY ||
               ui_state_machine_state() == UI_RIDE_STATE_UNSAVED) {
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
        case UI_ACTION_DEVICES: app_show(UI_PAGE_DEVICES); break;
        case UI_ACTION_QUICK: app_open_quick(); break;
        case UI_ACTION_SAVE_RIDE:
            if(ui_ride_history_repository_has_pending()) {
                (void)app_publish_event(UI_EVENT_STORAGE_RETRY, UI_EVENT_PRIORITY_P0_CRITICAL);
                break;
            }
            if(!app_prepare_ride_save() ||
               !app_submit_command(UI_COMMAND_RIDE_SAVE, 0, 5000U)) {
                (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED,
                                        UI_EVENT_PRIORITY_P0_CRITICAL);
            }
            break;
        case UI_ACTION_UPLOAD_RIDE:
            (void)app_submit_command(UI_COMMAND_RIDE_UPLOAD, 0, 5000U);
            break;
        case UI_ACTION_START_RIDE:
            if(s_pending_power_action != UI_ACTION_HOME) break;
            if(ui_ride_history_repository_has_pending()) {
                (void)app_publish_event(UI_EVENT_RIDE_SAVE_FAILED,
                                        UI_EVENT_PRIORITY_P0_CRITICAL);
                break;
            }
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
            (void)app_request_pause_toggle(UI_RIDE_PAUSE_REASON_MANUAL);
            break;
        case UI_ACTION_END_RIDE:
            if(ui_state_machine_is_riding()) {
                app_hide_quick_immediate();
                (void)app_publish_event(UI_EVENT_RIDE_FINISH_REQUESTED,
                                        UI_EVENT_PRIORITY_P2_RIDE_STATE);
            }
            break;
        case UI_ACTION_WIFI:
            if(!app_submit_command(UI_COMMAND_WIFI_SET_ENABLED,
                                   !state.wifi_enabled, 2000U)) {
                ui_control_center_set_feedback(
                    UI_CONTROL_CENTER_ACTION_WIFI,
                    UI_CONTROL_CENTER_FEEDBACK_FAILED);
            }
            break;
        case UI_ACTION_BT:
            if(!app_submit_command(UI_COMMAND_BLUETOOTH_SET_ENABLED,
                                   !state.bluetooth_enabled, 2000U)) {
                ui_control_center_set_feedback(
                    UI_CONTROL_CENTER_ACTION_BLUETOOTH,
                    UI_CONTROL_CENTER_FEEDBACK_FAILED);
            }
            break;
        case UI_ACTION_GPS:
            if(!app_submit_command(UI_COMMAND_GNSS_SET_ENABLED,
                                   !state.gps_enabled, 2000U))
                ui_control_center_set_feedback(
                    UI_CONTROL_CENTER_ACTION_GPS,
                    UI_CONTROL_CENTER_FEEDBACK_FAILED);
            break;
        case UI_ACTION_POWER_SAVE:
            if(!app_submit_command(UI_COMMAND_POWER_SAVE_SET_ENABLED,
                                   !state.power_save_enabled, 2000U))
                ui_control_center_set_feedback(
                    UI_CONTROL_CENTER_ACTION_POWER_SAVE,
                    UI_CONTROL_CENTER_FEEDBACK_FAILED);
            break;
        case UI_ACTION_TOUCH_LOCK:
            if(!app_submit_command(UI_COMMAND_TOUCH_LOCK_SET_ENABLED,
                                   !state.touch_locked, 2000U))
                ui_control_center_set_feedback(
                    UI_CONTROL_CENTER_ACTION_TOUCH_LOCK,
                    UI_CONTROL_CENTER_FEEDBACK_FAILED);
            break;
        case UI_ACTION_BRIGHTNESS:
        case UI_ACTION_SOUND:
            break;
        case UI_ACTION_HISTORY: app_show(UI_PAGE_HISTORY); break;
        case UI_ACTION_ACHIEVEMENT: app_show(UI_PAGE_ACHIEVEMENTS); break;
        case UI_ACTION_POWER_OFF:
        case UI_ACTION_POWER_RESTART:
            if(s_pending_power_action != UI_ACTION_HOME) break;
            s_pending_power_action = action;
            app_continue_power();
            break;
        case UI_ACTION_POWER_CANCEL: app_back(); break;
        default: break;
    }
    app_update_quick_states();
}
void ui_app_init(void) {
    s_pending_power_action = UI_ACTION_HOME;
    s_start_command_pending = s_system_command_pending = false;
    s_display_blank = false;
    s_last_event_tick = lv_tick_get();
    s_last_interaction_tick = s_last_event_tick;
    ui_motion_init();
    ui_background_init();
    ui_sound_init();
    ui_system_control_shade_init();
    ui_control_feedback_ownership_init();
    ui_bottom_surface_init();
    ui_app_state_init();
    ui_state_adapter_init();
    ui_input_queue_init();
    ui_navigation_service_init();
    ui_page_status_registry_init();
    ui_event_bridge_init();
    ui_event_bus_init();
    ui_dock_init();
    ui_auto_pause_init(&s_auto_pause);
    s_pending_pause_reason = UI_RIDE_PAUSE_REASON_MANUAL;
    s_pending_auto_pause_action = UI_AUTO_PAUSE_ACTION_NONE;
    s_pause_command_pending = false;
    ui_state_machine_init();
    ui_state_machine_set_changed_callback(app_ride_state_changed, NULL);
    ui_home_data_model_init();
    ui_preferences_init();
    ui_weather_model_init();
    ui_ride_history_repository_init();
    ui_ride_history_result_t history_result =
        ui_ride_history_repository_load();
    s_seen_storage_commits = ui_ride_history_repository_commits();
    if(history_result != UI_RIDE_HISTORY_OK)
        LV_LOG_WARN("Ride history load failed result=%d",
                    (int)history_result);
    ui_ride_data_model_init();
    (void)ui_ride_data_model_get(&s_latest_ride_data, NULL);
    s_display_valid_flags = UINT32_MAX;
    ui_ride_summary_model_init();
    app_restore_pending_summary();
    ui_router_init(UI_ROUTE_HOME);
    ui_router_set_changed_callback(app_route_changed, NULL);
    app_create_screens();
    (void)ui_preferences_get(&s_preferences);
    app_apply_preferences(&s_preferences);
    (void)app_submit_command(UI_COMMAND_SOUND_SET_LEVEL, s_preferences.sound, 1500U);
    (void)app_submit_command(UI_COMMAND_BRIGHTNESS_SET_LEVEL, s_preferences.brightness, 1500U);
    s_blank_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_blank_overlay);
    lv_obj_set_size(s_blank_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(s_blank_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_blank_overlay, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_blank_overlay, LV_OBJ_FLAG_HIDDEN);
    s_ride_badge = lv_label_create(s_pages[UI_PAGE_DASHBOARD].screen);
    lv_label_set_text(s_ride_badge, "");
    lv_obj_set_pos(s_ride_badge, 152, 78);
    lv_obj_set_size(s_ride_badge, 176, 32);
    lv_label_set_long_mode(s_ride_badge, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_ride_badge, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(s_ride_badge, 3, 0);
    lv_obj_set_ext_click_area(s_ride_badge, 8);
    lv_obj_set_style_text_font(s_ride_badge, UI_FONT_PAGE_TEXT, 0);
    lv_obj_set_style_text_color(s_ride_badge, lv_color_black(), 0);
    lv_obj_set_style_bg_color(s_ride_badge, UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(s_ride_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_ride_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(s_ride_badge, 8, 0);
    lv_obj_add_flag(s_ride_badge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ride_badge, app_ride_badge_clicked, LV_EVENT_CLICKED, NULL);
    app_refresh_history_models();
    app_register_routes();
    ui_dock_view_init();
    s_boot_fade_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_boot_fade_overlay);
    lv_obj_set_pos(s_boot_fade_overlay, 0, 0);
    lv_obj_set_size(s_boot_fade_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(s_boot_fade_overlay, lv_color_black(), 0);
    lv_obj_remove_flag(s_boot_fade_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_boot_fade_overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    app_boot_fade_reset();
    ui_event_bus_set_overflow_callback(app_event_overflow, NULL);
    (void)ui_event_bus_subscribe(UI_EVENT_MASK_ALL,
                                 app_event_received, NULL);
    ui_dock_controller_init();
    ui_ride_controller_init();
    (void)ui_command_service_subscribe(app_command_changed, NULL);
    (void)ui_app_state_subscribe(UI_APP_STATE_CHANGE_ALL,
                                 app_state_changed, NULL);
    (void)ui_ride_data_model_subscribe(app_ride_data_changed, NULL);
    app_update_slider_visuals();
    (void)ui_router_replace(UI_PAGE_BOOT, UI_MOTION_PAGE_NONE);
    s_event_timer = lv_timer_create(app_event_timer_cb, 20, NULL);
}
void ui_app_handle_back(void) {
    s_last_interaction_tick = lv_tick_get();
    if(ui_router_current() == UI_PAGE_BOOT || s_boot_fade_state != APP_BOOT_FADE_IDLE) return;
    if(ui_dashboard_editor_handle_input(UI_INPUT_BACK)) return;
    if(ui_zone_editor_visible()) { ui_zone_editor_close(); return; }
    if(ui_router_current() == UI_PAGE_ACHIEVEMENTS &&
       ui_achievements_page_detail_visible()) {
        ui_achievements_page_close_detail();
        return;
    }
    if(s_display_blank) {
        app_set_display_blank(false);
        return;
    }
    if(ui_state_machine_state() == UI_RIDE_STATE_STOPPING ||
       ui_state_machine_state() == UI_RIDE_STATE_SAVING) return;
    if(ui_state_machine_state() == UI_RIDE_STATE_UNSAVED) {
        app_show(UI_PAGE_SUMMARY);
        return;
    }
    /* A visible finish confirmation owns BACK before any hidden page/card
     * focus state. One click must always cancel the confirmation and restore
     * the previous Bottom Interaction Surface content. */
    if(ui_state_machine_state() == UI_RIDE_STATE_FINISHING) {
        app_back();
        return;
    }
    if(ui_state_machine_state() == UI_RIDE_STATE_SUMMARY) {
        app_action(UI_ACTION_HOME);
        return;
    }
    if(app_control_shade_visible() &&
       s_settings_control != APP_SETTINGS_CONTROL_NONE &&
       ui_dock_view_is_visible()) {
        app_dismiss_settings_control();
        return;
    }
    if(!app_control_shade_visible() && ui_dock_view_is_visible() &&
       s_settings_control != APP_SETTINGS_CONTROL_NONE) {
        app_dismiss_settings_control();
        return;
    }
    if(!app_control_shade_visible() && ui_dock_view_is_visible() &&
       ui_dock_view_handle_input(UI_INPUT_BACK)) {
        return;
    }
    if(!app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_DASHBOARD &&
       ui_dynamic_card_manager_back()) return;
    app_back();
}

void ui_app_handle_input(ui_input_event_t input) {
    s_last_interaction_tick = lv_tick_get();
    if(ui_router_current() == UI_PAGE_BOOT || s_boot_fade_state != APP_BOOT_FADE_IDLE) return;
    if(s_display_blank) {
        app_set_display_blank(false);
        return;
    }
    if(ui_dashboard_editor_handle_input(input)) return;
    if(ui_zone_editor_handle_input(input)) return;
    if(input == UI_INPUT_BACK_DOUBLE) {
        if(ui_state_machine_state() == UI_RIDE_STATE_RIDING ||
           ui_state_machine_state() == UI_RIDE_STATE_PAUSED)
            app_action(UI_ACTION_PAUSE_RIDE);
        return;
    }
    if(input == UI_INPUT_BACK_LONG) {
        if(ui_state_machine_state() == UI_RIDE_STATE_RIDING ||
           ui_state_machine_state() == UI_RIDE_STATE_PAUSED)
            app_action(UI_ACTION_END_RIDE);
        return;
    }
    if(input == UI_INPUT_BACK) {
        ui_app_handle_back();
        return;
    }
    if(app_control_shade_visible() &&
       (input == UI_INPUT_ENCODER_CW ||
        input == UI_INPUT_ENCODER_CCW ||
        input == UI_INPUT_ENCODER_PRESS)) {
        (void)ui_control_center_handle_input(input);
        return;
    }
    if(!app_control_shade_visible() && ui_dock_view_is_visible() &&
       (input == UI_INPUT_ENCODER_CW ||
        input == UI_INPUT_ENCODER_CCW ||
        input == UI_INPUT_ENCODER_PRESS)) {
        if(input == UI_INPUT_ENCODER_PRESS &&
           s_settings_control != APP_SETTINGS_CONTROL_NONE)
            app_dismiss_settings_control();
        else
            (void)ui_dock_view_handle_input(input);
        return;
    }
    if(input == UI_INPUT_PAGE_NEXT || input == UI_INPUT_PAGE_PREV) {
        if(s_powered_off || app_control_shade_visible()) return;
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
    if(input == UI_INPUT_RIDE_TOGGLE) {
        if(s_powered_off || app_safety_interaction_active()) return;
        if(ui_ride_history_repository_has_pending()) app_action(UI_ACTION_SUMMARY);
        else if(ui_state_machine_state() == UI_RIDE_STATE_HOME) app_action(UI_ACTION_START_RIDE);
        else if(ui_state_machine_state() == UI_RIDE_STATE_GPS_READY)
            (void)app_publish_event(UI_EVENT_RIDE_GO_REQUESTED, UI_EVENT_PRIORITY_P2_RIDE_STATE);
        else if(ui_state_machine_state() == UI_RIDE_STATE_RIDING ||
                ui_state_machine_state() == UI_RIDE_STATE_PAUSED) app_action(UI_ACTION_PAUSE_RIDE);
        return;
    }
    if(input == UI_INPUT_OPEN_PROFILE) {
        if(!s_powered_off && !app_ride_domain_active()) app_show(UI_PAGE_PROFILE);
        return;
    }
    if(input == UI_INPUT_OPEN_SETTINGS) {
        if(!s_powered_off) app_show(UI_PAGE_SETTINGS);
        return;
    }
    if(input == UI_INPUT_NAV_DEMO_NEXT) {
        if(!s_powered_off && !app_control_shade_visible() &&
           ui_router_current() == UI_PAGE_NAVIGATION)
            ui_navigation_demo_next_state();
        return;
    }
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_DASHBOARD &&
       ui_dynamic_card_manager_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_HOME &&
       ui_home_page_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_HISTORY &&
       ui_history_page_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_PROFILE &&
       ui_profile_page_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_ACHIEVEMENTS &&
       ui_achievements_page_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_NAVIGATION &&
       ui_navigation_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_SETTINGS &&
       ui_settings_page_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_SYSTEM &&
       ui_system_page_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_DEVICES &&
       ui_devices_page_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_SAVED_ROUTES &&
       (input == UI_INPUT_ENCODER_PRESS || input == UI_INPUT_ENCODER_CW ||
        input == UI_INPUT_ENCODER_CCW)) {
        if(input == UI_INPUT_ENCODER_PRESS) app_back();
        return;
    }
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_POWER &&
       ui_power_page_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_SUMMARY &&
       ui_summary_page_handle_input(input)) return;
    if(!s_powered_off && !app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_RIDE_DETAIL &&
       ui_ride_detail_page_handle_input(input)) return;

    if(input == UI_INPUT_ENCODER_CW) app_handle_key(LV_KEY_DOWN);
    else if(input == UI_INPUT_ENCODER_CCW) app_handle_key(LV_KEY_UP);
    else if(input == UI_INPUT_ENCODER_PRESS) app_handle_key(LV_KEY_ENTER);
}

static void app_handle_key(uint32_t key) {
    if(ui_router_current() == UI_PAGE_BOOT || s_boot_fade_state != APP_BOOT_FADE_IDLE) return;
    if(s_display_blank) {
        app_set_display_blank(false);
        return;
    }
    if(s_powered_off) {
        if(key == 'p' || key == 'P' || key == LV_KEY_ENTER) {
            s_powered_off = false;
            (void)ui_router_replace(UI_PAGE_BOOT, UI_MOTION_PAGE_NONE);
        }
        return;
    }
    if(ui_dashboard_editor_visible()) {
        if(key == LV_KEY_UP) ui_app_handle_input(UI_INPUT_ENCODER_CCW);
        else if(key == LV_KEY_DOWN) ui_app_handle_input(UI_INPUT_ENCODER_CW);
        else if(key == LV_KEY_ENTER) ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
        else if(key == LV_KEY_ESC || key == 8) ui_app_handle_back();
        else if(key == 'p' || key == 'P' || key == 'o' || key == 'O' || key == 'm' || key == 'M')
            ui_dashboard_editor_close();
        else return;
        if(ui_dashboard_editor_visible() || key == LV_KEY_UP || key == LV_KEY_DOWN ||
           key == LV_KEY_ENTER || key == LV_KEY_ESC || key == 8) return;
    }
    if(key == 'p' || key == 'P') {
        app_hide_quick_immediate();
        app_set_display_blank(true);
        return;
    }
    if(key == 'o' || key == 'O') { app_show(UI_PAGE_POWER); return; }
    if(key == 'm' || key == 'M') {
        ui_system_control_shade_state_t shade =
            ui_system_control_shade_state();
        if(shade == UI_SYSTEM_CONTROL_SHADE_CLOSED) app_open_quick();
        else if(shade == UI_SYSTEM_CONTROL_SHADE_COMPACT)
            app_expand_full_control();
        else app_hide_quick_immediate();
        return;
    }
    if(key == '[') { app_show(UI_PAGE_HOME); return; }
    if(key == ']') { app_show(UI_PAGE_SETTINGS); return; }
    if(key == LV_KEY_ESC || key == 8) { ui_app_handle_back(); return; }
    if(app_control_shade_visible()) return;
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

static uint8_t app_slider_level_from_x(int32_t screen_x) {
    return ui_control_center_level_from_x(screen_x);
}

static uint8_t app_preview_active_slider(int32_t screen_x) {
    uint8_t value = app_slider_level_from_x(screen_x);
    ui_control_center_preview_level(s_active_slider, value);
    return value;
}

static void app_commit_active_slider(int32_t screen_x) {
    uint8_t value = app_preview_active_slider(screen_x);
    if(s_active_slider == UI_CONTROL_CENTER_SLIDER_SOUND)
        (void)app_submit_command(UI_COMMAND_SOUND_SET_LEVEL, value, 1500U);
    else if(s_active_slider == UI_CONTROL_CENTER_SLIDER_BRIGHTNESS)
        (void)app_submit_command(UI_COMMAND_BRIGHTNESS_SET_LEVEL,
                                 value, 1500U);
}

void ui_app_handle_pointer(const ui_input_pointer_event_t *event) {
    if(event == NULL) return;
    s_last_interaction_tick = lv_tick_get();
    if(ui_router_current() == UI_PAGE_BOOT || s_boot_fade_state != APP_BOOT_FADE_IDLE) return;
    if(s_display_blank) {
        if(event->type == UI_INPUT_TOUCH_BEGIN) {
            app_set_display_blank(false);
            s_touch_sequence_blocked = true;
        }
        return;
    }
    if(s_control_surface == NULL || s_powered_off) return;
    if(ui_dashboard_editor_visible()) {
        s_touch_sequence_blocked = true;
        return;
    }
    if(ui_zone_editor_visible()) return;
    if(!app_control_shade_visible() &&
       ui_router_current() == UI_PAGE_ACHIEVEMENTS &&
       ui_achievements_page_detail_visible()) {
        /* If BACK closes the modal before finger release, consume the rest
         * of that touch sequence instead of starting an underlying swipe. */
        s_touch_sequence_blocked = true;
        return;
    }

    lv_point_t point = { .x = event->x, .y = event->y };

    if(event->type == UI_INPUT_TOUCH_BEGIN) {
        ui_system_control_shade_state_t shade =
            ui_system_control_shade_state();
        ui_app_state_t input_state;
        s_touch_sequence_blocked =
            ui_app_state_get(&input_state) && input_state.touch_locked &&
            shade == UI_SYSTEM_CONTROL_SHADE_CLOSED;
        if(s_touch_sequence_blocked) return;
        ui_motion_cancel_y(s_control_surface);
        ui_motion_cancel(s_control_surface, app_set_panel_height);
        s_control_interaction = APP_CONTROL_SURFACE_IDLE;
        s_pointer_start = point;
        s_pointer_start_tick = lv_tick_get();
        s_overlay_start_y = lv_obj_get_y(s_control_surface);
        s_panel_start_height = ui_control_center_panel_height();
        s_pointer_dragging = false;
        s_shade_drag = APP_SHADE_DRAG_NONE;
        s_slider_dragging = false;
        s_slider_candidate = false;
        s_page_swiping = false;
        s_home_swiping = false;
        s_active_slider = UI_CONTROL_CENTER_SLIDER_NONE;
        s_dynamic_card_gesture_owned =
            shade == UI_SYSTEM_CONTROL_SHADE_CLOSED &&
            ui_router_current() == UI_PAGE_DASHBOARD &&
            ui_dynamic_card_manager_contains_point(point.x, point.y);
        s_navigation_map_gesture_owned =
            shade == UI_SYSTEM_CONTROL_SHADE_CLOSED &&
            ui_router_current() == UI_PAGE_NAVIGATION &&
            ui_navigation_contains_point(point.x, point.y);
        s_bottom_surface_gesture_owned =
            app_control_shade_visible() && ui_dock_view_is_visible() &&
            app_point_inside(ui_bottom_surface_root(), point.x, point.y);
        ui_route_id_t route = ui_router_current();
        s_page_scroll_gesture_owned =
            shade == UI_SYSTEM_CONTROL_SHADE_CLOSED &&
            ((route == UI_PAGE_HISTORY &&
              ui_history_page_contains_scroll_point(point.x, point.y)) ||
             (route == UI_PAGE_ACHIEVEMENTS &&
              ui_achievements_page_contains_scroll_point(point.x, point.y)) ||
             (route == UI_PAGE_SETTINGS &&
              ui_settings_page_contains_scroll_point(point.x, point.y)) ||
             (route == UI_PAGE_SYSTEM &&
              ui_system_page_contains_scroll_point(point.x, point.y)) ||
             (route == UI_PAGE_DEVICES &&
              ui_devices_page_contains_scroll_point(point.x, point.y)) ||
             (route == UI_PAGE_SUMMARY &&
              ui_summary_page_contains_scroll_point(point.x, point.y)) ||
             (route == UI_PAGE_RIDE_DETAIL &&
              ui_ride_detail_page_contains_scroll_point(point.x, point.y)));

        if(s_dynamic_card_gesture_owned || s_navigation_map_gesture_owned ||
           s_bottom_surface_gesture_owned ||
           s_page_scroll_gesture_owned) return;

        if(shade == UI_SYSTEM_CONTROL_SHADE_EXPANDED &&
           lv_obj_get_y(s_control_surface) == 0) {
            s_active_slider = ui_control_center_slider_at(point.x, point.y);
            s_slider_candidate =
                s_active_slider != UI_CONTROL_CENTER_SLIDER_NONE;
        }
        return;
    }

    if(event->type == UI_INPUT_TOUCH_MOVE) {
        if(s_touch_sequence_blocked) return;
        if(s_dynamic_card_gesture_owned || s_navigation_map_gesture_owned ||
           s_bottom_surface_gesture_owned ||
           s_page_scroll_gesture_owned) return;

        int32_t delta_x = point.x - s_pointer_start.x;
        int32_t delta_y = point.y - s_pointer_start.y;
        int32_t abs_x = delta_x < 0 ? -delta_x : delta_x;
        int32_t abs_y = delta_y < 0 ? -delta_y : delta_y;

        if(s_slider_candidate && !s_slider_dragging &&
           !s_pointer_dragging) {
            if(abs_x <= QUICK_DRAG_THRESHOLD &&
               abs_y <= QUICK_DRAG_THRESHOLD) return;
            if(abs_x > abs_y) {
                s_slider_dragging = true;
                s_slider_candidate = false;
            }
            else {
                s_slider_candidate = false;
            }
        }

        if(s_slider_dragging) {
            (void)app_preview_active_slider(point.x);
            return;
        }

        if(!s_pointer_dragging && !s_page_swiping && !s_home_swiping) {
            ui_system_control_shade_state_t shade =
                ui_system_control_shade_state();
            if(abs_x > QUICK_DRAG_THRESHOLD && abs_x > abs_y &&
               shade == UI_SYSTEM_CONTROL_SHADE_CLOSED &&
               (ui_router_current() == UI_PAGE_HOME ||
                ui_router_current() == UI_PAGE_DASHBOARD ||
                ui_router_current() == UI_PAGE_NAVIGATION)) {
                s_page_swiping = true;
                return;
            }
            if(abs_y <= QUICK_DRAG_THRESHOLD || abs_y <= abs_x) return;
            if(shade == UI_SYSTEM_CONTROL_SHADE_CLOSED && delta_y > 0) {
                if(app_safety_interaction_active()) return;
                (void)ui_system_control_shade_open_compact();
                app_reclaim_control_feedback_to_top();
                s_shade_drag = APP_SHADE_DRAG_OPEN_COMPACT;
                s_overlay_start_y = -UI_CONTROL_CENTER_COMPACT_HEIGHT;
                ui_control_center_set_mode(UI_CONTROL_CENTER_MODE_COMPACT);
                lv_obj_clear_flag(s_control_surface, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(s_control_surface);
                lv_obj_set_y(s_control_surface, s_overlay_start_y);
                s_pointer_dragging = true;
                s_control_interaction = APP_CONTROL_SURFACE_DRAGGING;
                ui_control_center_prepare_show();
                app_update_quick_states();
            }
            else if(shade == UI_SYSTEM_CONTROL_SHADE_COMPACT &&
                    delta_y > 0) {
                (void)ui_system_control_shade_expand();
                app_reclaim_control_feedback_to_top();
                s_shade_drag = APP_SHADE_DRAG_EXPAND;
                s_panel_start_height = ui_control_center_panel_height();
                s_pointer_dragging = true;
                s_control_interaction = APP_CONTROL_SURFACE_DRAGGING;
            }
            else if(shade == UI_SYSTEM_CONTROL_SHADE_COMPACT &&
                    delta_y < 0) {
                s_shade_drag = APP_SHADE_DRAG_CLOSE_COMPACT;
                s_overlay_start_y = lv_obj_get_y(s_control_surface);
                s_pointer_dragging = true;
                s_control_interaction = APP_CONTROL_SURFACE_DRAGGING;
            }
            else if(shade == UI_SYSTEM_CONTROL_SHADE_EXPANDED &&
                    delta_y < 0) {
                (void)ui_control_center_exit_adjustment(true);
                s_shade_drag = APP_SHADE_DRAG_CLOSE_EXPANDED;
                s_panel_start_height = ui_control_center_panel_height();
                s_pointer_dragging = true;
                s_control_interaction = APP_CONTROL_SURFACE_DRAGGING;
            }
            else if(shade == UI_SYSTEM_CONTROL_SHADE_CLOSED && delta_y < 0) {
                s_home_swiping = true;
                return;
            }
            else {
                return;
            }
        }

        if(s_page_swiping || s_home_swiping) return;

        if(s_shade_drag == APP_SHADE_DRAG_OPEN_COMPACT ||
           s_shade_drag == APP_SHADE_DRAG_CLOSE_COMPACT) {
            int32_t target_y = LV_CLAMP(
                -UI_CONTROL_CENTER_COMPACT_HEIGHT,
                s_overlay_start_y + delta_y, 0);
            lv_obj_set_y(s_control_surface, target_y);
        }
        else if(s_shade_drag == APP_SHADE_DRAG_EXPAND ||
                s_shade_drag == APP_SHADE_DRAG_CLOSE_EXPANDED) {
            ui_control_center_set_panel_height(
                LV_CLAMP(UI_CONTROL_CENTER_COMPACT_HEIGHT,
                         s_panel_start_height + delta_y,
                         UI_CONTROL_CENTER_PANEL_HEIGHT));
        }
        return;
    }

    if(event->type == UI_INPUT_TOUCH_CANCEL) {
        if(s_navigation_map_gesture_owned) ui_navigation_cancel_pointer();
        ui_control_center_exit_adjustment(false);
        app_update_quick_states();
        for(lv_indev_t *indev = lv_indev_get_next(NULL); indev != NULL;
            indev = lv_indev_get_next(indev)) {
            if(lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
                lv_indev_reset(indev, NULL);
                lv_indev_wait_release(indev);
            }
        }
        s_touch_sequence_blocked = false;
        s_dynamic_card_gesture_owned = false;
        s_navigation_map_gesture_owned = false;
        s_bottom_surface_gesture_owned = false;
        s_page_scroll_gesture_owned = false;
        s_pointer_dragging = s_page_swiping = s_home_swiping = false;
        s_slider_dragging = s_slider_candidate = false;
        s_active_slider = UI_CONTROL_CENTER_SLIDER_NONE;
        s_shade_drag = APP_SHADE_DRAG_NONE;
        if(app_control_shade_visible()) {
            lv_obj_set_y(s_control_surface, 0);
            ui_control_center_set_panel_height(
                ui_system_control_shade_state() == UI_SYSTEM_CONTROL_SHADE_EXPANDED ?
                UI_CONTROL_CENTER_PANEL_HEIGHT : UI_CONTROL_CENTER_COMPACT_HEIGHT);
        }
        return;
    }
    if(event->type == UI_INPUT_TOUCH_END) {
        if(s_touch_sequence_blocked) {
            s_touch_sequence_blocked = false;
            return;
        }
        if(s_dynamic_card_gesture_owned) {
            s_dynamic_card_gesture_owned = false;
            s_pointer_dragging = false;
            s_page_swiping = false;
            s_home_swiping = false;
            return;
        }
        if(s_navigation_map_gesture_owned) {
            s_navigation_map_gesture_owned = false;
            s_pointer_dragging = false;
            s_page_swiping = false;
            s_home_swiping = false;
            return;
        }
        if(s_bottom_surface_gesture_owned) {
            s_bottom_surface_gesture_owned = false;
            s_pointer_dragging = false;
            s_page_swiping = false;
            s_home_swiping = false;
            return;
        }
        if(s_page_scroll_gesture_owned) {
            s_page_scroll_gesture_owned = false;
            s_pointer_dragging = false;
            s_page_swiping = false;
            s_home_swiping = false;
            return;
        }

        if(s_slider_dragging || s_slider_candidate) {
            app_commit_active_slider(point.x);
            s_slider_dragging = false;
            s_slider_candidate = false;
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
            uint32_t elapsed = lv_tick_elaps(s_pointer_start_tick);
            if(elapsed == 0U) elapsed = 1U;
            int32_t velocity = delta_y * 1000 / (int32_t)elapsed;
            switch(s_shade_drag) {
                case APP_SHADE_DRAG_OPEN_COMPACT:
                    if(delta_y >= CONTROL_OPEN_DISTANCE ||
                       velocity >= CONTROL_FAST_VELOCITY) {
                        s_control_interaction = APP_CONTROL_SURFACE_SETTLING;
                        ui_motion_move_y(
                            s_control_surface, 0, UI_MOTION_DURATION_DOCK,
                            lv_anim_path_ease_out,
                            app_control_settle_done, NULL);
                    }
                    else {
                        app_handoff_control_feedback_to_bottom();
                        app_close_quick();
                    }
                    break;
                case APP_SHADE_DRAG_CLOSE_COMPACT:
                    if(delta_y <= -CONTROL_CLOSE_DISTANCE ||
                       velocity <= -CONTROL_FAST_VELOCITY)
                        app_close_quick();
                    else {
                        s_control_interaction = APP_CONTROL_SURFACE_SETTLING;
                        ui_motion_move_y(
                            s_control_surface, 0, UI_MOTION_DURATION_DOCK,
                            lv_anim_path_ease_out,
                            app_control_settle_done, NULL);
                    }
                    break;
                case APP_SHADE_DRAG_EXPAND:
                    if(delta_y >= CONTROL_EXPAND_DISTANCE ||
                       velocity >= CONTROL_FAST_VELOCITY) {
                        s_control_interaction = APP_CONTROL_SURFACE_SETTLING;
                        app_animate_panel_height(
                            UI_CONTROL_CENTER_PANEL_HEIGHT);
                    }
                    else {
                        (void)ui_system_control_shade_set_compact();
                        s_control_interaction = APP_CONTROL_SURFACE_SETTLING;
                        app_animate_panel_height(
                            UI_CONTROL_CENTER_COMPACT_HEIGHT);
                    }
                    break;
                case APP_SHADE_DRAG_CLOSE_EXPANDED:
                    if(delta_y <= -CONTROL_CLOSE_DISTANCE ||
                       velocity <= -CONTROL_FAST_VELOCITY)
                        app_close_quick();
                    else {
                        s_control_interaction = APP_CONTROL_SURFACE_SETTLING;
                        app_animate_panel_height(
                            UI_CONTROL_CENTER_PANEL_HEIGHT);
                    }
                    break;
                default:
                    break;
            }
        }

        s_pointer_dragging = false;
        s_shade_drag = APP_SHADE_DRAG_NONE;
        s_page_swiping = false;
        s_home_swiping = false;
    }
}
