#include "platform/ui_platform_internal.h"
#include "ui/core/ui_background.h"
#include "ui/core/ui_sizes.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#define UI_BACKGROUND_RAW_SIZE ((size_t)UI_SCREEN_W * UI_SCREEN_H * 2U)
static uint8_t *s_background_data;
static lv_image_dsc_t s_background_image;
const void *ui_platform_background_load(void)
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
void ui_platform_background_release(const void *source)
{ (void)source; free(s_background_data); s_background_data = NULL;
  lv_memzero(&s_background_image, sizeof(s_background_image)); }
