#include "ui_text.h"
#include "ui_theme.h"
#include <string.h>

static const lv_font_t *s_font20, *s_font24;
void ui_text_register_fonts(const lv_font_t *font20, const lv_font_t *font24)
{
    s_font20 = font20;
    s_font24 = font24;
}

static bool has_glyph(const lv_font_t *font, uint32_t codepoint)
{
    lv_font_glyph_dsc_t description;
    for(unsigned i = 0; font != NULL && i < 8; ++i, font = font->fallback)
        if(font->get_glyph_dsc != NULL && font->get_glyph_dsc(font, &description, codepoint, 0)) return true;
    return false;
}

bool ui_text_prepare(const char *text,uint16_t size,char *result,size_t capacity,const lv_font_t **font_out)
{
    if(!text || !result || capacity<5) return false;
    const lv_font_t *font = size >= 24 ? s_font24 : s_font20;
    if(!font) font = size >= 24 ? &ui_font_misans_ui_24 : &ui_font_misans_ui_20;
    if(font_out) *font_out=font;
    size_t output = 0;
    bool complete = true;
    const unsigned char *input = (const unsigned char *)text;
    while(*input && output + 4 < capacity) {
        uint32_t codepoint = *input;
        size_t length = codepoint < 0x80 ? 1 : (codepoint & 0xE0) == 0xC0 ? 2 :
                        (codepoint & 0xF0) == 0xE0 ? 3 : (codepoint & 0xF8) == 0xF0 ? 4 : 0;
        bool valid = length != 0;
        if(length > 1) {
            codepoint &= (1U << (7 - length)) - 1;
            for(size_t i = 1; valid && i < length; ++i) {
                if(input[i] == 0 || (input[i] & 0xC0) != 0x80) { valid = false; break; }
                codepoint = (codepoint << 6) | (input[i] & 0x3F);
            }
            valid &= codepoint <= 0x10FFFF && !(codepoint >= 0xD800 && codepoint <= 0xDFFF) &&
                (length != 2 || codepoint >= 0x80) && (length != 3 || codepoint >= 0x800) &&
                (length != 4 || codepoint >= 0x10000);
        }
        if(valid && (codepoint == '\n' || codepoint == '\r' || has_glyph(font, codepoint))) {
            memcpy(result + output, input, length);
            output += length;
        }
        else { result[output++] = '?'; complete = false; }
        input += valid ? length : 1;
    }
    if(*input) complete = false;
    result[output] = '\0';
    return complete;
}
bool ui_text_set_label(lv_obj_t *label, const char *text, uint16_t size)
{
    if(!label || !text) return false;
    char result[256]; const lv_font_t *font=NULL;
    bool complete=ui_text_prepare(text,size,result,sizeof(result),&font);
    lv_obj_set_style_text_font(label,font,0);
    if(strcmp(lv_label_get_text(label),result)!=0) lv_label_set_text(label,result);
    return complete;
}
