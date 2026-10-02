#ifndef UI_BOOT_PAGE_H
#define UI_BOOT_PAGE_H

#include "lvgl.h"

lv_obj_t *ui_boot_page_create(void);
void ui_boot_page_set_active(bool active);
bool ui_boot_page_is_complete(void);

#endif /* UI_BOOT_PAGE_H */
