#include "ui_nav_map.h"

#include "ui_nav_animation.h"
#include "ui_nav_geometry.h"
#include "ui_nav_layout.h"
#include "../../core/ui_colors.h"
#include "../../core/ui_motion.h"
#include "../../core/ui_theme.h"
#include "services/ui_nav_raster.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/src/draw/lv_draw_triangle.h"

#define NAV_MAP_W             UI_NAV_MAP_WIDTH
#define NAV_MAP_H             UI_NAV_MAP_HEIGHT
#define NAV_CONTENT_SIZE      1024
#define NAV_CONTENT_CENTER_X  512
#define NAV_CONTENT_CENTER_Y  512
#define NAV_CONTENT_HOME_X    ((NAV_MAP_W - NAV_CONTENT_SIZE) / 2)
#define NAV_CONTENT_HOME_Y    ((NAV_MAP_H - NAV_CONTENT_SIZE) / 2)
#define NAV_CONTROL_SIZE      48
#define NAV_HUD_MARGIN        12
#define NAV_COMPASS_TOP       12
#define NAV_ZOOM_IN_TOP       (NAV_MAP_H - 188)
#define NAV_ZOOM_OUT_TOP      (NAV_MAP_H - 132)
#define NAV_CONTROL_RIGHT     (NAV_MAP_W - NAV_HUD_MARGIN - NAV_CONTROL_SIZE)
#define NAV_LOCATE_TOP        (NAV_MAP_H - NAV_HUD_MARGIN - NAV_CONTROL_SIZE)
#define NAV_SCALE_W           100
#define NAV_SCALE_H           38
#define NAV_SCALE_X           12
#define NAV_SCALE_Y           (NAV_MAP_H - 44)
#define NAV_MARKER_SIZE       32
#define NAV_MARKER_RADIUS     (NAV_MARKER_SIZE / 2)
#define NAV_ZOOM_MIN          10
#define NAV_ZOOM_MAX          18
#define NAV_ZOOM_BASE         14
#define NAV_ZOOM_SCALE_STEP   18
#define NAV_DRAG_INERTIA_MAX  56
#define NAV_YELLOW            lv_color_hex(0xFFD900)
#define NAV_MOCK_ORIGIN_LAT   34.3416f
#define NAV_MOCK_ORIGIN_LON   108.9398f

static int32_t clamp_content_x(int32_t value){return LV_CLAMP(-16000,value,16000);}
static int32_t clamp_content_y(int32_t value){return LV_CLAMP(-16000,value,16000);}

static void follow_render_position(ui_nav_map_t *map)
{
    if(map == NULL || map->pointer_down || (map->state != NAV_FOLLOW && map->state != NAV_FOLLOW_USER_ZOOM)) return;
    lv_point_precise_t marker = ui_nav_geometry_transform_point(
        map, (lv_point_precise_t){map->render_x, map->render_y});
    lv_obj_set_pos(map->map_content,
                   clamp_content_x(NAV_MAP_W / 2 - marker.x),
                   clamp_content_y(map->safe_height * 73 / 100 - marker.y));
}

static void update_marker_visual(ui_nav_map_t *map)
{
    if(map == NULL || map->position_marker == NULL) return;
    lv_point_precise_t marker = ui_nav_geometry_transform_point(
        map, (lv_point_precise_t){map->render_x, map->render_y});
    lv_obj_set_pos(map->position_marker, marker.x - NAV_MARKER_RADIUS,
                   marker.y - NAV_MARKER_RADIUS);
    follow_render_position(map);
}

static void set_content_x(void *target, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)target, clamp_content_x(value));
}

static void set_content_y(void *target, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)target, clamp_content_y(value));
}

static void set_marker_x(void *target, int32_t value)
{
    ui_nav_map_t *map = target;
    map->render_x = value;
    update_marker_visual(map);
}

static void set_marker_y(void *target, int32_t value)
{
    ui_nav_map_t *map = target;
    map->render_y = value;
    update_marker_visual(map);
}

static void invalidate_map_geometry(ui_nav_map_t *map)
{
    if(map == NULL) return;
    ui_nav_geometry_invalidate(map);
    update_marker_visual(map);
}

static void set_map_scale(void *target, int32_t value)
{
    ui_nav_map_t *map = target;
    map->render_scale = value;
    if((!map->demo || map->strict_geometry) && map->scale_label != NULL && value > 0)
        lv_label_set_text_fmt(map->scale_label, "%lu m",
            (unsigned long)lroundf(84.0f * map->meters_per_pixel * 256.0f / value));
    invalidate_map_geometry(map);
}

static void set_map_rotation(void *target, int32_t value)
{
    ui_nav_map_t *map = target;
    map->render_rotation = value % 3600;
    invalidate_map_geometry(map);if(map->compass.root)lv_obj_invalidate(map->compass.root);
}

static int32_t nearest_rotation(int32_t from, int32_t target)
{
    int32_t delta = (target - from) % 3600;
    if(delta > 1800) delta -= 3600;
    if(delta < -1800) delta += 3600;
    return from + delta;
}

static void set_scale_opa(void *target, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)target, (lv_opa_t)value, 0);
}

static void scale_draw_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;
    lv_obj_t *obj = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = UI_COLOR_WHITE;
    line.opa = LV_OPA_COVER;
    line.width = 2;
    line.p1 = (lv_point_precise_t){area.x1 + 8, area.y1 + 6};
    line.p2 = (lv_point_precise_t){area.x2 - 8, area.y1 + 6};
    lv_draw_line(layer, &line);
    line.p1 = (lv_point_precise_t){area.x1 + 8, area.y1 + 1};
    line.p2 = (lv_point_precise_t){area.x1 + 8, area.y1 + 11};
    lv_draw_line(layer, &line);
    line.p1 = (lv_point_precise_t){area.x2 - 8, area.y1 + 1};
    line.p2 = (lv_point_precise_t){area.x2 - 8, area.y1 + 11};
    lv_draw_line(layer, &line);
}

static void map_set_state(ui_nav_map_t *map, nav_state_t state)
{
    if(map == NULL) return;
    map->state = state;
    ui_nav_control_set_active(&map->locate, state == NAV_EXPLORE);
}

void ui_nav_map_set_focus(ui_nav_map_t *map, ui_nav_focus_t focus,
                          bool visible)
{
    if(map == NULL || focus >= UI_NAV_FOCUS_COUNT) return;
    map->focused_control = focus;
    map->focus_visible = visible;
    ui_nav_control_set_focused(&map->compass,
                               visible && focus == UI_NAV_FOCUS_COMPASS);
    ui_nav_control_set_focused(&map->zoom_in,
                               visible && focus == UI_NAV_FOCUS_ZOOM_IN);
    ui_nav_control_set_focused(&map->zoom_out,
                               visible && focus == UI_NAV_FOCUS_ZOOM_OUT);
    ui_nav_control_set_focused(&map->locate,
                               visible && focus == UI_NAV_FOCUS_LOCATE);
    if(map->pick_cursor){
        if(map->pick_mode||(visible&&focus==UI_NAV_FOCUS_PICK))lv_obj_clear_flag(map->pick_cursor,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(map->pick_cursor,LV_OBJ_FLAG_HIDDEN);
    }
}

static void recenter_done(lv_anim_t *animation)
{
    ui_nav_map_t *map = lv_anim_get_user_data(animation);
    if(map != NULL) map_set_state(map, NAV_FOLLOW);
}

static void recenter(ui_nav_map_t *map)
{
    if(map == NULL) return;
    map->zoom_bias=1.f; map_set_state(map, NAV_RECENTER);
    lv_point_precise_t marker = ui_nav_geometry_transform_point(
        map, (lv_point_precise_t){map->render_x, map->render_y});
    int32_t target_x = clamp_content_x(NAV_MAP_W / 2 - marker.x);
    int32_t target_y = clamp_content_y(map->safe_height * 73 / 100 - marker.y);
    int32_t from_x = lv_obj_get_x(map->map_content);
    int32_t from_y = lv_obj_get_y(map->map_content);
    ui_nav_animation_value(map->map_content, set_content_x, from_x,
                           target_x, UI_ANIM_RECENTER, lv_anim_path_ease_in_out);

    ui_motion_value(map->map_content, set_content_y, from_y, target_y,
                    UI_MOTION_DURATION_RECENTER, lv_anim_path_ease_in_out,
                    recenter_done, map);
}

static void compass_clicked(void *user_data)
{
    ui_nav_map_t *map = user_data;
    if(map == NULL) return;
    ui_nav_map_set_focus(map, UI_NAV_FOCUS_COMPASS, true);
    map->north_up = !map->north_up;
    int32_t rotation = map->north_up ? 0 : -(int32_t)(map->heading * 10.0f);
    rotation = nearest_rotation(map->render_rotation, rotation);
    ui_nav_animation_value(map, set_map_rotation, map->render_rotation,
                           rotation, UI_ANIM_NORMAL, lv_anim_path_ease_in_out);
    ui_nav_control_set_active(&map->compass, !map->north_up);
}
void ui_nav_map_set_north_up(ui_nav_map_t *map,bool north_up)
{ if(map && map->north_up!=north_up) compass_clicked(map); }
bool ui_nav_map_is_browsing(const ui_nav_map_t *map)
{return map && (map->state==NAV_EXPLORE||map->state==NAV_OVERVIEW);}
void ui_nav_map_set_overview(ui_nav_map_t *map,bool overview)
{
    if(!map)return;
    if(!overview){recenter(map);return;}
    map_set_state(map,NAV_OVERVIEW);
    if(map->route_point_count<2)return;
    float minx=map->route_points[0].x,maxx=minx,miny=map->route_points[0].y,maxy=miny;
    for(unsigned i=1;i<map->route_point_count;i++){minx=fminf(minx,map->route_points[i].x);maxx=fmaxf(maxx,map->route_points[i].x);miny=fminf(miny,map->route_points[i].y);maxy=fmaxf(maxy,map->route_points[i].y);}
    int scale=LV_CLAMP(16,256*fminf((NAV_MAP_W-80)/fmaxf(1,maxx-minx),(map->safe_height-80)/fmaxf(1,maxy-miny)),768);
    ui_nav_animation_value(map,set_map_rotation,map->render_rotation,nearest_rotation(map->render_rotation,0),400,lv_anim_path_ease_in_out);
    ui_nav_animation_value(map,set_map_scale,map->render_scale,scale,400,lv_anim_path_ease_in_out);
    int x=NAV_MAP_W/2-(512+((minx+maxx)/2-512)*scale/256),y=map->safe_height/2-(512+((miny+maxy)/2-512)*scale/256);
    ui_nav_animation_value(map->map_content,set_content_x,lv_obj_get_x(map->map_content),clamp_content_x(x),400,lv_anim_path_ease_in_out);
    ui_nav_animation_value(map->map_content,set_content_y,lv_obj_get_y(map->map_content),clamp_content_y(y),400,lv_anim_path_ease_in_out);
}

static void zoom_in_clicked(void *user_data)
{
    ui_nav_map_t *map = user_data;
    if(map == NULL) return;
    ui_nav_map_set_focus(map, UI_NAV_FOCUS_ZOOM_IN, true);
    if(map->state==NAV_OVERVIEW)map_set_state(map,NAV_EXPLORE);
    if(map->state!=NAV_EXPLORE && map->state!=NAV_OVERVIEW) map_set_state(map,NAV_FOLLOW_USER_ZOOM);
    if(map->raster_valid){
        map->zoom_bias=fminf(6.f,map->zoom_bias*1.5f);
        ui_nav_animation_value(map,set_map_scale,map->render_scale,LV_MIN(1536,map->render_scale*3/2),280,lv_anim_path_ease_in_out);return;
    }
    map->zoom_bias=fminf(2.f,map->zoom_bias*1.18f);
    if(map->zoom < NAV_ZOOM_MAX) nav_map_zoom(map, map->zoom + 1, true);
}

static void zoom_out_clicked(void *user_data)
{
    ui_nav_map_t *map = user_data;
    if(map == NULL) return;
    ui_nav_map_set_focus(map, UI_NAV_FOCUS_ZOOM_OUT, true);
    if(map->state==NAV_OVERVIEW)map_set_state(map,NAV_EXPLORE);
    if(map->state!=NAV_EXPLORE && map->state!=NAV_OVERVIEW) map_set_state(map,NAV_FOLLOW_USER_ZOOM);
    if(map->raster_valid){
        map->zoom_bias=fmaxf(.03125f,map->zoom_bias/1.5f);
        ui_nav_animation_value(map,set_map_scale,map->render_scale,LV_MAX(32,map->render_scale*2/3),280,lv_anim_path_ease_in_out);return;
    }
    map->zoom_bias=fmaxf(.5f,map->zoom_bias/1.18f);
    if(map->zoom > NAV_ZOOM_MIN) nav_map_zoom(map, map->zoom - 1, true);
}

static void locate_clicked(void *user_data)
{
    ui_nav_map_t *map = user_data;
    if(map == NULL) return;
    ui_nav_map_set_focus(map, UI_NAV_FOCUS_LOCATE, true);
    recenter(map);
}

static void map_pointer_cb(lv_event_t *event)
{
    ui_nav_map_t *map = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if(map == NULL || map->read_only) return;
    if(code == LV_EVENT_PRESSED) {
        map->pointer_down=true;
        ui_motion_cancel(map->map_content,set_content_x);ui_motion_cancel(map->map_content,set_content_y);
        ui_motion_cancel(map,set_map_scale);ui_motion_cancel(map,set_map_rotation);
        map->last_drag_vector = (lv_point_t){0, 0};map->drag_x=map->drag_y=0;map->pick_claimed=false;
        lv_indev_t *indev=lv_indev_active();if(indev)lv_indev_get_point(indev,&map->drag_point);
        map->drag_started=false;map->drag_sample_count=1;
        map->drag_samples[0].point=map->drag_point;map->drag_samples[0].at=lv_tick_get();
    } else if(code == LV_EVENT_PRESSING) {
        lv_indev_t *indev = lv_indev_active();
        if(indev == NULL) return;
        lv_point_t point;lv_indev_get_point(indev,&point);
        map->last_drag_vector=(lv_point_t){point.x-map->drag_point.x,point.y-map->drag_point.y};
        map->drag_point=point;
        if(!map->last_drag_vector.x&&!map->last_drag_vector.y)return;
        if(map->last_drag_vector.x||map->last_drag_vector.y){
            if(map->drag_sample_count==12){memmove(map->drag_samples,map->drag_samples+1,sizeof(map->drag_samples[0])*11);map->drag_sample_count--;}
            unsigned i=map->drag_sample_count++;map->drag_samples[i].point=point;map->drag_samples[i].at=lv_tick_get();
        }
        map->drag_x+=map->last_drag_vector.x;map->drag_y+=map->last_drag_vector.y;
        if(map->pick_claimed||abs(map->drag_x)+abs(map->drag_y)<8)return;
        if(!map->drag_started){map->drag_started=true;map_set_state(map,NAV_EXPLORE);nav_map_move(map,map->drag_x,map->drag_y,false);}
        else nav_map_move(map,map->last_drag_vector.x,map->last_drag_vector.y,false);
    } else if(code==LV_EVENT_LONG_PRESSED) {
        if(abs(map->drag_x)+abs(map->drag_y)>=8)return;
        lv_indev_t *indev=lv_indev_active();lv_point_t point;
        if(indev){lv_indev_get_point(indev,&point);map->pick_claimed=ui_nav_map_select_at(map,point.x,point.y);}
    } else if(code == LV_EVENT_PRESS_LOST || code == LV_EVENT_INDEV_RESET) {
        map->pointer_down=false;
        map->last_drag_vector = (lv_point_t){0, 0};
        ui_motion_cancel(map->map_content, set_content_x);
        ui_motion_cancel(map->map_content, set_content_y);
    } else if(code == LV_EVENT_RELEASED) {
        map->pointer_down=false;
        if(map->state==NAV_EXPLORE&&map->drag_started&&!map->pick_claimed&&!map->pick_mode&&map->drag_sample_count>=2){
            unsigned last=map->drag_sample_count-1,first=last;
            uint32_t now=lv_tick_get(),end=map->drag_samples[last].at;
            while(first&&end-map->drag_samples[first-1].at<=90)first--;
            uint32_t elapsed=end-map->drag_samples[first].at;
            if(now-end<=35&&elapsed>=20){
                float vx=(map->drag_samples[last].point.x-map->drag_samples[first].point.x)/(float)elapsed;
                float vy=(map->drag_samples[last].point.y-map->drag_samples[first].point.y)/(float)elapsed;
                float speed=hypotf(vx,vy);
                if(speed>.8f){
                    float length=fminf(NAV_DRAG_INERTIA_MAX,speed*55.f);
                    int dx=(int)lroundf(vx/speed*length),dy=(int)lroundf(vy/speed*length);
                    ui_nav_animation_value(map->map_content,set_content_x,lv_obj_get_x(map->map_content),clamp_content_x(lv_obj_get_x(map->map_content)+dx),120,lv_anim_path_ease_out);
                    ui_nav_animation_value(map->map_content,set_content_y,lv_obj_get_y(map->map_content),clamp_content_y(lv_obj_get_y(map->map_content)+dy),120,lv_anim_path_ease_out);
                }
            }
        }
    }
}

static lv_obj_t *plain_layer(lv_obj_t *parent)
{
    lv_obj_t *layer = lv_obj_create(parent);
    lv_obj_remove_style_all(layer);
    lv_obj_set_pos(layer, 0, 0);
    lv_obj_set_size(layer, NAV_CONTENT_SIZE, NAV_CONTENT_SIZE);
    lv_obj_clear_flag(layer, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return layer;
}

static void create_scale(ui_nav_map_t *map)
{
    map->scale_root = lv_obj_create(map->map_hud);
    lv_obj_remove_style_all(map->scale_root);
    lv_obj_set_pos(map->scale_root, NAV_SCALE_X, NAV_SCALE_Y);
    lv_obj_set_size(map->scale_root, NAV_SCALE_W, NAV_SCALE_H);
    lv_obj_clear_flag(map->scale_root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(map->scale_root, scale_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    map->scale_label = lv_label_create(map->scale_root);
    lv_label_set_text(map->scale_label, "500 m");
    lv_obj_set_pos(map->scale_label, 0, 13);
    lv_obj_set_width(map->scale_label, NAV_SCALE_W);
    lv_obj_set_style_text_align(map->scale_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(map->scale_label, UI_COLOR_WHITE, 0);
    lv_obj_set_style_text_font(map->scale_label, UI_FONT_PAGE_TEXT, 0);
}

static void compass_draw(lv_event_t *event)
{
 if(lv_event_get_code(event)!=LV_EVENT_DRAW_MAIN)return;
 const ui_nav_map_t *map=lv_event_get_user_data(event);lv_area_t area;lv_obj_get_coords(map->compass.root,&area);
 float angle=map->render_rotation*.00174532925199433,cs=cosf(angle),sn=sinf(angle);
 int cx=(area.x1+area.x2)/2,cy=area.y1+34;
 const int px[]={0,-5,5,0},py[]={-12,0,0,12};lv_point_precise_t points[4];
 for(int i=0;i<4;i++)points[i]=(lv_point_precise_t){cx+px[i]*cs-py[i]*sn,cy+px[i]*sn+py[i]*cs};
 lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);d.bg_opa=LV_OPA_COVER;
 d.bg_color=lv_color_hex(0xFFD900);d.p[0]=points[0];d.p[1]=points[1];d.p[2]=points[2];lv_draw_triangle(lv_event_get_layer(event),&d);
 d.bg_color=lv_color_white();d.p[0]=points[3];lv_draw_triangle(lv_event_get_layer(event),&d);
}
static void create_hud(ui_nav_map_t *map)
{
    map->map_hud = lv_obj_create(map->root);
    lv_obj_remove_style_all(map->map_hud);
    lv_obj_set_pos(map->map_hud, 0, 0);
    lv_obj_set_size(map->map_hud, NAV_MAP_W, NAV_MAP_H);
    lv_obj_clear_flag(map->map_hud, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    create_scale(map);
    ui_nav_control_create(&map->compass, map->map_hud, NAV_CONTROL_RIGHT, NAV_COMPASS_TOP,
                          LV_SYMBOL_UP, compass_clicked, map);
    lv_obj_add_flag(map->compass.icon,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(map->compass.root,56,56);lv_obj_set_x(map->compass.root,NAV_MAP_W-68);
    lv_obj_set_style_radius(map->compass.root,LV_RADIUS_CIRCLE,0);
    lv_obj_add_event_cb(map->compass.root,compass_draw,LV_EVENT_DRAW_MAIN,map);
    lv_obj_set_style_text_font(map->compass.icon, &lv_font_montserrat_20, 0);
    lv_obj_align(map->compass.icon, LV_ALIGN_TOP_MID, 0, 3);
    map->compass_n = lv_label_create(map->compass.root);
    lv_label_set_text(map->compass_n, "N");
    lv_obj_set_style_text_font(map->compass_n, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(map->compass_n, UI_COLOR_WHITE, 0);
    lv_obj_align(map->compass_n, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_clear_flag(map->compass_n, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    ui_nav_control_create(&map->zoom_in, map->map_hud, NAV_CONTROL_RIGHT, NAV_ZOOM_IN_TOP,
                          LV_SYMBOL_PLUS, zoom_in_clicked, map);
    ui_nav_control_create(&map->zoom_out, map->map_hud, NAV_CONTROL_RIGHT, NAV_ZOOM_OUT_TOP,
                          LV_SYMBOL_MINUS, zoom_out_clicked, map);
    ui_nav_control_create(&map->locate, map->map_hud, NAV_CONTROL_RIGHT,
                          NAV_LOCATE_TOP, LV_SYMBOL_GPS, locate_clicked, map);
    map->pick_cursor=lv_label_create(map->map_hud);
    lv_label_set_text(map->pick_cursor,LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(map->pick_cursor,&lv_font_montserrat_36,0);
    lv_obj_set_style_text_color(map->pick_cursor,NAV_YELLOW,0);
    lv_obj_set_style_bg_color(map->pick_cursor,lv_color_black(),0);
    lv_obj_set_style_bg_opa(map->pick_cursor,LV_OPA_60,0);
    lv_obj_set_style_radius(map->pick_cursor,8,0);
    lv_obj_align(map->pick_cursor,LV_ALIGN_CENTER,0,0);
    lv_obj_clear_flag(map->pick_cursor,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(map->pick_cursor,LV_OBJ_FLAG_HIDDEN);
}
static void position_draw(lv_event_t *event)
{
    const ui_nav_map_t *map=lv_event_get_user_data(event);
    lv_area_t a;lv_obj_get_coords(lv_event_get_target(event),&a);int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
    lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);d.bg_color=lv_color_white();d.bg_opa=LV_OPA_COVER;
    double angle=(map->heading+map->render_rotation/10.0)*.01745329252,cs=cos(angle),sn=sin(angle);
    const int px[]={0,-7,7},py[]={-9,7,7};
    for(int i=0;i<3;i++)d.p[i]=(lv_point_precise_t){x+px[i]*cs-py[i]*sn,y+px[i]*sn+py[i]*cs};
    lv_draw_triangle(lv_event_get_layer(event),&d);
}

void ui_nav_map_create(ui_nav_map_t *map, lv_obj_t *parent, int32_t x, int32_t y)
{
    if(map == NULL || parent == NULL) return;
    map->root = lv_obj_create(parent);
    lv_obj_remove_style_all(map->root);
    lv_obj_set_pos(map->root, x, y);
    lv_obj_set_size(map->root, NAV_MAP_W, NAV_MAP_H);
    lv_obj_set_style_bg_color(map->root, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(map->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(map->root, UI_TOKEN_COLOR_SURFACE_BORDER, 0);
    lv_obj_set_style_border_opa(map->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(map->root, 1, 0);
    lv_obj_set_style_radius(map->root, 24, 0);
    lv_obj_set_style_clip_corner(map->root, true, 0);
    lv_obj_set_style_border_post(map->root, true, 0);
    lv_obj_add_flag(map->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(map->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(map->root, map_pointer_cb, LV_EVENT_ALL, map);

    map->map_content = lv_obj_create(map->root);
    lv_obj_remove_style_all(map->map_content);
    lv_obj_set_pos(map->map_content, NAV_CONTENT_HOME_X, NAV_CONTENT_HOME_Y);
    lv_obj_set_size(map->map_content, NAV_CONTENT_SIZE, NAV_CONTENT_SIZE);
    lv_obj_clear_flag(map->map_content, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    map->map_tiles = plain_layer(map->map_content);
    lv_obj_set_style_bg_color(map->map_tiles, lv_color_hex(0x1B272C), 0);
    lv_obj_set_style_bg_opa(map->map_tiles, LV_OPA_COVER, 0);
    map->road_layer = plain_layer(map->map_content);
    map->route_layer = plain_layer(map->map_content);
    ui_nav_geometry_attach(map);

    map->render_x = NAV_CONTENT_CENTER_X;
    map->render_y = NAV_CONTENT_CENTER_Y;
    map->target_x = map->render_x;
    map->target_y = map->render_y;
    map->render_scale = 256;
    map->render_rotation = 0;
    map->position_marker = lv_obj_create(map->map_content);
    lv_obj_remove_style_all(map->position_marker);
    lv_obj_set_pos(map->position_marker, map->render_x - NAV_MARKER_RADIUS,
                   map->render_y - NAV_MARKER_RADIUS);
    lv_obj_set_size(map->position_marker, NAV_MARKER_SIZE, NAV_MARKER_SIZE);
    lv_obj_set_style_bg_color(map->position_marker, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(map->position_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(map->position_marker, NAV_YELLOW, 0);
    lv_obj_set_style_border_width(map->position_marker, 4, 0);
    lv_obj_set_style_radius(map->position_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(map->position_marker, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    map->marker_center = lv_obj_create(map->position_marker);
    lv_obj_remove_style_all(map->marker_center);
    lv_obj_set_size(map->marker_center, 6, 6);
    lv_obj_set_style_bg_color(map->marker_center, UI_COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(map->marker_center, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(map->marker_center, LV_RADIUS_CIRCLE, 0);
    lv_obj_center(map->marker_center);
    lv_obj_add_flag(map->marker_center,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(map->position_marker,position_draw,LV_EVENT_DRAW_MAIN,map);

    map->state = NAV_FOLLOW;
    map->zoom = NAV_ZOOM_BASE;
    map->heading = 0.0f;
    map->north_up = false;
    map->model_initialized = false;
    map->origin_valid = false;
    map->provider_ready=map->strict_geometry=false;
    map->heading_valid=true;
    map->road_count=0; map->camera_step=map->camera_route=map->cruise_hold_until=0;
    map->auto_scale=256;map->zoom_bias=1.f;map->safe_height=NAV_MAP_H;
    map->meters_per_pixel = 5;
    create_hud(map);
    map->focused_control = UI_NAV_FOCUS_LOCATE;
    map->focus_visible = false;

    map->route_point_count=0;
}

bool ui_nav_map_contains_point(const ui_nav_map_t *map, int32_t x, int32_t y)
{
    if(map == NULL || map->root == NULL) return false;
    lv_area_t area;
    lv_obj_get_coords(map->root, &area);
    return x >= area.x1 && x <= area.x2 && y >= area.y1 && y <= area.y2;
}

void nav_map_set_tile(ui_nav_map_t *map, uint16_t tile_id)
{
    if(map == NULL || map->map_tiles == NULL) return;
    (void)tile_id; /* Deprecated compatibility hook; never pretends to load a tile. */
    lv_obj_set_style_bg_color(map->map_tiles, lv_color_black(), 0);
}

void nav_map_move(ui_nav_map_t *map, int32_t delta_x, int32_t delta_y, bool animated)
{
    if(map == NULL || map->map_content == NULL) return;
    lv_obj_update_layout(map->root); /* Do not overwrite a pending provider rebase. */
    int32_t to_x = clamp_content_x(lv_obj_get_x(map->map_content) + delta_x);
    int32_t to_y = clamp_content_y(lv_obj_get_y(map->map_content) + delta_y);
    if(animated) {
        ui_nav_animation_value(map->map_content, set_content_x, lv_obj_get_x(map->map_content),
                               to_x, UI_ANIM_NORMAL, lv_anim_path_ease_out);
        ui_nav_animation_value(map->map_content, set_content_y, lv_obj_get_y(map->map_content),
                               to_y, UI_ANIM_NORMAL, lv_anim_path_ease_out);
    } else {
        lv_obj_set_pos(map->map_content, to_x, to_y);
    }
}

void nav_map_zoom(ui_nav_map_t *map, uint8_t zoom, bool animated)
{
    if(map == NULL || map->map_content == NULL) return;
    if(zoom < NAV_ZOOM_MIN) zoom = NAV_ZOOM_MIN;
    if(zoom > NAV_ZOOM_MAX) zoom = NAV_ZOOM_MAX;
    map->zoom = zoom;
    int32_t scale = 256 + ((int32_t)zoom - NAV_ZOOM_BASE) * NAV_ZOOM_SCALE_STEP;
    uint32_t scale_distance = 500U;
    int32_t level_delta = (int32_t)zoom - NAV_ZOOM_BASE;
    if(level_delta > 0) scale_distance >>= (uint32_t)((level_delta + 1) / 2);
    else if(level_delta < 0) scale_distance <<= (uint32_t)((-level_delta + 1) / 2);
    lv_label_set_text_fmt(map->scale_label, "%lu m", (unsigned long)scale_distance);
    if(animated) {
        ui_nav_animation_value(map, set_map_scale, map->render_scale,
                               scale, UI_ANIM_NORMAL, lv_anim_path_ease_in_out);
        ui_nav_animation_value(map->scale_root, set_scale_opa, LV_OPA_TRANSP,
                               LV_OPA_COVER, UI_ANIM_NORMAL, lv_anim_path_ease_in);
    } else {
        set_map_scale(map, scale);
    }
}

void nav_map_update_route(ui_nav_map_t *map, const lv_point_precise_t *points,
                          uint8_t point_count)
{
    if(map == NULL || points == NULL) return;
    if(point_count > UI_NAV_ROUTE_POINT_MAX) point_count = UI_NAV_ROUTE_POINT_MAX;
    for(uint8_t i = 0; i < point_count; i++) map->route_points[i] = points[i];
    map->route_point_count = point_count;
    lv_obj_invalidate(map->route_layer);
}

void nav_map_update_position(ui_nav_map_t *map, double latitude, double longitude,
                             float heading, bool animated)
{
    if(map == NULL) return;
    if(!isfinite(latitude) || !isfinite(longitude) || !isfinite(heading)) return;
    if(map->demo && !map->strict_geometry) {
        map->target_x = NAV_CONTENT_CENTER_X + (int32_t)((longitude - NAV_MOCK_ORIGIN_LON) * 15000.0f);
        map->target_y = NAV_CONTENT_CENTER_Y - (int32_t)((latitude - NAV_MOCK_ORIGIN_LAT) * 15000.0f);
    }
    else {
        if(!map->origin_valid) {
            map->origin_latitude = latitude;
            map->origin_longitude = longitude;
            map->origin_valid = true;
        }
        double x = (longitude - map->origin_longitude) * 111319.490 *
                   cos(map->origin_latitude * 0.0174532925199433) / map->meters_per_pixel;
        double y = (latitude - map->origin_latitude) * 111319.490 / map->meters_per_pixel;
        map->target_x = NAV_CONTENT_CENTER_X + (int32_t)LV_CLAMP(-100000, x, 100000);
        map->target_y = NAV_CONTENT_CENTER_Y - (int32_t)LV_CLAMP(-100000, y, 100000);
    }
    if(map->target_x < 30) map->target_x = 30;
    if(map->target_x > NAV_CONTENT_SIZE - 30) map->target_x = NAV_CONTENT_SIZE - 30;
    if(map->target_y < 30) map->target_y = 30;
    if(map->target_y > NAV_CONTENT_SIZE - 30) map->target_y = NAV_CONTENT_SIZE - 30;
    map->heading = fmodf(fmodf(heading, 360.0f) + 360.0f, 360.0f);
    if(!map->pointer_down && !map->north_up && map->heading_valid) {
        ui_nav_animation_value(map, set_map_rotation, map->render_rotation,
                               nearest_rotation(map->render_rotation, -(int32_t)(map->heading * 10.0f)), UI_ANIM_NORMAL,
                               lv_anim_path_ease_in_out);
    }
    if(animated) {
        ui_nav_animation_value(map, set_marker_x, map->render_x, map->target_x,
                               UI_ANIM_NORMAL, lv_anim_path_ease_in_out);
        ui_nav_animation_value(map, set_marker_y, map->render_y, map->target_y,
                               UI_ANIM_NORMAL, lv_anim_path_ease_in_out);
    } else {
        set_marker_x(map, map->target_x);
        set_marker_y(map, map->target_y);
    }
    follow_render_position(map);
}

void ui_nav_map_apply_model(ui_nav_map_t *map, const nav_model_t *model)
{
    if(map == NULL || model == NULL) return;
    if(!map->model_initialized) nav_map_zoom(map, model->zoom == 0 ? NAV_ZOOM_BASE : model->zoom, false);
    map->model_initialized = true;
    map->demo = model->demo;
    map->off_route=model->off_route||model->ui_state==NAV_UI_REROUTING;
    if(model->session_bound && map->provider_ready && model->coordinate_system!=map->coordinate_system) {
        lv_obj_add_flag(map->position_marker,LV_OBJ_FLAG_HIDDEN); return;
    }
    if(model->session_bound && !map->provider_ready) {
        if(model->coordinate_system!=map->coordinate_system) map->origin_valid=false;
        map->coordinate_system=model->coordinate_system;
    }
    map->heading_valid=!model->session_bound||model->heading_valid;
    if(model->session_bound) map->strict_geometry=true;
    if((model->demo && !model->session_bound)||map->provider_ready) lv_obj_clear_flag(map->road_layer, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(map->road_layer, LV_OBJ_FLAG_HIDDEN);
    if(!map->provider_ready && !model->demo && model->map_geometry_valid && isfinite(model->meters_per_pixel) &&
       model->meters_per_pixel > 0.01f && isfinite(model->map_origin_latitude) &&
       fabs(model->map_origin_latitude) <= 90 && isfinite(model->map_origin_longitude) &&
       fabs(model->map_origin_longitude) <= 180) {
        map->origin_latitude = model->map_origin_latitude;
        map->origin_longitude = model->map_origin_longitude;
        map->meters_per_pixel = model->meters_per_pixel;
        map->origin_valid = true;
    }
    if(!model->demo && !map->provider_ready) {
        if(!map->origin_valid && model->position_valid && isfinite(model->latitude) &&
           isfinite(model->longitude) && fabs(model->latitude) <= 90 && fabs(model->longitude) <= 180) {
            map->origin_latitude = model->latitude;
            map->origin_longitude = model->longitude;
            map->origin_valid = true;
        }
        if(map->render_scale > 0)
            lv_label_set_text_fmt(map->scale_label, "%lu m", (unsigned long)lroundf(
                84.0f * map->meters_per_pixel * 256.0f / map->render_scale));
        lv_point_precise_t *points=map->route_points;
        uint8_t count = 0;
        for(unsigned i = 0; map->origin_valid && i < model->route_point_count && i < UI_NAV_ROUTE_POINT_MAX; ++i) {
            double lat = model->route_latitude[i], lon = model->route_longitude[i];
            if(!isfinite(lat) || !isfinite(lon) || fabs(lat) > 90 || fabs(lon) > 180) break;
            double x = (lon - map->origin_longitude) * 111319.490 *
                       cos(map->origin_latitude * 0.0174532925199433) / map->meters_per_pixel;
            double y = (lat - map->origin_latitude) * 111319.490 / map->meters_per_pixel;
            points[count++] = (lv_point_precise_t){NAV_CONTENT_CENTER_X + (int32_t)LV_CLAMP(-10000, x, 10000),
                NAV_CONTENT_CENTER_Y - (int32_t)LV_CLAMP(-10000, y, 10000)};
        }
        nav_map_update_route(map, points, count);
    }
    bool valid = (model->demo && !model->session_bound) || (model->position_valid &&
        isfinite(model->latitude) && isfinite(model->longitude) && fabs(model->latitude) <= 90 && fabs(model->longitude) <= 180 &&
        (model->stale_after_ms == 0 || (uint32_t)(lv_tick_get() - model->sampled_at_ms) <= model->stale_after_ms));
    if(valid) {
        map->position_seen=true;lv_obj_set_style_opa(map->position_marker,LV_OPA_COVER,0);
        lv_obj_clear_flag(map->position_marker, LV_OBJ_FLAG_HIDDEN);
        nav_map_update_position(map, model->latitude, model->longitude, map->heading_valid?model->heading:map->heading, true);
    }
    else if(map->position_seen) {lv_obj_clear_flag(map->position_marker,LV_OBJ_FLAG_HIDDEN);lv_obj_set_style_opa(map->position_marker,LV_OPA_50,0);}
    else lv_obj_add_flag(map->position_marker, LV_OBJ_FLAG_HIDDEN);
}

static lv_point_precise_t local_point(const ui_nav_map_t *map,ui_nav_point_t p)
{
    double x=(p.longitude-map->origin_longitude)*111319.49*cos(map->origin_latitude*0.0174532925199433)/map->meters_per_pixel;
    double y=(p.latitude-map->origin_latitude)*111319.49/map->meters_per_pixel;
    return (lv_point_precise_t){512+(int32_t)LV_CLAMP(-20000,x,20000),512-(int32_t)LV_CLAMP(-20000,y,20000)};
}
void ui_nav_map_apply_frame(ui_nav_map_t *map,const ui_nav_map_frame_t *frame)
{
    if(!map || !frame) return;
    if(frame->road_count>UI_NAV_MAP_ROAD_MAX || frame->point_count>UI_NAV_WINDOW_POINT_MAX ||
        frame->area_count>UI_NAV_MAP_AREA_MAX || frame->label_count>UI_NAV_MAP_LABEL_MAX ||
        ((frame->status==UI_NAV_OK || (frame->status==UI_NAV_NO_MAP && frame->point_count>=2)) &&
            (!isfinite(frame->meters_per_pixel)||frame->meters_per_pixel<0.05f))) return;
    map->strict_geometry=true;
    if(frame->status!=UI_NAV_OK && !(frame->status==UI_NAV_NO_MAP && frame->point_count>=2)) {
        if(map->raster_valid)return; /* Keep the last complete map during transient I/O failures. */
        map->provider_ready=false; map->road_count=0;
        lv_obj_add_flag(map->road_layer,LV_OBJ_FLAG_HIDDEN); ui_nav_geometry_invalidate(map); return;
    }
    const uint16_t *raster_pixels=frame->raster_id?ui_nav_raster_take(frame->raster_id):NULL;
    if(frame->raster_id&&!raster_pixels)return; /* A cancelled buffer must not rebase the displayed map. */
    ui_nav_point_t old_marker={map->origin_latitude-(map->render_y-512)*map->meters_per_pixel/111319.49,
        map->origin_longitude+(map->render_x-512)*map->meters_per_pixel/(111319.49*cos(map->origin_latitude*0.0174532925199433)),map->coordinate_system};
    bool old_origin=map->origin_valid;float old_mpp=map->meters_per_pixel;ui_nav_point_t old_center=ui_nav_map_view_center(map);
    map->origin_latitude=frame->origin.latitude; map->origin_longitude=frame->origin.longitude;
    map->coordinate_system=frame->origin.system; map->meters_per_pixel=frame->meters_per_pixel;
    map->origin_valid=map->provider_ready=true;
    int32_t zoom_end=0;uint32_t zoom_remaining=0;
    if(old_origin&&frame->raster_id&&old_mpp>0&&old_mpp!=frame->meters_per_pixel){
        float factor=frame->meters_per_pixel/old_mpp;
        lv_anim_t *animation=lv_anim_get(map,set_map_scale);
        if(animation){zoom_end=lroundf(animation->end_value*factor);zoom_remaining=LV_MAX(1,animation->duration-animation->act_time);}
        lv_anim_delete(map,set_map_scale);
        map->render_scale=LV_CLAMP(8,lroundf(map->render_scale*factor),4096);
        map->auto_scale=LV_CLAMP(8,lroundf(map->auto_scale*factor),4096);
    }
    map->detail_frame=*frame;
    if(frame->raster_id){
        const uint16_t *pixels=raster_pixels;
        if(pixels){
            map->raster_image=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=768,.h=768,.stride=1536},.data_size=768*768*2,.data=(const uint8_t *)pixels};
            map->raster_valid=true;
        }
    }else map->raster_valid=false;
    lv_anim_delete(map,set_marker_x);lv_anim_delete(map,set_marker_y);
    if(old_origin) {
        lv_point_precise_t marker=local_point(map,old_marker); map->render_x=map->target_x=marker.x; map->render_y=map->target_y=marker.y;
    }
    /* Keep the browsed center stable when the provider rebases its local frame. */
    if((map->state==NAV_EXPLORE||map->pointer_down)&&old_origin){
        lv_point_precise_t point=ui_nav_geometry_transform_point(map,local_point(map,old_center));
        lv_obj_set_pos(map->map_content,NAV_MAP_W/2-point.x,map->safe_height/2-point.y);
    }
    map->road_count=frame->status==UI_NAV_OK?frame->road_count:0;
    for(unsigned i=0;i<frame->road_count;i++) {
        map->road_a[i]=local_point(map,frame->roads[i].a); map->road_b[i]=local_point(map,frame->roads[i].b);
        map->road_level[i]=frame->roads[i].level;
    }
    lv_point_precise_t *points=map->route_points;
    for(unsigned i=0;i<frame->point_count;i++) points[i]=local_point(map,frame->points[i]);
    nav_map_update_route(map,points,frame->point_count);
    lv_obj_clear_flag(map->road_layer,LV_OBJ_FLAG_HIDDEN); set_map_scale(map,map->render_scale);
    if(map->state==NAV_OVERVIEW)ui_nav_map_set_overview(map,true);
    else if(zoom_remaining)ui_nav_animation_value(map,set_map_scale,map->render_scale,LV_CLAMP(8,zoom_end,4096),zoom_remaining,lv_anim_path_ease_out);
}
void ui_nav_map_set_safe_height(ui_nav_map_t *map,int32_t height)
{
 if(!map)return;int32_t next=LV_CLAMP(140,height,NAV_MAP_H);if(next==map->safe_height)return;
 map->safe_height=next;
 lv_obj_set_y(map->locate.root,next-60);lv_obj_set_y(map->zoom_in.root,next-188);lv_obj_set_y(map->zoom_out.root,next-132);lv_obj_set_pos(map->scale_root,12,next-44);
 if(map->state==NAV_OVERVIEW)ui_nav_map_set_overview(map,true);else follow_render_position(map);
}
void ui_nav_map_resize_view(ui_nav_map_t *map,int32_t y,int32_t height)
{
 if(!map)return;int previous=map->safe_height;lv_obj_update_layout(map->root);int content_y=lv_obj_get_y(map->map_content);
 ui_nav_map_set_safe_height(map,height);lv_obj_set_y(map->root,y);lv_obj_set_height(map->root,map->safe_height);lv_obj_set_height(map->map_hud,map->safe_height);
 if(map->state==NAV_EXPLORE)lv_obj_set_y(map->map_content,content_y+map->safe_height/2-previous/2);
}
void ui_nav_map_center_point(ui_nav_map_t *map,ui_nav_point_t point)
{
 if(!map||!map->origin_valid||point.system!=map->coordinate_system)return;map_set_state(map,NAV_EXPLORE);
 ui_motion_cancel(map->map_content,set_content_x);ui_motion_cancel(map->map_content,set_content_y);ui_motion_cancel(map,set_map_scale);ui_motion_cancel(map,set_map_rotation);
 lv_point_precise_t p=ui_nav_geometry_transform_point(map,local_point(map,point));lv_obj_set_pos(map->map_content,NAV_MAP_W/2-p.x,map->safe_height/2-p.y);
 map->pick_point=point;map->pick_point_valid=true;ui_nav_geometry_invalidate(map);
}
void ui_nav_map_camera(ui_nav_map_t *map,const nav_model_t *model,uint32_t now_ms)
{
 if(!map||map->pointer_down||!model||!model->session_bound||!map->origin_valid||
  (map->state!=NAV_FOLLOW&&map->state!=NAV_FOLLOW_USER_ZOOM)||!model->position_valid||!model->heading_valid||model->speed<3)return;
 bool junction=model->junction_valid&&model->guidance_valid&&
  (model->ui_state==NAV_UI_APPROACH||model->ui_state==NAV_UI_IMMINENT||model->ui_state==NAV_UI_NOW);
 int32_t scale=256*map->zoom_bias*(map->raster_valid?map->meters_per_pixel/3.f:1.f);
 if(junction){
  double dx=(model->junction_longitude-model->longitude)*111319.49*cos(model->latitude*.0174532925199433);
  double dy=(model->junction_latitude-model->latitude)*111319.49;
  if(!isfinite(dx)||!isfinite(dy))return;
  double a=map->render_rotation*.00174532925199433;
  double rx=fabs(dx*cos(a)-dy*sin(a)),ry=fabs(dx*sin(a)+dy*cos(a));
  /* Fit rider + junction in the unobscured map. Bias cannot crop the junction. */
  double fit=fmin((NAV_MAP_W/2.-60)/fmax(25,rx),(map->safe_height*.64-48)/fmax(50,ry));
  if(model->exit_position_valid){
   double ex=(model->exit_longitude-model->longitude)*111319.49*cos(model->latitude*.0174532925199433);
   double ey=(model->exit_latitude-model->latitude)*111319.49;
   fit=fmin(fit,fmin((NAV_MAP_W/2.-60)/fmax(25,fabs(ex*cos(a)-ey*sin(a))),
                    (map->safe_height*.64-48)/fmax(50,fabs(ex*sin(a)+ey*cos(a)))));
  }
  scale=LV_CLAMP(64,fit*map->meters_per_pixel*256*map->zoom_bias,768);
  scale=LV_MIN(scale,(int32_t)(fit*map->meters_per_pixel*256));map->cruise_hold_until=now_ms+2000;
 }else if((int32_t)(now_ms-map->cruise_hold_until)<0)return;
 scale=LV_CLAMP(16,scale,768);
 if(abs(scale-map->auto_scale)<24&&map->camera_step==model->step_id&&map->camera_route==model->route_revision)return;
 map->auto_scale=scale;map->camera_step=model->step_id;map->camera_route=model->route_revision;
 ui_nav_animation_value(map,set_map_scale,map->render_scale,scale,320,lv_anim_path_ease_in_out);
}
ui_nav_point_t ui_nav_map_view_center(const ui_nav_map_t *map)
{
    if(!map || !map->origin_valid) return (ui_nav_point_t){0};
    /* Inverse camera transform; used to asynchronously fetch the visible window. */
    double dx=(NAV_MAP_W/2-lv_obj_get_x(map->map_content)-512)*256.0/map->render_scale;
    double dy=(map->safe_height/2-lv_obj_get_y(map->map_content)-512)*256.0/map->render_scale;
    double a=-map->render_rotation*0.00174532925199433;
    double x=dx*cos(a)-dy*sin(a),y=dx*sin(a)+dy*cos(a);
    return (ui_nav_point_t){map->origin_latitude-y*map->meters_per_pixel/111319.49,
        map->origin_longitude+x*map->meters_per_pixel/(111319.49*cos(map->origin_latitude*0.0174532925199433)),map->coordinate_system};
}

void ui_nav_map_request_coverage(const ui_nav_map_t *map,ui_nav_map_request_t *request)
{
    if(!map || !request || map->render_scale<=0 || !isfinite(request->meters_per_pixel) || request->meters_per_pixel<=0) return;
    double radians=(map->render_rotation/10)*0.0174532925199433;
    double factor=map->meters_per_pixel*256.0/(map->render_scale*request->meters_per_pixel);
    double c=fabs(cos(radians)),sn=fabs(sin(radians));
    /* North-aligned bounds in projection pixels, including a 20-screen-pixel margin. */
    request->width=(uint16_t)LV_CLAMP(1,ceil((c*(NAV_MAP_W+40)+sn*(map->safe_height+40))*factor),65535);
    request->height=(uint16_t)LV_CLAMP(1,ceil((sn*(NAV_MAP_W+40)+c*(map->safe_height+40))*factor),65535);
    if(map->origin_valid) request->center=ui_nav_map_view_center(map);
}

void ui_nav_map_activate_focus(ui_nav_map_t *map)
{
    if(!map) return;
    if(map->focused_control==UI_NAV_FOCUS_PICK){lv_area_t a;lv_obj_get_coords(map->root,&a);ui_nav_map_select_at(map,a.x1+NAV_MAP_W/2,a.y1+map->safe_height/2);return;}
    ui_nav_control_t *controls[]={&map->compass,&map->zoom_in,&map->zoom_out,&map->locate};
    if(map->focused_control<UI_NAV_FOCUS_PICK) ui_nav_control_activate(controls[map->focused_control]);
}

bool ui_nav_map_point_at(const ui_nav_map_t *map,int32_t x,int32_t y,ui_nav_point_t *point)
{
    if(!map||!point||!map->origin_valid||map->render_scale<=0||!ui_nav_map_contains_point(map,x,y))return false;
    /* Camera/provider updates can precede the next display refresh. Read
     * current object coordinates instead of a previous frame's cached area. */
    lv_obj_update_layout(map->map_content);
    lv_area_t area;lv_obj_get_coords(map->map_content,&area);
    double dx=(x-area.x1-512)*256./map->render_scale,dy=(y-area.y1-512)*256./map->render_scale;
    double a=-map->render_rotation*.00174532925199433,px=dx*cos(a)-dy*sin(a),py=dx*sin(a)+dy*cos(a);
    *point=(ui_nav_point_t){map->origin_latitude-py*map->meters_per_pixel/111319.49,
        map->origin_longitude+px*map->meters_per_pixel/(111319.49*cos(map->origin_latitude*.01745329252)),map->coordinate_system};
    return isfinite(point->latitude)&&isfinite(point->longitude)&&fabs(point->latitude)<=85&&fabs(point->longitude)<=180;
}
bool ui_nav_map_select_at(ui_nav_map_t *map,int32_t x,int32_t y)
{
    ui_nav_point_t point;
    if(!map||!map->provider_ready||!map->pick_callback||ui_nav_session_active()||!ui_nav_map_point_at(map,x,y,&point))return false;
    map->pick_point=point;map->pick_point_valid=true;map->pick_claimed=true;
    map->pick_callback(point,map->pick_context);ui_nav_geometry_invalidate(map);return true;
}
void ui_nav_map_set_pick_mode(ui_nav_map_t *map,bool enabled)
{
    if(!map)return;map->pick_mode=enabled;
    if(map->pick_cursor){if(enabled)lv_obj_clear_flag(map->pick_cursor,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(map->pick_cursor,LV_OBJ_FLAG_HIDDEN);}
}

void ui_nav_map_handle_input(ui_nav_map_t *map, int32_t zoom_delta, bool locate)
{
    if(map == NULL) return;
    ui_nav_control_t *control = NULL;
    if(locate) {
        ui_nav_map_set_focus(map, UI_NAV_FOCUS_LOCATE, true);
        control = &map->locate;
    }
    else if(zoom_delta > 0) {
        ui_nav_map_set_focus(map, UI_NAV_FOCUS_ZOOM_IN, true);
        control = &map->zoom_in;
    }
    else if(zoom_delta < 0) {
        ui_nav_map_set_focus(map, UI_NAV_FOCUS_ZOOM_OUT, true);
        control = &map->zoom_out;
    }
    if(control == NULL) return;
    ui_nav_control_activate(control);
}
