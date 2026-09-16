#ifndef UI_DESIGN_TOKENS_H
#define UI_DESIGN_TOKENS_H

#include "lvgl.h"

#define UI_TOKEN_SCREEN_WIDTH   480
#define UI_TOKEN_SCREEN_HEIGHT  854

/* Color roles. Values preserve the confirmed v1.0.0 visual baseline. */
#define UI_TOKEN_COLOR_CANVAS            lv_color_hex(0x000000)
#define UI_TOKEN_COLOR_SURFACE           lv_color_hex(0x030706)
#define UI_TOKEN_COLOR_TEXT_PRIMARY      lv_color_hex(0xFFFFFF)
#define UI_TOKEN_COLOR_TEXT_SECONDARY    lv_color_hex(0xA8ACAD)
#define UI_TOKEN_COLOR_TEXT_TERTIARY     lv_color_hex(0x8F9495)
#define UI_TOKEN_COLOR_ACCENT            lv_color_hex(0xFFD900)
#define UI_TOKEN_COLOR_CRITICAL          lv_color_hex(0xFF4D4F)
#define UI_TOKEN_COLOR_LINE              lv_color_hex(0xA7D8EF)
#define UI_TOKEN_COLOR_SURFACE_BORDER    lv_color_hex(0xD6D8D8)
#define UI_TOKEN_COLOR_SEGMENT_INACTIVE  lv_color_hex(0x30424F)
#define UI_TOKEN_COLOR_DECORATION        lv_color_hex(0x47677A)
#define UI_TOKEN_COLOR_INDICATOR         lv_color_hex(0x909596)

/* Opacity roles. */
#define UI_TOKEN_OPA_PRIMARY    LV_OPA_COVER
#define UI_TOKEN_OPA_SECONDARY  LV_OPA_70
#define UI_TOKEN_OPA_DISABLED   LV_OPA_30
#define UI_TOKEN_OPA_HIDDEN     LV_OPA_TRANSP

/* Spacing scale. */
#define UI_TOKEN_SPACE_2      2
#define UI_TOKEN_SPACE_4      4
#define UI_TOKEN_SPACE_8      8
#define UI_TOKEN_SPACE_12    12
#define UI_TOKEN_SPACE_16    16
#define UI_TOKEN_SPACE_20    20
#define UI_TOKEN_SPACE_24    24
#define UI_TOKEN_SPACE_32    32

/* Shape and stroke roles. */
#define UI_TOKEN_RADIUS_SMALL       8
#define UI_TOKEN_RADIUS_MEDIUM     12
#define UI_TOKEN_RADIUS_SURFACE    16
#define UI_TOKEN_BORDER_THIN        1
#define UI_TOKEN_BORDER_FOCUS       2

/* Shared Bottom Interaction Surface geometry. */
#define UI_BOTTOM_SURFACE_X                 8
#define UI_BOTTOM_SURFACE_BOTTOM_MARGIN    14
#define UI_BOTTOM_SURFACE_WIDTH           464
#define UI_BOTTOM_SURFACE_MINI_HEIGHT     156
#define UI_BOTTOM_SURFACE_NORMAL_HEIGHT   226
#define UI_BOTTOM_SURFACE_EXPANDED_HEIGHT 442
#define UI_BOTTOM_SURFACE_NORMAL_Y        614

/* Motion roles. */
#define UI_TOKEN_MOTION_FAST_MS      120U
#define UI_TOKEN_MOTION_STANDARD_MS  220U
#define UI_TOKEN_MOTION_SURFACE_MS   260U
#define UI_TOKEN_MOTION_SURFACE_FADE_MS 180U
#define UI_TOKEN_MOTION_DOCK_CONTENT_MS 220U

#endif /* UI_DESIGN_TOKENS_H */
