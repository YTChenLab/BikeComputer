#include "ui_background.h"

#include "ui_sizes.h"

#include <stdlib.h>
#include <stdio.h>


#define UI_BACKGROUND_RAW_SIZE  ((size_t)UI_SCREEN_W * (size_t)UI_SCREEN_H * 2U)
#define UI_BACKGROUND_TARGET_MAX 16U


static uint8_t *s_background_data = NULL;
static lv_image_dsc_t s_background_image;
static lv_obj_t *s_background_targets[UI_BACKGROUND_TARGET_MAX];
static uint8_t s_background_target_count = 0U;
static lv_opa_t s_background_opacity = LV_OPA_COVER;
static bool s_background_visible = true;


/*
 * Platform source abstraction.
 *
 * PC Simulator reads one pre-converted RGB565 file into RAM at startup.  A
 * future STM32 implementation may fetch UI_BG_STM32_RESOURCE_ID from W25Q128,
 * decode JPEG in hardware, use DMA2D and provide an SDRAM-backed descriptor.
 * No HAL dependency is introduced in this UI layer.
 */
#if defined(UI_BACKGROUND_PLATFORM_STM32)
static const void *ui_background_load_platform(void)
{
    /* TODO(STM32): return an SDRAM-backed RGB565 lv_image_dsc_t. */
    return NULL;
}
#else
static const void *ui_background_load_platform(void)
{
    FILE *raw_file = fopen(UI_BG_PC_RAW_PATH, "rb");
    if(raw_file == NULL)
    {
        LV_LOG_ERROR("Unable to open UI background RAW file");
        return NULL;
    }

    /*
     * The full-screen RAW image is intentionally outside LVGL's 1 MiB object
     * heap.  Keeping it there leaves LVGL enough deterministic space for
     * objects, styles and draw work buffers in every VS Code launch mode.
     */
    s_background_data = malloc(UI_BACKGROUND_RAW_SIZE);
    if(s_background_data == NULL)
    {
        fclose(raw_file);
        LV_LOG_ERROR("Unable to allocate UI background RGB565 buffer");
        return NULL;
    }

    size_t bytes_read = fread(s_background_data, 1U, UI_BACKGROUND_RAW_SIZE, raw_file);
    int close_result = fclose(raw_file);
    if(bytes_read != UI_BACKGROUND_RAW_SIZE || close_result != 0)
    {
        free(s_background_data);
        s_background_data = NULL;
        LV_LOG_ERROR("UI background RAW file has an invalid size or cannot be read");
        return NULL;
    }

    s_background_image.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_background_image.header.cf = LV_COLOR_FORMAT_RGB565;
    s_background_image.header.flags = 0U;
    s_background_image.header.w = UI_SCREEN_W;
    s_background_image.header.h = UI_SCREEN_H;
    s_background_image.header.stride = UI_SCREEN_W * 2U;
    s_background_image.header.reserved_2 = 0U;
    s_background_image.data_size = UI_BACKGROUND_RAW_SIZE;
    s_background_image.data = s_background_data;
    s_background_image.reserved = NULL;

    return &s_background_image;
}
#endif


void ui_background_init(void)
{
    if(s_background_data != NULL)
    {
        return;
    }

#if !defined(UI_BACKGROUND_PLATFORM_STM32)
    lv_display_t *display = lv_display_get_default();
    if(display == NULL || lv_display_get_color_format(display) != LV_COLOR_FORMAT_RGB565)
    {
        LV_LOG_ERROR("PC background RAW requires an RGB565 display");
        return;
    }
#endif

    const void *source = ui_background_load_platform();
    if(source == NULL)
    {
        LV_LOG_ERROR("UI background source is unavailable");
        return;
    }

    lv_image_header_t header;
    if(lv_image_decoder_get_info(source, &header) != LV_RESULT_OK)
    {
        LV_LOG_ERROR("Unable to read UI background image header");
        return;
    }

    if(header.w != UI_SCREEN_W || header.h != UI_SCREEN_H)
    {
        LV_LOG_WARN("UI background size does not match UI_SCREEN_W/UI_SCREEN_H");
    }

}


void ui_background_apply(lv_obj_t *screen)
{
    if(screen == NULL || s_background_data == NULL) return;

    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_image_src(screen, &s_background_image, 0);
    lv_obj_set_style_bg_image_opa(screen, s_background_visible ? s_background_opacity : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_image_tiled(screen, false, 0);

    for(uint8_t i = 0U; i < s_background_target_count; i++) {
        if(s_background_targets[i] == screen) return;
    }
    if(s_background_target_count < UI_BACKGROUND_TARGET_MAX) {
        s_background_targets[s_background_target_count++] = screen;
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
    if(s_background_data == NULL)
    {
        return;
    }

#if !defined(UI_BACKGROUND_PLATFORM_STM32)
    free(s_background_data);
    s_background_data = NULL;
    lv_memzero(&s_background_image, sizeof(s_background_image));
#endif
    lv_memzero(s_background_targets, sizeof(s_background_targets));
    s_background_target_count = 0U;
}
