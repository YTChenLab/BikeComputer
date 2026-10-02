#include "ui/models/ui_dashboard_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    uint8_t fields[6];
    for(unsigned preset=0;preset<3;++preset) {
        assert(ui_dashboard_preset_fields(preset,fields));
        assert(ui_dashboard_layout_valid(preset,fields));
        fields[1]=fields[0];
        assert(!ui_dashboard_layout_valid(UI_DASHBOARD_CUSTOM,fields));
    }
    assert(!ui_dashboard_preset_fields(UI_DASHBOARD_CUSTOM,fields));
    assert(ui_dashboard_preset_fields(UI_DASHBOARD_CRUISE,fields));
    ui_ride_field_t next=ui_dashboard_next_field(fields,0,1);
    assert(next==UI_RIDE_FIELD_AVERAGE_SPEED); /* Skip five occupied fields. */
    fields[0]=(uint8_t)next;
    assert(ui_dashboard_layout_valid(UI_DASHBOARD_CUSTOM,fields));
    assert(!ui_dashboard_layout_valid(UI_DASHBOARD_CRUISE,fields));
    fields[0]=UI_RIDE_FIELD_BATTERY;
    assert(!ui_dashboard_layout_valid(UI_DASHBOARD_CUSTOM,fields));
    assert(ui_dashboard_field_at(12)==NULL);

    ui_ride_data_t data={.quality_known=true,.power=0,.grade=-7.4f,.distance=0,
        .ride_seconds=360000,.temperature_c=0};
    char text[48];
    assert(!ui_dashboard_format_field(UI_RIDE_FIELD_POWER,&data,100,text,sizeof(text)));
    assert(strcmp(text,"--")==0);
    ui_ride_data_set_quality(&data,UI_RIDE_FIELD_POWER,true,true,100,500,UI_DATA_SOURCE_BLE);
    assert(ui_dashboard_format_field(UI_RIDE_FIELD_POWER,&data,600,text,sizeof(text)));
    assert(strcmp(text,"0")==0);
    assert(!ui_dashboard_format_field(UI_RIDE_FIELD_POWER,&data,601,text,sizeof(text)));
    assert(!ui_dashboard_format_field(UI_RIDE_FIELD_POWER,&data,99,text,sizeof(text)));
    ui_ride_data_set_quality(&data,UI_RIDE_FIELD_POWER,true,true,UINT32_MAX-100,500,UI_DATA_SOURCE_BLE);
    assert(ui_dashboard_format_field(UI_RIDE_FIELD_POWER,&data,100,text,sizeof(text)));
    ui_ride_data_set_quality(&data,UI_RIDE_FIELD_GRADE,true,true,100,0,UI_DATA_SOURCE_BAROMETER);
    assert(ui_dashboard_format_field(UI_RIDE_FIELD_GRADE,&data,100,text,sizeof(text)));
    assert(strcmp(text,"-7.4")==0);
    data.grade=NAN;
    assert(!ui_dashboard_format_field(UI_RIDE_FIELD_GRADE,&data,100,text,sizeof(text)));
    ui_ride_data_set_quality(&data,UI_RIDE_FIELD_DISTANCE,true,true,100,0,UI_DATA_SOURCE_FUSED);
    assert(ui_dashboard_format_field(UI_RIDE_FIELD_DISTANCE,&data,100,text,sizeof(text)));
    assert(strcmp(text,"0.00")==0);
    ui_ride_data_set_quality(&data,UI_RIDE_FIELD_TIME,true,true,100,0,UI_DATA_SOURCE_BOARD);
    assert(ui_dashboard_format_field(UI_RIDE_FIELD_TIME,&data,100,text,sizeof(text)));
    assert(strcmp(text,"100:00:00")==0);
    ui_ride_data_set_quality(&data,UI_RIDE_FIELD_TEMPERATURE,true,true,100,0,UI_DATA_SOURCE_BOARD);
    assert(ui_dashboard_format_field(UI_RIDE_FIELD_TEMPERATURE,&data,100,text,sizeof(text)));
    assert(strcmp(text,"0")==0);
    assert(!ui_dashboard_format_field(UI_RIDE_FIELD_TIME,NULL,100,text,sizeof(text)));
    assert(!ui_dashboard_format_field(UI_RIDE_FIELD_TIME,&data,100,NULL,0));
    assert(ui_dashboard_speed_color(-1)==0x30424F);
    assert(ui_dashboard_speed_color(NAN)==0x30424F);
    assert(ui_dashboard_speed_color(14.99f)==0x56B7E0);
    assert(ui_dashboard_speed_color(15)==0x70C68B);
    assert(ui_dashboard_speed_color(25)==0xFFD900);
    assert(ui_dashboard_speed_color(35)==0xFFBE59);
    assert(ui_dashboard_speed_color(45)==0xB59CFF);
    assert(ui_dashboard_speed_color(80)==0xB59CFF);
    puts("ui_dashboard_layout_test: PASS");
    return 0;
}
