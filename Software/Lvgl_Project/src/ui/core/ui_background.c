#include "ui_background.h"

#include "ui_sizes.h"

#include "platform/ui_platform_internal.h"


#define UI_BACKGROUND_RAW_SIZE  ((size_t)UI_SCREEN_W * (size_t)UI_SCREEN_H * 2U)
#define UI_BACKGROUND_TARGET_MAX 16U


static lv_obj_t *s_background_targets[UI_BACKGROUND_TARGET_MAX];
static uint8_t s_background_target_count = 0U;
static lv_opa_t s_background_opacity = LV_OPA_COVER;
static bool s_background_visible = true;
static const void *s_background_source;
static ui_background_port_t s_port;

bool ui_background_register_port(const ui_background_port_t *port)
{
    if(port == NULL || port->load == NULL || s_background_source != NULL) return false;
    s_port = *port;
    return true;
}

static void background_target_deleted(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);
    for(unsigned i = 0; i < s_background_target_count; ++i) {
        if(s_background_targets[i] != target) continue;
        s_background_targets[i] = s_background_targets[--s_background_target_count];
        break;
    }
}


void ui_background_init(void)
{
    if(!UI_BACKGROUND_BITMAP_ENABLED) return;
    if(s_background_source != NULL)
    {
        return;
    }

    lv_display_t *display = lv_display_get_default();
    if(display == NULL || lv_display_get_color_format(display) != LV_COLOR_FORMAT_RGB565)
    {
        LV_LOG_ERROR("Background requires an RGB565 display");
        return;
    }

    const void *source = s_port.load != NULL ?
        s_port.load(UI_BG_STM32_RESOURCE_ID, s_port.context) : ui_platform_background_load();
    s_background_source = source;
    if(source == NULL)
    {
        LV_LOG_ERROR("UI background source is unavailable");
        return;
    }

    lv_image_header_t header;
    if(lv_image_decoder_get_info(source, &header) != LV_RESULT_OK)
    {
        LV_LOG_ERROR("Unable to read UI background image header");
        ui_background_deinit();
        return;
    }

    if(header.w != UI_SCREEN_W || header.h != UI_SCREEN_H)
    {
        LV_LOG_WARN("UI background size does not match UI_SCREEN_W/UI_SCREEN_H");
    }

}


void ui_background_apply(lv_obj_t *screen)
{
    if(screen == NULL) return;
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_image_src(screen, NULL, 0);
    if(s_background_source == NULL) return;
    lv_obj_set_style_bg_image_src(screen, s_background_source, 0);
    lv_obj_set_style_bg_image_opa(screen, s_background_visible ? s_background_opacity : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_image_tiled(screen, false, 0);

    for(uint8_t i = 0U; i < s_background_target_count; i++) {
        if(s_background_targets[i] == screen) return;
    }
    if(s_background_target_count < UI_BACKGROUND_TARGET_MAX) {
        s_background_targets[s_background_target_count++] = screen;
        lv_obj_add_event_cb(screen, background_target_deleted, LV_EVENT_DELETE, NULL);
    } else {
        LV_LOG_ERROR("Too many UI screens registered for the shared background");
    }
}


void ui_background_set_opacity(lv_opa_t opacity)
{
    s_background_opacity = opacity;
    if(!s_background_visible) return;
    for(uint8_t i = 0U; i < s_background_target_count; i++) {
        lv_obj_set_style_bg_image_opa(s_background_targets[i], opacity, 0);
    }
}


void ui_background_set_visible(bool visible)
{
    s_background_visible = visible;
    lv_opa_t opacity = visible ? s_background_opacity : LV_OPA_TRANSP;
    for(uint8_t i = 0U; i < s_background_target_count; i++) {
        lv_obj_set_style_bg_image_opa(s_background_targets[i], opacity, 0);
    }
}


void ui_background_deinit(void)
{
    for(unsigned i = 0; i < s_background_target_count; ++i) {
        lv_obj_set_style_bg_image_src(s_background_targets[i], NULL, 0);
        lv_obj_remove_event_cb(s_background_targets[i], background_target_deleted);
    }
    if(s_port.release != NULL && s_background_source != NULL)
        s_port.release(s_background_source, s_port.context);
    if(s_port.load == NULL && s_background_source != NULL)
        ui_platform_background_release(s_background_source);
    s_background_source = NULL;

    lv_memzero(s_background_targets, sizeof(s_background_targets));
    s_background_target_count = 0U;
}
