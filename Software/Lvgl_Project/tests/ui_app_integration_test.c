#include "ui/core/ui_app.h"
#include "ui/core/ui_app_state.h"
#include "ui/core/ui_state_machine.h"
#include "ui/core/ui_router.h"
#include "ui/core/ui_event_bus.h"
#include "ui/core/ui_motion.h"
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
    lv_obj_t *label=find_label(parent,text); assert(label);
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
    assert(find_label(lv_screen_active(),"暂无收藏路线"));
    capture("saved_routes_empty");
    ui_app_handle_input(UI_INPUT_ENCODER_PRESS); advance(250);
    assert(ui_router_current()==UI_ROUTE_HOME);
    tap_label(lv_screen_active(),"设备与传感器",false); advance(250);
    assert(ui_router_current()==UI_ROUTE_DEVICES);
    tap_label(lv_screen_active(),"搜索附近设备",false); advance(100);
    assert(find_label(lv_screen_active(),"暂无法扫描，蓝牙服务待接入") ||
           find_label(lv_screen_active(),"请先在系统设置中开启蓝牙"));
    capture("devices");
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
int main(int argc, char **argv)
{
    bool partial = argc > 1 && strcmp(argv[1], "--partial") == 0;
    if(partial && argc > 2) capture_directory=argv[2];
    else if(!partial && argc > 1) capture_directory=argv[1];
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
    assert(find_label(lv_screen_active(),"骑行已结束，记录尚未保存"));
    event(UI_EVENT_STORAGE_RETRY);
    advance(1800);
    assert(ui_state_machine_state() == UI_RIDE_STATE_SUMMARY);
    assert(!ui_ride_history_repository_has_pending());
    assert(record_count == 1 && records[0].duration_seconds > 0);
    assert(records[0].moving_time_seconds <= records[0].duration_seconds);
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
    assert(record_count==2 && !ui_ride_summary_model_active());
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
