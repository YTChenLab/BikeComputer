#ifndef UI_PC_ONLINE_FONTS_H
#define UI_PC_ONLINE_FONTS_H
#include <stdbool.h>
/* UI thread only, descriptors and the shared TTF data live for the process. */
bool ui_pc_online_fonts_enable(const char *path);
#endif
