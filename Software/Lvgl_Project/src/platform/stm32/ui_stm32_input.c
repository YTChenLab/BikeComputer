#include "ui_stm32_input.h"
#include "ui_stm32_services.h"
#include "ui/core/ui_runtime.h"

static lv_indev_t *s_pointer;
static void read_pointer(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    int32_t x = 0, y = 0; bool pressed = false;
    ui_stm32_services_get_touch(&x, &y, &pressed);
    data->point.x = x; data->point.y = y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
lv_indev_t *ui_stm32_input_create_pointer(void)
{
    if(s_pointer || !ui_runtime_require_ui()) return NULL;
    s_pointer = lv_indev_create();
    if(s_pointer) {
        lv_indev_set_type(s_pointer, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_pointer, read_pointer);
    }
    return s_pointer;
}
