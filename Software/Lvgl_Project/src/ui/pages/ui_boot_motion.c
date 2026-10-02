#include "ui_boot_motion.h"
#include "ui_boot_prts_assets.h"

typedef struct {
    lv_obj_t *view, *diamond, *backdrop, *words, *ticks[2];
    lv_obj_t *tower, *prts, *information, *analysis, *login_clip, *login;
    lv_obj_t *track, *rail, *field[2], *field_name[2], *value[2], *cursor[2];
    lv_timer_t *timer;
    uint32_t started;
    uint8_t typed[2];
    bool completed;
} boot_motion_t;

static int32_t phase(uint32_t t, uint32_t begin, uint32_t end)
{
    if(t <= begin) return 0;
    if(t >= end) return 1000;
    return (int32_t)((t - begin) * 1000U / (end - begin));
}

static int32_t smooth(int32_t p)
{
    return p * p / 1000 * (3000 - 2 * p) / 1000;
}

static lv_opa_t opa(int32_t p)
{
    return (lv_opa_t)(255 * p / 1000);
}

static uint8_t characters(uint32_t t, uint32_t begin, uint32_t step, uint8_t maximum)
{
    if(t <= begin) return 0;
    const uint32_t count = (t - begin) / step;
    return (uint8_t)(count > maximum ? maximum : count);
}

static lv_obj_t *box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    return obj;
}

static lv_obj_t *image(lv_obj_t *parent, const lv_image_dsc_t *source, int32_t x, int32_t y)
{
    lv_obj_t *obj = lv_image_create(parent);
    lv_image_set_src(obj, source);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_image_recolor(obj, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static void image_opacity(lv_obj_t *obj, int32_t p)
{
    /* Per-part alpha avoids a full-screen intermediate compositing layer. */
    lv_obj_set_style_image_opa(obj, opa(p), 0);
}

static void render(boot_motion_t *ctx, uint32_t t)
{
    if(t > UI_BOOT_MOTION_DURATION_MS) t = UI_BOOT_MOTION_DURATION_MS;
    const int32_t first = 1000 - smooth(phase(t, 1980, 2240));
    const int32_t second = smooth(phase(t, 2160, 2460));
    const int32_t lift = smooth(phase(t,1980,2240)) * 24 / 1000;
    lv_obj_set_y(ctx->backdrop,251-lift);
    lv_obj_set_y(ctx->diamond,251-lift);
    lv_obj_set_y(ctx->words,394-lift);
    const int32_t frame_alpha = phase(t, 0, 240) * first / 1000;
    const int32_t rotation = 90000 * smooth(phase(t,650,1000)) / 1000
                           + 90000 * smooth(phase(t,1100,1450)) / 1000
                           + 90000 * smooth(phase(t,1550,1900)) / 1000;
    /* LVGL uses tenths of a degree. Only the transparent frame rotates. */
    lv_image_set_rotation(ctx->diamond, rotation / 100);
    image_opacity(ctx->diamond, frame_alpha);
    image_opacity(ctx->backdrop, phase(t,80,500) * first / 1000 * 80 / 100);
    image_opacity(ctx->words, smooth(phase(t,180,780)) * first / 1000);
    for(unsigned i = 0; i < 2; ++i) {
        lv_obj_set_y(ctx->ticks[i],(i == 0 ? 355 : 473)-lift);
        lv_obj_set_style_bg_opa(ctx->ticks[i], opa(phase(t,180,780) * first / 1000), 0);
    }

    const int32_t settle = smooth(phase(t,2160,2700));
    lv_obj_set_y(ctx->prts, 182 + (1000 - settle) * 14 / 1000);
    image_opacity(ctx->prts, phase(t,2160,2650) * second / 1000);
    image_opacity(ctx->information, phase(t,2400,2780) * second / 1000);
    image_opacity(ctx->analysis, phase(t,2500,2880) * second / 1000);
    image_opacity(ctx->tower, phase(t,2800,3420) * second / 1000 * 65 / 1000);

    const uint8_t login_count = characters(t,2800,50,11);
    lv_obj_set_width(ctx->login_clip, login_count == 0 ? 0 : ui_boot_login_glyph_ends[login_count-1]);
    const int32_t rail_alpha = phase(t,2560,2800) * second / 1000;
    lv_obj_set_style_bg_opa(ctx->track, opa(rail_alpha * 25 / 100), 0);
    lv_obj_set_style_bg_opa(ctx->rail, opa(rail_alpha), 0);
    lv_obj_set_width(ctx->rail, 336 * phase(t,2460,4720) / 1000);

    for(unsigned i = 0; i < 2; ++i) {
        const int32_t reveal = smooth(phase(t,3100+i*100,3460+i*100)) * second / 1000;
        const uint32_t begin = i == 0 ? 3500U : 4160U;
        const uint8_t count = characters(t,begin,70,8);
        image_opacity(ctx->field_name[i], reveal);
        lv_obj_set_style_border_opa(ctx->field[i], opa(reveal * 65 / 100), 0);
        lv_obj_set_style_text_opa(ctx->value[i], opa(reveal), 0);
        if(count != ctx->typed[i]) {
            ctx->typed[i] = count;
            char text[9];
            for(uint8_t c = 0; c < count; ++c) text[c] = '*';
            text[count] = '\0';
            lv_label_set_text(ctx->value[i], text);
        }
        const bool typing = t >= begin && t < begin + 560U;
        lv_obj_set_style_bg_opa(ctx->cursor[i], typing ? opa(reveal) : LV_OPA_TRANSP, 0);
        lv_obj_set_x(ctx->cursor[i], 88 + (int32_t)count * 18);
    }
    ctx->completed = t == UI_BOOT_MOTION_DURATION_MS;
    if(ctx->completed) lv_timer_pause(ctx->timer);
}

static void frame(lv_timer_t *timer)
{
    boot_motion_t *ctx = lv_timer_get_user_data(timer);
    render(ctx, lv_tick_get() - ctx->started);
}

static void deleted(lv_event_t *event)
{
    boot_motion_t *ctx = lv_event_get_user_data(event);
    lv_timer_delete(ctx->timer);
    lv_free(ctx);
}

lv_obj_t *ui_boot_motion_create(lv_obj_t *parent)
{
    boot_motion_t *ctx = lv_malloc_zeroed(sizeof(*ctx));
    if(ctx == NULL) return NULL;
    ctx->view = box(parent,0,0,480,854);
    lv_obj_set_style_bg_color(ctx->view, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->view, LV_OPA_COVER, 0);
    lv_obj_set_user_data(ctx->view,ctx);
    ctx->backdrop = image(ctx->view,&ui_boot_asset_backdrop,64,251);
    ctx->diamond = image(ctx->view,&ui_boot_asset_diamond,64,251);
    lv_image_set_pivot(ctx->diamond,176,176);
    lv_image_set_antialias(ctx->diamond,true);
    ctx->words = image(ctx->view,&ui_boot_asset_words,142,394);
    for(unsigned i = 0; i < 2; ++i) {
        ctx->ticks[i] = box(ctx->view,238,i==0 ? 355 : 473,4,25);
        lv_obj_set_style_bg_color(ctx->ticks[i],lv_color_white(),0);
    }
    ctx->tower = image(ctx->view,&ui_boot_asset_tower,52,440);
    ctx->prts = image(ctx->view,&ui_boot_asset_prts,90,182);
    ctx->information = image(ctx->view,&ui_boot_asset_information,90,303);
    ctx->analysis = image(ctx->view,&ui_boot_asset_analysis,90,331);
    ctx->login_clip = box(ctx->view,72,414,336,23);
    ctx->login = image(ctx->login_clip,&ui_boot_asset_login,0,0);
    ctx->track = box(ctx->view,72,458,336,2);
    ctx->rail = box(ctx->view,72,458,0,2);
    lv_obj_set_style_bg_color(ctx->track,lv_color_white(),0);
    lv_obj_set_style_bg_color(ctx->rail,lv_color_white(),0);
    for(unsigned i = 0; i < 2; ++i) {
        const int32_t y = 508 + (int32_t)i * 104;
        ctx->field_name[i] = image(ctx->view,i==0 ? &ui_boot_asset_username : &ui_boot_asset_password,70,y-30);
        ctx->field[i] = box(ctx->view,68,y,344,60);
        lv_obj_set_style_border_width(ctx->field[i],1,0);
        lv_obj_set_style_border_color(ctx->field[i],lv_color_white(),0);
        ctx->value[i] = lv_label_create(ctx->view);
        lv_obj_set_pos(ctx->value[i],88,y+16);
        lv_label_set_text(ctx->value[i],"");
        lv_obj_set_style_text_font(ctx->value[i],&lv_font_montserrat_24,0);
        lv_obj_set_style_text_color(ctx->value[i],lv_color_white(),0);
        lv_obj_set_style_text_letter_space(ctx->value[i],6,0);
        ctx->cursor[i] = box(ctx->view,88,y+15,2,30);
        lv_obj_set_style_bg_color(ctx->cursor[i],lv_color_white(),0);
        ctx->typed[i] = 255;
    }
    ctx->timer = lv_timer_create(frame,33,ctx);
    lv_timer_pause(ctx->timer);
    lv_obj_add_event_cb(ctx->view,deleted,LV_EVENT_DELETE,ctx);
    render(ctx,0);
    return ctx->view;
}

void ui_boot_motion_set_active(lv_obj_t *view, bool active, bool reduced_motion)
{
    if(view == NULL) return;
    boot_motion_t *ctx = lv_obj_get_user_data(view);
    if(ctx == NULL) return;
    lv_timer_pause(ctx->timer);
    if(!active) return;
    ctx->started = lv_tick_get();
    render(ctx,reduced_motion ? UI_BOOT_MOTION_DURATION_MS : 0);
    if(!reduced_motion) lv_timer_resume(ctx->timer);
}

bool ui_boot_motion_is_complete(lv_obj_t *view)
{
    if(view == NULL) return false;
    const boot_motion_t *ctx = lv_obj_get_user_data(view);
    return ctx != NULL && ctx->completed;
}
