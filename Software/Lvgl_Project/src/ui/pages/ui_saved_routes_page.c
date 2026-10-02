#include "ui_saved_routes_page.h"

#include "ui_page_common.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"

#include <string.h>

typedef struct
{
    lv_obj_t *screen;
    ui_status_bar_t status;
    ui_page_action_cb_t callback;
    void *user_data;
} saved_routes_context_t;

static saved_routes_context_t s_routes;

static void back_clicked(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED &&
       s_routes.callback != NULL)
        s_routes.callback(UI_PAGE_ACTION_BACK, s_routes.user_data);
}

static lv_obj_t *text(lv_obj_t *parent, int32_t x, int32_t y,
                      int32_t width, const char *value,
                      const lv_font_t *font, lv_color_t color,
                      lv_text_align_t align)
{
    lv_obj_t *label = ui_page_label(parent, x, y, width, value, font, align);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

lv_obj_t *ui_saved_routes_page_create(ui_page_action_cb_t callback,
                                      void *user_data)
{
    memset(&s_routes, 0, sizeof(s_routes));
    s_routes.callback = callback;
    s_routes.user_data = user_data;
    s_routes.screen = ui_page_screen_create();
    ui_page_status_create(&s_routes.status, s_routes.screen);
    ui_page_title(s_routes.screen, "收藏路线", "SAVED ROUTES");

    lv_obj_t *card = ui_page_box(s_routes.screen, 16, 158, 448, 290,
                                 UI_TOKEN_COLOR_SURFACE_OUTLINE, 1);
    lv_obj_set_style_bg_color(card, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, UI_TOKEN_RADIUS_MEDIUM, 0);

    lv_obj_t *icon = ui_page_box(card, 174, 36, 100, 100,
                                 UI_TOKEN_COLOR_ACCENT, 2);
    lv_obj_set_style_bg_color(icon, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
    ui_align_label_center(text(icon, 0, 0, 100, LV_SYMBOL_GPS, &lv_font_montserrat_32,
         UI_TOKEN_COLOR_ACCENT, LV_TEXT_ALIGN_CENTER));
    text(card, 24, 156, 400, "暂无收藏路线", &ui_font_misans_ui_24,
         UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *empty_note = text(card, 24, 198, 400,
         "添加路线后，可在这里查看并开始导航",
         UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY,
         LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(empty_note, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(empty_note, 58);

    lv_obj_t *back = ui_page_box(s_routes.screen, 16, UI_PAGE_ACTION_Y, 448, UI_PAGE_ACTION_HEIGHT,
                                 UI_TOKEN_COLOR_SURFACE_OUTLINE, 1);
    lv_obj_set_style_bg_color(back, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(back, UI_TOKEN_RADIUS_SMALL, 0);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, back_clicked, LV_EVENT_CLICKED, NULL);
    ui_align_label_center(text(back, 0, 0, 448, "返回", UI_FONT_PAGE_TEXT,
         UI_TOKEN_COLOR_TEXT_PRIMARY, LV_TEXT_ALIGN_CENTER));
    return s_routes.screen;
}
