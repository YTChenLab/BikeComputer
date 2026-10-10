#include "ui/core/ui_memory.h"
#include "ui_nav_geometry.h"
#include "../../core/ui_text.h"
#include "lvgl/src/draw/lv_draw_line.h"
#include "lvgl/src/draw/lv_draw_triangle.h"
#include "lvgl/src/draw/lv_draw_label.h"
#include "lvgl/src/draw/lv_draw_arc.h"
#include <math.h>
#include <string.h>
lv_point_precise_t ui_nav_geometry_transform_point(const ui_nav_map_t *m,lv_point_precise_t p)
{
    int32_t dx=((int32_t)p.x-512)*m->render_scale/256,dy=((int32_t)p.y-512)*m->render_scale/256;
    int angle=(m->render_rotation/10)%360;if(angle<0)angle+=360;
    int32_t sn=lv_trigo_sin(angle),cs=lv_trigo_sin(angle+90);
    return (lv_point_precise_t){512+((int64_t)dx*cs-(int64_t)dy*sn)/(1<<LV_TRIGO_SHIFT),512+((int64_t)dx*sn+(int64_t)dy*cs)/(1<<LV_TRIGO_SHIFT)};
}
static void line(lv_layer_t *l,lv_point_precise_t a,lv_point_precise_t b,int w,uint32_t color,lv_opa_t opacity)
{
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.p1=a;d.p2=b;d.width=w;d.color=lv_color_hex(color);d.opa=opacity;d.round_start=d.round_end=1;lv_draw_line(l,&d);
}
static void triangle(lv_layer_t *l,lv_point_precise_t a,lv_point_precise_t b,lv_point_precise_t c,uint32_t color)
{
    lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);d.p[0]=a;d.p[1]=b;d.p[2]=c;d.bg_color=lv_color_hex(color);d.bg_opa=LV_OPA_COVER;lv_draw_triangle(l,&d);
}
static lv_point_precise_t project(const lv_area_t *a,const ui_nav_map_frame_t *f,ui_nav_local_point_t p,const ui_nav_map_t *m,float scale)
{
    float x=p.east_m/f->meters_per_pixel,y=-p.north_m/f->meters_per_pixel;lv_point_precise_t q;
    if(m)q=ui_nav_geometry_transform_point(m,(lv_point_precise_t){512+(int)LV_CLAMP(-20000,x,20000),512+(int)LV_CLAMP(-20000,y,20000)});
    else q=(lv_point_precise_t){lv_area_get_width(a)/2+x*scale,lv_area_get_height(a)/2+y*scale};
    q.x+=a->x1;q.y+=a->y1;return q;
}
static ui_nav_local_point_t local(const ui_nav_map_frame_t *f,ui_nav_point_t p)
{return (ui_nav_local_point_t){(p.longitude-f->origin.longitude)*111319.49*cos(f->origin.latitude*.01745329252),(p.latitude-f->origin.latitude)*111319.49};}
static void details(lv_layer_t *l,const lv_area_t *a,const ui_nav_map_frame_t *f,const ui_nav_map_t *m,float scale)
{
    for(unsigned i=0;i<f->area_count;i++) {
        const ui_nav_area_t *p=&f->areas[i];if(p->count<3||p->count>UI_NAV_MAP_AREA_POINT_MAX)continue;
        uint32_t color=p->kind==UI_NAV_AREA_WATER?0x13252F:p->kind==UI_NAV_AREA_PARK?0x12221A:0x25272A;
        lv_point_precise_t points[UI_NAV_MAP_AREA_POINT_MAX];for(unsigned j=0;j<p->count;j++)points[j]=project(a,f,p->points[j],m,scale);
        for(unsigned j=1;j+1<p->count;j++)triangle(l,points[0],points[j],points[j+1],color);
        if(p->kind==UI_NAV_AREA_BUILDING)for(unsigned j=0;j<p->count;j++)line(l,points[j],points[(j+1)%p->count],2,j==0?0x35383B:0x191B1D,LV_OPA_COVER);
    }
    if(f->status!=UI_NAV_OK)return;
    for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<f->road_count;i++) {
        lv_point_precise_t p=project(a,f,local(f,f->roads[i].a),m,scale),q=project(a,f,local(f,f->roads[i].b),m,scale);
        int w=LV_CLAMP(3,(f->roads[i].level==0?12:f->roads[i].level==1?7:3)*scale,22);
        if(!pass)line(l,p,q,w+3,0x101112,LV_OPA_COVER);
        else line(l,p,q,w,f->roads[i].level==0?0x4C5053:f->roads[i].level==1?0x34373B:0x25292C,LV_OPA_COVER);
    }
    static UI_LARGE_RAM lv_area_t used[UI_NAV_MAP_LABEL_MAX];unsigned accepted=0;
    for(unsigned i=0;i<f->label_count;i++) {
        lv_point_precise_t p=project(a,f,f->labels[i].point,m,scale);
        char text[64];const lv_font_t *font;ui_text_prepare(f->labels[i].text,20,text,sizeof(text),&font);
        int maximum=f->labels[i].kind>=3?196:f->labels[i].kind==1?156:124;
        size_t length=strlen(text);bool shortened=false;
        while(length&&lv_text_get_width(text,length,font,0)>maximum){
            length--;while(length&&((uint8_t)text[length]&0xC0)==0x80)length--;
            text[length]=0;shortened=true;
        }
        if(shortened){
            while(length&&lv_text_get_width(text,length,font,0)+lv_text_get_width("…",3,font,0)>maximum){length--;while(length&&((uint8_t)text[length]&0xC0)==0x80)length--;text[length]=0;}
            memcpy(text+length,"…",4);
        }
        int text_width=lv_text_get_width(text,strlen(text),font,0);
        lv_area_t box={p.x-text_width/2-6,p.y-13,p.x+(text_width+1)/2+6,p.y+13};
        if(box.x2<a->x1||box.x1>a->x2||box.y2<a->y1||box.y1>a->y2)continue;
        if(m){
            lv_area_t view;lv_obj_get_coords(m->root,&view);
            if(box.x2<view.x1||box.x1>view.x2||box.y2<view.y1||box.y1>view.y2)continue;
            if(p.x<view.x1||p.x>view.x2||box.y1<view.y1+6||box.y2>view.y2-6)continue;
            int centered=LV_CLAMP(view.x1+text_width/2+8,p.x,view.x2-(text_width+1)/2-8);
            int shift=centered-p.x;p.x=centered;box.x1+=shift;box.x2+=shift;
            const lv_obj_t *controls[]={m->compass.root,m->zoom_in.root,m->zoom_out.root,m->locate.root,m->scale_root};bool hud=false;
            for(unsigned j=0;j<5;j++){if(lv_obj_has_flag(controls[j],LV_OBJ_FLAG_HIDDEN))continue;lv_area_t r;lv_obj_get_coords(controls[j],&r);if(box.x1<=r.x2+4&&box.x2>=r.x1-4&&box.y1<=r.y2+4&&box.y2>=r.y1-4)hud=true;}
            if(box.y1<view.y1+62&&box.x1<view.x1+360)hud=true;
            if(hud)continue;
        }else if(box.x2>a->x2-70)continue;
        bool overlap=false;for(unsigned j=0;j<accepted;j++)if(box.x1<=used[j].x2&&box.x2>=used[j].x1&&box.y1<=used[j].y2&&box.y2>=used[j].y1)overlap=true;
        if(overlap)continue;used[accepted++]=box;
        /* Draw one bounded line. Wrapped text could extend outside the declared
         * draw area and lose its last pixels at partial-refresh boundaries. */
        lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.font=font;d.text=text;d.text_local=1;d.align=LV_TEXT_ALIGN_CENTER;
        d.flag=LV_TEXT_FLAG_EXPAND;
        if(m&&m->raster_valid){
            int width=lv_text_get_width(text,strlen(text),font,0);lv_area_t plate={p.x-width/2-4,box.y1,p.x+width/2+4,box.y2};
            lv_draw_rect_dsc_t bg;lv_draw_rect_dsc_init(&bg);bg.radius=4;bg.bg_color=lv_color_hex(0x132025);bg.bg_opa=LV_OPA_60;
            lv_draw_rect(l,&bg,&plate);
        }
        const uint32_t colors[]={0xDAE3E8,0x9CCBA7,0x8FCBE8,0xE0CBA2,0xF4D789,0x82C4F0,0xC2BEDD,0xDAE3E8};
        d.color=lv_color_hex(colors[f->labels[i].kind<=7?f->labels[i].kind:0]);lv_draw_label(l,&d,&box);
    }
}
static void route_segment(lv_layer_t *l,lv_point_precise_t p,lv_point_precise_t q,bool dim,unsigned pass)
{
    double dx=q.x-p.x,dy=q.y-p.y,distance=hypot(dx,dy);if(distance<1)return;double x=dx/distance,y=dy/distance;
    if(dim){if(pass==0)for(double t=0;t<distance;t+=22)line(l,(lv_point_precise_t){p.x+x*t,p.y+y*t},(lv_point_precise_t){p.x+x*fmin(t+12,distance),p.y+y*fmin(t+12,distance)},6,0x8C8037,LV_OPA_70);return;}
    if(pass==0){line(l,p,q,16,0x171610,LV_OPA_COVER);return;}
    if(pass==1){line(l,p,q,11,0xFFE600,LV_OPA_COVER);return;}
    if(pass==2){line(l,p,q,2,0xFFF254,LV_OPA_80);return;}
    for(double t=50;t<distance-20;t+=88){double cx=p.x+x*t,cy=p.y+y*t;lv_point_precise_t tip={cx+x*6,cy+y*6},left={cx-x*3-y*5,cy-y*3+x*5},right={cx-x*3+y*5,cy-y*3-x*5};line(l,left,tip,3,0xFFFFFF,LV_OPA_COVER);line(l,tip,right,3,0xFFFFFF,LV_OPA_COVER);}
}
static void marker(lv_layer_t *l,lv_point_precise_t p,bool finish)
{
    lv_draw_rect_dsc_t bg;lv_draw_rect_dsc_init(&bg);bg.bg_color=lv_color_black();bg.bg_opa=LV_OPA_COVER;bg.radius=LV_RADIUS_CIRCLE;
    lv_area_t circle={p.x-12,p.y-12,p.x+12,p.y+12};lv_draw_rect(l,&bg,&circle);
    lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.center=(lv_point_t){p.x,p.y};d.radius=12;d.width=4;d.start_angle=0;d.end_angle=360;d.opa=LV_OPA_COVER;d.color=lv_color_hex(0xFFE600);lv_draw_arc(l,&d);
    if(!finish)triangle(l,(lv_point_precise_t){p.x,p.y-7},(lv_point_precise_t){p.x-6,p.y+5},(lv_point_precise_t){p.x+6,p.y+5},0xFFFFFF);
    else for(int row=0;row<2;row++)for(int col=0;col<2;col++)if((row+col)%2==0){lv_point_precise_t a={p.x-6+col*6,p.y-6+row*6},b={a.x+6,a.y},c={a.x+6,a.y+6},e={a.x,a.y+6};triangle(l,a,b,c,0xFFFFFF);triangle(l,a,c,e,0xFFFFFF);}
}
void ui_nav_geometry_draw_frame(lv_layer_t *l,const lv_area_t *a,const ui_nav_map_frame_t *f,float scale)
{
    if(!f||!isfinite(f->meters_per_pixel)||f->meters_per_pixel<.05||(f->status!=UI_NAV_OK&&!(f->status==UI_NAV_NO_MAP&&f->point_count>=2)))return;
    details(l,a,f,NULL,scale);for(unsigned pass=0;pass<4;pass++)for(unsigned i=1;i<f->point_count;i++)route_segment(l,project(a,f,local(f,f->points[i-1]),NULL,scale),project(a,f,local(f,f->points[i]),NULL,scale),false,pass);
    if(f->point_count)marker(l,project(a,f,local(f,f->points[0]),NULL,scale),false);
    if(f->destination_valid)marker(l,project(a,f,local(f,f->destination),NULL,scale),true);
}
static void roads_draw(lv_event_t *e)
{
    ui_nav_map_t *m=lv_event_get_user_data(e);if(!m)return;lv_area_t a;lv_obj_get_coords(lv_event_get_target(e),&a);
    if(m->raster_valid&&m->raster_image.data){
        lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&m->raster_image;d.rotation=m->render_rotation;
        d.scale_y=m->render_scale;
        d.scale_x=lround(m->render_scale*cos(m->origin_latitude*.01745329252)/cos(m->detail_frame.raster_latitude_ref*.01745329252));
        d.pivot=(lv_point_t){384,384};d.antialias=true;
        lv_area_t image_area={a.x1+128,a.y1+128,a.x1+895,a.y1+895};lv_draw_image(lv_event_get_layer(e),&d,&image_area);
        details(lv_event_get_layer(e),&a,&m->detail_frame,m,m->render_scale/256.f);
    }else if(m->provider_ready)details(lv_event_get_layer(e),&a,&m->detail_frame,m,m->render_scale/256.f);
}
static void route_draw(lv_event_t *e)
{
    ui_nav_map_t *m=lv_event_get_user_data(e);if(!m)return;lv_area_t a;lv_obj_get_coords(lv_event_get_target(e),&a);lv_layer_t *l=lv_event_get_layer(e);
    for(unsigned pass=0;pass<4;pass++)for(unsigned i=1;i<m->route_point_count;i++){lv_point_precise_t p=ui_nav_geometry_transform_point(m,m->route_points[i-1]),q=ui_nav_geometry_transform_point(m,m->route_points[i]);p.x+=a.x1;p.y+=a.y1;q.x+=a.x1;q.y+=a.y1;route_segment(l,p,q,m->off_route,pass);}
    if(m->provider_ready&&m->detail_frame.destination_valid)marker(l,project(&a,&m->detail_frame,local(&m->detail_frame,m->detail_frame.destination),m,1),true);
    else if(m->pick_point_valid)marker(l,project(&a,&m->detail_frame,local(&m->detail_frame,m->pick_point),m,1),true);
}
void ui_nav_geometry_attach(ui_nav_map_t *m)
{lv_obj_add_event_cb(m->road_layer,roads_draw,LV_EVENT_DRAW_MAIN,m);lv_obj_add_event_cb(m->route_layer,route_draw,LV_EVENT_DRAW_MAIN,m);}
void ui_nav_geometry_invalidate(ui_nav_map_t *m)
{if(m){lv_obj_invalidate(m->road_layer);lv_obj_invalidate(m->route_layer);}}
