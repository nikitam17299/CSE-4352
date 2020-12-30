#include "time.h"
#include "rtc.h"
#include <stdint.h>



void InitializeTimeStructure(struct Time *init, uint8_t hour, uint8_t month, uint8_t second)
{
    init->hour = hour;
    init->minute = month;
    init->second = second;

    init->starting_rtcc_value = getSecondsValue();
}

void InitializeDateStructure(struct Date *init, uint8_t day, uint8_t month, uint16_t year)
{
    init->day = day;
    init->month = month;
    init->year = year;
}

uint32_t CalculateCurrentDifference(struct Time *reference_set_time)
{
    int seconds_value = (int)getSecondsValue();
    int ref_time = (int)reference_set_time->starting_rtcc_value;
    int diff = (int)(seconds_value - ref_time);
    return diff;
}

void UpdateDateandTimeValues(struct Date *date_struct_pointer, struct Time *time_struct_pointer, uint32_t difference)
{
    uint32_t new_year_value = (uint32_t)difference/(31536000);
    date_struct_pointer->year = date_struct_pointer->year + new_year_value;
    difference = difference % 31536000;

    uint32_t new_month_value = (uint32_t)difference/(2592000);
    date_struct_pointer->month = date_struct_pointer->month + new_month_value;
    if(date_struct_pointer->month > 12)
    {
        date_struct_pointer->month = date_struct_pointer->month -12;
        date_struct_pointer->year = date_struct_pointer->year + 1;
    }
    difference = difference % 2592000;

    uint32_t new_day_value = (uint32_t)difference/(86400);
    date_struct_pointer->day = date_struct_pointer->day + new_day_value;
    if(date_struct_pointer->day > 30)
    {
        date_struct_pointer->day = date_struct_pointer->day - 30;
        date_struct_pointer->month = date_struct_pointer->month + 1;
    }
    difference = difference % 86400;

    uint32_t new_hour_value = (uint32_t)difference/(3600);
    time_struct_pointer->hour = time_struct_pointer->hour + new_hour_value;
    if( time_struct_pointer->hour >= 24)
    {
        time_struct_pointer->hour =  time_struct_pointer->hour - 24;
        date_struct_pointer->day =  date_struct_pointer->day + 1;
    }
    difference = difference% 3600;

    uint32_t new_minute_value = (uint32_t)difference/60;
    time_struct_pointer->minute = time_struct_pointer->minute + new_minute_value;
    if(time_struct_pointer->minute >= 60)
    {
        time_struct_pointer->minute = time_struct_pointer->minute - 60;
        time_struct_pointer->hour = time_struct_pointer->hour + 1;
    }
    difference = difference%60;

    uint32_t new_seconds_value = (uint32_t)difference;
    time_struct_pointer->second = time_struct_pointer->second + new_seconds_value;
    if(time_struct_pointer->second >= 60)
    {
        time_struct_pointer->second = time_struct_pointer->second - 60;
        time_struct_pointer->minute = time_struct_pointer->minute + 1;
    }

    time_struct_pointer->starting_rtcc_value =  getSecondsValue();
}
