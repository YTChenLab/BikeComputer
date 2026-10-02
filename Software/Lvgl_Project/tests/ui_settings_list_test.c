#include "ui/components/settings/ui_settings_list.h"
#include "ui/core/ui_motion.h"
#include "ui/core/ui_sound.h"
#include "ui/design/ui_alignment.h"
#include "ui/core/ui_text.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_W 480
#define TEST_H 854

static uint8_t s_draw_buffer[TEST_W * 20 * 2];
static uint16_t s_last_action = UINT16_MAX;

static void test_flush(lv_display_t *display, const lv_area_t *area,
                       uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}

static void activated(uint16_t id, void *user_data)
{
    (void)user_data;
    s_last_action = id;
}

static void assert_center_y(lv_obj_t *child, lv_obj_t *parent)
{
    lv_obj_update_layout(parent);
    lv_area_t child_area, parent_area;
    lv_obj_get_coords(child, &child_area);
    lv_obj_get_coords(parent, &parent_area);
    assert(abs(child_area.y1 + child_area.y2 -
               parent_area.y1 - parent_area.y2) <= 1);
}

static void test_dynamic_font_alignment(void)
{
    lv_obj_t *button = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, 216, 72);
    lv_obj_t *text = lv_label_create(button);
    ui_text_set_label(text, "SAVE", 20);
    ui_align_label_center(text);
    assert_center_y(text, button);
    /* CJK fallback and a thicker focus border must preserve the center. */
    ui_text_set_label(text, "保存布局", 24);
    lv_obj_set_style_border_width(button, 2, 0);
    assert_center_y(text, button);
    lv_area_t child_area, parent_area;
    lv_obj_get_coords(text, &child_area);
    lv_obj_get_coords(button, &parent_area);
    assert(abs(child_area.x1 + child_area.x2 -
               parent_area.x1 - parent_area.x2) <= 1);
    lv_obj_delete(button);
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(TEST_W, TEST_H);
    assert(display != NULL);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, s_draw_buffer, NULL,
                           sizeof(s_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, test_flush);
    ui_motion_init();
    ui_sound_init();
    test_dynamic_font_alignment();

    ui_settings_list_t list;
    ui_settings_list_create(&list, lv_screen_active(), 20, 100, 440, 300,
                            activated, NULL);
    assert(ui_settings_list_add(&list, 10U, UI_SETTINGS_ROW_TOGGLE,
                                "WiFi", "NETWORK", ""));
    assert(ui_settings_list_add(&list, 11U, UI_SETTINGS_ROW_SLIDER,
                                "声音", "VOLUME", "50%"));
    assert(ui_settings_list_add(&list, 12U, UI_SETTINGS_ROW_INFO,
                                "版本", "VERSION", "v1.1.1"));
    assert(list.row_count == 3U && list.selection == 0U);
    lv_obj_update_layout(list.root);
    assert(ui_settings_list_contains_point(&list, 20, 100));
    assert(ui_settings_list_contains_point(&list, 459, 399));
    assert(!ui_settings_list_contains_point(&list, 19, 100));
    assert(!ui_settings_list_contains_point(&list, 20, 400));
    assert(ui_settings_list_set_toggle(&list, 10U, true));
    assert(list.rows[0].enabled);
    assert(ui_settings_list_set_value(&list, 11U, "65%"));
    assert(strcmp(lv_label_get_text(list.rows[1].value), "65%") == 0);
    assert(ui_settings_list_set_feedback(&list, 10U,
                                         UI_SETTINGS_ROW_PENDING));
    assert(strcmp(lv_label_get_text(list.rows[0].value), "等待") == 0);
    assert_center_y(list.rows[0].icon_box, list.rows[0].root);
    assert_center_y(list.rows[0].icon, list.rows[0].icon_box);
    assert_center_y(list.rows[0].value, list.rows[0].root);
    lv_obj_t *pair = lv_obj_get_parent(list.rows[1].title);
    assert(lv_obj_get_parent(pair) == list.rows[1].root);
    ui_text_set_label(list.rows[1].subtitle, "音量调整", 24);
    assert_center_y(pair, list.rows[1].root);
    lv_obj_update_layout(pair);
    lv_area_t title_area, subtitle_area, row_area;
    lv_obj_get_coords(list.rows[1].title, &title_area);
    lv_obj_get_coords(list.rows[1].subtitle, &subtitle_area);
    lv_obj_get_coords(list.rows[1].root, &row_area);
    assert(abs(title_area.y1 + subtitle_area.y2 - row_area.y1 - row_area.y2) <= 1);
    assert(ui_settings_list_set_accent(&list,10U,0xB59CFF,LV_SYMBOL_BLUETOOTH));
    assert(lv_color_eq(lv_obj_get_style_bg_color(list.rows[0].toggle_track,0),lv_color_hex(0xB59CFF)));
    assert(!lv_obj_has_flag(list.rows[2].root,LV_OBJ_FLAG_CLICKABLE));
    assert(lv_obj_get_scrollbar_mode(list.root)==LV_SCROLLBAR_MODE_AUTO);
    lv_obj_t *hint=lv_label_create(lv_screen_active());
    ui_settings_list_set_scroll_hint(&list,hint);
    assert(strstr(lv_label_get_text(hint),"1-3 / 3"));
    for(unsigned i=3;i<10;++i)
        assert(ui_settings_list_add(&list,(uint16_t)(10+i),UI_SETTINGS_ROW_ACTION,"声音","VOLUME","50%"));
    ui_settings_list_set_scroll_hint(&list,hint);
    assert(lv_obj_get_scroll_bottom(list.root)>0);
    lv_obj_scroll_to_y(list.root,10000,LV_ANIM_OFF);
    assert(strstr(lv_label_get_text(hint),"已到底部"));
    lv_obj_scroll_to_y(list.root,0,LV_ANIM_OFF);

    assert(ui_settings_list_handle_input(&list, UI_INPUT_ENCODER_CW));
    assert(list.selection == 1U);
    assert(ui_settings_list_handle_input(&list, UI_INPUT_ENCODER_PRESS));
    assert(s_last_action == 11U);
    assert(ui_settings_list_handle_input(&list, UI_INPUT_ENCODER_CW));
    assert(list.selection == 2U);
    assert(lv_obj_get_style_border_width(list.rows[2].root,0) == 1);
    assert(ui_settings_list_handle_input(&list, UI_INPUT_ENCODER_PRESS));
    assert(s_last_action == 11U); /* Read-only row produces no command. */
    assert(ui_settings_list_handle_input(&list, UI_INPUT_ENCODER_CCW));
    assert(ui_settings_list_handle_input(&list, UI_INPUT_ENCODER_CCW));
    assert(list.selection == 0U);

    lv_display_delete(display);
    puts("ui_settings_list_test: PASS");
    return 0;
}
