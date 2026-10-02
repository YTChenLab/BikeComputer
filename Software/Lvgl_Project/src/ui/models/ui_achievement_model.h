#ifndef UI_ACHIEVEMENT_MODEL_H
#define UI_ACHIEVEMENT_MODEL_H
#include "ui_ride_history_model.h"
#define UI_ACHIEVEMENT_COUNT 16U
typedef enum { UI_ACHIEVEMENT_RIDES, UI_ACHIEVEMENT_DISTANCE,
    UI_ACHIEVEMENT_ASCENT, UI_ACHIEVEMENT_TIME } ui_achievement_category_t;
typedef struct {
    const char *title, *category_name, *requirement, *code;
    ui_achievement_category_t category;
    double target; /* rides, km, m, hours according to category */
    unsigned tier;
} ui_achievement_definition_t;
typedef struct {
    double current, remaining;
    unsigned progress_permille;
    bool unlocked;
} ui_achievement_progress_t;
const ui_achievement_definition_t *ui_achievement_definition(size_t index);
bool ui_achievement_evaluate(size_t index,
    const ui_ride_history_totals_t *totals, ui_achievement_progress_t *progress);
#endif
