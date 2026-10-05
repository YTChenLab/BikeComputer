#include "ui_pc_online_fonts.h"
#include "ui/core/ui_text.h"
#include "ui/core/ui_theme.h"
#include "lvgl/src/libs/tiny_ttf/lv_tiny_ttf.h"
#include <stdio.h>
#include <stdlib.h>

bool ui_pc_online_fonts_enable(const char *path)
{
 static bool ready;static void *data;static lv_font_t font20,font24,latin20,latin24;
 if(ready)return true;if(!path)return false;FILE *file=fopen(path,"rb");if(!file)return false;
 if(fseek(file,0,SEEK_END)){fclose(file);return false;}long size=ftell(file);
 if(size<12||size>16*1024*1024||fseek(file,0,SEEK_SET)){fclose(file);return false;}
 data=malloc((size_t)size);if(!data){fclose(file);return false;}
 bool loaded=fread(data,1,(size_t)size,file)==(size_t)size;fclose(file);
 if(!loaded){free(data);data=NULL;return false;}
 lv_font_t *extra20=lv_tiny_ttf_create_data_ex(data,(size_t)size,20,LV_FONT_KERNING_NONE,48);
 lv_font_t *extra24=extra20?lv_tiny_ttf_create_data_ex(data,(size_t)size,24,LV_FONT_KERNING_NONE,48):NULL;
 if(!extra20||!extra24){if(extra20)lv_tiny_ttf_destroy(extra20);if(extra24)lv_tiny_ttf_destroy(extra24);free(data);data=NULL;return false;}
 /* Retain the existing Latin typography before trying extra CJK glyphs.
  * Copy descriptors because global built-in fonts are immutable. */
 font20=ui_font_misans_ui_20;font24=ui_font_misans_ui_24;
 latin20=*font20.fallback;latin24=*font24.fallback;
 latin20.fallback=extra20;latin24.fallback=extra24;
 font20.fallback=&latin20;font24.fallback=&latin24;
 ui_text_register_fonts(&font20,&font24);ready=true;return true;
}
