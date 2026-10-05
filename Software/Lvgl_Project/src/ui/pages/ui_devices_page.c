#include "ui_devices_page.h"

#include "ui_page_common.h"
#include "../core/ui_motion.h"
#include "../core/ui_sizes.h"
#include "../core/ui_sound.h"
#include "../core/ui_text.h"
#include "../core/ui_theme.h"
#include "../components/ui_liquid_selection.h"
#include "../design/ui_design_tokens.h"

#include <string.h>
#include <stdlib.h>

#define DEVICE_ROW_COUNT (UI_DEVICE_COUNT + 1U) /* scan + five device types */
#define DEVICE_SCAN_ROW  0U

typedef struct
{
    lv_obj_t *screen;
    lv_obj_t *list;
    lv_obj_t *feedback;
    lv_obj_t *rows[DEVICE_ROW_COUNT];
    lv_obj_t *status[UI_DEVICE_COUNT];
    lv_obj_t *pairs[DEVICE_ROW_COUNT],*trailing[DEVICE_ROW_COUNT],*icons[DEVICE_ROW_COUNT];
    ui_liquid_selection_t liquid;bool touch_scrolling;
    ui_status_bar_t status_bar;
    ui_page_action_cb_t callback;
    void *user_data;
    int32_t selected;
    bool active;
} devices_context_t;

static devices_context_t s_devices;
static ui_device_connection_port_t s_connection;
void ui_devices_page_register_connection_port(const ui_device_connection_port_t *port)
{s_connection=port?*port:(ui_device_connection_port_t){0};}
bool ui_devices_page_request_scan(void)
{return s_connection.scan&&s_connection.scan(s_connection.context);}
bool ui_devices_page_request_selected(void)
{unsigned kind=ui_devices_page_selected_device();return kind<UI_DEVICE_COUNT&&s_connection.connect&&s_connection.connect((ui_device_kind_t)kind,s_connection.context);}
static void device_layout(lv_obj_t *object,int width,int height)
{
    (void)height;unsigned i=(unsigned)(uintptr_t)lv_obj_get_user_data(object);
    if(i>=DEVICE_ROW_COUNT)return;
    if(s_devices.pairs[i])lv_obj_set_width(s_devices.pairs[i],width-(i?202:164));
    if(i){ui_align_center_y(s_devices.status[i-1],width-124);ui_align_center_y(s_devices.trailing[i],width-22);}
    else ui_align_center_y(s_devices.trailing[i],width-78);
}

static const char *s_titles[UI_DEVICE_COUNT] = {
    "手机", "心率带", "手表", "功率计", "速度 / 踏频"
};
static const char *s_subtitles[UI_DEVICE_COUNT] = {
    "COMPANION PHONE", "HEART RATE SENSOR", "WATCH / HEAD UNIT",
    "POWER METER", "SPEED & CADENCE"
};
static const char *s_symbols[UI_DEVICE_COUNT] = {
    LV_SYMBOL_WIFI, LV_SYMBOL_GPS, LV_SYMBOL_BLUETOOTH,
    LV_SYMBOL_CHARGE, LV_SYMBOL_LOOP
};

static lv_obj_t *label(lv_obj_t *parent, int32_t x, int32_t y,
                       int32_t width, int32_t height, const char *text,
                       const lv_font_t *font, lv_color_t color,
                       lv_text_align_t align)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, LV_SIZE_CONTENT);
    lv_obj_set_y(object, y + (height - font->line_height) / 2);
    lv_label_set_text(object, text != NULL ? text : "");
    lv_label_set_long_mode(object, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_color(object, color, 0);
    lv_obj_set_style_text_align(object, align, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

static lv_obj_t *card(lv_obj_t *parent, int32_t x, int32_t y,
                      int32_t width, int32_t height)
{
    lv_obj_t *object = ui_page_box(parent, x, y, width, height,
                                    UI_TOKEN_COLOR_SURFACE_OUTLINE, 1);
    lv_obj_set_style_bg_color(object, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(object, UI_TOKEN_RADIUS_MEDIUM, 0);
    lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
    return object;
}

static void update_focus(void)
{
    ui_liquid_selection_select(&s_devices.liquid,s_devices.selected,true);
}

static void row_event(lv_event_t *event)
{
    unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    if(index >= DEVICE_ROW_COUNT) return;
    switch(lv_event_get_code(event)) {
        case LV_EVENT_CLICKED:
            s_devices.selected = (int32_t)index;
            update_focus();
            if(s_devices.callback != NULL)
                s_devices.callback(index==DEVICE_SCAN_ROW?UI_PAGE_ACTION_DEVICE_SCAN:UI_PAGE_ACTION_DEVICE_SELECT,
                                   s_devices.user_data);
            break;
        default: break;
    }
}

static void back_event(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED &&
       s_devices.callback != NULL)
        s_devices.callback(UI_PAGE_ACTION_BACK, s_devices.user_data);
}

static void list_scrolled(lv_event_t *event)
{
    lv_event_code_t code=lv_event_get_code(event);
    if(code==LV_EVENT_SCROLL_BEGIN){lv_indev_t *input=lv_indev_active();
        s_devices.touch_scrolling=input&&lv_indev_get_type(input)==LV_INDEV_TYPE_POINTER&&
            lv_indev_get_state(input)==LV_INDEV_STATE_PRESSED&&lv_indev_get_scroll_obj(input)==s_devices.list;}
    if(code==LV_EVENT_SCROLL_END&&s_devices.touch_scrolling){s_devices.touch_scrolling=false;
        int center=lv_obj_get_scroll_y(s_devices.list)+lv_obj_get_height(s_devices.list)/2,best=INT32_MAX;
        for(unsigned i=0;i<DEVICE_ROW_COUNT;i++){int d=abs(s_devices.liquid.items[i].y+UI_TOKEN_LIST_LARGE_ROW_HEIGHT/2-center);if(d<best){best=d;s_devices.selected=i;}}
        update_focus();
    }
}

lv_obj_t *ui_devices_page_create(ui_page_action_cb_t callback,
                                 void *user_data)
{
    memset(&s_devices, 0, sizeof(s_devices));
    s_devices.callback = callback;
    s_devices.user_data = user_data;
    s_devices.selected = 0;
    s_devices.screen = ui_page_screen_create();
    lv_obj_set_style_bg_color(s_devices.screen, UI_TOKEN_COLOR_CANVAS, 0);
    ui_page_status_create(&s_devices.status_bar, s_devices.screen);

    lv_obj_t *back = ui_page_box(s_devices.screen, 8, 62, 48, 42,
                                 UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, back_event, LV_EVENT_CLICKED, NULL);
    ui_align_label_center(label(back, 0, 0, 48, 42, LV_SYMBOL_LEFT,
          &lv_font_montserrat_28, UI_TOKEN_COLOR_TEXT_PRIMARY,
          LV_TEXT_ALIGN_CENTER));
    label(s_devices.screen, 58, 66, 230, 34, "连接设备",
          &ui_font_misans_ui_24, UI_TOKEN_COLOR_TEXT_PRIMARY,
          LV_TEXT_ALIGN_LEFT);
    label(s_devices.screen, 290, 72, 170, 22, "DEVICES", &lv_font_montserrat_16,
          UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_RIGHT);
    ui_page_line(s_devices.screen, UI_LEFT, 143, UI_CONTENT_W, 1,
                 UI_TOKEN_COLOR_DIVIDER);

    s_devices.list = lv_obj_create(s_devices.screen);
    lv_obj_remove_style_all(s_devices.list);
    lv_obj_set_pos(s_devices.list, 8, 156);
    lv_obj_set_size(s_devices.list, 464, UI_PAGE_LIST_WITH_FOOTNOTE_BOTTOM - 156);
    lv_obj_set_scroll_dir(s_devices.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_devices.list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(s_devices.list, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(s_devices.list, UI_TOKEN_COLOR_ACCENT,
                              LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(s_devices.list, LV_OPA_70, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(s_devices.list, LV_RADIUS_CIRCLE,
                            LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(s_devices.list, list_scrolled, LV_EVENT_ALL, NULL);
    lv_obj_clear_flag(s_devices.list,LV_OBJ_FLAG_SCROLL_MOMENTUM);
    ui_liquid_selection_init(&s_devices.liquid,s_devices.list,true,device_layout);
    ui_liquid_selection_set_wide_focus(&s_devices.liquid,true);

    lv_obj_t *scan = card(s_devices.list, 28,12,408,UI_TOKEN_LIST_LARGE_ROW_HEIGHT);lv_obj_set_user_data(scan,(void *)(uintptr_t)0);
    s_devices.rows[DEVICE_SCAN_ROW] = scan;
    lv_obj_t *scan_icon = label(scan, 16, 0, 48, 54, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_28,
          UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    ui_align_center_y(scan_icon, 16);
    lv_obj_t *scan_title = label(scan, 80, 10, 220, 26, "搜索附近设备", &ui_font_misans_ui_24,
          UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *scan_subtitle = label(scan, 80, 43, 250, 22, "扫描并配对蓝牙外设", &ui_font_misans_ui_20,
          UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_devices.pairs[0]=ui_align_text_pair(scan, scan_title, scan_subtitle, 80, 240, 8);
    lv_obj_t *scan_value = label(scan, 330, 0, 70, 24, "扫描", &ui_font_misans_ui_20,
          UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    ui_align_center_y(scan_value, 330);
    s_devices.trailing[0]=scan_value;
    lv_obj_add_event_cb(scan, row_event, LV_EVENT_ALL,
                        (void *)(uintptr_t)DEVICE_SCAN_ROW);

    for(unsigned i = 0U; i < UI_DEVICE_COUNT; ++i) {
        lv_obj_t *row = card(s_devices.list,28,12+(i+1)*(UI_TOKEN_LIST_LARGE_ROW_HEIGHT+UI_TOKEN_LIST_LARGE_ROW_GAP),408,UI_TOKEN_LIST_LARGE_ROW_HEIGHT);
        unsigned row_index = i + 1U;
        lv_obj_set_user_data(row,(void *)(uintptr_t)row_index);
        s_devices.rows[row_index] = row;
        lv_obj_t *icon = ui_page_box(row, 14, 14, 52, 52,
                                     UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
        lv_obj_set_style_bg_color(icon, UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
        lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(icon, 16, 0);
        ui_align_center_y(icon, 14);
        ui_align_label_center(label(icon, 0, 0, 52, 28, s_symbols[i], &lv_font_montserrat_24,
              UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER));
        lv_obj_t *title = label(row, 82, 10, 210, 26, s_titles[i], &ui_font_misans_ui_24,
              UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
        lv_obj_t *subtitle = label(row, 82, 44, 210, 22, s_subtitles[i], &lv_font_montserrat_12,
              UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
        s_devices.pairs[row_index]=ui_align_text_pair(row, title, subtitle, 82, 200, 8);
        s_devices.status[i] = label(row, 292, 20, 98, 24, "未连接",
                                    &ui_font_misans_ui_20,
                                    UI_TOKEN_COLOR_TEXT_SECONDARY,
                                    LV_TEXT_ALIGN_RIGHT);
        ui_align_center_y(s_devices.status[i], 292);
        lv_obj_t *arrow = label(row, 398, 0, 18, 26, LV_SYMBOL_RIGHT, &lv_font_montserrat_20,
              UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER);
        ui_align_center_y(arrow, 398);
        s_devices.trailing[row_index]=arrow;
        lv_obj_add_event_cb(row, row_event, LV_EVENT_ALL,
                            (void *)(uintptr_t)row_index);
    }

    s_devices.feedback = label(s_devices.screen, 20, UI_PAGE_FOOTNOTE_Y, 440, UI_PAGE_FOOTNOTE_HEIGHT,
          "支持手机与骑行传感器",
          &ui_font_misans_ui_20, UI_TOKEN_COLOR_TEXT_TERTIARY,
          LV_TEXT_ALIGN_CENTER);
    for(unsigned i=0;i<DEVICE_ROW_COUNT;i++)ui_liquid_selection_add(&s_devices.liquid,s_devices.rows[i]);
    update_focus();
    return s_devices.screen;
}

void ui_devices_page_set_active(bool active)
{
    s_devices.active = active;
    ui_liquid_selection_set_active(&s_devices.liquid,active);
    update_focus();
}

bool ui_devices_page_handle_input(ui_input_event_t input)
{
    if(!s_devices.active) return false;
    if(input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW) {
        int32_t next = s_devices.selected +
            (input == UI_INPUT_ENCODER_CW ? 1 : -1);
        next=LV_CLAMP(0,next,(int32_t)DEVICE_ROW_COUNT-1);
        s_devices.selected = next;
        update_focus();
        ui_sound_play(UI_SFX_FOCUS);
        return true;
    }
    if(input == UI_INPUT_ENCODER_PRESS && s_devices.selected >= 0) {
        if(s_devices.callback != NULL)
            s_devices.callback(s_devices.selected==DEVICE_SCAN_ROW?UI_PAGE_ACTION_DEVICE_SCAN:UI_PAGE_ACTION_DEVICE_SELECT,
                               s_devices.user_data);
        ui_sound_play(UI_SFX_PRESS);
        return true;
    }
    return false;
}

bool ui_devices_page_contains_scroll_point(int32_t x, int32_t y)
{
    if(s_devices.list == NULL) return false;
    lv_area_t area;
    lv_obj_get_coords(s_devices.list, &area);
    return x >= area.x1 && x <= area.x2 && y >= area.y1 && y <= area.y2;
}

unsigned ui_devices_page_selected_device(void)
{
    return s_devices.selected > 0 ? (unsigned)(s_devices.selected - 1) : UI_DEVICE_COUNT;
}

void ui_devices_page_set_status(ui_device_kind_t kind, const char *status,
                                uint32_t color)
{
    if(kind >= UI_DEVICE_COUNT || s_devices.status[kind] == NULL ||
       status == NULL) return;
    (void)ui_text_set_label(s_devices.status[kind], status, 20);
    ui_liquid_selection_color(&s_devices.liquid,s_devices.status[kind],lv_color_hex(color));
}

void ui_devices_page_set_feedback(const char *message)
{
    if(s_devices.feedback == NULL || message == NULL) return;
    (void)ui_text_set_label(s_devices.feedback, message, 20);
    lv_obj_set_style_text_color(s_devices.feedback, UI_TOKEN_COLOR_ACCENT, 0);
}
