#ifndef UI_BACKGROUND_H
#define UI_BACKGROUND_H

#include "lvgl.h"

#include <stdbool.h>

/* The shipping theme has a pure black canvas. The optional resource port is
 * retained for explicitly enabled bitmap themes. */
#ifndef UI_BACKGROUND_BITMAP_ENABLED
#define UI_BACKGROUND_BITMAP_ENABLED 0
#endif

/* Unified background configuration. Only the PC RAW path is a runtime file path. */
#define UI_BG_MASTER_PATH         "assets/background/bg_6_master.jpg"
#define UI_BG_PC_RAW_PATH         "assets/generated/bg_6_480x854.rgb565"
#define UI_BG_STM32_RESOURCE_ID   "bg_6_q75.jpg"
typedef struct {
    const void *(*load)(const char *resource_id, void *context);
    void (*release)(const void *source, void *context);
    void *context;
} ui_background_port_t;
/* Register before init on the LVGL thread. Source remains alive until deinit.
 * The board may supply a const RGB565 descriptor backed by SDRAM/flash. */
bool ui_background_register_port(const ui_background_port_t *port);

/** Load the shared bitmap only when UI_BACKGROUND_BITMAP_ENABLED is enabled. */
void ui_background_init(void);

/** Apply an opaque black root, plus the optional shared bitmap theme. */
void ui_background_apply(lv_obj_t *screen);

/** Change the bitmap opacity on every registered screen without duplicating image data. */
void ui_background_set_opacity(lv_opa_t opacity);

/** Show or hide the shared background on every registered screen. */
void ui_background_set_visible(bool visible);

/** Release the global background object during a controlled UI shutdown. */
void ui_background_deinit(void);

#endif
