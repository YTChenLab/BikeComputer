#include "ui_dashboard_editor.h"
#include "services/ui_preferences.h"
#include "../core/ui_motion.h"
#include "../core/ui_theme.h"
#include "../design/ui_design_tokens.h"
#include "../pages/ui_page_common.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    lv_obj_t *root, *list, *rows[9], *values[7], *hint;
    lv_obj_t *preview, *preview_fields[6], *preview_title;
    lv_obj_t *picker, *options[UI_DASHBOARD_FIELD_COUNT];
    ui_preferences_t draft;
    unsigned selected, choice, choices;
    bool presets;
} editor_context_t;
static editor_context_t s_editor;
static const char *s_positions[6]={"上左", "上右", "中左", "中右", "下左", "下右"};

static lv_obj_t *caption(lv_obj_t *parent,int x,int y,int width,const char *text)
{
    lv_obj_t *label=ui_page_label(parent,x,y,width,text,UI_FONT_PAGE_TEXT,LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
    return label;
}
bool ui_dashboard_editor_visible(void) { return s_editor.root!=NULL; }
static void close_picker(void)
{
    if(s_editor.picker) lv_obj_delete(s_editor.picker);
    s_editor.picker=NULL;
    memset(s_editor.options,0,sizeof(s_editor.options));
}
void ui_dashboard_editor_close(void)
{
    if(s_editor.root) lv_obj_delete(s_editor.root);
    memset(&s_editor,0,sizeof(s_editor));
}
static void refresh(void)
{
    lv_label_set_text(s_editor.values[0],ui_dashboard_preset_name(s_editor.draft.dashboard_preset));
    for(unsigned i=0;i<6;++i) {
        const ui_dashboard_field_definition_t *definition =
            ui_dashboard_field_definition((ui_ride_field_t)s_editor.draft.dashboard_fields[i]);
        const char *title = definition != NULL ? definition->title : "未配置";
        lv_label_set_text(s_editor.values[i+1], title);
        if(s_editor.preview_fields[i] != NULL) lv_label_set_text(s_editor.preview_fields[i], title);
    }
    if(s_editor.preview_title != NULL)
        lv_label_set_text(s_editor.preview_title,
                          ui_dashboard_preset_name(s_editor.draft.dashboard_preset));
    for(unsigned i=0;i<9;++i)
        lv_obj_set_style_border_color(s_editor.rows[i],i==s_editor.selected ?
            UI_TOKEN_COLOR_ACCENT : UI_TOKEN_COLOR_SURFACE_OUTLINE,0);
}
static bool choice_available(unsigned choice)
{
    if(s_editor.presets) return choice<3;
    const ui_dashboard_field_definition_t *definition=ui_dashboard_field_at(choice);
    if(definition==NULL || s_editor.selected==0 || s_editor.selected>6) return false;
    unsigned slot=s_editor.selected-1;
    for(unsigned i=0;i<6;++i)
        if(i!=slot && s_editor.draft.dashboard_fields[i]==(unsigned)definition->field) return false;
    return true;
}
static void focus_choice(void)
{
    for(unsigned i=0;i<s_editor.choices;++i)
        lv_obj_set_style_border_color(s_editor.options[i],i==s_editor.choice ?
            UI_TOKEN_COLOR_ACCENT : UI_TOKEN_COLOR_SURFACE_OUTLINE,0);
}
static void apply_choice(unsigned choice)
{
    if(!choice_available(choice)) return;
    if(s_editor.presets) {
        s_editor.draft.dashboard_preset=(uint8_t)choice;
        ui_dashboard_preset_fields(choice,s_editor.draft.dashboard_fields);
    } else {
        uint8_t field=(uint8_t)ui_dashboard_field_at(choice)->field;
        if(s_editor.draft.dashboard_fields[s_editor.selected-1]!=field) {
            s_editor.draft.dashboard_fields[s_editor.selected-1]=field;
            s_editor.draft.dashboard_preset=UI_DASHBOARD_CUSTOM;
        }
    }
    close_picker(); refresh();
}
static void option_clicked(lv_event_t *event)
{
    if(lv_event_get_code(event)==LV_EVENT_CLICKED)
        apply_choice((unsigned)(uintptr_t)lv_event_get_user_data(event));
}
static void picker_back_clicked(lv_event_t *event)
{
    if(lv_event_get_code(event)==LV_EVENT_CLICKED) close_picker();
}
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int width,int height)
{
    lv_obj_t *object=ui_page_box(parent,x,y,width,height,UI_TOKEN_COLOR_SURFACE_OUTLINE,1);
    lv_obj_set_style_bg_color(object,UI_TOKEN_COLOR_SURFACE,0);
    lv_obj_set_style_bg_opa(object,LV_OPA_COVER,0);
    lv_obj_set_style_radius(object,12,0);
    lv_obj_add_flag(object,LV_OBJ_FLAG_CLICKABLE);
    return object;
}
static void open_picker(void)
{
    s_editor.presets=s_editor.selected==0;
    s_editor.choices=s_editor.presets ? 3 : UI_DASHBOARD_FIELD_COUNT;
    s_editor.picker=ui_page_box(s_editor.root,0,0,480,854,lv_color_black(),0);
    lv_obj_set_style_bg_opa(s_editor.picker,LV_OPA_COVER,0);
    lv_obj_add_flag(s_editor.picker,LV_OBJ_FLAG_CLICKABLE);
    caption(s_editor.picker,20,24,440,s_editor.presets ? "选择骑行方案" : "选择指标");
    caption(s_editor.picker,20,65,440,s_editor.presets ?
        "方案会替换全部六格指标" : "已在其他格中的指标不可重复选择");
    lv_obj_t *list=lv_obj_create(s_editor.picker);
    lv_obj_remove_style_all(list);
    lv_obj_set_pos(list,16,112); lv_obj_set_size(list,448,UI_PAGE_ACTION_Y - 12 - 112);
    lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list,10,0);
    lv_obj_set_scroll_dir(list,LV_DIR_VER);
    ui_page_scrollbar_style(list);
    lv_obj_set_style_pad_right(list,12,0);
    s_editor.choice=0;
    for(unsigned i=0;i<s_editor.choices;++i) {
        lv_obj_t *row=box(list,0,0,432,96);
        s_editor.options[i]=row;
        const ui_dashboard_field_definition_t *definition=s_editor.presets ? NULL : ui_dashboard_field_at(i);
        caption(row,16,16,400,s_editor.presets ? ui_dashboard_preset_name(i) : definition->title);
        bool available=choice_available(i);
        const char *note=s_editor.presets ? (i==0 ? "速度优先 · 综合骑行" :
            i==1 ? "功率与心率 · 训练数据" : "坡度与海拔 · 爬升数据") :
            available ? "选择此指标" : "已用于其他格";
        lv_obj_t *hint=caption(row,16,55,400,note);
        lv_obj_set_style_text_color(hint,UI_TOKEN_COLOR_TEXT_SECONDARY,0);
        if(available) lv_obj_add_event_cb(row,option_clicked,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        else { lv_obj_clear_flag(row,LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_opa(row,LV_OPA_40,0); }
        if((s_editor.presets && i==s_editor.draft.dashboard_preset) ||
           (!s_editor.presets && (unsigned)definition->field==s_editor.draft.dashboard_fields[s_editor.selected-1]))
            s_editor.choice=i;
    }
    lv_obj_t *back=box(s_editor.picker,16,UI_PAGE_ACTION_Y,448,UI_PAGE_ACTION_HEIGHT);
    lv_obj_add_event_cb(back,picker_back_clicked,LV_EVENT_CLICKED,NULL);
    ui_align_label_center(caption(back,0,0,448,"返回布局编辑"));
    focus_choice();
    lv_obj_scroll_to_view(s_editor.options[s_editor.choice],LV_ANIM_OFF);
}
static void activate(unsigned index)
{
    s_editor.selected=index;
    if(index<7) { refresh(); open_picker(); return; }
    if(index==7) {
        ui_preferences_t latest;
        if(!ui_preferences_get(&latest)) return;
        /* Merge just the fields edited here, preserving concurrent changes. */
        memcpy(latest.dashboard_fields,s_editor.draft.dashboard_fields,UI_DASHBOARD_SLOT_COUNT);
        latest.dashboard_preset=s_editor.draft.dashboard_preset;
        if(!ui_preferences_submit(&latest)) {
            lv_label_set_text(s_editor.hint,"指标配置无效，请重新选择");
            lv_obj_clear_flag(s_editor.hint,LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }
    ui_dashboard_editor_close();
}
static void row_clicked(lv_event_t *event)
{
    if(lv_event_get_code(event)==LV_EVENT_CLICKED)
        activate((unsigned)(uintptr_t)lv_event_get_user_data(event));
}
void ui_dashboard_editor_open(void)
{
    if(s_editor.root || !ui_preferences_get(&s_editor.draft)) return;
    s_editor.root=ui_page_box(lv_layer_top(),0,0,480,854,lv_color_black(),0);
    lv_obj_set_style_bg_opa(s_editor.root,LV_OPA_COVER,0);
    lv_obj_add_flag(s_editor.root,LV_OBJ_FLAG_CLICKABLE);
    caption(s_editor.root,20,24,440,"骑行数据布局");
    caption(s_editor.root,20,65,440,"速度固定 · 六格指标可分别更换");
    s_editor.list=lv_obj_create(s_editor.root);
    lv_obj_remove_style_all(s_editor.list);
    lv_obj_set_pos(s_editor.list,16,112); lv_obj_set_size(s_editor.list,448,422);
    lv_obj_set_flex_flow(s_editor.list,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_editor.list,LV_DIR_VER);
    lv_obj_set_style_pad_row(s_editor.list,10,0);
    lv_obj_set_style_pad_right(s_editor.list,12,0);
    lv_obj_set_scrollbar_mode(s_editor.list,LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_width(s_editor.list,4,LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(s_editor.list,UI_TOKEN_COLOR_ACCENT,
                              LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(s_editor.list,LV_OPA_70,LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(s_editor.list,LV_RADIUS_CIRCLE,LV_PART_SCROLLBAR);
    for(unsigned i=0;i<7;++i) {
        lv_obj_t *row=box(s_editor.list,0,0,432,96);
        s_editor.rows[i]=row;
        caption(row,16,18,120,i==0 ? "骑行方案" : s_positions[i-1]);
        s_editor.values[i]=caption(row,144,18,270,"");
        lv_obj_t *hint=caption(row,16,55,400,i==0 ? "选择巡航 / 训练 / 爬坡" : "更换此位置的指标");
        lv_obj_set_style_text_color(hint,UI_TOKEN_COLOR_TEXT_SECONDARY,0);
        lv_obj_add_event_cb(row,row_clicked,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
    }
    s_editor.preview=box(s_editor.root,16,546,448,176);
    lv_obj_clear_flag(s_editor.preview,LV_OBJ_FLAG_CLICKABLE);
    caption(s_editor.preview,16,10,180,"当前布局预览");
    s_editor.preview_title=caption(s_editor.preview,210,10,214,"巡航");
    lv_obj_set_style_text_color(s_editor.preview_title,UI_TOKEN_COLOR_ACCENT,0);
    for(unsigned i=0;i<6;++i) {
        unsigned column=i%2U, row=i/2U;
        lv_obj_t *cell=ui_page_box(s_editor.preview,12+(int)column*210,
                                   42+(int)row*42,200,36,
                                   UI_TOKEN_COLOR_SURFACE_OUTLINE,1);
        lv_obj_set_style_bg_color(cell,UI_TOKEN_COLOR_CANVAS,0);
        lv_obj_set_style_bg_opa(cell,LV_OPA_COVER,0);
        lv_obj_set_style_radius(cell,8,0);
        s_editor.preview_fields[i]=caption(cell,0,0,200,"未配置");
        ui_align_label_center(s_editor.preview_fields[i]);
        lv_obj_set_style_text_color(s_editor.preview_fields[i],
                                    UI_TOKEN_COLOR_TEXT_SECONDARY,0);
    }
    /* Keep the validation channel available without showing persistent
     * encoder instructions that compete with the controls. */
    s_editor.hint=caption(s_editor.root,20,730,440,"");
    lv_obj_set_style_text_color(s_editor.hint,UI_TOKEN_COLOR_ACCENT,0);
    lv_obj_add_flag(s_editor.hint,LV_OBJ_FLAG_HIDDEN);
    for(unsigned i=7;i<9;++i) {
        lv_obj_t *button=box(s_editor.root,i==7 ? 16 : 248,UI_PAGE_ACTION_Y,216,UI_PAGE_ACTION_HEIGHT);
        s_editor.rows[i]=button;
        lv_obj_add_event_cb(button,row_clicked,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        ui_align_label_center(caption(button,0,0,216,i==7 ? "保存布局" : "取消"));
    }
    lv_obj_set_style_bg_color(s_editor.rows[7],UI_TOKEN_COLOR_ACCENT,0);
    lv_obj_set_style_text_color(lv_obj_get_child(s_editor.rows[7],0),UI_TOKEN_COLOR_CANVAS,0);
    refresh();
    ui_motion_translate_fade(s_editor.root,12,0,LV_OPA_COVER,LV_OPA_COVER,180,NULL,NULL);
}
bool ui_dashboard_editor_handle_input(ui_input_event_t input)
{
    if(!s_editor.root) return false;
    if(input==UI_INPUT_POWER_TOGGLE || input==UI_INPUT_OPEN_POWER ||
       input==UI_INPUT_RIDE_TOGGLE || input==UI_INPUT_TOGGLE_CONTROL_CENTER) {
        ui_dashboard_editor_close(); return false;
    }
    if(input==UI_INPUT_BACK || input==UI_INPUT_BACK_LONG) {
        if(s_editor.picker) close_picker(); else ui_dashboard_editor_close();
        return true;
    }
    if(input==UI_INPUT_ENCODER_PRESS) {
        if(s_editor.picker) apply_choice(s_editor.choice); else activate(s_editor.selected);
        return true;
    }
    int direction=input==UI_INPUT_ENCODER_CW ? 1 : input==UI_INPUT_ENCODER_CCW ? -1 : 0;
    if(direction) {
        if(s_editor.picker) {
            for(unsigned step=1;step<=s_editor.choices;++step) {
                unsigned next=(s_editor.choice+(direction>0 ? step : s_editor.choices-step))%s_editor.choices;
                if(choice_available(next)) { s_editor.choice=next; break; }
            }
            focus_choice(); lv_obj_scroll_to_view(s_editor.options[s_editor.choice],ui_motion_scroll_mode());
        } else {
            int next=(int)s_editor.selected+direction;
            s_editor.selected=(unsigned)LV_CLAMP(0,next,8);
            refresh();
            if(s_editor.selected<7) lv_obj_scroll_to_view(s_editor.rows[s_editor.selected],ui_motion_scroll_mode());
        }
    }
    return true;
}
