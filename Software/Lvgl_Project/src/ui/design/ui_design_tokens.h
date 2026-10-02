#ifndef UI_DESIGN_TOKENS_H
#define UI_DESIGN_TOKENS_H

#include "lvgl.h"

#define UI_TOKEN_SCREEN_WIDTH   480
#define UI_TOKEN_SCREEN_HEIGHT  854

/* Black canvas, neutral surfaces and one yellow product accent. */
#define UI_TOKEN_COLOR_CANVAS            lv_color_hex(0x000000)
#define UI_TOKEN_COLOR_SURFACE           lv_color_hex(0x181818)
#define UI_TOKEN_COLOR_TEXT_PRIMARY      lv_color_hex(0xFFFFFF)
#define UI_TOKEN_COLOR_TEXT_SECONDARY    lv_color_hex(0xAAAAAA)
#define UI_TOKEN_COLOR_TEXT_TERTIARY     lv_color_hex(0x888888)
#define UI_TOKEN_ACCENT_HEX              0xFFD900
#define UI_TOKEN_COLOR_ACCENT            lv_color_hex(UI_TOKEN_ACCENT_HEX)
#define UI_TOKEN_COLOR_CRITICAL          lv_color_hex(0xFF4D4F)
#define UI_TOKEN_COLOR_LINE              lv_color_hex(0xAAAAAA)
#define UI_TOKEN_COLOR_SURFACE_BORDER    lv_color_hex(0xD6D6D6)
#define UI_TOKEN_COLOR_SURFACE_OUTLINE   lv_color_hex(0x303030)
#define UI_TOKEN_COLOR_DIVIDER           lv_color_hex(0x303030)
#define UI_TOKEN_COLOR_SEGMENT_INACTIVE  lv_color_hex(0x303030)
#define UI_TOKEN_COLOR_DECORATION        lv_color_hex(0x484848)
#define UI_TOKEN_COLOR_INDICATOR         lv_color_hex(0x909090)

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

/* Product layout; the diagnostic overlay never reserves page height. */
#define UI_PAGE_BOTTOM_MARGIN              16
#define UI_PAGE_CONTENT_BOTTOM             (UI_TOKEN_SCREEN_HEIGHT - UI_PAGE_BOTTOM_MARGIN)
#define UI_PAGE_ACTION_HEIGHT              72
#define UI_PAGE_ACTION_Y                   (UI_PAGE_CONTENT_BOTTOM - UI_PAGE_ACTION_HEIGHT)
#define UI_PAGE_FOOTNOTE_HEIGHT             28
#define UI_PAGE_FOOTNOTE_Y                  (UI_PAGE_CONTENT_BOTTOM - UI_PAGE_FOOTNOTE_HEIGHT)
#define UI_PAGE_LIST_WITH_FOOTNOTE_BOTTOM   (UI_PAGE_FOOTNOTE_Y - UI_TOKEN_SPACE_12)

/* Shared Bottom Interaction Surface geometry. */
#define UI_BOTTOM_SURFACE_X                 8
#define UI_BOTTOM_SURFACE_BOTTOM_MARGIN    UI_PAGE_BOTTOM_MARGIN
#define UI_BOTTOM_SURFACE_WIDTH           464
#define UI_BOTTOM_SURFACE_MINI_HEIGHT     156
#define UI_BOTTOM_SURFACE_NORMAL_HEIGHT   226
#define UI_BOTTOM_SURFACE_EXPANDED_HEIGHT 442
#define UI_BOTTOM_SURFACE_NORMAL_Y        (UI_PAGE_CONTENT_BOTTOM - UI_BOTTOM_SURFACE_NORMAL_HEIGHT)

/* Motion roles. */
#define UI_TOKEN_MOTION_FAST_MS      120U
#define UI_TOKEN_MOTION_STANDARD_MS  220U
#define UI_TOKEN_MOTION_SURFACE_MS   260U
#define UI_TOKEN_MOTION_DOCK_CONTENT_MS 220U
#define UI_TOKEN_MOTION_CAPSULE_FOCUS_MS 150U
#define UI_TOKEN_MOTION_CAPSULE_PRESS_MS 140U
#define UI_TOKEN_MOTION_TOGGLE_MS        180U
#define UI_TOKEN_MOTION_VALUE_STEP_MS     90U
#define UI_TOKEN_MOTION_SURFACE_SWITCH_MS 200U
#define UI_TOKEN_MOTION_CONTROL_EXPAND_MS 260U

#endif /* UI_DESIGN_TOKENS_H */
