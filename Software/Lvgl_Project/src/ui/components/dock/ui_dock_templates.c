#include "ui_dock_templates.h"

#include "../../core/ui_colors.h"
#include "../../core/ui_theme.h"
#include "../../design/ui_design_tokens.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static lv_obj_t *make_label(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t width, const char *text,
                            const lv_font_t *font, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, UI_COLOR_WHITE, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, align, 0);
    return label;
}

static lv_obj_t *make_button(lv_obj_t *parent, int32_t x, int32_t y,
                             int32_t width, int32_t height,
                             ui_dock_interaction_t interaction,
                             lv_event_cb_t event_cb)
{
    lv_obj_t *button = lv_obj_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(button, UI_TOKEN_OPA_PRIMARY, 0);
    lv_obj_set_style_border_color(button, UI_TOKEN_COLOR_SURFACE_BORDER, 0);
    lv_obj_set_style_border_width(button, UI_TOKEN_BORDER_THIN, 0);
    lv_obj_set_style_radius(button, UI_TOKEN_RADIUS_MEDIUM, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(button, event_cb, LV_EVENT_ALL,
                        (void *)(uintptr_t)interaction);
    return button;
}

static void add_button_label(lv_obj_t *button, lv_obj_t **label,
                             const char *text, const lv_font_t *font)
{
    /*
     * Do not derive the child width from lv_obj_get_width() here.  At template
     * construction time LVGL may not have completed the parent layout yet,
     * which used to leave the action label with a zero/invalid width.
     */
    *label = lv_label_create(button);
    lv_label_set_text(*label, text);
    lv_label_set_long_mode(*label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(*label, LV_PCT(100));
    lv_obj_set_style_text_color(*label, UI_TOKEN_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(*label, font, 0);
    lv_obj_set_style_text_align(*label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(*label);
    lv_obj_clear_flag(*label, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_common(ui_dock_template_t *view, lv_obj_t *parent,
                          ui_dock_type_t type, lv_event_cb_t event_cb)
{
    memset(view, 0, sizeof(*view));
    view->type = type;
    view->root = lv_obj_create(parent);
    lv_obj_remove_style_all(view->root);
    lv_obj_set_size(view->root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(view->root, 0, 0);
    lv_obj_set_style_bg_opa(view->root, UI_TOKEN_OPA_HIDDEN, 0);
    lv_obj_set_style_border_width(view->root, 0, 0);
    lv_obj_set_style_radius(view->root, 0, 0);
    lv_obj_clear_flag(view->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(view->root, LV_OBJ_FLAG_CLICKABLE);

    view->accent = lv_obj_create(view->root);
    lv_obj_remove_style_all(view->accent);
    lv_obj_set_pos(view->accent, 0, 18);
    lv_obj_set_size(view->accent, UI_TOKEN_SPACE_4, 120);
    lv_obj_set_style_bg_color(view->accent, UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(view->accent, UI_TOKEN_OPA_PRIMARY, 0);

    view->icon_bg = lv_obj_create(view->root);
    lv_obj_remove_style_all(view->icon_bg);
    lv_obj_set_pos(view->icon_bg, 16, 45);
    lv_obj_set_size(view->icon_bg, 46, 46);
    lv_obj_set_style_bg_color(view->icon_bg, UI_TOKEN_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(view->icon_bg, UI_TOKEN_OPA_PRIMARY, 0);
    lv_obj_set_style_border_color(view->icon_bg, UI_TOKEN_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(view->icon_bg, UI_TOKEN_BORDER_FOCUS, 0);
    lv_obj_set_style_radius(view->icon_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(view->icon_bg, LV_OBJ_FLAG_SCROLLABLE);

    view->icon = make_label(view->icon_bg, 0, 11, 46, LV_SYMBOL_OK,
                            LV_FONT_DEFAULT, LV_TEXT_ALIGN_CENTER);
    view->title = make_label(view->root, 76, 16, 230, "",
                             UI_FONT_SURFACE_TITLE, LV_TEXT_ALIGN_LEFT);
    view->message = make_label(view->root, 76, 52, 350, "",
                               UI_FONT_SURFACE_BODY, LV_TEXT_ALIGN_LEFT);
    /* Reserve an explicit gap before the close button; CONFIRM is the widest. */
    view->badge = make_label(view->root, 316, 18, 98, "INFO",
                             UI_FONT_SURFACE_BADGE, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_style_text_color(view->badge, UI_TOKEN_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_opa(view->badge, UI_TOKEN_OPA_SECONDARY, 0);

    view->close_button = make_button(view->root, 424, 10, 30, 30,
                                     UI_DOCK_INTERACTION_DISMISS, event_cb);
    lv_obj_set_style_border_width(view->close_button, 0, 0);
    lv_obj_t *close_icon = make_label(view->close_button, 0, 6, 30,
                                      LV_SYMBOL_CLOSE, LV_FONT_DEFAULT,
                                      LV_TEXT_ALIGN_CENTER);
    (void)close_icon;
    lv_obj_add_flag(view->root, LV_OBJ_FLAG_HIDDEN);
}

static void create_info(ui_dock_template_t *view)
{
    lv_obj_set_pos(view->message, 76, 61);
    lv_obj_set_width(view->message, 338);
}

static void create_action(ui_dock_template_t *view, lv_event_cb_t event_cb)
{
    lv_obj_set_width(view->message, 226);
    view->primary_button = make_button(view->root, 312, 74, 122, 54,
                                       UI_DOCK_INTERACTION_PRIMARY, event_cb);
    add_button_label(view->primary_button, &view->primary_label, "OK",
                     UI_FONT_PAGE_TEXT);
}

static void create_confirm(ui_dock_template_t *view, lv_event_cb_t event_cb)
{
    lv_obj_set_height(view->accent, 184);
    lv_obj_set_width(view->message, 344);
    view->secondary_button = make_button(view->root, 204, 158, 112, 48,
                                         UI_DOCK_INTERACTION_SECONDARY,
                                         event_cb);
    add_button_label(view->secondary_button, &view->secondary_label, "NO",
                     UI_FONT_PAGE_TEXT);
    view->primary_button = make_button(view->root, 328, 158, 112, 48,
                                       UI_DOCK_INTERACTION_PRIMARY, event_cb);
    add_button_label(view->primary_button, &view->primary_label, "YES",
                     UI_FONT_PAGE_TEXT);
    lv_obj_set_pos(view->message, 76, 66);
}

static void create_control(ui_dock_template_t *view, lv_event_cb_t event_cb)
{
    lv_obj_set_height(view->accent, 184);
    lv_obj_set_width(view->message, 245);
    view->value_label = make_label(view->root, 346, 72, 80, "0%",
                                   UI_FONT_SURFACE_BODY, LV_TEXT_ALIGN_RIGHT);
    view->slider = lv_slider_create(view->root);
    lv_obj_set_pos(view->slider, 76, 158);
    lv_obj_set_size(view->slider, 350, 14);
    lv_obj_set_style_bg_color(view->slider, UI_COLOR_DECORATION, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view->slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(view->slider, UI_COLOR_YELLOW, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(view->slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(view->slider, UI_COLOR_WHITE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(view->slider, 5, LV_PART_KNOB);
    lv_obj_add_event_cb(view->slider, event_cb, LV_EVENT_VALUE_CHANGED,
                        (void *)(uintptr_t)UI_DOCK_INTERACTION_VALUE_CHANGED);
}

static void create_navigation(ui_dock_template_t *view,
                              lv_event_cb_t event_cb)
{
    lv_obj_set_height(view->accent, 184);
    lv_obj_set_pos(view->icon_bg, 18, 82);
    lv_obj_set_size(view->icon_bg, 58, 58);
    lv_obj_set_pos(view->icon, 0, 16);
    lv_obj_set_width(view->icon, 58);
    lv_obj_set_pos(view->message, 92, 82);
    lv_obj_set_width(view->message, 236);
    view->primary_button = make_button(view->root, 342, 142, 98, 54,
                                       UI_DOCK_INTERACTION_PRIMARY, event_cb);
    add_button_label(view->primary_button, &view->primary_label, "OPEN",
                     &lv_font_montserrat_16);
}

static void create_media(ui_dock_template_t *view, lv_event_cb_t event_cb)
{
    lv_obj_set_height(view->accent, 184);
    lv_obj_set_width(view->message, 330);
    view->decrement_button = make_button(view->root, 76, 150, 50, 50,
                                         UI_DOCK_INTERACTION_DECREMENT,
                                         event_cb);
    make_label(view->decrement_button, 0, 11, 48, LV_SYMBOL_PREV,
               LV_FONT_DEFAULT, LV_TEXT_ALIGN_CENTER);
    view->toggle_button = make_button(view->root, 138, 150, 50, 50,
                                      UI_DOCK_INTERACTION_TOGGLE, event_cb);
    make_label(view->toggle_button, 0, 11, 48, LV_SYMBOL_PLAY,
               LV_FONT_DEFAULT, LV_TEXT_ALIGN_CENTER);
    view->increment_button = make_button(view->root, 200, 150, 50, 50,
                                         UI_DOCK_INTERACTION_INCREMENT,
                                         event_cb);
    make_label(view->increment_button, 0, 11, 48, LV_SYMBOL_NEXT,
               LV_FONT_DEFAULT, LV_TEXT_ALIGN_CENTER);
    view->secondary_button = make_button(view->root, 268, 150, 112, 50,
                                         UI_DOCK_INTERACTION_SECONDARY,
                                         event_cb);
    add_button_label(view->secondary_button, &view->secondary_label, "MODE",
                     &lv_font_montserrat_16);
}

void ui_dock_template_create(ui_dock_template_t *view, lv_obj_t *parent,
                             ui_dock_type_t type, lv_event_cb_t event_cb)
{
    if(view == NULL || parent == NULL || type >= UI_DOCK_TYPE_COUNT ||
       event_cb == NULL) return;
    create_common(view, parent, type, event_cb);
    switch(type) {
        case UI_DOCK_TYPE_INFO: create_info(view); break;
        case UI_DOCK_TYPE_ACTION: create_action(view, event_cb); break;
        case UI_DOCK_TYPE_CONFIRM: create_confirm(view, event_cb); break;
        case UI_DOCK_TYPE_CONTROL: create_control(view, event_cb); break;
        case UI_DOCK_TYPE_NAVIGATION: create_navigation(view, event_cb); break;
        case UI_DOCK_TYPE_MEDIA: create_media(view, event_cb); break;
        default: break;
    }
}

static lv_color_t priority_color(ui_dock_priority_t priority)
{
    if(priority == UI_DOCK_PRIORITY_P0_CRITICAL) return lv_color_hex(0xFF4D4F);
    if(priority <= UI_DOCK_PRIORITY_P2_RIDE_STATE) return UI_COLOR_YELLOW;
    return UI_COLOR_WHITE;
}

void ui_dock_template_update(ui_dock_template_t *view,
                             const ui_dock_card_t *card)
{
    static const char * const badges[] = {
        "INFO", "ACTION", "CONFIRM", "CONTROL", "NAV", "MEDIA"
    };
    static const char * const icons[] = {
        LV_SYMBOL_OK, LV_SYMBOL_PLAY, LV_SYMBOL_WARNING,
        LV_SYMBOL_SETTINGS, LV_SYMBOL_GPS, LV_SYMBOL_AUDIO
    };
    if(view == NULL || card == NULL || view->type != card->type) return;

    lv_color_t accent = priority_color(card->priority);
    lv_obj_set_style_bg_color(view->accent, accent, 0);
    lv_obj_set_style_border_color(view->icon_bg, accent, 0);
    lv_label_set_text(view->icon, icons[card->type]);
    lv_label_set_text(view->title, card->title);
    lv_label_set_text(view->message, card->message);
    lv_label_set_text(view->badge, badges[card->type]);

    if(card->dismissible) lv_obj_clear_flag(view->close_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(view->close_button, LV_OBJ_FLAG_HIDDEN);

    if(view->primary_label != NULL) {
        lv_label_set_text(view->primary_label,
                          card->primary_action[0] ? card->primary_action : "OK");
        lv_obj_set_style_border_color(view->primary_button, accent, 0);
    }
    if(view->secondary_label != NULL) {
        lv_label_set_text(view->secondary_label,
                          card->secondary_action[0] ? card->secondary_action :
                          (card->type == UI_DOCK_TYPE_MEDIA ? "MODE" : "BACK"));
    }
    if(view->slider != NULL) {
        int32_t minimum = card->minimum;
        int32_t maximum = card->maximum;
        if(maximum <= minimum) { minimum = 0; maximum = 100; }
        int32_t value = card->value;
        if(value < minimum) value = minimum;
        if(value > maximum) value = maximum;
        lv_slider_set_range(view->slider, minimum, maximum);
        lv_slider_set_value(view->slider, value, LV_ANIM_OFF);
        char value_text[20];
        snprintf(value_text, sizeof(value_text), "%ld%%", (long)value);
        lv_label_set_text(view->value_label, value_text);
    }
}
