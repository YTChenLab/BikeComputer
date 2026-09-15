#include "ui/components/dock/ui_dock_view.h"
#include "ui/core/ui_dock.h"
#include "ui/core/ui_dock_controller.h"
#include "ui/core/ui_event_bus.h"
#include "ui/core/ui_motion.h"
#include "ui/core/ui_sound.h"
#include "ui/core/ui_theme.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_DISPLAY_W 480
#define TEST_DISPLAY_H 854
#define TEST_DOCK_X      12
#define TEST_DOCK_Y     682
#define TEST_DOCK_W     456
#define TEST_DOCK_H     156

static uint8_t s_draw_buffer[TEST_DISPLAY_W * 20 * 2];
static ui_event_type_t s_last_event = UI_EVENT_COUNT;

static void capture_event(const ui_event_t *event, void *user_data)
{
    (void)user_data;
    s_last_event = event->type;
}

static ui_dock_card_t make_card(ui_dock_type_t type)
{
    ui_dock_card_t card = {0};
    card.id = 100U + (uint32_t)type;
    card.type = type;
    card.priority = UI_DOCK_PRIORITY_P3_USER_ACTION;
    snprintf(card.title, sizeof(card.title), "Card %u", (unsigned)type);
    snprintf(card.message, sizeof(card.message), "Renderer smoke test");
    snprintf(card.primary_action, sizeof(card.primary_action), "OK");
    snprintf(card.secondary_action, sizeof(card.secondary_action), "BACK");
    card.minimum = 0;
    card.maximum = 100;
    card.value = 50;
    card.dismissible = true;
    return card;
}

static void finish_animations(void)
{
    lv_tick_inc(300);
    lv_anim_refr_now();
}

static uint32_t utf8_next(const char *text, uint32_t *index)
{
    const uint8_t *bytes = (const uint8_t *)text;
    uint8_t first = bytes[*index];
    if(first < 0x80) { (*index)++; return first; }
    if((first & 0xE0) == 0xC0) {
        uint32_t value = ((uint32_t)(first & 0x1F) << 6) |
                         (bytes[*index + 1] & 0x3F);
        *index += 2;
        return value;
    }
    if((first & 0xF0) == 0xE0) {
        uint32_t value = ((uint32_t)(first & 0x0F) << 12) |
                         ((uint32_t)(bytes[*index + 1] & 0x3F) << 6) |
                         (bytes[*index + 2] & 0x3F);
        *index += 3;
        return value;
    }
    *index += 4;
    return 0xFFFD;
}

static void test_dock_font_coverage(void)
{
    static const char * const texts[] = {
        "GPS 正在定位", "正在搜索卫星信号", "GPS 准备完成",
        "定位良好，可以开始骑行", "GPS 定位失败", "请移动到开阔区域后重试",
        "传感器已断开", "请检查蓝牙与传感器电量", "电量过低",
        "请尽快充电或开启省电模式", "骑行已开始", "骑行已暂停",
        "骑行已恢复", "计时与里程统计已暂停", "骑行数据正在记录",
        "结束骑行", "是否结束并保存本次骑行", "前方转向",
        "继续沿当前道路骑行", "已偏离路线", "正在重新规划路线",
        "音乐播放", "未提供曲目信息", "存储异常", "骑行数据保存失败",
        "参数调节", "拖动滑条进行调整", "重试", "检查", "结束",
        "取消", "打开", "模式", "稍后", "正在保存骑行数据",
        "保存完成后自动进入骑行总结"
    };
    for(uint32_t text_index = 0;
        text_index < sizeof(texts) / sizeof(texts[0]); text_index++) {
        uint32_t index = 0;
        while(texts[text_index][index] != '\0') {
            uint32_t codepoint = utf8_next(texts[text_index], &index);
            lv_font_glyph_dsc_t descriptor;
            if(!lv_font_get_glyph_dsc(UI_FONT_PAGE_TEXT, &descriptor,
                                      codepoint, 0)) {
                fprintf(stderr, "Missing Dock glyph U+%04lX in: %s\n",
                        (unsigned long)codepoint, texts[text_index]);
                assert(false);
            }
        }
    }
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(TEST_DISPLAY_W, TEST_DISPLAY_H);
    assert(display != NULL);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, s_draw_buffer, NULL, sizeof(s_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    ui_motion_init();
    ui_sound_init();
    ui_event_bus_init();
    ui_dock_init();
    ui_dock_view_init();
    ui_dock_controller_init();
    test_dock_font_coverage();

    lv_obj_t *layer = ui_dock_view_root();
    assert(layer != NULL);
    assert(lv_obj_has_flag(layer, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_child_count(layer) == 1);
    lv_obj_t *viewport = lv_obj_get_child(layer, 0);
    lv_obj_update_layout(layer);
    assert(lv_obj_get_x(viewport) == TEST_DOCK_X);
    assert(lv_obj_get_y(viewport) == TEST_DOCK_Y);
    assert(lv_obj_get_width(viewport) == TEST_DOCK_W);
    assert(lv_obj_get_height(viewport) == TEST_DOCK_H);
    assert(lv_obj_get_child_count(viewport) == UI_DOCK_TYPE_COUNT);

    for(uint8_t type = 0; type < UI_DOCK_TYPE_COUNT; type++) {
        ui_dock_card_t card = make_card((ui_dock_type_t)type);
        if(type == 0) assert(ui_dock_show(&card));
        else assert(ui_dock_replace(&card));
        finish_animations();
        assert(ui_dock_view_is_visible());

        for(uint8_t child_index = 0; child_index < UI_DOCK_TYPE_COUNT;
            child_index++) {
            lv_obj_t *card_root = lv_obj_get_child(viewport, child_index);
            bool hidden = lv_obj_has_flag(card_root, LV_OBJ_FLAG_HIDDEN);
            assert(hidden == (child_index != type));
            assert(lv_obj_get_width(card_root) == TEST_DOCK_W);
            assert(lv_obj_get_height(card_root) == TEST_DOCK_H);
            if(child_index == type) assert(lv_obj_get_y(card_root) == 0);
        }
    }

    const ui_dock_card_t *current = ui_dock_current();
    assert(current != NULL);
    assert(ui_dock_hide(current->id));
    finish_animations();
    assert(!ui_dock_view_is_visible());

    assert(ui_event_bus_subscribe(UI_EVENT_MASK(UI_EVENT_RIDE_GO),
                                  capture_event, NULL) > 0);
    ui_event_t gps_ready = {0};
    gps_ready.type = UI_EVENT_GPS_READY;
    gps_ready.priority = UI_EVENT_PRIORITY_P2_RIDE_STATE;
    assert(ui_event_bus_publish(&gps_ready));
    assert(ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX) == 1);
    finish_animations();
    assert(ui_dock_view_is_visible());
    lv_obj_t *action_root = lv_obj_get_child(viewport, UI_DOCK_TYPE_ACTION);
    assert(!lv_obj_has_flag(action_root, LV_OBJ_FLAG_HIDDEN));
    lv_obj_t *primary_button = lv_obj_get_child(action_root, 6);
    assert(primary_button != NULL);
    lv_obj_t *primary_label = lv_obj_get_child(primary_button, 0);
    assert(primary_label != NULL);
    lv_obj_update_layout(primary_button);
    assert(strcmp(lv_label_get_text(primary_label), "GO") == 0);
    assert(lv_obj_get_width(primary_label) == lv_obj_get_content_width(primary_button));
    assert(lv_obj_get_height(primary_label) > 0);
    assert(lv_obj_get_style_bg_opa(action_root, LV_PART_MAIN) == LV_OPA_COVER);
    assert(lv_obj_send_event(primary_button, LV_EVENT_CLICKED, NULL) == LV_RESULT_OK);
    (void)ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX);
    assert(s_last_event == UI_EVENT_RIDE_GO);
    finish_animations();
    assert(!ui_dock_view_is_visible());

    ui_event_t finish_request = {0};
    finish_request.type = UI_EVENT_RIDE_FINISH_REQUESTED;
    finish_request.priority = UI_EVENT_PRIORITY_P2_RIDE_STATE;
    assert(ui_event_bus_publish(&finish_request));
    assert(ui_event_bus_process(UI_EVENT_BUS_QUEUE_MAX) == 1);
    finish_animations();
    lv_obj_t *confirm_root = lv_obj_get_child(viewport, UI_DOCK_TYPE_CONFIRM);
    assert(!lv_obj_has_flag(confirm_root, LV_OBJ_FLAG_HIDDEN));
    lv_obj_t *badge = lv_obj_get_child(confirm_root, 4);
    lv_obj_t *close_button = lv_obj_get_child(confirm_root, 5);
    lv_obj_t *no_button = lv_obj_get_child(confirm_root, 6);
    lv_obj_t *yes_button = lv_obj_get_child(confirm_root, 7);
    assert(strcmp(lv_label_get_text(badge), "CONFIRM") == 0);
    assert(lv_obj_get_x(badge) + lv_obj_get_width(badge) <
           lv_obj_get_x(close_button));
    assert(lv_text_get_width("CONFIRM", 7, &lv_font_montserrat_16, 0) <=
           lv_obj_get_width(badge));
    assert(strcmp(lv_label_get_text(lv_obj_get_child(no_button, 0)), "NO") == 0);
    assert(strcmp(lv_label_get_text(lv_obj_get_child(yes_button, 0)), "YES") == 0);

    ui_dock_controller_deinit();
    ui_dock_view_deinit();
    lv_display_delete(display);
    puts("ui_dock_view_test: PASS");
    return 0;
}
