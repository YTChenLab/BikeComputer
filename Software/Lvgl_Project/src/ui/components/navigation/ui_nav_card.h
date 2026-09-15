#ifndef UI_NAV_CARD_H
#define UI_NAV_CARD_H

#include "lvgl.h"
#include "ui_nav_model.h"

typedef enum
{
    UI_NAV_CARD_METRIC = 0,
    UI_NAV_CARD_ETA,
    UI_NAV_CARD_DIRECTION
} ui_nav_card_kind_t;

typedef struct
{
    lv_obj_t *root;
    lv_obj_t *title;
    lv_obj_t *content;
    lv_obj_t *value;
    lv_obj_t *unit;
    lv_obj_t *arrow;
    lv_obj_t *instruction;
    ui_nav_card_kind_t kind;
    nav_direction_t direction;
    nav_direction_t pending_direction;
    uint32_t pending_distance;
} ui_nav_card_t;

void nav_card_create(ui_nav_card_t *card, lv_obj_t *grid,
                     uint8_t column, uint8_t row,
                     const char *title, ui_nav_card_kind_t kind);
void ui_nav_card_create_metric(ui_nav_card_t *card, lv_obj_t *grid,
                               uint8_t column, uint8_t row,
                               const char *title, const char *unit);
void ui_nav_card_create_eta(ui_nav_card_t *card, lv_obj_t *grid,
                            uint8_t column, uint8_t row);
void ui_nav_card_create_direction(ui_nav_card_t *card, lv_obj_t *grid,
                                  uint8_t column, uint8_t row);
void ui_nav_card_set_metric(ui_nav_card_t *card, const char *value);
void ui_nav_card_set_eta(ui_nav_card_t *card, uint8_t hour, uint8_t minute);
void ui_nav_card_set_direction(ui_nav_card_t *card, nav_direction_t direction,
                               uint32_t distance_m, bool animate);

#endif /* UI_NAV_CARD_H */
