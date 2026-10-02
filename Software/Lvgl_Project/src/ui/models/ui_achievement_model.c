#include "ui_achievement_model.h"
#include <math.h>
/* Stable catalog IDs use saved lifetime totals, independent of page caches.
 * Unlock dates, streaks and personal records require additional real data. */
static const ui_achievement_definition_t s_catalog[UI_ACHIEVEMENT_COUNT] = {
    {"首骑完成", "骑行次数", "保存你的第一次骑行", "R01", UI_ACHIEVEMENT_RIDES, 1, 1},
    {"十次骑行", "骑行次数", "累计保存 10 次骑行", "R02", UI_ACHIEVEMENT_RIDES, 10, 2},
    {"五十次骑行", "骑行次数", "累计保存 50 次骑行", "R03", UI_ACHIEVEMENT_RIDES, 50, 3},
    {"百次骑行", "骑行次数", "累计保存 100 次骑行", "R04", UI_ACHIEVEMENT_RIDES, 100, 4},
    {"百公里", "累计距离", "累计骑行 100 公里", "D01", UI_ACHIEVEMENT_DISTANCE, 100, 1},
    {"探索者", "累计距离", "累计骑行 500 公里", "D02", UI_ACHIEVEMENT_DISTANCE, 500, 2},
    {"千里骑士", "累计距离", "累计骑行 1,000 公里", "D03", UI_ACHIEVEMENT_DISTANCE, 1000, 3},
    {"远行者", "累计距离", "累计骑行 5,000 公里", "D04", UI_ACHIEVEMENT_DISTANCE, 5000, 4},
    {"初登山峰", "累计爬升", "累计爬升 1,000 米", "A01", UI_ACHIEVEMENT_ASCENT, 1000, 1},
    {"爬升达人", "累计爬升", "累计爬升 2,000 米", "A02", UI_ACHIEVEMENT_ASCENT, 2000, 2},
    {"登高进阶", "累计爬升", "累计爬升 10,000 米", "A03", UI_ACHIEVEMENT_ASCENT, 10000, 3},
    {"群山之上", "累计爬升", "累计爬升 50,000 米", "A04", UI_ACHIEVEMENT_ASCENT, 50000, 4},
    {"耐力起步", "累计时间", "累计骑行 5 小时", "T01", UI_ACHIEVEMENT_TIME, 5, 1},
    {"耐力骑手", "累计时间", "累计骑行 50 小时", "T02", UI_ACHIEVEMENT_TIME, 50, 2},
    {"百时之旅", "累计时间", "累计骑行 100 小时", "T03", UI_ACHIEVEMENT_TIME, 100, 3},
    {"长期积累", "累计时间", "累计骑行 500 小时", "T04", UI_ACHIEVEMENT_TIME, 500, 4}
};
const ui_achievement_definition_t *ui_achievement_definition(size_t index)
{
    return index < UI_ACHIEVEMENT_COUNT ? &s_catalog[index] : NULL;
}
bool ui_achievement_evaluate(size_t index,
    const ui_ride_history_totals_t *totals, ui_achievement_progress_t *progress)
{
    const ui_achievement_definition_t *definition = ui_achievement_definition(index);
    if(definition == NULL || totals == NULL || progress == NULL) return false;
    double value;
    switch(definition->category) {
        case UI_ACHIEVEMENT_RIDES: value = totals->ride_count; break;
        case UI_ACHIEVEMENT_DISTANCE: value = totals->total_distance_km; break;
        case UI_ACHIEVEMENT_ASCENT: value = (double)totals->total_ascent_m; break;
        case UI_ACHIEVEMENT_TIME: value = (double)totals->total_duration_seconds / 3600.0; break;
        default: return false;
    }
    if(!isfinite(value) || value < 0) return false;
    progress->current = value;
    progress->unlocked = value >= definition->target;
    progress->remaining = progress->unlocked ? 0 : definition->target - value;
    progress->progress_permille = progress->unlocked ? 1000U :
        (unsigned)(value * 1000.0 / definition->target);
    return true;
}
