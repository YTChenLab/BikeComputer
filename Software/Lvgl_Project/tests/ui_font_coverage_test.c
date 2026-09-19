#include "ui/core/ui_theme.h"
#include "ui/fonts/ui_font_runtime_codepoints.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    lv_init();
    for(size_t index = 0U; index < UI_FONT_RUNTIME_CODEPOINT_COUNT; index++) {
        uint32_t codepoint = ui_font_runtime_codepoints[index];
        lv_font_glyph_dsc_t descriptor;
        if(!lv_font_get_glyph_dsc(UI_FONT_PAGE_TEXT, &descriptor,
                                  codepoint, 0U) ||
           !lv_font_get_glyph_dsc(UI_FONT_SURFACE_EMPHASIS, &descriptor,
                                  codepoint, 0U)) {
            fprintf(stderr, "Missing runtime UI/emphasis glyph U+%04lX\n",
                    (unsigned long)codepoint);
            assert(false);
        }
    }
    lv_font_glyph_dsc_t navigation_descriptor;
    assert(lv_font_get_glyph_dsc(UI_FONT_NAV_STRIP_VALUE,
                                 &navigation_descriptor, '8', 0U));
    assert(lv_font_get_glyph_dsc(UI_FONT_NAV_ACTION,
                                 &navigation_descriptor, 0x5DE6U, 0U));
    assert(lv_font_get_glyph_dsc(UI_FONT_NAV_ACTION,
                                 &navigation_descriptor, 0x8F6CU, 0U));
    assert(lv_font_get_glyph_dsc(UI_FONT_NAV_STRIP_UNIT,
                                 &navigation_descriptor, 'k', 0U));
    assert(lv_font_get_glyph_dsc(UI_FONT_NAV_STRIP_UNIT,
                                 &navigation_descriptor, '/', 0U));
    printf("ui_font_coverage_test: PASS (%lu codepoints)\n",
           (unsigned long)UI_FONT_RUNTIME_CODEPOINT_COUNT);
    return 0;
}
