#include "ui_achievements_page.h"
#include "ui_page_common.h"
#include "../core/ui_motion.h"
#include "../core/ui_sound.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"
#include "../models/ui_achievement_model.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    lv_obj_t *screen, *list, *rows[UI_ACHIEVEMENT_COUNT];
    lv_obj_t *badges[UI_ACHIEVEMENT_COUNT], *progress[UI_ACHIEVEMENT_COUNT];
    lv_obj_t *fills[UI_ACHIEVEMENT_COUNT], *states[UI_ACHIEVEMENT_COUNT];
    lv_obj_t *summary_status, *summary_percent;
    lv_obj_t *detail, *detail_title, *detail_condition, *detail_progress, *detail_note;
    int32_t selected;
    bool active;
} achievements_context_t;
static achievements_context_t s_achievements;
static const uint32_t s_category_colors[] = {
    UI_TOKEN_ACCENT_HEX, UI_TOKEN_ACCENT_HEX, UI_TOKEN_ACCENT_HEX, UI_TOKEN_ACCENT_HEX
};
static const char *s_category_codes[] = {"RIDES", "DISTANCE", "ASCENT", "TIME"};
static const char *s_units[] = {"次", "km", "m", "h"};
/* Show the four growth tracks at each tier, while catalog IDs stay stable. */
static size_t catalog_index(size_t row) { return (row % 4U) * 4U + row / 4U; }

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int width,
                       const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *object = ui_page_label(parent, x, y, width, text, font, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_color(object, color, 0);
    lv_label_set_long_mode(object, LV_LABEL_LONG_CLIP);
    return object;
}
static void update_focus(void)
{
    for(size_t i = 0; i < UI_ACHIEVEMENT_COUNT; ++i) {
        bool focused = s_achievements.active && s_achievements.selected == (int32_t)i;
        lv_obj_set_style_border_color(s_achievements.rows[i], focused ?
            UI_TOKEN_COLOR_ACCENT : UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
        lv_obj_set_style_border_width(s_achievements.rows[i], focused ? 2 : 1, 0);
    }
}
static void refresh_detail(void)
{
    if(s_achievements.selected < 0) return;
    size_t index = (size_t)s_achievements.selected;
    const ui_achievement_definition_t *definition = ui_achievement_definition(catalog_index(index));
    ui_ride_history_totals_t totals;
    ui_ride_history_model_totals(&totals);
    ui_achievement_progress_t progress;
    if(!ui_achievement_evaluate(catalog_index(index), &totals, &progress)) return;
    lv_label_set_text(s_achievements.detail_title, definition->title);
    lv_label_set_text(s_achievements.detail_condition, definition->requirement);
    char text[128];
    const char *format = definition->category == UI_ACHIEVEMENT_RIDES ||
        definition->category == UI_ACHIEVEMENT_ASCENT ?
        "%s · 等级 %u\n当前 %.0f %s / 目标 %.0f %s" :
        "%s · 等级 %u\n当前 %.1f %s / 目标 %.0f %s";
    snprintf(text, sizeof(text), format, definition->category_name,
        definition->tier, progress.current, s_units[definition->category],
        definition->target, s_units[definition->category]);
    lv_label_set_text(s_achievements.detail_progress, text);
    if(progress.unlocked) snprintf(text, sizeof(text), "已达成 · 继续积累下一阶段");
    else snprintf(text, sizeof(text), "还需 %.1f %s · 已完成 %u%%",
        progress.remaining, s_units[definition->category], progress.progress_permille / 10U);
    lv_label_set_text(s_achievements.detail_note, text);
}
static void show_detail(void)
{
    refresh_detail();
    lv_obj_clear_flag(s_achievements.detail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_achievements.detail);
    ui_motion_translate_fade(s_achievements.detail, 10, 0, LV_OPA_TRANSP,
                            LV_OPA_COVER, 180, NULL, NULL);
}
static void detail_close_clicked(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED)
        ui_achievements_page_close_detail();
}
static void row_clicked(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    s_achievements.selected = (int32_t)(uintptr_t)lv_event_get_user_data(event);
    update_focus();
    show_detail();
    ui_sound_play(UI_SFX_PRESS);
}
static void create_row(size_t index)
{
    const ui_achievement_definition_t *definition = ui_achievement_definition(catalog_index(index));
    lv_color_t color = lv_color_hex(s_category_colors[definition->category]);
    lv_obj_t *row = ui_page_box(s_achievements.list, 0, 0, 432, 116,
                                UI_TOKEN_COLOR_SURFACE_OUTLINE, 1);
    s_achievements.rows[index] = row;
    lv_obj_set_style_bg_color(row, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, row_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)index);
    lv_obj_t *badge = ui_page_box(row, 14, 17, 54, 54, color, 1);
    s_achievements.badges[index] = badge;
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_t *code = label(badge, 0, 18, 54, definition->code,
                           &lv_font_montserrat_14, color);
    ui_align_label_center(code);
    label(row, 84, 11, 236, definition->title, UI_FONT_PAGE_TEXT,
          UI_TOKEN_COLOR_TEXT_PRIMARY);
    s_achievements.states[index] = label(row, 325, 12, 92, "进行中",
        UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY);
    lv_obj_set_style_text_align(s_achievements.states[index], LV_TEXT_ALIGN_RIGHT, 0);
    char text[64];
    snprintf(text, sizeof(text), "%s  /  TIER %02u",
              s_category_codes[definition->category], definition->tier);
    label(row, 84, 41, 320, text, &lv_font_montserrat_12, color);
    s_achievements.progress[index] = label(row, 84, 63, 318, "--",
        UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY);
    lv_obj_set_style_text_align(s_achievements.progress[index], LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *rail = ui_page_box(row, 84, 100, 318, 4, color, 0);
    lv_obj_set_style_bg_color(rail, UI_TOKEN_COLOR_SEGMENT_INACTIVE, 0);
    lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);
    s_achievements.fills[index] = ui_page_box(rail, 0, 0, 1, 4, color, 0);
    lv_obj_set_style_bg_color(s_achievements.fills[index], color, 0);
    lv_obj_set_style_bg_opa(s_achievements.fills[index], LV_OPA_COVER, 0);
}
lv_obj_t *ui_achievements_page_create(void)
{
    memset(&s_achievements, 0, sizeof(s_achievements));
    s_achievements.selected = -1;
    s_achievements.screen = ui_page_screen_create();
    lv_obj_set_style_bg_image_src(s_achievements.screen, NULL, 0);
    lv_obj_set_style_bg_color(s_achievements.screen, UI_TOKEN_COLOR_CANVAS, 0);
    static ui_status_bar_t status;
    ui_page_status_create(&status, s_achievements.screen);
    ui_page_title(s_achievements.screen, "成就", "RIDER / ARCHIVE");
    lv_obj_t *summary = ui_page_card(s_achievements.screen, 16, 158, 448, 88);
    lv_obj_set_style_border_color(summary, UI_TOKEN_COLOR_SURFACE_OUTLINE, 0);
    label(summary, 16, 14, 260, "骑行成长档案", UI_FONT_PAGE_TEXT,
          UI_TOKEN_COLOR_TEXT_PRIMARY);
    s_achievements.summary_status = label(summary, 16, 49, 300, "--",
        UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY);
    s_achievements.summary_percent = label(summary, 326, 25, 70, "0",
        &ui_font_misans_num_48, UI_TOKEN_COLOR_ACCENT);
    label(summary, 398, 48, 30, "%", &lv_font_montserrat_18, UI_TOKEN_COLOR_ACCENT);
    s_achievements.list = lv_obj_create(s_achievements.screen);
    lv_obj_remove_style_all(s_achievements.list);
    lv_obj_set_pos(s_achievements.list, 16, 260);
    lv_obj_set_size(s_achievements.list, 448, UI_PAGE_CONTENT_BOTTOM - 260);
    lv_obj_set_flex_flow(s_achievements.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_achievements.list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_achievements.list, 8, 0);
    lv_obj_set_style_pad_bottom(s_achievements.list, 12, 0);
    lv_obj_set_scroll_dir(s_achievements.list, LV_DIR_VER);
    ui_page_scrollbar_style(s_achievements.list);
    for(size_t i = 0; i < UI_ACHIEVEMENT_COUNT; ++i) create_row(i);

    /* Modal belongs to this screen: leaving the route cannot leak it. */
    s_achievements.detail = ui_page_box(s_achievements.screen, 0, 0, 480, 854,
                                        lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_achievements.detail, LV_OPA_80, 0);
    lv_obj_add_flag(s_achievements.detail, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *panel = ui_page_card(s_achievements.detail, 24, 220, 432, 408);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, UI_TOKEN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, UI_TOKEN_COLOR_ACCENT, 0);
    label(panel, 24, 22, 384, "MILESTONE / DETAIL", &lv_font_montserrat_12,
          UI_TOKEN_COLOR_ACCENT);
    s_achievements.detail_title = label(panel, 24, 57, 384, "--",
        UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_PRIMARY);
    s_achievements.detail_condition = label(panel, 24, 100, 384, "--",
        UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_SECONDARY);
    s_achievements.detail_progress = label(panel, 24, 145, 384, "--",
        UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_TEXT_PRIMARY);
    s_achievements.detail_note = label(panel, 24, 211, 384, "--",
        UI_FONT_PAGE_TEXT, UI_TOKEN_COLOR_ACCENT);
    label(panel, 24, 253, 384, "仅统计已保存的骑行累计数据", UI_FONT_PAGE_TEXT,
          UI_TOKEN_COLOR_TEXT_SECONDARY);
    lv_obj_t *close = ui_page_box(panel, 24, 310, 384, UI_PAGE_ACTION_HEIGHT, UI_TOKEN_COLOR_ACCENT, 1);
    lv_obj_set_style_radius(close, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(close, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close, detail_close_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_label = label(close, 0, 36, 384, "返回成就", UI_FONT_PAGE_TEXT,
                                  UI_TOKEN_COLOR_ACCENT);
    ui_align_label_center(close_label);
    ui_achievements_page_refresh();
    return s_achievements.screen;
}
void ui_achievements_page_refresh(void)
{
    if(s_achievements.screen == NULL) return;
    ui_ride_history_totals_t totals;
    ui_ride_history_model_totals(&totals);
    unsigned unlocked = 0;
    for(size_t i = 0; i < UI_ACHIEVEMENT_COUNT; ++i) {
        const ui_achievement_definition_t *definition = ui_achievement_definition(catalog_index(i));
        ui_achievement_progress_t progress;
        if(!ui_achievement_evaluate(catalog_index(i), &totals, &progress)) continue;
        if(progress.unlocked) ++unlocked;
        char text[64];
        double displayed = progress.unlocked ? definition->target : progress.current;
        /* Truncate incomplete decimal progress so rounding cannot display
         * '100 / 100' while the actual achievement is still locked. */
        double shown = (double)(uint64_t)(displayed * 10.0) / 10.0;
        if(definition->category == UI_ACHIEVEMENT_RIDES ||
           definition->category == UI_ACHIEVEMENT_ASCENT)
            snprintf(text, sizeof(text), "%.0f / %.0f %s", shown, definition->target,
                      s_units[definition->category]);
        else snprintf(text, sizeof(text), "%.1f / %.0f %s", shown, definition->target,
                       s_units[definition->category]);
        lv_label_set_text(s_achievements.progress[i], text);
        lv_label_set_text(s_achievements.states[i], progress.unlocked ? "已达成" : "进行中");
        lv_obj_set_style_text_color(s_achievements.states[i], progress.unlocked ?
            lv_color_hex(s_category_colors[definition->category]) : UI_TOKEN_COLOR_TEXT_TERTIARY, 0);
        int width = (int)(318U * progress.progress_permille / 1000U);
        lv_obj_set_width(s_achievements.fills[i], width > 0 ? width : 1);
        lv_obj_set_style_bg_opa(s_achievements.fills[i], width > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(s_achievements.badges[i], progress.unlocked ? LV_OPA_COVER : LV_OPA_40, 0);
    }
    char text[64];
    snprintf(text, sizeof(text), "已达成 %u / %u · 四条成长线", unlocked, UI_ACHIEVEMENT_COUNT);
    lv_label_set_text(s_achievements.summary_status, text);
    snprintf(text, sizeof(text), "%u", unlocked * 100U / UI_ACHIEVEMENT_COUNT);
    lv_label_set_text(s_achievements.summary_percent, text);
    if(ui_achievements_page_detail_visible()) refresh_detail();
}
void ui_achievements_page_close_detail(void)
{
    if(s_achievements.detail != NULL) {
        lv_anim_delete(s_achievements.detail, NULL);
        lv_obj_add_flag(s_achievements.detail, LV_OBJ_FLAG_HIDDEN);
    }
}
bool ui_achievements_page_detail_visible(void)
{
    return s_achievements.detail != NULL &&
        !lv_obj_has_flag(s_achievements.detail, LV_OBJ_FLAG_HIDDEN);
}
void ui_achievements_page_set_active(bool active)
{
    s_achievements.active = active;
    if(active) {
        ui_achievements_page_refresh();
        if(s_achievements.selected < 0) s_achievements.selected = 0;
    } else ui_achievements_page_close_detail();
    update_focus();
}
bool ui_achievements_page_handle_input(ui_input_event_t input)
{
    if(!s_achievements.active) return false;
    if(ui_achievements_page_detail_visible()) {
        if(input == UI_INPUT_BACK || input == UI_INPUT_ENCODER_PRESS)
            ui_achievements_page_close_detail();
        return input == UI_INPUT_BACK || input == UI_INPUT_ENCODER_PRESS ||
               input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW;
    }
    if(input == UI_INPUT_ENCODER_CW || input == UI_INPUT_ENCODER_CCW) {
        int32_t next = s_achievements.selected + (input == UI_INPUT_ENCODER_CW ? 1 : -1);
        if(next < 0) next = 0;
        if(next >= (int32_t)UI_ACHIEVEMENT_COUNT) next = UI_ACHIEVEMENT_COUNT - 1;
        if(next != s_achievements.selected) {
            s_achievements.selected = next;
            update_focus();
            lv_obj_scroll_to_view(s_achievements.rows[next], ui_motion_scroll_mode());
            ui_sound_play(UI_SFX_FOCUS);
        }
        return true;
    }
    if(input == UI_INPUT_ENCODER_PRESS) {
        show_detail();
        ui_sound_play(UI_SFX_PRESS);
        return true;
    }
    return false;
}
size_t ui_achievements_page_count(void) { return UI_ACHIEVEMENT_COUNT; }
int32_t ui_achievements_page_selected_index(void) { return s_achievements.selected; }
bool ui_achievements_page_contains_scroll_point(int32_t x, int32_t y)
{
    if(s_achievements.list == NULL || ui_achievements_page_detail_visible()) return false;
    lv_area_t area;
    lv_obj_get_coords(s_achievements.list, &area);
    return x >= area.x1 && x <= area.x2 && y >= area.y1 && y <= area.y2;
}
