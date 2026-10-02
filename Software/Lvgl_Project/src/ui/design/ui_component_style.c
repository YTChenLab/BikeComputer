#include "ui_component_style.h"

#include "ui_design_tokens.h"

#include <stdbool.h>

static lv_style_t s_bottom_surface_style;
static lv_style_t s_surface_host_style;
static bool s_initialized;

void ui_component_style_init(void)
{
    if(s_initialized) return;

    lv_style_init(&s_bottom_surface_style);
    lv_style_set_bg_color(&s_bottom_surface_style, UI_TOKEN_COLOR_SURFACE);
    lv_style_set_bg_opa(&s_bottom_surface_style, UI_TOKEN_OPA_PRIMARY);
    lv_style_set_border_color(&s_bottom_surface_style,
                              UI_TOKEN_COLOR_SURFACE_OUTLINE);
    lv_style_set_border_opa(&s_bottom_surface_style, UI_TOKEN_OPA_PRIMARY);
    lv_style_set_border_width(&s_bottom_surface_style, UI_TOKEN_BORDER_THIN);
    lv_style_set_radius(&s_bottom_surface_style, UI_TOKEN_RADIUS_SURFACE);
    lv_style_set_clip_corner(&s_bottom_surface_style, true);
    lv_style_set_pad_all(&s_bottom_surface_style, 0);

    lv_style_init(&s_surface_host_style);
    lv_style_set_bg_opa(&s_surface_host_style, UI_TOKEN_OPA_HIDDEN);
    lv_style_set_border_width(&s_surface_host_style, 0);
    lv_style_set_radius(&s_surface_host_style, 0);
    lv_style_set_pad_all(&s_surface_host_style, 0);

    s_initialized = true;
}

void ui_component_style_apply_bottom_surface(lv_obj_t *object)
{
    if(object == NULL) return;
    ui_component_style_init();
    lv_obj_add_style(object, &s_bottom_surface_style, 0);
}

void ui_component_style_apply_surface_host(lv_obj_t *object)
{
    if(object == NULL) return;
    ui_component_style_init();
    lv_obj_add_style(object, &s_surface_host_style, 0);
}
