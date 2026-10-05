#include "ui_home_page.h"
#include "../core/ui_text.h"
#include "../core/ui_sound.h"
#include "../core/ui_motion.h"
#include "../components/ui_liquid_selection.h"
#include "../components/navigation/ui_nav_visual.h"

#include "ui_page_common.h"
#include "../core/ui_colors.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"
#include "../models/ui_home_data_model.h"
#include "../assets/ui_home_watermark.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HOME_ACTION_COUNT 9
#define HOME_ENTER_COMPONENT_COUNT 10
#define HOME_ENTER_DURATION_MS 360U
#define HOME_ENTER_STEP_MS 60U

typedef struct
{
    lv_obj_t *screen;
    ui_liquid_selection_t selection;lv_obj_t *refresh_icon;
    ui_status_bar_t status;
    lv_obj_t *actions[HOME_ACTION_COUNT];
    ui_page_action_t action_ids[HOME_ACTION_COUNT];
    lv_obj_t *user_name;
    lv_obj_t *tagline;
    lv_obj_t *avatar_text;
    lv_obj_t *recent_status;
    lv_obj_t *total_distance;
    lv_obj_t *distance_unit;
    lv_obj_t *total_time;
    lv_obj_t *total_ascent;
    lv_obj_t *last_date;
    lv_obj_t *last_distance;
    lv_obj_t *last_duration;
    lv_obj_t *last_ascent;
    lv_obj_t *last_caption;
    lv_obj_t *recent_route;
    lv_obj_t *recent_start;
    ui_page_action_cb_t callback;
    void *user_data;
    int32_t model_subscription;
    int8_t focus_index;
    bool active, refreshing;
    lv_obj_t *enter_components[HOME_ENTER_COMPONENT_COUNT];
    uint8_t enter_groups[HOME_ENTER_COMPONENT_COUNT];
    uint8_t enter_count;
    bool entering;
    lv_anim_completed_cb_t entered_callback;
} ui_home_context_t;

static ui_home_context_t s_home;
static void update_focus(void);
static void refresh_rotation(void *object,int32_t value){lv_obj_set_style_transform_rotation(object,value,0);}
static void refresh_done(lv_anim_t *animation)
{
    (void)animation;s_home.refreshing=false;
    lv_obj_remove_state(s_home.actions[1],LV_STATE_DISABLED);
}
bool ui_home_page_refresh_feedback(void)
{
    if(s_home.refreshing)return false;
    s_home.refreshing=true;lv_obj_add_state(s_home.actions[1],LV_STATE_DISABLED);
    ui_motion_value(s_home.refresh_icon,refresh_rotation,0,3600,480,lv_anim_path_ease_in_out,refresh_done,NULL);
    return true;
}

static void register_enter_component(lv_obj_t *object, uint8_t group)
{
    if(object == NULL || s_home.enter_count >= HOME_ENTER_COMPONENT_COUNT) return;
    unsigned index = s_home.enter_count++;
    s_home.enter_components[index] = object;
    s_home.enter_groups[index] = group;
}

static void enter_component_progress(void *target, int32_t progress)
{
    lv_obj_t *object = target;
    /* Regular opa multiplies child drawing alpha without an offscreen
     * group layer. Translate preserves the page's fixed layout positions. */
    lv_obj_set_style_opa(object, (lv_opa_t)(255 * progress / 1000), 0);
    lv_obj_set_style_translate_y(object, 12 * (1000 - progress) / 1000, 0);
}

void ui_home_page_cancel_enter(void)
{
    for(unsigned i = 0; i < s_home.enter_count; ++i) {
        lv_anim_delete(s_home.enter_components[i], enter_component_progress);
        enter_component_progress(s_home.enter_components[i], 1000);
    }
    s_home.entering = false;
    s_home.entered_callback = NULL;
}

static void entered(lv_anim_t *animation)
{
    s_home.entering = false;
    lv_anim_completed_cb_t completed = s_home.entered_callback;
    s_home.entered_callback = NULL;
    if(completed != NULL) completed(animation);
}

void ui_home_page_animate_enter(bool reduced_motion,
                                lv_anim_completed_cb_t completed, void *user_data)
{
    ui_home_page_cancel_enter();
    if(s_home.enter_count == 0 || reduced_motion) {
        if(completed != NULL) {
            lv_anim_t animation;
            lv_anim_init(&animation);
            lv_anim_set_user_data(&animation, user_data);
            completed(&animation);
        }
        return;
    }
    s_home.entering = true;
    s_home.entered_callback = completed;
    for(unsigned i = 0; i < s_home.enter_count; ++i) {
        enter_component_progress(s_home.enter_components[i], 0);
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, s_home.enter_components[i]);
        lv_anim_set_exec_cb(&animation, enter_component_progress);
        lv_anim_set_values(&animation, 0, 1000);
        lv_anim_set_time(&animation, HOME_ENTER_DURATION_MS);
        lv_anim_set_delay(&animation, s_home.enter_groups[i] * HOME_ENTER_STEP_MS);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        if(i == s_home.enter_count - 1U) {
            lv_anim_set_user_data(&animation, user_data);
            lv_anim_set_completed_cb(&animation, entered);
        }
        lv_anim_start(&animation);
    }
}

bool ui_home_page_is_entering(void)
{
    return s_home.entering;
}

static const lv_point_precise_t s_bike_frame_points[] = {
    {9, 31}, {22, 12}, {35, 31}, {16, 31}, {27, 9}, {34, 8}
};

static lv_obj_t *home_label(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t width, const char *text,
                            const lv_font_t *font, lv_color_t color,
                            lv_text_align_t align)
{
    lv_obj_t *label = ui_page_label(parent, x, y, width, text, font, align);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

static lv_obj_t *home_card(lv_obj_t *parent, int32_t x, int32_t y,
                           int32_t width, int32_t height)
{
    lv_obj_t *card = ui_page_box(parent, x, y, width, height,
                                 UI_TOKEN_COLOR_SURFACE_OUTLINE, 1);
    lv_obj_set_style_bg_color(card, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_set_style_clip_corner(card, true, 0);
    return card;
}

static lv_obj_t *home_line(lv_obj_t *parent,
                           const lv_point_precise_t *points,
                           uint32_t count, lv_color_t color,
                           int32_t width, lv_opa_t opacity)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, points, count);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_opa(line, opacity, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(line, true, 0);
    return line;
}

static void home_action_event(lv_event_t *event)
{
    if(s_home.entering) return;
    lv_obj_t *object = lv_event_get_target(event);
    lv_event_code_t code = lv_event_get_code(event);
    if(code == LV_EVENT_PRESSED) return;
    if(code != LV_EVENT_CLICKED || s_home.callback == NULL || lv_obj_has_state(object,LV_STATE_DISABLED)) return;
    ui_page_action_t action = (ui_page_action_t)(uintptr_t)
                              lv_event_get_user_data(event);
    for(unsigned i=0;i<HOME_ACTION_COUNT;i++)if(s_home.actions[i]==object){s_home.focus_index=i;update_focus();break;}
    ui_sound_play(UI_SFX_PRESS);
    s_home.callback(action, s_home.user_data);
}

static void make_action(lv_obj_t *object, uint8_t index,
                        ui_page_action_t action)
{
    if(object == NULL || index >= HOME_ACTION_COUNT) return;
    s_home.actions[index] = object;
    s_home.action_ids[index] = action;
    lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(object, home_action_event, LV_EVENT_ALL,
                        (void *)(uintptr_t)action);
    lv_obj_set_style_opa(object, LV_OPA_80, LV_STATE_PRESSED);
}

static void update_focus(void)
{
    if(s_home.focus_index>=0)ui_liquid_selection_select(&s_home.selection,s_home.focus_index,true);
}

static void create_avatar(lv_obj_t *card)
{
    lv_obj_t *avatar = ui_page_box(card, 12, 8, 62, 62,
                                   lv_color_hex(0xC7CDD3), 1);
    lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(avatar, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);
    s_home.avatar_text=home_label(avatar, 0, 19, 62, "骑", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER);
    ui_align_label_center(s_home.avatar_text);
}

static void create_bike_icon(lv_obj_t *parent)
{
    lv_obj_t *plate=home_card(parent,10,12,64,64);lv_obj_set_style_bg_color(plate,lv_color_hex(0x282828),0);parent=plate;
    lv_obj_t *left = ui_page_box(parent, 15, 31, 19, 19,
                                 UI_TOKEN_COLOR_ACCENT, 2);
    lv_obj_t *right = ui_page_box(parent, 46, 31, 19, 19,
                                  UI_TOKEN_COLOR_ACCENT, 2);
    lv_obj_set_style_radius(left, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(right, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_t *frame = home_line(parent, s_bike_frame_points,
                                sizeof(s_bike_frame_points) /
                                sizeof(s_bike_frame_points[0]),
                                UI_TOKEN_COLOR_ACCENT, 3, LV_OPA_COVER);
    lv_obj_set_pos(left,5,25);lv_obj_set_pos(right,36,25);lv_obj_set_pos(frame,9,12);
}

static void create_lifetime_decoration(lv_obj_t *card)
{
    lv_obj_t *watermark=lv_image_create(card);
    lv_image_set_src(watermark,&ui_home_watermark);
    lv_obj_set_pos(watermark,280,4);
    lv_obj_set_style_image_recolor(watermark,UI_TOKEN_COLOR_ACCENT,0);
    lv_obj_set_style_image_recolor_opa(watermark,LV_OPA_COVER,0);
    lv_obj_set_style_image_opa(watermark,(lv_opa_t)24,0);
    lv_obj_clear_flag(watermark,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
}

static void create_recent_summary(lv_obj_t *card)
{
    lv_obj_t *preview=ui_page_box(card,16,46,156,66,UI_TOKEN_COLOR_SURFACE_OUTLINE,1);
    lv_obj_set_style_radius(preview,UI_TOKEN_RADIUS_SMALL,0);
    lv_obj_set_style_bg_color(preview,lv_color_hex(0x1B1B1B),0);
    lv_obj_set_style_bg_opa(preview,LV_OPA_COVER,0);
    /* Until a real trace is supplied by the Ride Record backend, use a
     * neutral route icon instead of fabricating a path. */
    s_home.recent_route = home_label(preview, 0, 12, 156, LV_SYMBOL_GPS,
                                     &lv_font_montserrat_28,
                                     UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    s_home.recent_start = NULL;
    s_home.recent_status=home_label(preview,0,51,156,"暂无记录",UI_FONT_PAGE_TEXT,
        UI_TOKEN_COLOR_TEXT_SECONDARY,LV_TEXT_ALIGN_CENTER);
    ui_align_text_pair(preview, s_home.recent_route, s_home.recent_status,
                        0, 154, 4);
}

static lv_obj_t *create_menu_row(lv_obj_t *screen, int32_t y,
                                  const char *icon, const char *title,
                                  const char *subtitle,
                                  uint8_t action_index,
                                  ui_page_action_t action)
{
    lv_obj_t *row = home_card(screen, 8, y, 464,
                              action == UI_PAGE_ACTION_SETTINGS ?
                              UI_PAGE_ACTION_HEIGHT : 52);
    lv_obj_t *icon_box = ui_page_box(row, 14, 8, 36, 36,
                                     lv_color_hex(0x394149), 0);
    lv_obj_set_style_radius(icon_box, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_set_style_bg_color(icon_box, lv_color_hex(0x282828), 0);
    lv_obj_set_style_bg_opa(icon_box, LV_OPA_COVER, 0);
    ui_align_center_y(icon_box, 14);
    if(strcmp(icon, "STAR") == 0) {
        static const lv_point_precise_t star[] = {
            {18, 3}, {22, 12}, {32, 13}, {24, 20}, {27, 31},
            {18, 25}, {9, 31}, {12, 20}, {4, 13}, {14, 12}, {18, 3}
        };
        lv_obj_t *star_line = home_line(icon_box, star,
            sizeof(star) / sizeof(star[0]), UI_TOKEN_COLOR_ACCENT, 3, LV_OPA_COVER);
        lv_obj_set_pos(star_line, 0, 0);
    }
    else {
        lv_obj_t *symbol = home_label(icon_box, 0, 0, 36, icon,
            &lv_font_montserrat_20, UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
        ui_align_label_center(symbol);
    }
    lv_obj_t *primary = home_label(row, 70, 5, 300, title, UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *secondary = home_label(row, 70, 28, 300, subtitle, &lv_font_montserrat_12,
               UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    ui_align_text_pair(row, primary, secondary, 70, 300, 4);
    lv_obj_t *arrow = home_label(row, 416, 12, 28, LV_SYMBOL_RIGHT,
               &lv_font_montserrat_22, UI_TOKEN_COLOR_TEXT_PRIMARY,
               LV_TEXT_ALIGN_CENTER);
    ui_align_center_y(arrow, 416);
    make_action(row, action_index, action);
    return row;
}

static void summary_number(lv_obj_t *object,const char *text,int32_t width)
{
    const lv_font_t *fonts[]={&lv_font_montserrat_16,&lv_font_montserrat_14,&lv_font_montserrat_12};
    for(unsigned i=0;i<sizeof(fonts)/sizeof(fonts[0]);++i) {
        lv_obj_set_style_text_font(object,fonts[i],0);
        lv_point_t extent;
        lv_text_get_size(&extent,text,fonts[i],0,0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
        if(extent.x<=width) break;
    }
    lv_label_set_long_mode(object,LV_LABEL_LONG_DOT);
    lv_label_set_text(object,text);
}

static void update_data(const ui_home_data_t *data)
{
    if(data == NULL || s_home.screen == NULL) return;
    char text[64];
    ui_text_set_label(s_home.user_name,
                      strcmp(data->user_name, "Alex Chen") == 0 ?
                      "骑行者" : data->user_name, 24);
    const char *display_name = strcmp(data->user_name, "Alex Chen") == 0 ?
                               "骑行者" : data->user_name;
    const unsigned char *name=(const unsigned char *)display_name;
    unsigned length=name[0]<0x80 ? 1 : (name[0]&0xE0)==0xC0 ? 2 : (name[0]&0xF0)==0xE0 ? 3 : 4;
    char initial[5]={0};
    if(name[0] && strlen(display_name)>=length) memcpy(initial,name,length);
    ui_text_set_label(s_home.avatar_text,initial[0] ? initial : "骑",20);
    ui_text_set_label(s_home.tagline, data->tagline, 20);
    snprintf(text, sizeof(text), "%lu",
             (unsigned long)data->total_distance_km);
    lv_label_set_text(s_home.total_distance, text);
    lv_obj_set_style_text_font(s_home.total_distance,data->total_distance_km<10000 ?
        &ui_font_misans_metric_60 : data->total_distance_km<1000000 ?
        &lv_font_montserrat_48 : &lv_font_montserrat_32,0);
    lv_point_t extent;
    const lv_font_t *distance_font=lv_obj_get_style_text_font(s_home.total_distance,0);
    lv_text_get_size(&extent,text,distance_font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
    lv_obj_set_x(s_home.distance_unit,18+extent.x+12);
    lv_obj_set_y(s_home.distance_unit,43+distance_font->line_height-26);
    snprintf(text, sizeof(text), "%lu h  %u m",
             (unsigned long)data->total_time_hours,
             (unsigned int)data->total_time_minutes);
    summary_number(s_home.total_time,text,128);
    snprintf(text, sizeof(text), "%lu m",
             (unsigned long)data->total_ascent_m);
    summary_number(s_home.total_ascent,text,130);
    lv_label_set_text(s_home.last_date, data->last_ride_date);
    snprintf(text, sizeof(text), "%lu.%lu km",
             (unsigned long)(data->last_distance_tenths_km / 10U),
             (unsigned long)(data->last_distance_tenths_km % 10U));
    summary_number(s_home.last_distance,text,88);
    snprintf(text, sizeof(text), "%lu h %02lu m",
             (unsigned long)(data->last_duration_minutes / 60U),
             (unsigned long)(data->last_duration_minutes % 60U));
    summary_number(s_home.last_duration,text,86);
    snprintf(text, sizeof(text), "%lu m",
             (unsigned long)data->last_ascent_m);
    summary_number(s_home.last_ascent,text,68);
    bool available=data->last_ride_available;
    lv_label_set_text(s_home.recent_status,available ? "已保存" : "暂无记录");
    ui_text_set_label(s_home.last_caption,available ? "记录已保存，可查看详情" : "完成首次骑行后显示摘要",20);
    if(s_home.recent_route != NULL)
        lv_obj_clear_flag(s_home.recent_route, LV_OBJ_FLAG_HIDDEN);
    if(!available) {
        lv_label_set_text(s_home.last_duration,"--");
    }
    if(!available || !data->last_distance_available) lv_label_set_text(s_home.last_distance,"--");
    if(!available || !data->last_ascent_available) lv_label_set_text(s_home.last_ascent,"--");
}

static void home_data_changed(const ui_home_data_t *data, void *user_data)
{
    (void)user_data;
    update_data(data);
}

lv_obj_t *ui_home_page_create(ui_page_action_cb_t callback,
                              void *user_data)
{
    memset(&s_home, 0, sizeof(s_home));
    s_home.callback = callback;
    s_home.user_data = user_data;
    s_home.focus_index = -1;

    s_home.screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_home.screen);
    lv_obj_set_size(s_home.screen, UI_TOKEN_SCREEN_WIDTH,
                    UI_TOKEN_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_home.screen, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(s_home.screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_home.screen, LV_OBJ_FLAG_SCROLLABLE);
    ui_page_status_create(&s_home.status, s_home.screen);
    register_enter_component(s_home.status.root, 0);

    lv_obj_t *identity = home_card(s_home.screen, 8, 64, 464, 86);
    register_enter_component(identity, 1);
    create_avatar(identity);
    s_home.user_name = home_label(identity, 92, 10, 230, "",
                                  &lv_font_montserrat_22,
                                  UI_TOKEN_COLOR_TEXT_PRIMARY,
                                  LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(s_home.user_name,LV_LABEL_LONG_DOT);
    ui_page_line(identity, 92, 53, 20, 3, UI_TOKEN_COLOR_ACCENT);
    s_home.tagline = home_label(identity, 120, 45, 250, "",
                               &lv_font_montserrat_12,
                               UI_TOKEN_COLOR_TEXT_SECONDARY,
                               LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(s_home.tagline,LV_LABEL_LONG_DOT);
    home_label(identity, 374, 18, 72, "LOCAL\nRIDER",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);
    make_action(identity, 0, UI_PAGE_ACTION_PROFILE);

    lv_obj_t *lifetime = home_card(s_home.screen, 8, 160, 464, 180);
    register_enter_component(lifetime, 2);
    create_lifetime_decoration(lifetime);
    home_label(lifetime, 18, 17, 240, "TOTAL DISTANCE",
               &lv_font_montserrat_16, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);
    home_label(lifetime, 354, 17, 92, "LIFETIME",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_RIGHT);
    lv_obj_t *refresh=home_card(lifetime,390,47,56,48);
    lv_obj_set_style_radius(refresh,24,0);
    s_home.refresh_icon=home_label(refresh,0,0,56,LV_SYMBOL_REFRESH,&lv_font_montserrat_22,UI_TOKEN_COLOR_TEXT_PRIMARY,LV_TEXT_ALIGN_CENTER);
    ui_align_label_center(s_home.refresh_icon);lv_obj_set_style_transform_pivot_x(s_home.refresh_icon,28,0);
    lv_obj_set_style_transform_pivot_y(s_home.refresh_icon,11,0);
    make_action(refresh,1,UI_PAGE_ACTION_HOME_REFRESH);
    s_home.total_distance = home_label(lifetime, 18, 43, 264, "",
                                       &ui_font_misans_metric_60,
                                       UI_TOKEN_COLOR_TEXT_PRIMARY,
                                       LV_TEXT_ALIGN_LEFT);
    s_home.distance_unit=home_label(lifetime, 80, 78, 48, "km", &lv_font_montserrat_20,
                                    UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    home_label(lifetime, 52, 129, 100, "TOTAL TIME",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);
    ui_nav_icon(lifetime,18,126,28,NAV_ICON_CLOCK,0xAAAAAA);
    s_home.total_time = home_label(lifetime, 52, 147, 128, "",
                                   &lv_font_montserrat_16,
                                   UI_TOKEN_COLOR_TEXT_PRIMARY,
                                   LV_TEXT_ALIGN_LEFT);
    ui_page_line(lifetime, 190, 130, 1, 36,
                 UI_TOKEN_COLOR_TEXT_TERTIARY);
    home_label(lifetime, 236, 129, 118, "TOTAL ASCENT",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);
    home_label(lifetime, 200, 126, 28, LV_SYMBOL_UP,
               &lv_font_montserrat_22, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_CENTER);
    s_home.total_ascent = home_label(lifetime, 236, 147, 130, "",
                                     &lv_font_montserrat_16,
                                     UI_TOKEN_COLOR_TEXT_PRIMARY,
                                     LV_TEXT_ALIGN_LEFT);

    lv_obj_t *start = ui_page_box(s_home.screen, 8, 350, 250, 88,
                                  UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_set_style_radius(start, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_set_style_bg_color(start, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(start, LV_OPA_COVER, 0);
    create_bike_icon(start);
    lv_obj_t *start_title = home_label(start, 82, 22, 130, "开始骑行", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *start_subtitle = home_label(start, 82, 52, 132, "START RIDE",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);
    ui_align_text_pair(start, start_title, start_subtitle, 82, 130, 4);
    lv_obj_t *start_arrow = home_label(start, 218, 29, 24, LV_SYMBOL_RIGHT,
               &lv_font_montserrat_22, UI_TOKEN_COLOR_TEXT_PRIMARY,
               LV_TEXT_ALIGN_CENTER);
    ui_align_center_y(start_arrow, 218);
    make_action(start, 2, UI_PAGE_ACTION_START_RIDE);
    register_enter_component(start, 3);

    lv_obj_t *navigation = home_card(s_home.screen, 266, 350, 206, 88);
    lv_obj_t *nav_icon = home_label(navigation, 16, 27, 42, LV_SYMBOL_GPS,
               &lv_font_montserrat_28, UI_TOKEN_COLOR_ACCENT,
               LV_TEXT_ALIGN_CENTER);
    ui_align_center_y(nav_icon, 16);
    lv_obj_t *nav_title = home_label(navigation, 64, 22, 112, "导航", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *nav_subtitle = home_label(navigation, 64, 52, 120, "NAVIGATION",
               &lv_font_montserrat_12, UI_TOKEN_COLOR_TEXT_SECONDARY,
               LV_TEXT_ALIGN_LEFT);
    ui_align_text_pair(navigation, nav_title, nav_subtitle, 64, 108, 4);
    lv_obj_t *nav_arrow = home_label(navigation, 174, 29, 24, LV_SYMBOL_RIGHT,
               &lv_font_montserrat_22, UI_TOKEN_COLOR_TEXT_PRIMARY,
               LV_TEXT_ALIGN_CENTER);
    ui_align_center_y(nav_arrow, 174);
    make_action(navigation, 3, UI_PAGE_ACTION_START_NAVIGATION);
    register_enter_component(navigation, 3);

    lv_obj_t *recent = home_card(s_home.screen, 8, 448, 464, 126);
    register_enter_component(recent, 4);
    home_label(recent, 16, 12, 150, "最近一次骑行", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_LEFT);
    s_home.last_date = home_label(recent, 248, 15, 184, "",
                                  &lv_font_montserrat_12,
                                  UI_TOKEN_COLOR_TEXT_SECONDARY,
                                  LV_TEXT_ALIGN_RIGHT);
    home_label(recent, 432, 13, 22, LV_SYMBOL_RIGHT,
               &lv_font_montserrat_20, UI_TOKEN_COLOR_TEXT_PRIMARY,
               LV_TEXT_ALIGN_CENTER);
    create_recent_summary(recent);
    ui_page_line(recent, 184, 46, 1, 66,
                 UI_TOKEN_COLOR_TEXT_TERTIARY);
    home_label(recent, 200, 46, 72, "距离", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_home.last_distance = home_label(recent, 200, 72, 88, "",
                                      &lv_font_montserrat_16,
                                      UI_TOKEN_COLOR_TEXT_PRIMARY,
                                      LV_TEXT_ALIGN_LEFT);
    home_label(recent, 294, 46, 54, "用时", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_home.last_duration = home_label(recent, 294, 72, 86, "",
                                      &lv_font_montserrat_16,
                                      UI_TOKEN_COLOR_TEXT_PRIMARY,
                                      LV_TEXT_ALIGN_LEFT);
    home_label(recent, 386, 46, 54, "爬升", UI_FONT_PAGE_TEXT,
               UI_TOKEN_COLOR_TEXT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    s_home.last_ascent = home_label(recent, 386, 72, 68, "",
                                    &lv_font_montserrat_16,
                                    UI_TOKEN_COLOR_TEXT_PRIMARY,
                                    LV_TEXT_ALIGN_LEFT);
    s_home.last_caption = home_label(recent, 200, 96, 240, "",
                                     UI_FONT_PAGE_TEXT,
                                     UI_TOKEN_COLOR_TEXT_PRIMARY,
                                     LV_TEXT_ALIGN_LEFT);
    make_action(recent, 4, UI_PAGE_ACTION_RECENT_RIDE_DETAIL);

    register_enter_component(create_menu_row(s_home.screen, 584, LV_SYMBOL_LIST,
                    "历史记录", "RIDE HISTORY", 5,
                    UI_PAGE_ACTION_HISTORY), 5);
    register_enter_component(create_menu_row(s_home.screen, 644, LV_SYMBOL_WIFI,
                    "设备与传感器", "DEVICES & SENSORS", 6,
                    UI_PAGE_ACTION_DEVICES), 6);
    register_enter_component(create_menu_row(s_home.screen, 704, "STAR",
                    "收藏路线", "SAVED ROUTES", 7,
                    UI_PAGE_ACTION_SAVED_ROUTES), 7);
    register_enter_component(create_menu_row(s_home.screen, UI_PAGE_ACTION_Y, LV_SYMBOL_SETTINGS,
                    "系统设置", "SYSTEM SETTINGS", 8,
                    UI_PAGE_ACTION_SETTINGS), 8);

    ui_home_data_t data;
    if(ui_home_data_model_get(&data)) update_data(&data);
    s_home.model_subscription = ui_home_data_model_subscribe(
        home_data_changed, NULL);
    ui_liquid_selection_init(&s_home.selection,s_home.screen,false,NULL);
    for(unsigned i=0;i<HOME_ACTION_COUNT;i++)ui_liquid_selection_add(&s_home.selection,s_home.actions[i]);
    update_focus();
    return s_home.screen;
}

void ui_home_page_set_active(bool active)
{
    if(!active) ui_home_page_cancel_enter();
    s_home.active = active;
    s_home.focus_index = active ? 0 : -1;
    ui_liquid_selection_set_active(&s_home.selection,active);
    update_focus();
}

bool ui_home_page_handle_input(ui_input_event_t input)
{
    if(!s_home.active || s_home.entering) return false;
    if(input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW) {
        int32_t direction = input == UI_INPUT_ENCODER_CW ? 1 : -1;
        int32_t next = s_home.focus_index;
        if(next < 0) next = direction > 0 ? 0 : HOME_ACTION_COUNT - 1;
        else {
            next += direction;
            if(next < 0) next = HOME_ACTION_COUNT - 1;
            if(next >= HOME_ACTION_COUNT) next = 0;
        }
        s_home.focus_index = (int8_t)next;
        update_focus();
        return true;
    }
    if(input == UI_INPUT_ENCODER_PRESS && s_home.focus_index >= 0 &&
       s_home.focus_index < HOME_ACTION_COUNT) {
        if(s_home.callback != NULL && !lv_obj_has_state(s_home.actions[s_home.focus_index],LV_STATE_DISABLED)) {
            ui_sound_play(UI_SFX_PRESS);
            s_home.callback(s_home.action_ids[s_home.focus_index],
                            s_home.user_data);
        }
        return true;
    }
    return false;
}
