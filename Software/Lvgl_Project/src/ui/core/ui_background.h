#ifndef UI_BACKGROUND_H
#define UI_BACKGROUND_H

#include "lvgl.h"

#include <stdbool.h>

/* Unified background configuration.  Only the PC RAW path is a runtime file path. */
#define UI_BG_MASTER_PATH         "assets/background/bg_6_master.jpg"
#define UI_BG_PC_RAW_PATH         "assets/generated/bg_6_480x854.rgb565"
#define UI_BG_STM32_RESOURCE_ID   "bg_6_q75.jpg"

/** Load the single shared background bitmap resource. */
void ui_background_init(void);

/** Apply the shared bitmap as an opaque background directly to one screen root. */
void ui_background_apply(lv_obj_t *screen);

/** Change the bitmap opacity on every registered screen without duplicating image data. */
void ui_background_set_opacity(lv_opa_t opacity);

/** Show or hide the shared background on every registered screen. */
void ui_background_set_visible(bool visible);

/** Release the global background object during a controlled UI shutdown. */
void ui_background_deinit(void);

#endif
