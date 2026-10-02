#ifndef UI_TEXT_H
#define UI_TEXT_H
#include "lvgl.h"
#include <stdbool.h>

/* Register before creating pages; descriptors and bitmaps must outlive UI.
 * A board may supply a complete CJK font from external flash. */
void ui_text_register_fonts(const lv_font_t *font20, const lv_font_t *font24);
/* Bounded UTF-8 sanitizer for dynamic names. Unsupported codepoints become
 * '?' instead of requesting a missing glyph or reading malformed UTF-8. */
bool ui_text_set_label(lv_obj_t *label, const char *text, uint16_t size);
#endif
