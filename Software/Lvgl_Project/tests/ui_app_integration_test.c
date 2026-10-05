#include "ui/pages/navigation/ui_nav_workspace.h"
#include "ui/pages/navigation/ui_navigation.h"
#include "ui/components/navigation/ui_nav_search_panel.h"
#include "ui/components/navigation/ui_nav_layout.h"
#include "services/ui_nav_session.h"
#include "services/ui_navigation_service.h"
#include "services/ui_nav_map_provider.h"
#include "platform/pc/ui_pc_navigation.h"
#include "platform/pc/ui_pc_offline_navigation.h"
#include "ui/core/ui_app.h"
#include "ui/core/ui_app_state.h"
#include "ui/core/ui_state_machine.h"
#include "ui/core/ui_router.h"
#include "ui/core/ui_event_bus.h"
#include "ui/core/ui_motion.h"
#include "ui/core/ui_bottom_surface.h"
#include "ui/core/ui_system_control_shade.h"
#include "ui/core/ui_dock.h"
#include "ui/overlays/ui_zone_editor.h"
#include "ui/overlays/ui_dashboard_editor.h"
#include "ui/screens/ui_dashboard.h"
#include "ui/components/ui_dynamic_card_manager.h"
#include "ui/components/ui_metric_card.h"
#include "services/ui_preferences.h"
#include "ui/pages/ui_power_page.h"
#include "ui/pages/ui_achievements_page.h"
#include "ui/pages/ui_boot_page.h"
#include "ui/pages/ui_home_page.h"
#include "ui/pages/ui_devices_page.h"
#include "ui/components/ui_liquid_selection.h"
#include "ui/pages/ui_ride_report_page.h"
#include "services/ui_command_service.h"
#include "ui/models/ui_ride_data_model.h"
#include "ui/models/ui_home_data_model.h"
#include "ui/models/ui_ride_history_model.h"
#include "ui/models/ui_ride_summary_model.h"
#include "ui/overlays/ui_control_center.h"
#include "services/ui_ride_history_repository.h"
#include "platform/pc/ui_pc_services.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#endif

static uint8_t draw_buffer[480 * 854 * 2];
static uint16_t frame_buffer[480 * 854];
static const char *capture_directory;
static bool render_failure;
static ui_ride_record_t records[UI_RIDE_HISTORY_MAX_RECORDS];
static size_t record_count;
static lv_indev_state_t pointer_state=LV_INDEV_STATE_RELEASED;
static lv_point_t pointer_position;
static uint8_t preferences_blob[UI_PREFERENCES_BLOB_SIZE];
static bool preferences_saved;
static unsigned boot_completions;
static void boot_completed(const ui_event_t *event, void *context)
{
    (void)context;
    if(event->type == UI_EVENT_BOOT_COMPLETE) ++boot_completions;
}
static bool preferences_load(uint8_t *blob,void *context)
{ (void)context; memcpy(blob,preferences_blob,sizeof(preferences_blob)); return preferences_saved; }
static bool preferences_store(const uint8_t *blob,void *context)
{ (void)context; memcpy(preferences_blob,blob,sizeof(preferences_blob)); preferences_saved=true; return true; }
static bool preferences_request(void *context)
{ (void)context; ui_preferences_worker(); return true; } /* In-memory fake: no file I/O. */
static void read_pointer(lv_indev_t *indev,lv_indev_data_t *data)
{ (void)indev; data->point=pointer_position; data->state=pointer_state; }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    unsigned width = (unsigned)(area->x2 - area->x1 + 1);
    unsigned stride = lv_display_get_buf_active(display)->header.stride;
    for(int y = area->y1; y <= area->y2; ++y)
        memcpy(&frame_buffer[y * 480 + area->x1], pixels + (y - area->y1) * stride, width * 2);
    lv_display_flush_ready(display);
}
static void capture(const char *name)
{
    if(capture_directory == NULL) return;
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
    lv_refr_now(lv_display_get_default());
    char path[1024]; snprintf(path,sizeof(path),"%s/%s.ppm",capture_directory,name);
    FILE *file=fopen(path,"wb"); assert(file);
    fprintf(file,"P6\n480 854\n255\n");
    for(unsigned i=0;i<480*854;++i) {
        uint16_t pixel=frame_buffer[i];
        uint8_t rgb[3]={(uint8_t)((pixel>>11)*255/31),(uint8_t)(((pixel>>5)&63)*255/63),(uint8_t)((pixel&31)*255/31)};
        assert(fwrite(rgb,1,3,file)==3);
    }
    assert(fclose(file)==0);
}
static void log_message(lv_log_level_t level, const char *message)
{
    (void)level;
    if(strstr(message, "No memory") || strstr(message, "layer buffer failed") ||
       strstr(message, "glyph dsc. not found")) {
        render_failure = true;
        fprintf(stderr,"LVGL render failure: %s",message);
    }
}
static void advance(unsigned ms)
{
    for(unsigned elapsed = 0; elapsed < ms; elapsed += 10) {
        lv_tick_inc(10);
        (void)lv_timer_handler();
#ifdef _WIN32
        if(ui_ride_history_repository_busy()) Sleep(1);
#endif
    }
    assert(!render_failure);
}
static ui_ride_history_result_t load(ui_ride_record_t *output, size_t capacity,
    size_t *count, void *context)
{
    (void)context;
    assert(capacity >= record_count);
    memcpy(output, records, record_count * sizeof(records[0]));
    *count = record_count;
    return record_count ? UI_RIDE_HISTORY_OK : UI_RIDE_HISTORY_EMPTY;
}
static ui_ride_history_result_t store(const ui_ride_record_t *input, size_t count, void *context)
{
    (void)context;
    memcpy(records, input, count * sizeof(records[0]));
    record_count = count;
    return UI_RIDE_HISTORY_OK;
}
static int64_t utc(void *context) { (void)context; return 1750000000 + lv_tick_get() / 1000; }
static void event(ui_event_type_t type)
{
    ui_event_t value = {.type = type, .priority = UI_EVENT_PRIORITY_P2_RIDE_STATE};
    assert(ui_event_bus_publish(&value));
    ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
}
static void confirm_power(void)
{
    ui_app_handle_input(UI_INPUT_OPEN_POWER);
    assert(ui_router_current()==UI_ROUTE_POWER);
    capture("power");
    assert(ui_power_page_handle_input(UI_INPUT_ENCODER_PRESS));
    const ui_dock_card_t *card=ui_dock_current();
    assert(card && card->id==0x2101U);
    assert(ui_dock_invoke(card->id,UI_DOCK_INTERACTION_PRIMARY,0));
}
static lv_obj_t *find_label(lv_obj_t *parent, const char *text)
{
    if(lv_obj_check_type(parent, &lv_label_class) &&
       strcmp(lv_label_get_text(parent), text) == 0) return parent;
    for(unsigned i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *found = find_label(lv_obj_get_child(parent, i), text);
        if(found != NULL) return found;
    }
    return NULL;
}
static void touch(int x,int y,bool pressed)
{
    pointer_position=(lv_point_t){x,y};
    ui_input_pointer_event_t input={.x=x,.y=y,.type=pressed ?
        (pointer_state==LV_INDEV_STATE_PRESSED ? UI_INPUT_TOUCH_MOVE : UI_INPUT_TOUCH_BEGIN) : UI_INPUT_TOUCH_END};
    pointer_state=pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    ui_app_handle_pointer(&input); advance(40);
}
static void tap_label(lv_obj_t *parent,const char *text,bool scroll)
{
    lv_obj_t *label=NULL;
    /* Hidden overlays intentionally retain labels with the same actions. */
    lv_obj_t *stack[192]; unsigned count=0;stack[count++]=parent;
    while(count) {
        lv_obj_t *o=stack[--count]; if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))continue;
        if(lv_obj_check_type(o,&lv_label_class)&&strcmp(lv_label_get_text(o),text)==0){label=o;break;}
        for(unsigned i=0;i<lv_obj_get_child_count(o);i++){assert(count<192);stack[count++]=lv_obj_get_child(o,i);}
    }
    if(!label)fprintf(stderr,"Missing visible action: %s, route=%u\n",text,(unsigned)ui_router_current());
    assert(label);
    lv_obj_t *row=lv_obj_get_parent(label);
    if(scroll) { lv_obj_scroll_to_view_recursive(row,LV_ANIM_OFF); advance(40); }
    lv_obj_update_layout(row); lv_area_t area; lv_obj_get_coords(row,&area);
    int x=(area.x1+area.x2)/2,y=(area.y1+area.y2)/2;
    if(y < 0 || y >= 854)
        fprintf(stderr, "Tap target outside screen: %s (%d, %d)\n", text, x, y);
    assert(y>=0 && y<854);
    touch(x,y,true); touch(x,y,false);
}
static void confirm_restart(void)
{
    ui_app_handle_input(UI_INPUT_OPEN_POWER);
    assert(ui_router_current() == UI_ROUTE_POWER);
    tap_label(lv_screen_active(), "重新启动", false);
    const ui_dock_card_t *card = ui_dock_current();
    assert(card && card->id == 0x2101U);
    assert(ui_dock_invoke(card->id, UI_DOCK_INTERACTION_PRIMARY, 0));
}
static void apply_preset(unsigned preset)
{
    ui_preferences_t preferences; assert(ui_preferences_get(&preferences));
    preferences.dashboard_preset=(uint8_t)preset;
    assert(ui_dashboard_preset_fields(preset,preferences.dashboard_fields));
    assert(ui_preferences_submit(&preferences)); advance(100);
    for(unsigned i=0;i<6;++i) assert(ui_dashboard_slot_field(i)==preferences.dashboard_fields[i]);
}
static lv_obj_t *speed_segment(void)
{
    lv_obj_t *screen=ui_dashboard_get_screen();
    for(unsigned i=0;i<lv_obj_get_child_count(screen);++i) {
        lv_obj_t *object=lv_obj_get_child(screen,i);
        if(lv_obj_get_x(object)==24 && lv_obj_get_y(object)==278 && lv_obj_get_width(object)==52) return object;
    }
    assert(false); return NULL;
}
static void test_layout_editor(void)
{
    ui_app_handle_input(UI_INPUT_OPEN_SETTINGS); advance(600);
    assert(ui_router_current()==UI_ROUTE_SETTINGS);capture("settings_initial");
    tap_label(lv_screen_active(),"骑行数据布局",true);
    assert(ui_dashboard_editor_visible()); advance(300); capture("layout_editor");
    tap_label(lv_layer_top(),"骑行方案",false); capture("preset_picker");
    ui_app_handle_input(UI_INPUT_ENCODER_CW);
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
    ui_preferences_t before; assert(ui_preferences_get(&before));
    assert(before.dashboard_preset==UI_DASHBOARD_CRUISE);
    tap_label(lv_layer_top(),"取消",false);
    assert(!ui_dashboard_editor_visible());
    ui_preferences_t after; assert(ui_preferences_get(&after));
    assert(memcmp(before.dashboard_fields,after.dashboard_fields,6)==0);

    tap_label(lv_screen_active(),"骑行数据布局",false); advance(300);
    tap_label(lv_layer_top(),"骑行方案",false);
    ui_app_handle_input(UI_INPUT_ENCODER_CW); ui_app_handle_input(UI_INPUT_ENCODER_CW);
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS); /* Climb draft. */
    tap_label(lv_layer_top(),"上左",false); capture("field_picker");
    lv_obj_t *picker=lv_obj_get_parent(find_label(lv_layer_top(),"选择指标"));
    tap_label(picker,"功率",false); /* Disabled: already used in upper right. */
    assert(find_label(lv_layer_top(),"选择指标"));
    ui_app_handle_input(UI_INPUT_ENCODER_CW); /* Skip distance/time already in other slots. */
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS); /* Average speed. */
    assert(!find_label(lv_layer_top(),"选择指标"));
    after.sound=47; assert(ui_preferences_submit(&after)); advance(100);
    tap_label(lv_layer_top(),"保存布局",false); advance(150);
    assert(!ui_dashboard_editor_visible()); assert(ui_preferences_get(&after));
    assert(after.dashboard_preset==UI_DASHBOARD_CUSTOM && after.sound==47);
    assert(after.dashboard_fields[0]==UI_RIDE_FIELD_AVERAGE_SPEED);
    assert(ui_dashboard_slot_field(0)==UI_RIDE_FIELD_AVERAGE_SPEED);
    assert(ui_dashboard_slot_field(2)==UI_RIDE_FIELD_ALTITUDE);
    advance(5100); /* Existing asynchronous save throttle, including concurrent changes. */
    ui_preferences_t decoded; assert(ui_preferences_decode(preferences_blob,&decoded));
    assert(memcmp(decoded.dashboard_fields,after.dashboard_fields,6)==0);

    ui_dashboard_editor_open(); advance(300);
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS); /* Preset picker. */
    ui_app_handle_input(UI_INPUT_BACK); /* Close picker only. */
    assert(ui_dashboard_editor_visible());
    ui_input_pointer_event_t modal={.type=UI_INPUT_TOUCH_BEGIN,.x=450,.y=100};
    ui_app_handle_pointer(&modal);
    ui_app_handle_input(UI_INPUT_BACK); assert(!ui_dashboard_editor_visible());
    modal.type=UI_INPUT_TOUCH_MOVE; modal.x=20; ui_app_handle_pointer(&modal);
    modal.type=UI_INPUT_TOUCH_END; ui_app_handle_pointer(&modal);
    assert(ui_router_current()==UI_ROUTE_SETTINGS);
    apply_preset(UI_DASHBOARD_CRUISE);
    ui_app_handle_input(UI_INPUT_BACK); advance(600);
    assert(ui_router_current()==UI_ROUTE_HOME);
}
static unsigned scan_requests,connection_requests;static ui_device_kind_t requested_device;
static bool scan_devices(void *context){(void)context;++scan_requests;return true;}
static bool connect_device(ui_device_kind_t kind,void *context){(void)context;++connection_requests;requested_device=kind;return true;}
static void test_shell_pages(void)
{
    ui_home_data_t initial_home_data;
    assert(ui_home_data_model_get(&initial_home_data));
    ui_app_handle_input(UI_INPUT_OPEN_POWER); advance(250);
    assert(ui_router_current() == UI_ROUTE_POWER);
    tap_label(lv_screen_active(), "取消", false); advance(250);
    assert(ui_router_current() == UI_ROUTE_HOME);
    ui_app_handle_input(UI_INPUT_OPEN_POWER); advance(250);
    assert(ui_power_page_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_power_page_handle_input(UI_INPUT_ENCODER_CW));
    assert(ui_power_page_handle_input(UI_INPUT_ENCODER_PRESS)); advance(250);
    assert(ui_router_current() == UI_ROUTE_HOME);
    /* Leave the independent power tests with their original OFF selection. */
    ui_app_handle_input(UI_INPUT_OPEN_POWER); advance(250);
    assert(ui_power_page_handle_input(UI_INPUT_ENCODER_CCW));
    assert(ui_power_page_handle_input(UI_INPUT_ENCODER_CCW));
    ui_app_handle_input(UI_INPUT_BACK); advance(250);
    tap_label(lv_screen_active(),"历史记录",false); advance(250);
    assert(ui_router_current()==UI_ROUTE_HISTORY); capture("history_empty");
    ui_app_handle_input(UI_INPUT_BACK); advance(250);
    ui_app_handle_input(UI_INPUT_PAGE_NEXT); advance(500);
    assert(ui_router_current()==UI_ROUTE_NAVIGATION); capture("navigation");
    ui_app_handle_input(UI_INPUT_BACK); advance(500);
    assert(ui_router_current()==UI_ROUTE_HOME);
    tap_label(lv_screen_active(),"收藏路线",false); advance(250);
    assert(ui_router_current()==UI_ROUTE_SAVED_ROUTES);
    capture("saved_routes_empty");
    assert(find_label(lv_screen_active(),"导航服务暂不可用"));
    ui_app_handle_input(UI_INPUT_BACK); advance(250);
    assert(ui_router_current()==UI_ROUTE_HOME);
    tap_label(lv_screen_active(),"设备与传感器",false); advance(250);
    assert(ui_router_current()==UI_ROUTE_DEVICES);
    tap_label(lv_screen_active(),"搜索附近设备",false); advance(100);
    assert(find_label(lv_screen_active(),"暂无法扫描，蓝牙服务待接入") ||
           find_label(lv_screen_active(),"请先在系统设置中开启蓝牙"));
    capture("devices");
    const ui_device_connection_port_t device_port={.scan=scan_devices,.connect=connect_device};
    ui_devices_page_register_connection_port(&device_port);
    ui_app_state_t device_state;assert(ui_app_state_get(&device_state));
    if(!device_state.bluetooth_enabled){assert(ui_command_service_submit(UI_COMMAND_BLUETOOTH_SET_ENABLED,1,2000));advance(600);}
    tap_label(lv_screen_active(),"搜索附近设备",false);advance(100);assert(scan_requests==1);
    ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(70);capture("devices_liquid_moving");
    ui_app_handle_input(UI_INPUT_ENCODER_CCW);advance(40);capture("devices_liquid_reverse");
    tap_label(lv_screen_active(),"心率带",false);advance(700);
    assert(connection_requests==1&&requested_device==UI_DEVICE_HEART_RATE);
    assert(find_label(lv_screen_active(),"未连接")); /* Request acceptance cannot fake a connection. */
    unsigned device_selection=ui_devices_page_selected_device();
    ui_devices_page_set_status(UI_DEVICE_HEART_RATE,"正在连接",0xFFD900);advance(100);
    assert(ui_devices_page_selected_device()==device_selection);capture("devices_heart_rate");
    lv_obj_t *device_row=lv_obj_get_parent(lv_obj_get_parent(find_label(lv_screen_active(),"心率带")));
    lv_obj_t *device_list=lv_obj_get_parent(device_row);
    assert(lv_obj_get_height(device_row)>110&&lv_obj_get_scroll_bottom(device_list)>0);
    for(unsigned i=0;i<8;i++)ui_app_handle_input(UI_INPUT_ENCODER_CW);
    advance(700);assert(ui_devices_page_selected_device()==UI_DEVICE_SPEED_CADENCE);
    assert(lv_obj_get_scroll_bottom(device_list)<=1);capture("devices_bottom");
    for(unsigned i=0;i<8;i++)ui_app_handle_input(UI_INPUT_ENCODER_CCW);
    advance(700);assert(lv_obj_get_scroll_y(device_list)==0);capture("devices_top");
    ui_devices_page_register_connection_port(NULL);ui_devices_page_set_status(UI_DEVICE_HEART_RATE,"未连接",0xAAAAAA);
    ui_app_handle_input(UI_INPUT_BACK); advance(250);
    /* A home shortcut must open newest data even before history is entered. */
    ui_ride_record_t recent_records[2] = {{.id=701,.distance_km=7.0f,.duration_seconds=600},
                                        {.id=702,.distance_km=9.0f,.duration_seconds=900}};
    assert(ui_ride_history_model_replace(recent_records,2U));
    tap_label(lv_screen_active(),"最近一次骑行",false); advance(250);
    assert(ui_router_current()==UI_ROUTE_RIDE_DETAIL);
    assert(find_label(lv_screen_active(),"9.0"));
    capture("ride_detail");
    ui_app_handle_input(UI_INPUT_BACK); advance(250);
    ui_ride_history_model_init();
    assert(ui_home_data_model_update(&initial_home_data)); advance(20);
    assert(lv_color_eq(lv_obj_get_style_bg_color(lv_screen_active(),0),lv_color_black()));
    assert(lv_obj_get_style_bg_image_src(lv_screen_active(),0)==NULL);
    lv_obj_t *lifetime=lv_obj_get_parent(find_label(lv_screen_active(),"TOTAL DISTANCE"));
    assert(lifetime);
    assert(!lv_obj_has_flag(lifetime,LV_OBJ_FLAG_CLICKABLE));
    touch(100,230,true);touch(100,230,false);advance(100);
    touch(320,310,true);touch(320,310,false);advance(100);
    assert(ui_router_current()==UI_ROUTE_HOME);
    ui_liquid_selection_t *home_selection=lv_obj_get_user_data(lv_screen_active());
    assert(home_selection->count==9);
    for(unsigned i=0;i<9;i++){
        assert(home_selection->items[i].object!=lifetime);
    }
    /* A refresh is the only operable descendant in lifetime data, and remains on Home. */
    ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(700);capture("home_refresh_selected");
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS);ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
    assert(lv_obj_has_state(home_selection->items[1].object,LV_STATE_DISABLED));
    advance(700);assert(!lv_obj_has_state(home_selection->items[1].object,LV_STATE_DISABLED));
    assert(ui_router_current()==UI_ROUTE_HOME);capture("home_refreshed");
    ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(60);capture("home_liquid_moving");
    ui_app_handle_input(UI_INPUT_ENCODER_CCW);advance(40);capture("home_liquid_reverse");
    unsigned images=0;
    for(unsigned i=0;i<lv_obj_get_child_count(lifetime);++i) {
        lv_obj_t *child=lv_obj_get_child(lifetime,i);
        assert(!lv_obj_check_type(child,&lv_line_class));
        if(lv_obj_check_type(child,&lv_image_class)) {
            ++images; assert(lv_obj_get_style_image_opa(child,0)==24);
            assert(!lv_obj_has_flag(child,LV_OBJ_FLAG_CLICKABLE));
        }
    }
    assert(images==1 && find_label(lv_screen_active(),"暂无记录"));
    ui_home_data_t baseline,sample;
    assert(ui_home_data_model_get(&baseline)); sample=baseline;
    sample.total_distance_km=1234; sample.total_time_hours=73; sample.total_time_minutes=24;
    sample.total_ascent_m=18460; sample.last_ride_available=true;
    sample.last_distance_available=sample.last_ascent_available=true;
    sample.last_distance_tenths_km=527; sample.last_duration_minutes=127; sample.last_ascent_m=836;
    snprintf(sample.last_ride_date,sizeof(sample.last_ride_date),"2026.10.01  08:30");
    assert(ui_home_data_model_update(&sample)); advance(20); capture("home_populated");
    assert(find_label(lv_screen_active(),"52.7 km") && find_label(lv_screen_active(),"836 m"));
    sample.last_distance_available=sample.last_ascent_available=false;
    assert(ui_home_data_model_update(&sample)); advance(20); capture("home_unavailable");
    assert(!find_label(lv_screen_active(),"52.7 km") && !find_label(lv_screen_active(),"836 m"));
    sample.total_distance_km=UINT32_MAX;
    assert(ui_home_data_model_update(&sample)); advance(20);
    lv_obj_t *number=find_label(lv_screen_active(),"4294967295");
    lv_obj_t *unit=find_label(lifetime,"km");
    assert(number && unit);
    lv_area_t number_area,unit_area;
    lv_obj_get_coords(number,&number_area); lv_obj_get_coords(unit,&unit_area);
    assert(unit_area.x1>number_area.x1 && unit_area.x2<lv_obj_get_x(lifetime)+lv_obj_get_width(lifetime));
    capture("home_long_values");
    assert(ui_home_data_model_update(&baseline)); advance(20);
    ui_app_handle_input(UI_INPUT_OPEN_PROFILE); advance(600); capture("profile_empty");
    assert(lv_color_eq(lv_obj_get_style_bg_color(lv_screen_active(),0),lv_color_black()));
    assert(lv_obj_get_style_bg_image_src(lv_screen_active(),0)==NULL);
    assert(find_label(lv_screen_active(),"成就进度"));
    assert(!find_label(lv_screen_active(),"个人主页"));
    assert(!find_label(lv_screen_active(),"LV 12") && !find_label(lv_screen_active(),"ACTIVE"));
    ui_app_handle_input(UI_INPUT_BACK); advance(600);
    ui_app_handle_input(UI_INPUT_OPEN_SETTINGS); advance(600);
    assert(ui_router_current()==UI_ROUTE_SETTINGS);capture("settings_initial");
    assert(lv_color_eq(lv_obj_get_style_bg_color(lv_screen_active(),0),lv_color_black()));
    assert(lv_obj_get_style_bg_image_src(lv_screen_active(),0)==NULL);
    lv_obj_t *list = find_label(lv_screen_active(), "WiFi");
    while(list && lv_obj_get_style_width(list, LV_PART_SCROLLBAR) != 4)
        list = lv_obj_get_parent(list);
    assert(list != NULL);
    assert(lv_obj_get_scrollbar_mode(list)==LV_SCROLLBAR_MODE_AUTO);
    assert(lv_obj_get_style_width(list,LV_PART_SCROLLBAR)==4);
    assert(lv_color_eq(lv_obj_get_style_bg_color(list,LV_PART_SCROLLBAR),
        lv_color_hex(0xFFD900)));
    assert(find_label(lv_screen_active(),"设备、版本与存储"));
    assert(find_label(lv_screen_active(),"心率、功率区间阈值"));
    capture("settings_top");
    lv_obj_t *wifi_row=lv_obj_get_parent(lv_obj_get_parent(find_label(lv_screen_active(),"WiFi")));
    assert(lv_obj_get_height(wifi_row)>=110);
    assert(find_label(lv_screen_active(),"1-5 / 15  向上滑动查看更多"));
    ui_app_handle_input(UI_INPUT_ENCODER_CW);
    for(unsigned i=0;i<12;i++){advance(25);char name[48];snprintf(name,sizeof(name),"settings_liquid_%02u",i);capture(name);}
    ui_app_handle_input(UI_INPUT_ENCODER_CCW);
    for(unsigned i=0;i<12;i++){advance(25);char name[48];snprintf(name,sizeof(name),"settings_reverse_%02u",i);capture(name);}advance(500);
    ui_app_state_t before,after; assert(ui_app_state_get(&before));
    touch(240,744,true);
    for(int y=724;y>=284;y-=20) touch(240,y,true);
    touch(240,284,false); advance(600);
    assert(lv_obj_get_scroll_y(list)>100 && ui_router_current()==UI_ROUTE_SETTINGS);
    capture("settings_middle");
    assert(ui_app_state_get(&after));
    assert(before.wifi_enabled==after.wifi_enabled && before.bluetooth_enabled==after.bluetooth_enabled);
    assert(before.power_save_enabled==after.power_save_enabled && before.touch_locked==after.touch_locked);
    for(unsigned i=0;i<16;++i) ui_app_handle_input(UI_INPUT_ENCODER_CW);
    advance(600);
    assert(lv_obj_get_scroll_bottom(list)<=1);
    capture("settings_bottom");
    for(unsigned i=0;i<16;++i) ui_app_handle_input(UI_INPUT_ENCODER_CCW);
    advance(600); assert(lv_obj_get_scroll_y(list)==0);
    /* INFO rows are readable, with no false action/chevron. */
    tap_label(lv_screen_active(),"系统信息",true); advance(600);
    assert(ui_router_current()==UI_ROUTE_SYSTEM);
    capture("system_information");
    ui_app_handle_input(UI_INPUT_BACK); advance(600);
    assert(ui_router_current()==UI_ROUTE_SETTINGS);
    ui_app_handle_input(UI_INPUT_BACK); advance(600);
    assert(ui_router_current()==UI_ROUTE_HOME);
}
static void test_dashboard_variants(void)
{
    apply_preset(UI_DASHBOARD_TRAINING); capture("riding_training");
    apply_preset(UI_DASHBOARD_CLIMB); capture("riding_climb");
    ui_preferences_t preferences; assert(ui_preferences_get(&preferences));
    ui_dashboard_preset_fields(UI_DASHBOARD_CRUISE,preferences.dashboard_fields);
    preferences.dashboard_fields[0]=UI_RIDE_FIELD_GRADE;
    preferences.dashboard_fields[3]=UI_RIDE_FIELD_HEART_RATE;
    preferences.dashboard_preset=UI_DASHBOARD_CUSTOM;
    assert(ui_preferences_submit(&preferences)); advance(100); capture("riding_custom");
    lv_obj_t *hr=find_label(ui_dashboard_get_screen(),"心率"); assert(hr);
    lv_obj_t *hr_card=lv_obj_get_parent(hr);
    assert(lv_obj_get_y(hr_card)>400 && lv_obj_get_x(hr_card)>240);
    assert(lv_obj_get_style_bg_opa(hr_card,0)==32);
    lv_obj_t *grade=find_label(ui_dashboard_get_screen(),"坡度"); assert(grade);
    assert(lv_obj_get_style_bg_opa(lv_obj_get_parent(grade),0)==LV_OPA_TRANSP);
    apply_preset(UI_DASHBOARD_CRUISE);

    assert(ui_dynamic_card_manager_get_extended_page()==0);
    tap_label(ui_dashboard_get_screen(),"DATA 1/2",false);
    for(unsigned i=0;i<10;++i) {
        advance(20); char name[32]; snprintf(name,sizeof(name),"dataflip_%02u",i); capture(name);
    }
    assert(ui_dynamic_card_manager_get_extended_page()==1); capture("extended_page2");
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
    assert(ui_dynamic_card_manager_get_mode()==UI_DYNAMIC_CARD_CONTROL);
    ui_app_handle_input(UI_INPUT_ENCODER_CW); advance(200);
    assert(ui_dynamic_card_manager_get_extended_page()==0);
    assert(ui_dynamic_card_manager_get_active_card()==UI_DYNAMIC_CARD_EXTENDED_DATA);
    ui_app_handle_input(UI_INPUT_BACK);
    assert(ui_dynamic_card_manager_get_mode()==UI_DYNAMIC_CARD_BROWSER);
    /* A native vertical card drag must scroll to navigation, never act as a tap. */
    touch(300,790,true);
    for(int y=770;y>=650;y-=20) touch(300,y,true);
    touch(300,650,false); advance(600);
    assert(ui_dynamic_card_manager_get_extended_page()==0);
    assert(ui_dynamic_card_manager_get_active_card()==UI_DYNAMIC_CARD_NAVIGATION);
    ui_app_handle_input(UI_INPUT_ENCODER_CCW); advance(600);
    assert(ui_dynamic_card_manager_get_active_card()==UI_DYNAMIC_CARD_EXTENDED_DATA);

    ui_ride_data_t real; assert(ui_ride_data_model_get(&real,NULL));
    static const float speeds[]={10,20,30,40,50,24.9f,25.0f};
    static const char *names[]={"speed_blue","speed_green","speed_yellow","speed_orange","speed_purple","speed_boundary_before","speed_boundary_after"};
    for(unsigned i=0;i<7;++i) {
        ui_ride_data_t empty={.quality_known=true}; ui_dashboard_set_data(&empty);
        ui_ride_data_t probe=real; probe.speed=speeds[i];
        ui_ride_data_set_quality(&probe,UI_RIDE_FIELD_SPEED,true,true,lv_tick_get(),1000,UI_DATA_SOURCE_DEMO);
        ui_dashboard_set_data(&probe); capture(names[i]);
        assert(lv_color_eq(lv_obj_get_style_bg_color(speed_segment(),0),lv_color_hex(ui_dashboard_speed_color(speeds[i]))));
    }
    ui_ride_data_t extreme=real;
    extreme.power=65535; extreme.distance=123456.78f; extreme.ride_seconds=UINT32_MAX;
    ui_dashboard_set_data(&extreme); capture("riding_long_values");
    static const char *titles[]={"功率","里程","骑行时间"};
    for(unsigned i=0;i<3;++i) {
        lv_obj_t *card=lv_obj_get_parent(find_label(ui_dashboard_get_screen(),titles[i]));
        lv_obj_t *value=lv_obj_get_child(card,1); lv_obj_update_layout(card);
        assert(lv_obj_get_x(value)>=0 && lv_obj_get_x(value)+lv_obj_get_width(value)<=lv_obj_get_width(card));
    }
    ui_ride_data_t empty={.quality_known=true}; ui_dashboard_set_data(&empty);
    ui_dynamic_card_manager_refresh_data(&empty);
    capture("riding_missing_data");
    ui_dashboard_set_data(&real);
    ui_dynamic_card_manager_refresh_data(&real);
}
static void assert_navigation_pair(const char *first,const char *second)
{
 lv_obj_t *a=lv_obj_get_parent(find_label(lv_screen_active(),first));
 lv_obj_t *b=lv_obj_get_parent(find_label(lv_screen_active(),second));
 lv_obj_update_layout(a);lv_obj_update_layout(b);
 lv_area_t aa,bb,dock,inner;lv_obj_get_coords(a,&aa);lv_obj_get_coords(b,&bb);lv_obj_get_coords(ui_bottom_surface_root(),&dock);
 lv_obj_get_coords(lv_obj_get_parent(a),&inner);
 assert(aa.y1==bb.y1&&aa.y2==bb.y2&&lv_area_get_height(&aa)==72);
 assert(aa.y1==inner.y1+UI_NAV_FIXED_DOCK_HEIGHT-84&&aa.y2<dock.y2);
 assert(aa.x2<bb.x1&&aa.x1>dock.x1&&bb.x2<dock.x2);
}
static void test_navigation_workflow(void)
{
 size_t before_records=record_count;
 ui_pc_navigation_register();advance(300);ui_app_handle_input(UI_INPUT_PAGE_NEXT);advance(400);
 assert(ui_router_current()==UI_ROUTE_NAVIGATION&&ui_nav_workspace_view()==UI_NAV_VIEW_CENTER);
 const ui_nav_map_t *map=ui_navigation_map();lv_obj_t *persistent=map->root;assert(map->safe_height==UI_NAV_MAP_HEIGHT);
 assert(!find_label(lv_screen_active(),"演示地图 · 非真实道路"));
 assert(ui_navigation_selected_card()==UI_NAV_CARD_MAP&&!ui_navigation_card_entered());
 uint8_t initial_zoom=map->zoom;
 ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(200);
 assert(ui_navigation_selected_card()==UI_NAV_CARD_DOCK&&!ui_navigation_card_entered()&&map->zoom==initial_zoom);
 ui_app_handle_input(UI_INPUT_ENCODER_PRESS);advance(300);
 assert(ui_navigation_card_entered());
 lv_obj_t *my=find_label(lv_screen_active(),"我的路线"),*recent=find_label(lv_screen_active(),"最近使用");
 assert(lv_obj_get_style_border_width(lv_obj_get_parent(my),0)==0);
 assert(lv_color_eq(lv_obj_get_style_text_color(my,0),lv_color_hex(0x101010)));
 /* Search is now the first focusable dock control, before the route pair. */
 ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(700);
 ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(70);
 lv_obj_t *selection=lv_obj_get_child(lv_obj_get_parent(lv_obj_get_parent(my)),1);lv_obj_update_layout(selection);
 assert(lv_obj_get_width(selection)>lv_obj_get_width(lv_obj_get_parent(my)));capture("nav_capsule_switch");
 ui_app_handle_input(UI_INPUT_ENCODER_CCW);advance(40);capture("nav_capsule_reverse");
 ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(700);
 assert(lv_color_eq(lv_obj_get_style_text_color(recent,0),lv_color_hex(0x101010)));
 assert(lv_color_eq(lv_obj_get_style_text_color(my,0),lv_color_hex(0x101010)));
 assert(lv_obj_get_x(selection)==lv_obj_get_x(lv_obj_get_parent(recent)));
 assert_navigation_pair("我的路线","最近使用");capture("nav_standby");
 tap_label(lv_screen_active(),"最近使用",false);advance(450);
 assert(ui_router_current()==UI_ROUTE_SAVED_ROUTES&&find_label(lv_screen_active(),"暂无最近使用的路线"));capture("nav_recent_empty");
 ui_app_handle_input(UI_INPUT_BACK);advance(400);tap_label(lv_screen_active(),"我的路线",false);advance(500);
 assert(ui_router_current()==UI_ROUTE_SAVED_ROUTES);capture("nav_routes");
 lv_obj_t *maps_button=lv_obj_get_parent(find_label(lv_screen_active(),"离线地图"));
 lv_obj_t *refresh_button=lv_obj_get_parent(find_label(lv_screen_active(),"刷新"));
 lv_obj_t *toolbar=lv_obj_get_parent(maps_button),*footer=lv_obj_get_parent(toolbar);
 lv_obj_update_layout(footer);lv_area_t footer_bounds;lv_obj_get_coords(footer,&footer_bounds);
 assert(lv_area_get_height(&footer_bounds)==UI_NAV_FIXED_DOCK_HEIGHT&&footer_bounds.y1==UI_NAV_DATA_TOP&&footer_bounds.y2==837);
 assert(lv_obj_get_height(maps_button)==72&&lv_obj_get_style_border_width(maps_button,0)==0);
 for(unsigned i=0;i<3;i++)ui_app_handle_input(UI_INPUT_ENCODER_CW);
 advance(700);ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(70);capture("nav_resources_capsule_switch");
 lv_obj_t *resource_capsule=lv_obj_get_child(toolbar,0);lv_obj_update_layout(resource_capsule);
 assert(lv_obj_get_width(resource_capsule)>lv_obj_get_width(maps_button));
 ui_app_handle_input(UI_INPUT_ENCODER_CCW);advance(40);capture("nav_resources_capsule_reverse");
 ui_app_handle_input(UI_INPUT_ENCODER_CW);ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(700);
 ui_app_handle_input(UI_INPUT_ENCODER_PRESS);advance(500);
 assert(ui_router_current()==UI_ROUTE_SAVED_ROUTES&&!ui_nav_session_active());
 assert(lv_obj_get_x(resource_capsule)==lv_obj_get_x(refresh_button));
 ui_app_handle_input(UI_INPUT_ENCODER_PRESS);advance(500);assert(ui_router_current()==UI_ROUTE_SAVED_ROUTES);capture("nav_resources_refreshed");
 tap_label(lv_screen_active(),"城市骑行",false);advance(160);
 assert(ui_nav_session_busy()&&!ui_nav_session_active());capture("nav_preparing");
 ui_app_handle_input(UI_INPUT_BACK);advance(700);assert(!ui_nav_session_busy()&&!ui_nav_session_active());
 tap_label(lv_screen_active(),"城市骑行",false);advance(1000);
 assert(ui_nav_workspace_view()==UI_NAV_VIEW_PREVIEW&&!ui_nav_session_active());
 assert_navigation_pair("返回","开始导航");
 lv_area_t route_title,metrics;lv_obj_get_coords(find_label(lv_screen_active(),"城市骑行"),&route_title);
 lv_obj_get_coords(find_label(lv_screen_active(),"总距离"),&metrics);assert(route_title.y2<metrics.y1);
 capture("nav_preview");
 assert(ui_navigation_map()->root==persistent);
 tap_label(lv_screen_active(),"开始导航",false);advance(70);
 assert(!ui_nav_session_active()&&ui_state_machine_state()==UI_RIDE_STATE_PRE_RIDE);capture("nav_ride_preparing");
 ui_app_handle_input(UI_INPUT_BACK);advance(4000);
 assert(ui_router_current()==UI_ROUTE_HOME&&ui_state_machine_state()==UI_RIDE_STATE_HOME);
 assert(!ui_nav_session_active()&&!ui_ride_summary_model_active()&&record_count==before_records);
 ui_app_handle_input(UI_INPUT_PAGE_NEXT);advance(400);
 assert(ui_nav_workspace_view()==UI_NAV_VIEW_PREVIEW);
 tap_label(lv_screen_active(),"开始导航",false);advance(1800);
 assert(ui_state_machine_state()==UI_RIDE_STATE_GPS_READY&&!ui_nav_session_active());capture("nav_ride_ready");
 assert(find_label(lv_screen_active(),"GO")&&find_label(lv_screen_active(),"取消"));
 tap_label(lv_screen_active(),"GO",false);advance(70);
 ui_app_handle_input(UI_INPUT_BACK); /* Recording ACK pending: do not cancel as preparation. */
 advance(2400);assert(ui_state_machine_state()==UI_RIDE_STATE_RIDING&&ui_nav_session_active()&&ui_nav_workspace_view()==UI_NAV_VIEW_LIVE);
 ui_ride_summary_t ride_session;assert(ui_ride_summary_model_get(&ride_session));uint32_t ride_id=ride_session.session_id;
 capture("nav_guidance");
 lv_obj_update_layout(map->root);lv_area_t original_map;lv_obj_get_coords(map->root,&original_map);
 int32_t dock_height=lv_obj_get_height(ui_bottom_surface_root());assert(dock_height==UI_NAV_FIXED_DOCK_HEIGHT);
 assert(ui_navigation_selected_card()==UI_NAV_CARD_MAP&&!ui_navigation_card_entered());
 ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(200);
 assert(ui_navigation_selected_card()==UI_NAV_CARD_DOCK&&!ui_navigation_card_entered());
 ui_app_handle_input(UI_INPUT_ENCODER_CCW);advance(200);
 assert(ui_navigation_selected_card()==UI_NAV_CARD_MAP&&!ui_navigation_card_entered());
 ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
 assert(ui_navigation_card_entered());
 for(unsigned i=0;i<UI_NAV_FOCUS_COUNT&&map->focused_control!=UI_NAV_FOCUS_ZOOM_IN;i++)ui_app_handle_input(UI_INPUT_ENCODER_CW);
 uint8_t before_zoom=map->zoom;ui_app_handle_input(UI_INPUT_ENCODER_PRESS);advance(300);
 assert(map->zoom>before_zoom&&map->state==NAV_FOLLOW_USER_ZOOM);
 /* A stationary press must not create browsing mode. */
 lv_obj_send_event(map->root,LV_EVENT_PRESSED,NULL);lv_obj_send_event(map->root,LV_EVENT_PRESSING,NULL);lv_obj_send_event(map->root,LV_EVENT_RELEASED,NULL);
 assert(map->state==NAV_FOLLOW_USER_ZOOM);
 ui_pc_navigation_next_scenario();advance(400);capture("nav_approach");
 ui_pc_navigation_next_scenario();advance(400);capture("nav_near");
 ui_app_handle_input(UI_INPUT_PAGE_PREV);advance(300);assert(ui_router_current()==UI_ROUTE_RIDE_DASHBOARD&&ui_nav_session_active());
 ui_ride_summary_t accumulated;assert(ui_ride_summary_model_get(&accumulated)&&accumulated.session_id==ride_id&&accumulated.duration_seconds>ride_session.duration_seconds);
 ui_pc_navigation_next_scenario();advance(300);assert(ui_dock_current()&&ui_dock_current()->type==UI_DOCK_TYPE_NAVIGATION);capture("nav_cross_page");
 ui_app_handle_input(UI_INPUT_PAGE_NEXT);advance(400);
 touch(200,670,true);touch(200,584,true);touch(200,584,false);advance(600);
 assert(ui_nav_workspace_view()==UI_NAV_VIEW_MENU);capture("nav_quick_actions");
 lv_area_t dock_bounds,host_bounds;lv_obj_get_coords(ui_bottom_surface_root(),&dock_bounds);lv_obj_get_coords(lv_obj_get_parent(ui_bottom_surface_root()),&host_bounds);
 assert(dock_bounds.y2==837&&dock_bounds.y1>=host_bounds.y1&&dock_bounds.y2<=host_bounds.y2);
 lv_area_t rider;lv_obj_get_coords(map->position_marker,&rider);assert(rider.y1>=62&&rider.y2<dock_bounds.y1);
 touch(200,650,true);touch(200,732,true);touch(200,732,false);advance(600);assert(ui_nav_workspace_view()==UI_NAV_VIEW_LIVE);
 /* Hardware Back (also simulator right-click) exposes Stop without leaving navigation. */
 ui_app_handle_input(UI_INPUT_BACK);advance(600);assert(ui_nav_workspace_view()==UI_NAV_VIEW_MENU);
 assert(ui_nav_session_active()&&ui_navigation_selected_card()==UI_NAV_CARD_DOCK&&ui_navigation_card_entered());
 lv_area_t unchanged_map;lv_obj_get_coords(map->root,&unchanged_map);
 assert(memcmp(&original_map,&unchanged_map,sizeof(original_map))==0);
 assert(lv_obj_get_height(ui_bottom_surface_root())==dock_height);
 bool sound=ui_navigation_prompts_enabled();tap_label(lv_screen_active(),sound?"提示音：开":"提示音：关",false);advance(300);assert(ui_navigation_prompts_enabled()!=sound);
 tap_label(lv_screen_active(),"路线总览",false);advance(700);assert(ui_navigation_overview());capture("nav_overview");
 ui_app_handle_input(UI_INPUT_ENCODER_PRESS);advance(700);assert(!ui_navigation_overview());
 ui_navigation_open_menu();advance(300);tap_label(lv_screen_active(),"结束导航",false);advance(400);assert(ui_nav_workspace_view()==UI_NAV_VIEW_STOP);capture("nav_confirm");
 ui_app_handle_input(UI_INPUT_BACK);advance(300);assert(ui_nav_workspace_view()==UI_NAV_VIEW_MENU&&ui_nav_session_active());
 ui_app_handle_input(UI_INPUT_BACK);advance(300);assert(ui_nav_workspace_view()==UI_NAV_VIEW_LIVE);
 ui_navigation_open_maps();advance(500);assert(ui_nav_workspace_view()==UI_NAV_VIEW_MAPS);capture("nav_maps");
 tap_label(lv_screen_active(),"存储空间管理",false);advance(500);assert(find_label(lv_screen_active(),"存储容量暂不可用"));capture("nav_storage");
 ui_app_handle_input(UI_INPUT_BACK);advance(500);tap_label(lv_screen_active(),"本地地图",false);advance(400);assert(ui_nav_workspace_view()==UI_NAV_VIEW_MAP_DETAIL);capture("nav_map_detail");
 ui_app_handle_input(UI_INPUT_BACK);advance(500);ui_app_handle_input(UI_INPUT_BACK);advance(400);assert(ui_nav_workspace_view()==UI_NAV_VIEW_LIVE);
 ui_pc_navigation_next_scenario();advance(200);ui_pc_navigation_next_scenario();advance(200);capture("nav_roundabout");
 ui_pc_navigation_next_scenario();advance(300);assert(ui_nav_workspace_view()==UI_NAV_VIEW_ERROR);capture("nav_reroute_failed");
 ui_nav_session_t before,after;ui_nav_session_get(&before);tap_label(lv_screen_active(),"重试",false);advance(200);assert(ui_nav_session_busy());capture("nav_rerouting");
 advance(1000);ui_nav_session_get(&after);assert(after.route.revision>before.route.revision&&ui_nav_workspace_view()==UI_NAV_VIEW_LIVE);capture("nav_reroute_success");
 for(unsigned i=0;i<7;i++)ui_pc_navigation_next_scenario();advance(400);capture("nav_gnss_lost");
 ui_pc_navigation_next_scenario();advance(100);ui_pc_navigation_next_scenario();advance(400);
 assert(ui_nav_workspace_view()==UI_NAV_VIEW_ARRIVAL);assert_navigation_pair("查看地图","完成导航");capture("nav_arrival");
 tap_label(lv_screen_active(),"查看地图",false);advance(800);assert(ui_nav_workspace_view()==UI_NAV_VIEW_LIVE&&ui_navigation_overview());capture("nav_arrival_map");
 assert(ui_state_machine_state()==UI_RIDE_STATE_RIDING&&ui_ride_summary_model_active()); /* Arrival alone never stops recording. */
 tap_label(lv_screen_active(),"完成导航",false);advance(70);
 assert(ui_state_machine_state()==UI_RIDE_STATE_STOPPING&&ui_nav_session_active());
 event(UI_EVENT_RIDE_FINISH_CONFIRMED); /* Repeated input cannot submit a second finish. */
 advance(2200);
 assert(ui_state_machine_state()==UI_RIDE_STATE_SUMMARY&&!ui_nav_session_active()&&!ui_ride_summary_model_active());
 assert(record_count==before_records+1&&records[before_records].duration_seconds>0);
 capture("nav_arrival_saved");ui_app_handle_input(UI_INPUT_BACK);advance(400);assert(ui_router_current()==UI_ROUTE_HOME);
 /* Starting navigation during an existing paused ride reuses that recording. */
 ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);advance(1800);tap_label(lv_screen_active(),"GO",false);advance(2400);
 assert(ui_ride_summary_model_get(&accumulated));ride_id=accumulated.session_id;
 ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);advance(2400);assert(ui_state_machine_state()==UI_RIDE_STATE_PAUSED);
 ui_app_handle_input(UI_INPUT_PAGE_NEXT);advance(400);tap_label(lv_screen_active(),"我的路线",false);advance(500);
 tap_label(lv_screen_active(),"城市骑行",false);advance(1000);tap_label(lv_screen_active(),"开始导航",false);advance(1000);
 assert(ui_nav_session_active()&&ui_state_machine_state()==UI_RIDE_STATE_PAUSED);
 assert(ui_ride_summary_model_get(&accumulated)&&accumulated.session_id==ride_id);
 ui_app_handle_input(UI_INPUT_BACK);advance(400);tap_label(lv_screen_active(),"结束导航",false);advance(400);
 tap_label(lv_screen_active(),"取消",false);advance(300);
 assert(ui_state_machine_state()==UI_RIDE_STATE_PAUSED&&ui_nav_session_active());
 tap_label(lv_screen_active(),"结束导航",false);advance(400);
 ui_pc_services_set_next_command_result(false);tap_label(lv_screen_active(),"结束导航",false);advance(900);
 assert(ui_state_machine_state()==UI_RIDE_STATE_PAUSED&&ui_nav_session_active()&&record_count==before_records+1);
 assert(ui_ride_summary_model_get(&accumulated)&&accumulated.session_id==ride_id&&ui_ride_summary_model_active());
 capture("nav_finish_failed");ui_app_handle_input(UI_INPUT_BACK);advance(400);
 assert(ui_nav_workspace_view()==UI_NAV_VIEW_STOP);
 ui_pc_services_set_next_storage_result(false);tap_label(lv_screen_active(),"结束导航",false);advance(1800);
 assert(ui_state_machine_state()==UI_RIDE_STATE_UNSAVED&&!ui_nav_session_active()&&!ui_ride_summary_model_active());
 assert(ui_ride_history_repository_has_pending()&&record_count==before_records+1);
 capture("nav_save_failed");ui_app_handle_input(UI_INPUT_BACK);advance(300);
 assert(ui_state_machine_state()==UI_RIDE_STATE_UNSAVED);
 event(UI_EVENT_STORAGE_RETRY);advance(1800);
 assert(ui_state_machine_state()==UI_RIDE_STATE_SUMMARY&&!ui_nav_session_active());
 assert(record_count==before_records+2&&records[before_records+1].duration_seconds>0);
 capture("nav_ride_saved");ui_app_handle_input(UI_INPUT_BACK);advance(350);assert(ui_router_current()==UI_ROUTE_HOME);
 tap_label(lv_screen_active(),"收藏路线",false);advance(500);assert(ui_router_current()==UI_ROUTE_SAVED_ROUTES);
 capture("nav_saved_routes");tap_label(lv_screen_active(),"获取路线",false);advance(350);capture("nav_transfer_unavailable");
 ui_app_handle_input(UI_INPUT_PAGE_PREV);advance(350);assert(ui_router_current()==UI_ROUTE_HOME);
 assert(ui_state_machine_state()==UI_RIDE_STATE_HOME);
 assert(ui_nav_session_register_backend(NULL));assert(ui_nav_map_provider_register(NULL));
}

static void offline_settle(void)
{
 ui_pc_offline_navigation_set_replay_speed(0);
 for(unsigned i=0;i<300;i++){advance(10);
#ifdef _WIN32
  Sleep(2);
#endif
 }
 assert(!ui_nav_map_provider_busy());
}
static ui_nav_search_request_t alpha_request;
static unsigned alpha_requests;
static bool alpha_request_copy(const ui_nav_search_request_t *request,void *context)
{(void)context;alpha_request=*request;++alpha_requests;return true;}
static void alpha_submit(void)
{
 ui_place_results_t result={.request_id=alpha_request.id,.count=3};
 for(unsigned i=0;i<3;i++){
  result.items[i]=(ui_place_t){.id=100+i,.kind=2,.point={.latitude=34.26,.longitude=108.94,.system=UI_NAV_COORD_WGS84}};
  snprintf(result.items[i].name,64,"测试地点 %u",i+1);snprintf(result.items[i].detail,64,"地标");
 }
 assert(ui_nav_search_submit(&result));advance(100);
}
static void test_alpha_input(void)
{
 ui_nav_search_backend_t backend={.request=alpha_request_copy};assert(ui_nav_search_register(&backend));
 assert(ui_router_replace(UI_ROUTE_NAVIGATION,UI_MOTION_PAGE_NONE));
 for(unsigned i=0;i<8&&!ui_nav_workspace_map_visible();i++)assert(ui_navigation_handle_back());
 assert(ui_nav_workspace_map_visible());ui_navigation_open_search();advance(700);
 assert(ui_nav_search_panel_active()&&!ui_nav_search_panel_query()[0]);
 assert(ui_navigation_map()->safe_height==UI_NAV_SEARCH_MAP_IDLE_HEIGHT);capture("alpha_empty");
 lv_obj_t *delete_text=find_label(lv_screen_active(),"退格"),*delete_button=lv_obj_get_parent(delete_text);
 lv_obj_t *input_card=lv_obj_get_parent(delete_button),*search_root=lv_obj_get_parent(input_card),*candidate_card=lv_obj_get_child(search_root,1);
 assert(lv_obj_has_flag(candidate_card,LV_OBJ_FLAG_HIDDEN));
 assert(lv_obj_get_y(input_card)+lv_obj_get_y(search_root)==UI_NAV_SEARCH_INPUT_TOP);
 lv_obj_update_layout(delete_button);lv_area_t text_bounds,button_bounds;
 lv_obj_get_coords(delete_text,&text_bounds);lv_obj_get_coords(delete_button,&button_bounds);
 assert(text_bounds.x1>=button_bounds.x1&&text_bounds.x2<=button_bounds.x2&&text_bounds.y1>=button_bounds.y1&&text_bounds.y2<=button_bounds.y2);
 assert(abs(text_bounds.y1+text_bounds.y2-button_bounds.y1-button_bounds.y2)<=2);
 /* Fast reversal/wrap and a press before animation completion commit the logical letter. */
 ui_app_handle_input(UI_INPUT_ENCODER_CCW);ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
 assert(strcmp(ui_nav_search_panel_query(),"z")==0);
 assert(lv_obj_has_flag(candidate_card,LV_OBJ_FLAG_HIDDEN));
 ui_app_handle_input(UI_INPUT_ENCODER_CW);ui_app_handle_input(UI_INPUT_ENCODER_CW);ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
 assert(strcmp(ui_nav_search_panel_query(),"zb")==0);advance(220);assert(alpha_requests==1);
 assert(lv_obj_has_flag(candidate_card,LV_OBJ_FLAG_HIDDEN)&&find_label(search_root,"查找中"));
 uint32_t old_request=alpha_request.id;alpha_submit();
 assert(ui_navigation_map()->safe_height>UI_NAV_SEARCH_MAP_HEIGHT&&ui_navigation_map()->safe_height<UI_NAV_SEARCH_MAP_IDLE_HEIGHT);
 assert(!lv_obj_has_flag(candidate_card,LV_OBJ_FLAG_HIDDEN)&&lv_obj_get_height(candidate_card)<UI_NAV_SEARCH_LIST_HEIGHT);
 assert(lv_obj_get_style_opa(candidate_card,0)>0&&lv_obj_get_style_opa(candidate_card,0)<LV_OPA_COVER);capture("alpha_candidates_opening");
 advance(320);assert(ui_navigation_map()->safe_height==UI_NAV_SEARCH_MAP_HEIGHT);capture("alpha_results_arrived");
 ui_app_handle_input(UI_INPUT_ENCODER_CW);ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
 assert(strcmp(ui_nav_search_panel_query(),"zbc")==0); /* Results must not steal letter focus. */
 lv_obj_t *stale=find_label(candidate_card,"测试地点 1");assert(stale&&!lv_obj_has_flag(lv_obj_get_parent(stale),LV_OBJ_FLAG_CLICKABLE));
 assert(!ui_nav_search_submit(&(ui_place_results_t){.request_id=old_request}));advance(220);alpha_submit();
 ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);assert(ui_state_machine_state()==UI_RIDE_STATE_HOME);
 assert(find_label(lv_screen_active(),"继续输入"));ui_app_handle_input(UI_INPUT_ENCODER_CW);capture("alpha_result_focus");
 ui_app_handle_input(UI_INPUT_BACK);assert(ui_nav_search_panel_active()&&strcmp(ui_nav_search_panel_query(),"zbc")==0);
 advance(150);tap_label(lv_screen_active(),"D",false);assert(strcmp(ui_nav_search_panel_query(),"zbcd")==0);
 /* One-pitch drag follows the finger; holding the same point cannot add movement or letters. */
 advance(180);touch(240,715,true);touch(156,715,true);touch(156,715,true);touch(156,715,false);
 assert(strcmp(ui_nav_search_panel_query(),"zbcd")==0);advance(180);ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
 assert(strcmp(ui_nav_search_panel_query(),"zbcde")==0);capture("alpha_after_swipe");
 ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(48);capture("alpha_dial_detent");
 ui_app_handle_input(UI_INPUT_ENCODER_CCW);advance(180);
 touch(240,715,true);touch(240,749,true);touch(240,749,false);assert(strcmp(ui_nav_search_panel_query(),"zbcde")==0);
 ui_app_handle_input(UI_INPUT_PAGE_PREV);assert(strcmp(ui_nav_search_panel_query(),"zbcd")==0);
 advance(220);alpha_submit();ui_app_handle_input(UI_INPUT_PAGE_NEXT);ui_app_handle_input(UI_INPUT_TOGGLE_CONTROL_CENTER);
 assert(find_label(lv_screen_active(),"查看结果")&&ui_state_machine_state()==UI_RIDE_STATE_HOME);
 /* Return exits without erasing; reopening retains the query and current letter. */
 ui_app_handle_input(UI_INPUT_BACK);advance(700);assert(!ui_nav_search_panel_active());
 ui_navigation_open_search();advance(700);assert(strcmp(ui_nav_search_panel_query(),"zbcd")==0);
 assert(lv_obj_has_flag(candidate_card,LV_OBJ_FLAG_HIDDEN)&&ui_navigation_map()->safe_height==UI_NAV_SEARCH_MAP_IDLE_HEIGHT);
 /* Empty/error completions remain compact, with status confined to the query. */
 assert(ui_nav_search_submit(&(ui_place_results_t){.request_id=alpha_request.id}));advance(400);
 assert(find_label(search_root,"无结果")&&lv_obj_has_flag(candidate_card,LV_OBJ_FLAG_HIDDEN));capture("alpha_no_results");
 ui_app_handle_input(UI_INPUT_ENCODER_PRESS);advance(220);
 assert(ui_nav_search_submit(&(ui_place_results_t){.request_id=alpha_request.id,.code=UI_NAV_NO_MAP}));advance(400);
 assert(find_label(search_root,"无索引")&&lv_obj_has_flag(candidate_card,LV_OBJ_FLAG_HIDDEN));
 ui_app_handle_input(UI_INPUT_PAGE_PREV);advance(220);alpha_submit();advance(45);
 capture("alpha_candidates_reverse"); /* Refresh also advances the pending LVGL animation tick. */
 int reversing_height=ui_navigation_map()->safe_height;
 size_t reversing_length=strlen(ui_nav_search_panel_query());
 ui_app_handle_input(UI_INPUT_ENCODER_PRESS);advance(60);
 assert(strlen(ui_nav_search_panel_query())==reversing_length+1);
 assert(ui_navigation_map()->safe_height>=reversing_height);capture("alpha_candidates_retracting");
 assert(lv_obj_get_y(input_card)+lv_obj_get_y(search_root)==UI_NAV_SEARCH_INPUT_TOP);
 advance(160);alpha_submit();advance(350);
 assert(!lv_obj_has_flag(candidate_card,LV_OBJ_FLAG_HIDDEN)&&ui_navigation_map()->safe_height==UI_NAV_SEARCH_MAP_HEIGHT);
 /* Reopening during candidate animation must restore one coherent geometry. */
 ui_navigation_close_search();advance(50);ui_navigation_open_search();advance(700);
 assert(ui_nav_search_panel_active()&&lv_obj_has_flag(candidate_card,LV_OBJ_FLAG_HIDDEN)&&ui_navigation_map()->safe_height==UI_NAV_SEARCH_MAP_IDLE_HEIGHT);
 ui_motion_set_reduced(true);alpha_submit();advance(20);
 assert(ui_navigation_map()->safe_height==UI_NAV_SEARCH_MAP_HEIGHT);
 ui_app_handle_input(UI_INPUT_PAGE_PREV);advance(20);
 assert(lv_obj_has_flag(candidate_card,LV_OBJ_FLAG_HIDDEN)&&ui_navigation_map()->safe_height==UI_NAV_SEARCH_MAP_IDLE_HEIGHT);
 ui_motion_set_reduced(false);
 for(unsigned i=0;i<70;i++)ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
 assert(strlen(ui_nav_search_panel_query())==UI_PLACE_QUERY_MAX-1);capture("alpha_long_query");
 for(unsigned i=0;i<UI_PLACE_QUERY_MAX;i++)ui_app_handle_input(UI_INPUT_PAGE_PREV);
 assert(!ui_nav_search_panel_query()[0]);ui_app_handle_input(UI_INPUT_BACK);advance(700);
 assert(!ui_nav_search_panel_active()&&ui_navigation_map()->safe_height==UI_NAV_MAP_HEIGHT);
 assert(ui_nav_search_register(NULL));assert(ui_router_replace(UI_ROUTE_HOME,UI_MOTION_PAGE_NONE));
}
static void test_ride_preparation_cancel(void)
{
 size_t before=record_count;assert(ui_router_current()==UI_ROUTE_HOME);
 ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);advance(400);
 assert(ui_state_machine_state()==UI_RIDE_STATE_PRE_RIDE&&find_label(lv_screen_active(),"取消"));
 tap_label(lv_screen_active(),"取消",false);advance(300);
 assert(ui_state_machine_state()==UI_RIDE_STATE_HOME&&ui_router_current()==UI_ROUTE_HOME);
 assert(!ui_ride_summary_model_active()&&record_count==before);
 ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);advance(1800);
 assert(ui_state_machine_state()==UI_RIDE_STATE_GPS_READY);capture("ride_prepare_cancel");
 assert(find_label(lv_screen_active(),"00:00:00")&&find_label(lv_screen_active(),"0.00"));
 tap_label(lv_screen_active(),"取消",false);advance(600);
 assert(ui_state_machine_state()==UI_RIDE_STATE_HOME&&ui_router_current()==UI_ROUTE_HOME);
 ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);advance(1800);
 ui_pc_services_set_next_command_result(false);tap_label(lv_screen_active(),"GO",false);advance(900);
 assert(ui_state_machine_state()==UI_RIDE_STATE_GPS_READY&&!ui_ride_summary_model_active());
 assert(ui_dock_current()&&strcmp(ui_dock_current()->primary_action,"GO")==0);
 capture("ride_start_failed");tap_label(lv_screen_active(),"取消",false);advance(600);
 assert(ui_state_machine_state()==UI_RIDE_STATE_HOME&&ui_router_current()==UI_ROUTE_HOME&&record_count==before);
}
static int compare_time(const void *a,const void *b)
{double x=*(const double *)a,y=*(const double *)b;return x<y?-1:x>y;}
static void offline_render_benchmark(void)
{
 double times[100];ui_nav_map_t *map=(ui_nav_map_t *)ui_navigation_map();
 for(unsigned i=0;i<100;i++){
  nav_map_move(map,i%2?2:-2,0,false);
  clock_t started=clock();lv_refr_now(lv_display_get_default());
  times[i]=(clock()-started)*1000./CLOCKS_PER_SEC;
 }
 qsort(times,100,sizeof(times[0]),compare_time);
 ui_pc_offline_performance_t performance;assert(ui_pc_offline_navigation_performance(&performance));
 printf("PC map refresh: p50=%.1f ms p95=%.1f ms max=%.1f ms; last tile job=%.2f ms, bytes=%u, hits=%u, misses=%u\n",
   times[50],times[95],times[99],performance.map_ms,performance.read_bytes,performance.cache_hits,performance.cache_misses);
}
static void test_offline_workflow(void)
{
 assert(ui_pc_offline_navigation_register("assets/maps/xian"));ui_pc_offline_navigation_set_epoch(1790992800);advance(6500);
 assert(ui_router_replace(UI_ROUTE_NAVIGATION,UI_MOTION_PAGE_NONE));offline_settle();
 const ui_nav_map_t *map=ui_navigation_map();assert(map->provider_ready&&!map->detail_frame.demo&&map->raster_valid);capture("xian_standby");
 /* Real pointer samples, including a stationary hold, must not repeat deltas. */
 ui_nav_map_t *mutable_map=(ui_nav_map_t *)map;mutable_map->state=NAV_EXPLORE;
 ui_nav_point_t anchor,under;touch(140,350,true);assert(ui_nav_map_point_at(map,140,350,&anchor));
 touch(156,350,true);advance(160);touch(172,350,true);advance(160);
 assert(ui_nav_map_point_at(map,172,350,&under));
 double resolution=map->meters_per_pixel*256./map->render_scale;
 fprintf(stderr,"Drag error %.3f m, resolution %.3f, displacement=%ld,%ld, content=%ld,%ld\n",hypot((under.latitude-anchor.latitude)*111319.,(under.longitude-anchor.longitude)*92000.),resolution,(long)map->drag_x,(long)map->drag_y,(long)lv_obj_get_x(map->map_content),(long)lv_obj_get_y(map->map_content));
 assert(hypot((under.latitude-anchor.latitude)*111319.,(under.longitude-anchor.longitude)*92000.)<resolution*2.1);
 touch(172,350,false);advance(250);assert(ui_nav_map_point_at(map,172,350,&under));
 assert(hypot((under.latitude-anchor.latitude)*111319.,(under.longitude-anchor.longitude)*92000.)<resolution*2.1);
 ui_nav_map_set_focus(mutable_map,UI_NAV_FOCUS_LOCATE,true);ui_nav_map_activate_focus(mutable_map);offline_settle();capture("xian_drag_stopped");
 tap_label(lv_screen_active(),"我的路线",false);advance(300);assert(ui_router_current()==UI_ROUTE_SAVED_ROUTES);capture("xian_routes");
 tap_label(lv_screen_active(),"西安城区骑行",false);offline_settle();assert(ui_nav_workspace_view()==UI_NAV_VIEW_PREVIEW);assert(map->detail_frame.point_count>2);capture("xian_preview");
 tap_label(lv_screen_active(),"开始导航",false);advance(1800);
 assert(ui_state_machine_state()==UI_RIDE_STATE_GPS_READY&&!ui_nav_session_active());
 tap_label(lv_screen_active(),"GO",false);offline_settle();assert(ui_nav_session_active()&&ui_state_machine_state()==UI_RIDE_STATE_RIDING);capture("xian_follow");
 ui_pc_offline_navigation_set_replay_speed(20);advance(8000);offline_settle();capture("xian_junction");
 ui_app_handle_input(UI_INPUT_BACK);advance(300);assert(ui_nav_workspace_view()==UI_NAV_VIEW_MENU);capture("xian_actions");
 tap_label(lv_screen_active(),"路线总览",false);offline_settle();capture("xian_overview");
 ui_navigation_open_maps();advance(400);tap_label(lv_screen_active(),"西安城区基础地图",false);advance(300);capture("xian_map_detail");
 ui_app_handle_input(UI_INPUT_BACK);advance(300);ui_app_handle_input(UI_INPUT_BACK);advance(300);
 ui_pc_offline_navigation_set_replay_speed(100);advance(30000);offline_settle();assert(ui_nav_workspace_view()==UI_NAV_VIEW_ARRIVAL);capture("xian_arrived");
 assert(ui_state_machine_state()==UI_RIDE_STATE_RIDING&&ui_ride_summary_model_active());
 tap_label(lv_screen_active(),"完成导航",false);offline_settle();assert(!ui_nav_session_active());
 assert(ui_state_machine_state()==UI_RIDE_STATE_SUMMARY&&record_count==1&&!ui_ride_summary_model_active());capture("xian_arrival_saved");
 ui_app_handle_input(UI_INPUT_BACK);advance(400);assert(ui_router_current()==UI_ROUTE_HOME);
 ui_app_handle_input(UI_INPUT_PAGE_NEXT);offline_settle();assert(ui_nav_workspace_view()==UI_NAV_VIEW_CENTER);
 /* A real pointer long press selects the geographic point under the map. */
 ui_nav_point_t picked;touch(170,310,true);assert(ui_nav_map_point_at(map,170,310,&picked));
 assert(map->pointer_down);
 double pick_resolution=map->meters_per_pixel*256./map->render_scale;
 advance(700);touch(170,310,false);advance(100);
 assert(ui_nav_workspace_view()==UI_NAV_VIEW_DESTINATION&&map->pick_point_valid);
 fprintf(stderr,"Selected coordinates drift: %.3f m north, %.3f m east\n",(map->pick_point.latitude-picked.latitude)*111319.,(map->pick_point.longitude-picked.longitude)*92000.);
 assert(hypot((map->pick_point.latitude-picked.latitude)*111319.,(map->pick_point.longitude-picked.longitude)*92000.)<pick_resolution*2);
 capture("xian_pick_destination");tap_label(lv_screen_active(),"规划路线",false);offline_settle();
 ui_nav_session_t planned;ui_nav_session_get(&planned);assert(planned.state==UI_NAV_SESSION_PREVIEW&&planned.route.id==2);
 assert(planned.route.distance_m>10&&planned.route.point.system==UI_NAV_COORD_WGS84);capture("xian_picked_preview");
 ui_pc_offline_performance_t performance;assert(ui_pc_offline_navigation_performance(&performance));
 printf("PC picked route: %.2f ms, %u expanded nodes, %u bytes workspace, distance=%u m\n",performance.planning_ms,performance.planning_expanded,performance.planning_workspace_bytes,planned.route.distance_m);
 tap_label(lv_screen_active(),"开始导航",false);advance(1800);
 assert(ui_state_machine_state()==UI_RIDE_STATE_GPS_READY&&!ui_nav_session_active());
 tap_label(lv_screen_active(),"GO",false);offline_settle();assert(ui_nav_session_active());capture("xian_picked_follow");
 assert(!ui_nav_map_select_at((ui_nav_map_t *)map,170,310));offline_render_benchmark();
 ui_app_handle_input(UI_INPUT_BACK);advance(300);tap_label(lv_screen_active(),"结束导航",false);advance(400);
 assert(ui_nav_workspace_view()==UI_NAV_VIEW_STOP);tap_label(lv_screen_active(),"结束导航",false);offline_settle();
 assert(!ui_nav_session_active());
 assert(ui_state_machine_state()==UI_RIDE_STATE_SUMMARY&&record_count==2&&!ui_ride_summary_model_active());capture("xian_ride_saved");
 ui_app_handle_input(UI_INPUT_BACK);advance(400);ui_app_handle_input(UI_INPUT_PAGE_NEXT);offline_settle();
 /* Encoder can select the map's center crosshair without touching the dock. */
 ui_nav_map_set_focus((ui_nav_map_t *)map,UI_NAV_FOCUS_PICK,true);ui_nav_map_activate_focus((ui_nav_map_t *)map);advance(400);
 assert(ui_nav_workspace_view()==UI_NAV_VIEW_DESTINATION&&map->pick_point_valid);capture("xian_pick_crosshair");tap_label(lv_screen_active(),"返回地图",false);advance(400);assert(!map->pick_mode);
 double before_zoom=map->meters_per_pixel*256./map->render_scale;
 ui_nav_map_handle_input((ui_nav_map_t *)map,-1,false);offline_settle();
 double after_zoom=map->meters_per_pixel*256./map->render_scale;assert(after_zoom>before_zoom*1.35&&after_zoom<before_zoom*1.65);
 capture("xian_zoom_out");ui_nav_map_handle_input((ui_nav_map_t *)map,1,false);offline_settle();
 assert(fabs(map->meters_per_pixel*256./map->render_scale-before_zoom)<before_zoom*.1);capture("xian_zoom_in");
 /* Alphabet strip -> real POI -> automatic graph plan, no preset route. */
 tap_label(lv_screen_active(),"搜索地点",false);
 for(unsigned i=0;i<12;i++){advance(25);char name[64];snprintf(name,sizeof(name),"xian_search_open_%02u",i);capture(name);if(i==5)capture("xian_search_opening");}advance(700);
 assert(ui_nav_search_panel_active()&&map->safe_height==UI_NAV_SEARCH_MAP_IDLE_HEIGHT&&lv_obj_get_height(map->root)==UI_NAV_SEARCH_MAP_IDLE_HEIGHT&&lv_obj_get_y(map->root)==UI_NAV_SEARCH_MAP_TOP);capture("xian_search_alphabet");
 char current='a';const char *query="zhonglou";
 for(const char *p=query;*p;p++){
  int delta=(*p-current+26)%26;if(delta>13)delta-=26;
  for(int i=0;i<abs(delta);i++)ui_app_handle_input(delta>0?UI_INPUT_ENCODER_CW:UI_INPUT_ENCODER_CCW);
  for(unsigned frame=0;frame<3;frame++){advance(40);if(p==query+1){char name[64];snprintf(name,sizeof(name),"xian_dial_tick_%02u",frame);capture(name);}}
  advance(60);char key[2]={(char)(*p-32),0};tap_label(lv_screen_active(),key,false);current=*p;
 }
 assert(strcmp(ui_nav_search_panel_query(),query)==0);
 for(unsigned i=0;i<400;i++){advance(10);
#ifdef _WIN32
  Sleep(2);
#endif
 }
 ui_place_results_t results;bool search_busy;ui_nav_search_get(&results,NULL,&search_busy);assert(!search_busy&&results.code==UI_NAV_OK&&results.count);capture("xian_search_results");
 ui_app_handle_input(UI_INPUT_ENCODER_CW);advance(100);ui_app_handle_input(UI_INPUT_ENCODER_CCW);advance(100);
 tap_label(lv_screen_active(),"查看结果",false);advance(400);assert(ui_nav_search_panel_active());capture("xian_search_result_focus");
 unsigned chosen=0;for(unsigned i=0;i<results.count;i++)if(strstr(results.items[i].name,"钟楼")){chosen=i;break;}
 tap_label(lv_screen_active(),results.items[chosen].name,true);
 for(unsigned i=0;i<12;i++){advance(25);char name[64];snprintf(name,sizeof(name),"xian_search_close_%02u",i);capture(name);if(i==4)capture("xian_search_closing");}offline_settle();
 assert(!ui_nav_search_panel_active()&&map->safe_height==540&&lv_obj_get_height(map->root)==540&&lv_obj_get_y(map->root)==62);
 ui_nav_session_get(&planned);assert(planned.state==UI_NAV_SESSION_PREVIEW&&planned.route.id==2&&strstr(planned.route.name,"钟楼"));capture("xian_search_preview");
 ui_app_handle_input(UI_INPUT_BACK);advance(700);assert(ui_nav_search_panel_active()&&strcmp(ui_nav_search_panel_query(),query)==0);
 ui_nav_session_get(&planned);assert(planned.state==UI_NAV_SESSION_IDLE&&!planned.route.id);
 ui_app_handle_input(UI_INPUT_BACK);advance(700);assert(!ui_nav_search_panel_active()&&map->safe_height==540);
 ui_navigation_open_search();advance(90);ui_navigation_close_search();advance(60);ui_navigation_open_search();advance(800);assert(ui_nav_search_panel_active()&&map->safe_height>=UI_NAV_SEARCH_MAP_HEIGHT&&map->safe_height<=UI_NAV_SEARCH_MAP_IDLE_HEIGHT);ui_navigation_close_search();advance(800);assert(!ui_nav_search_panel_active()&&map->safe_height==540);
 ui_motion_set_reduced(true);ui_navigation_open_search();advance(100);assert(map->safe_height>=UI_NAV_SEARCH_MAP_HEIGHT&&map->safe_height<=UI_NAV_SEARCH_MAP_IDLE_HEIGHT);ui_navigation_close_search();advance(100);assert(map->safe_height==540&&!ui_nav_search_panel_active());ui_motion_set_reduced(false);
 assert(ui_state_machine_state()==UI_RIDE_STATE_HOME&&record_count==2&&!ui_ride_summary_model_active());
 assert(!render_failure);ui_pc_offline_navigation_shutdown();puts("ui_offline_navigation_integration: PASS");
}
int main(int argc, char **argv)
{
    bool offline=argc>1&&(strcmp(argv[1],"--offline")==0||strcmp(argv[1],"--offline-partial")==0);
    bool partial = argc > 1 && (strcmp(argv[1], "--partial") == 0||strcmp(argv[1],"--offline-partial")==0);
    if(partial && argc > 2) capture_directory=argv[2];
    else if(offline && argc>2)capture_directory=argv[2];
    else if(!partial && !offline && argc > 1) capture_directory=argv[1];
    lv_init();
    lv_log_register_print_cb(log_message);
    lv_display_t *display = lv_display_create(480, 854);
    assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL,
        partial ? 480 * 40 * 2 : sizeof(draw_buffer),
        partial ? LV_DISPLAY_RENDER_MODE_PARTIAL : LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    lv_sysmon_hide_performance(display);
    lv_indev_t *touch_device=lv_indev_create();
    lv_indev_set_type(touch_device,LV_INDEV_TYPE_POINTER); lv_indev_set_display(touch_device,display);
    lv_indev_set_read_cb(touch_device,read_pointer);
    ui_pc_services_register();
    ui_preferences_port_t preference_port={.load=preferences_load,.store=preferences_store,.request_worker=preferences_request};
    assert(ui_preferences_register(&preference_port));
    ui_pc_services_set_demo_seed(0xBC202610U);
    ui_ride_history_storage_t storage = {.load = load, .store = store, .now_utc = utc};
    assert(ui_ride_history_repository_register_storage(&storage));
    ui_app_init();
    if(offline){test_offline_workflow();return 0;}
    assert(ui_nav_session_register_backend(NULL));
    assert(ui_nav_map_provider_register(NULL));
    assert(ui_event_bus_subscribe(UI_EVENT_MASK(UI_EVENT_BOOT_COMPLETE), boot_completed, NULL) > 0);
    assert(ui_router_current() == UI_ROUTE_BOOT);
    ui_app_handle_back();
    ui_app_handle_input(UI_INPUT_OPEN_PROFILE);
    ui_app_handle_input(UI_INPUT_OPEN_SETTINGS);
    ui_app_handle_input(UI_INPUT_TOGGLE_CONTROL_CENTER);
    ui_input_pointer_event_t boot_pointer = { .type = UI_INPUT_TOUCH_BEGIN, .x = 240, .y = 10 };
    ui_app_handle_pointer(&boot_pointer);
    boot_pointer.type = UI_INPUT_TOUCH_MOVE; boot_pointer.y = 450;
    ui_app_handle_pointer(&boot_pointer);
    boot_pointer.type = UI_INPUT_TOUCH_END;
    ui_app_handle_pointer(&boot_pointer);
    assert(ui_router_current() == UI_ROUTE_BOOT);
    assert(ui_system_control_shade_state() == UI_SYSTEM_CONTROL_SHADE_CLOSED);
    for(unsigned i = 0; i < 50; ++i) {
        assert(ui_router_current() == UI_ROUTE_BOOT);
        assert(!ui_boot_page_is_complete());
        assert(boot_completions == 0);
        char name[32]; snprintf(name, sizeof(name), "boot_%02u", i); capture(name);
        if(i < 49) advance(100);
    }
    for(unsigned i = 0; i < 73; ++i) {
        if(i == 4) assert(ui_router_current() == UI_ROUTE_BOOT && !ui_boot_page_is_complete());
        if(i == 9) assert(ui_router_current() == UI_ROUTE_BOOT && ui_boot_page_is_complete());
        if(i == 9 || i == 30) {
            ui_route_id_t route = ui_router_current();
            if(i == 30) assert(route == UI_ROUTE_HOME);
            ui_app_handle_input(UI_INPUT_OPEN_SETTINGS);
            ui_app_handle_input(UI_INPUT_TOGGLE_CONTROL_CENTER);
            ui_app_handle_back();
            assert(ui_router_current() == route);
            assert(ui_system_control_shade_state() == UI_SYSTEM_CONTROL_SHADE_CLOSED);
        }
        if(i == 30) {
            pointer_position = (lv_point_t){400, 760};
            pointer_state = LV_INDEV_STATE_PRESSED;
        }
        if(i == 31) pointer_state = LV_INDEV_STATE_RELEASED;
        if(i == 32) assert(ui_router_current() == UI_ROUTE_HOME); /* Fade overlay consumes touch. */
        if(i == 30 || i == 50) assert(ui_home_page_is_entering());
        if(i == 68) assert(!ui_home_page_is_entering());
        char name[32]; snprintf(name, sizeof(name), "handoff_%02u", i); capture(name);
        if(i < 72) advance(20);
    }
    assert(ui_router_current() == UI_ROUTE_HOME && ui_boot_page_is_complete());
    assert(boot_completions == 1);
    assert(ui_state_machine_state() == UI_RIDE_STATE_HOME);
    capture("home");
    test_shell_pages();
    test_navigation_workflow();
    test_alpha_input();
    test_ride_preparation_cancel();
    size_t existing_records=record_count;
    ui_app_handle_input(UI_INPUT_OPEN_PROFILE);
    ui_app_handle_input(UI_INPUT_ENCODER_CW); /* Refresh precedes achievements. */
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
    assert(ui_router_current() == UI_ROUTE_ACHIEVEMENTS);
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
    assert(ui_achievements_page_detail_visible());
    ui_input_pointer_event_t modal_pointer = { .type = UI_INPUT_TOUCH_BEGIN, .x = 20, .y = 100 };
    ui_app_handle_pointer(&modal_pointer);
    modal_pointer.type = UI_INPUT_TOUCH_MOVE; modal_pointer.x = 450;
    ui_app_handle_pointer(&modal_pointer);
    assert(ui_router_current() == UI_ROUTE_ACHIEVEMENTS);
    assert(ui_achievements_page_detail_visible());
    ui_app_handle_input(UI_INPUT_BACK);
    assert(!ui_achievements_page_detail_visible());
    modal_pointer.type = UI_INPUT_TOUCH_MOVE; modal_pointer.x = 460;
    ui_app_handle_pointer(&modal_pointer);
    modal_pointer.type = UI_INPUT_TOUCH_END;
    ui_app_handle_pointer(&modal_pointer);
    assert(ui_router_current() == UI_ROUTE_ACHIEVEMENTS);
    ui_app_handle_input(UI_INPUT_BACK);
    ui_app_handle_input(UI_INPUT_BACK);
    assert(ui_router_current() == UI_ROUTE_HOME);
    ui_zone_editor_open();
    advance(300); capture("zone_editor");
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
    ui_app_handle_input(UI_INPUT_ENCODER_CW);
    ui_app_handle_input(UI_INPUT_BACK);
    assert(!ui_zone_editor_visible());
    /* Editing clamps at adjacent thresholds; cancelling preserves storage. */
    ui_preferences_t zones_before,zones_after;
    assert(ui_preferences_get(&zones_before));
    ui_zone_editor_open(); advance(200);
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS);
    for(unsigned i=0U;i<250U;++i) ui_app_handle_input(UI_INPUT_ENCODER_CW);
    char bound_text[24]; snprintf(bound_text,sizeof(bound_text),"%u bpm",zones_before.heart_zone_bounds[1]-1U);
    assert(find_label(lv_layer_top(),bound_text));
    ui_app_handle_input(UI_INPUT_BACK);
    assert(ui_preferences_get(&zones_after));
    assert(memcmp(zones_before.heart_zone_bounds,zones_after.heart_zone_bounds,
        sizeof(zones_before.heart_zone_bounds))==0);
    /* Touch selection must enter edit mode for that row, then save its draft. */
    ui_zone_editor_open(); advance(200);
    tap_label(lv_layer_top(),"心率 2",false);
    ui_app_handle_input(UI_INPUT_ENCODER_CW);
    tap_label(lv_layer_top(),"保存",false); advance(100);
    assert(!ui_zone_editor_visible());
    assert(ui_preferences_get(&zones_after));
    assert(zones_after.heart_zone_bounds[1]==zones_before.heart_zone_bounds[1]+1U);
    assert(ui_preferences_submit(&zones_before)); advance(100);
    test_layout_editor();
    ui_app_handle_input(UI_INPUT_TOGGLE_CONTROL_CENTER);
    advance(300);
    capture("compact");
    ui_app_handle_input(UI_INPUT_TOGGLE_CONTROL_CENTER);
    for(unsigned i=0;i<3;++i) { advance(20); char name[32]; snprintf(name,sizeof(name),"expand_%02u",i); capture(name); }
    advance(10);
    int32_t interrupted_height = ui_control_center_panel_height();
    assert(interrupted_height > UI_CONTROL_CENTER_COMPACT_HEIGHT);
    ui_app_handle_input(UI_INPUT_BACK);
    advance(500);
    assert(ui_motion_springs_running() == 0);
    ui_app_handle_input(UI_INPUT_TOGGLE_CONTROL_CENTER);
    advance(300);
    ui_app_handle_input(UI_INPUT_TOGGLE_CONTROL_CENTER);
    for(unsigned i=0;i<45;++i) { advance(20); char name[32]; snprintf(name,sizeof(name),"fluid_%02u",i); capture(name); }
    capture("expanded");
    ui_app_handle_input(UI_INPUT_BACK);
    advance(600);
    ui_route_id_t route_before_cancel=ui_router_current();
    ui_input_pointer_event_t pointer={.type=UI_INPUT_TOUCH_BEGIN,.x=400,.y=650};
    ui_app_handle_pointer(&pointer);
    pointer.type=UI_INPUT_TOUCH_MOVE; pointer.x=50; ui_app_handle_pointer(&pointer);
    pointer.type=UI_INPUT_TOUCH_CANCEL; ui_app_handle_pointer(&pointer);
    advance(300);
    assert(ui_router_current()==route_before_cancel);
    ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);
    advance(1800);
    assert(ui_state_machine_state() == UI_RIDE_STATE_GPS_READY);
    ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);
    advance(600);
    assert(ui_state_machine_state() == UI_RIDE_STATE_RIDING);
    assert(lv_color_eq(lv_obj_get_style_bg_color(lv_screen_active(),0),lv_color_black()));
    assert(lv_obj_get_style_bg_image_src(lv_screen_active(),0)==NULL);
    lv_obj_t *badge = find_label(lv_screen_active(), "骑行记录中");
    assert(badge != NULL && lv_obj_get_parent(badge) == lv_screen_active());
    assert(!lv_obj_has_flag(badge, LV_OBJ_FLAG_CLICKABLE));
    lv_obj_update_layout(lv_screen_active());
    lv_area_t badge_area;
    lv_obj_get_coords(badge, &badge_area);
    assert(badge_area.y1 >= 70 && badge_area.y2 < 122);
    assert(badge_area.x1 + badge_area.x2 == 479);
    capture("riding");
    advance(3000);
    capture("riding_steady");
    test_dashboard_variants();
    uint16_t hr_zones[4], power_zones[4];
    ui_ride_summary_model_get_zones(hr_zones, power_zones);
    uint16_t other_hr[4] = {100, 130, 160, 190}, other_power[4] = {100, 200, 300, 400};
    assert(!ui_ride_summary_model_set_zones(other_hr, other_power));
    uint16_t retained_hr[4], retained_power[4];
    ui_ride_summary_model_get_zones(retained_hr, retained_power);
    assert(memcmp(retained_hr, hr_zones, sizeof(hr_zones)) == 0);
    assert(memcmp(retained_power, power_zones, sizeof(power_zones)) == 0);
    ui_ride_summary_t before, after;
    assert(ui_ride_summary_model_get(&before));
    ui_app_handle_input(UI_INPUT_POWER_TOGGLE);
    advance(1500);
    assert(ui_ride_summary_model_get(&after));
    assert(after.duration_seconds > before.duration_seconds);
    assert(ui_state_machine_state() == UI_RIDE_STATE_RIDING);
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS); /* wake, consumes first press */
    ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);
    advance(600);
    assert(ui_state_machine_state() == UI_RIDE_STATE_PAUSED);
    capture("paused");
    ui_ride_summary_model_get(&before);
    advance(1600);
    ui_ride_summary_model_get(&after);
    assert(after.duration_seconds == before.duration_seconds);
    ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);
    advance(600);
    assert(ui_state_machine_state() == UI_RIDE_STATE_RIDING);
    ui_app_handle_input(UI_INPUT_BACK_LONG);
    assert(ui_state_machine_state() == UI_RIDE_STATE_FINISHING);
    ui_pc_services_set_next_storage_result(false);
    event(UI_EVENT_RIDE_FINISH_CONFIRMED);
    advance(1800);
    assert(ui_state_machine_state() == UI_RIDE_STATE_UNSAVED);
    assert(!ui_ride_summary_model_active());
    assert(ui_ride_history_repository_has_pending());
    capture("pending_save");
    ui_app_handle_input(UI_INPUT_BACK);
    assert(ui_state_machine_state() == UI_RIDE_STATE_UNSAVED);
    advance(250);
    capture("pending_save_after_back");
    assert(find_label(lv_screen_active(),"骑行已结束，记录尚未保存"));
    event(UI_EVENT_STORAGE_RETRY);
    advance(1800);
    assert(ui_state_machine_state() == UI_RIDE_STATE_SUMMARY);
    assert(!ui_ride_history_repository_has_pending());
    assert(record_count == existing_records+1 && records[existing_records].duration_seconds > 0);
    assert(records[existing_records].moving_time_seconds <= records[existing_records].duration_seconds);
    assert(find_label(lv_screen_active(),"骑行已结束，记录已保存"));
    assert(!find_label(lv_screen_active(),"骑行已结束，记录尚未保存"));
    capture("saved_report");
    for(unsigned i=0U;i<4U;++i) ui_app_handle_input(UI_INPUT_ENCODER_CW);
    advance(600); capture("report_middle");
    for(unsigned i=0U;i<24U;++i) ui_app_handle_input(UI_INPUT_ENCODER_CW);
    advance(600);
    assert(ui_ride_report_page_scroll_y(UI_RIDE_REPORT_SUMMARY_MODE) >=
        (int32_t)ui_ride_report_page_content_height(UI_RIDE_REPORT_SUMMARY_MODE)-684-1);
    capture("report_bottom");
    ui_app_handle_input(UI_INPUT_BACK);
    tap_label(lv_screen_active(),"历史记录",false); advance(600);
    assert(ui_router_current()==UI_ROUTE_HISTORY); capture("history_populated");
    ui_app_handle_input(UI_INPUT_BACK); advance(600);
    ui_pc_services_set_next_command_result(false);
    confirm_power();
    advance(600);
    assert(ui_router_current()!=UI_ROUTE_POWER_OFF); /* Failed shutdown never fakes power-off. */
    ui_app_handle_input(UI_INPUT_BACK);
    ui_app_handle_input(UI_INPUT_RIDE_TOGGLE);
    advance(1800);
    assert(ui_state_machine_state()==UI_RIDE_STATE_GPS_READY);
    ui_app_handle_input(UI_INPUT_RIDE_TOGGLE); /* Start is queued, not yet acknowledged. */
    confirm_power();
    advance(3500);
    assert(ui_router_current()==UI_ROUTE_POWER_OFF);
    assert(record_count==existing_records+2 && !ui_ride_summary_model_active());
    ui_app_handle_input(UI_INPUT_POWER_TOGGLE);
    assert(ui_router_current() == UI_ROUTE_BOOT && !ui_boot_page_is_complete());
    advance(1600);
    assert(ui_router_current() == UI_ROUTE_BOOT && boot_completions == 1);
    capture("boot_power_on");
    advance(4800);
    assert(ui_router_current() == UI_ROUTE_HOME && boot_completions == 2);

    confirm_restart();
    advance(600);
    assert(ui_router_current() == UI_ROUTE_BOOT && !ui_boot_page_is_complete());
    capture("boot_restart");
    advance(4000);
    assert(ui_router_current() == UI_ROUTE_BOOT && boot_completions == 2);
    advance(2000);
    assert(ui_router_current() == UI_ROUTE_HOME && boot_completions == 3);

    ui_preferences_t boot_preferences;
    assert(ui_preferences_get(&boot_preferences));
    boot_preferences.reduced_motion = true;
    assert(ui_preferences_submit(&boot_preferences));
    advance(100);
    confirm_restart();
    advance(600);
    assert(ui_router_current() == UI_ROUTE_HOME && ui_boot_page_is_complete());
    assert(!ui_home_page_is_entering());
    assert(boot_completions == 4);

    boot_preferences.reduced_motion = false;
    assert(ui_preferences_submit(&boot_preferences));
    advance(100);
    assert(ui_router_replace(UI_ROUTE_BOOT, UI_MOTION_PAGE_NONE));
    advance(1000);
    assert(ui_router_replace(UI_ROUTE_SETTINGS, UI_MOTION_PAGE_NONE));
    advance(5200);
    assert(ui_router_current() == UI_ROUTE_SETTINGS && boot_completions == 4);
    assert(!ui_boot_page_is_complete()); /* Hidden animation stays paused. */
    assert(ui_router_replace(UI_ROUTE_HOME, UI_MOTION_PAGE_NONE));
    for(unsigned i = 0; i < 2; ++i) {
        assert(ui_router_replace(UI_ROUTE_BOOT, UI_MOTION_PAGE_NONE));
        advance(i == 0 ? 5100 : 5500); /* Interrupt fade-out, then fade-in. */
        assert(ui_router_current() == (i == 0 ? UI_ROUTE_BOOT : UI_ROUTE_HOME));
        assert(ui_router_replace(UI_ROUTE_SETTINGS, UI_MOTION_PAGE_NONE));
        assert(!ui_home_page_is_entering());
        advance(600);
        ui_app_handle_input(UI_INPUT_OPEN_PROFILE);
        assert(ui_router_current() == UI_ROUTE_PROFILE); /* Overlay and input lock cleared. */
        assert(ui_router_replace(UI_ROUTE_HOME, UI_MOTION_PAGE_NONE));
        lv_obj_t *home_screen = ui_router_screen(UI_ROUTE_HOME);
        for(unsigned j = 0; j < lv_obj_get_child_count(home_screen); ++j) {
            lv_obj_t *component = lv_obj_get_child(home_screen, j);
            assert(lv_obj_get_style_translate_y(component, 0) == 0);
            assert(lv_obj_get_style_opa(component, 0) == LV_OPA_COVER);
        }
    }
    assert(!render_failure);
    puts("ui_app_integration_test: PASS");
    return 0;
}
