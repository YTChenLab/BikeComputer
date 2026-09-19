#include "ui/core/ui_motion.h"
#include "lvgl.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TEST_W 480
#define TEST_H 854

static uint8_t s_draw_buffer[TEST_W * 20 * 2];
static bool s_layer_failure;

static void test_flush(lv_display_t *display, const lv_area_t *area,
                       uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}

static void capture_log(lv_log_level_t level, const char *message)
{
    (void)level;
    if(message != NULL &&
       (strstr(message, "No memory") != NULL ||
        strstr(message, "Allocating layer buffer failed") != NULL))
        s_layer_failure = true;
}

static void finish_motion(uint8_t steps)
{
    for(uint8_t step = 0; step < steps; step++) {
        lv_tick_inc(20U);
        (void)lv_timer_handler();
    }
}

int main(void)
{
    lv_init();
    lv_log_register_print_cb(capture_log);
    lv_display_t *display = lv_display_create(TEST_W, TEST_H);
    assert(display != NULL);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, s_draw_buffer, NULL,
                           sizeof(s_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, test_flush);
    ui_motion_init();

    lv_obj_t *capsule = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(capsule, 12, 20);
    lv_obj_set_size(capsule, 108, 88);
    ui_motion_focus_capsule(capsule, 244, 108);
    finish_motion(12U);
    assert(lv_obj_get_x(capsule) == 244);
    assert(lv_obj_get_width(capsule) == 108);

    ui_motion_toggle_press(capsule);
    finish_motion(5U);
    assert(lv_obj_get_style_transform_scale_x(capsule, 0) == 246);
    ui_motion_toggle_release(capsule);
    finish_motion(5U);
    assert(lv_obj_get_style_transform_scale_x(capsule, 0) ==
           UI_MOTION_PRESS_SCALE_NORMAL);

    ui_motion_pulse(capsule, 272, 180U);
    finish_motion(12U);
    assert(lv_obj_get_style_transform_scale_x(capsule, 0) ==
           UI_MOTION_PRESS_SCALE_NORMAL);

    uint32_t child_count = lv_obj_get_child_count(lv_screen_active());
    for(uint16_t cycle = 0U; cycle < 250U; cycle++) {
        int32_t target_x = (cycle & 1U) != 0U ? 244 : 12;
        ui_motion_focus_capsule(capsule, target_x, 108);
        ui_motion_pulse(capsule, 268, 80U);
        finish_motion(10U);
        assert(lv_obj_get_x(capsule) == target_x);
        assert(lv_obj_get_width(capsule) == 108);
        assert(lv_obj_get_style_transform_scale_x(capsule, 0) ==
               UI_MOTION_PRESS_SCALE_NORMAL);
    }
    assert(lv_anim_count_running() == 0U);
    assert(lv_obj_get_child_count(lv_screen_active()) == child_count);
    lv_refr_now(display);
    assert(!s_layer_failure);

    lv_display_delete(display);
    puts("ui_capsule_motion_logic_test: PASS");
    return 0;
}
