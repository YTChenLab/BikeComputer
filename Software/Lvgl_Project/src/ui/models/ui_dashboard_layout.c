#include "ui_dashboard_layout.h"
#include <stdio.h>
#include <string.h>
static const ui_dashboard_field_definition_t s_fields[UI_DASHBOARD_FIELD_COUNT] = {
    {UI_RIDE_FIELD_HEART_RATE, "心率", "bpm", false},
    {UI_RIDE_FIELD_POWER, "功率", "W", false},
    {UI_RIDE_FIELD_CADENCE, "踏频", "rpm", false},
    {UI_RIDE_FIELD_GRADE, "坡度", "%", false},
    {UI_RIDE_FIELD_DISTANCE, "里程", "km", true},
    {UI_RIDE_FIELD_TIME, "骑行时间", "", true},
    {UI_RIDE_FIELD_AVERAGE_SPEED, "平均速度", "km/h", true},
    {UI_RIDE_FIELD_MAXIMUM_SPEED, "最大速度", "km/h", true},
    {UI_RIDE_FIELD_ALTITUDE, "海拔", "m", true},
    {UI_RIDE_FIELD_ASCENT, "累计爬升", "m", true},
    {UI_RIDE_FIELD_CALORIES, "卡路里", "kcal", true},
    {UI_RIDE_FIELD_TEMPERATURE, "温度", "℃", false}
};
static const uint8_t s_presets[3][UI_DASHBOARD_SLOT_COUNT] = {
    {UI_RIDE_FIELD_HEART_RATE, UI_RIDE_FIELD_POWER, UI_RIDE_FIELD_CADENCE,
        UI_RIDE_FIELD_GRADE, UI_RIDE_FIELD_DISTANCE, UI_RIDE_FIELD_TIME},
    {UI_RIDE_FIELD_HEART_RATE, UI_RIDE_FIELD_POWER, UI_RIDE_FIELD_CADENCE,
        UI_RIDE_FIELD_AVERAGE_SPEED, UI_RIDE_FIELD_DISTANCE, UI_RIDE_FIELD_TIME},
    {UI_RIDE_FIELD_GRADE, UI_RIDE_FIELD_POWER, UI_RIDE_FIELD_ALTITUDE,
        UI_RIDE_FIELD_ASCENT, UI_RIDE_FIELD_DISTANCE, UI_RIDE_FIELD_TIME}
};
const ui_dashboard_field_definition_t *ui_dashboard_field_definition(ui_ride_field_t field)
{
    for(unsigned i = 0; i < UI_DASHBOARD_FIELD_COUNT; ++i)
        if(s_fields[i].field == field) return &s_fields[i];
    return NULL;
}
const ui_dashboard_field_definition_t *ui_dashboard_field_at(unsigned index)
{
    return index < UI_DASHBOARD_FIELD_COUNT ? &s_fields[index] : NULL;
}
const char *ui_dashboard_preset_name(unsigned preset)
{
    static const char *names[] = {"巡航", "训练", "爬坡", "自定义"};
    return preset < 4 ? names[preset] : "巡航";
}
bool ui_dashboard_preset_fields(unsigned preset, uint8_t fields[UI_DASHBOARD_SLOT_COUNT])
{
    if(preset >= 3 || fields == NULL) return false;
    memcpy(fields, s_presets[preset], UI_DASHBOARD_SLOT_COUNT);
    return true;
}
bool ui_dashboard_layout_valid(unsigned preset, const uint8_t fields[UI_DASHBOARD_SLOT_COUNT])
{
    if(preset > UI_DASHBOARD_CUSTOM || fields == NULL) return false;
    for(unsigned i = 0; i < UI_DASHBOARD_SLOT_COUNT; ++i) {
        if(ui_dashboard_field_definition((ui_ride_field_t)fields[i]) == NULL) return false;
        for(unsigned j = 0; j < i; ++j) if(fields[i] == fields[j]) return false;
    }
    return preset == UI_DASHBOARD_CUSTOM ||
           memcmp(fields, s_presets[preset], UI_DASHBOARD_SLOT_COUNT) == 0;
}
ui_ride_field_t ui_dashboard_next_field(const uint8_t fields[UI_DASHBOARD_SLOT_COUNT],
                                       unsigned slot, int direction)
{
    if(fields == NULL || slot >= UI_DASHBOARD_SLOT_COUNT) return UI_RIDE_FIELD_COUNT;
    unsigned current = 0;
    for(unsigned i = 0; i < UI_DASHBOARD_FIELD_COUNT; ++i)
        if((unsigned)s_fields[i].field == fields[slot]) current = i;
    for(unsigned step = 1; step <= UI_DASHBOARD_FIELD_COUNT; ++step) {
        unsigned candidate = (current + (direction < 0 ? UI_DASHBOARD_FIELD_COUNT - step : step))
                             % UI_DASHBOARD_FIELD_COUNT;
        bool used = false;
        for(unsigned i = 0; i < UI_DASHBOARD_SLOT_COUNT; ++i)
            if(i != slot && fields[i] == (unsigned)s_fields[candidate].field) used = true;
        if(!used) return s_fields[candidate].field;
    }
    return (ui_ride_field_t)fields[slot];
}
bool ui_dashboard_format_field(ui_ride_field_t field, const ui_ride_data_t *data,
                               uint32_t now, char *text, size_t capacity)
{
    if(text == NULL || capacity == 0) return false;
    if(ui_dashboard_field_definition(field) == NULL ||
       !ui_ride_data_field_valid(data, field, now)) {
        snprintf(text, capacity, "--"); return false;
    }
    switch(field) {
        case UI_RIDE_FIELD_HEART_RATE: snprintf(text, capacity, "%u", data->heart_rate); break;
        case UI_RIDE_FIELD_POWER: snprintf(text, capacity, "%u", data->power); break;
        case UI_RIDE_FIELD_CADENCE: snprintf(text, capacity, "%u", data->cadence); break;
        case UI_RIDE_FIELD_GRADE: snprintf(text, capacity, "%.1f", (double)data->grade); break;
        case UI_RIDE_FIELD_DISTANCE: snprintf(text, capacity, "%.2f", (double)data->distance); break;
        case UI_RIDE_FIELD_AVERAGE_SPEED: snprintf(text, capacity, "%.1f", (double)data->average_speed); break;
        case UI_RIDE_FIELD_MAXIMUM_SPEED: snprintf(text, capacity, "%.1f", (double)data->maximum_speed); break;
        case UI_RIDE_FIELD_ALTITUDE: snprintf(text, capacity, "%ld", (long)data->altitude_m); break;
        case UI_RIDE_FIELD_ASCENT: snprintf(text, capacity, "%lu", (unsigned long)data->ascent_m); break;
        case UI_RIDE_FIELD_CALORIES: snprintf(text, capacity, "%lu", (unsigned long)data->calories_kcal); break;
        case UI_RIDE_FIELD_TEMPERATURE: snprintf(text, capacity, "%d", data->temperature_c); break;
        case UI_RIDE_FIELD_TIME: snprintf(text, capacity, "%02lu:%02lu:%02lu",
            (unsigned long)(data->ride_seconds / 3600U),
            (unsigned long)(data->ride_seconds / 60U % 60U),
            (unsigned long)(data->ride_seconds % 60U)); break;
        default: snprintf(text, capacity, "--"); return false;
    }
    return true;
}
uint32_t ui_dashboard_speed_color(float speed)
{
    if(!isfinite(speed) || speed < 0) return 0x30424F;
    /* Display bands are product styling, never a risk/intensity classifier. */
    return speed < 15 ? 0x56B7E0 : speed < 25 ? 0x70C68B : speed < 35 ? 0xFFD900 :
           speed < 45 ? 0xFFBE59 : 0xB59CFF;
}
