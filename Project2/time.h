#include<stdint.h>

#ifndef __TIME_H
#define __TIME_H_
#endif



struct Time{

    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    uint32_t starting_rtcc_value;
};

struct Date{

    uint8_t day;
    uint8_t month;
    uint16_t year;
};

void InitializeTimeStructure(struct Time *init, uint8_t hour, uint8_t month, uint8_t second);

void InitializeDateStructure(struct Date *init, uint8_t day, uint8_t month, uint16_t year);

uint32_t CalculateCurrentDifference(struct Time *reference_set_time);
