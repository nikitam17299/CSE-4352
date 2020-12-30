#include "periodic.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "rtc.h"
#include "uart0.h"

void EnableNoHibWakeUpPeriodic(int time_value)
{
    int current_reading = (int)getSecondsValue();
    LoadRTCValue(getSecondsValue());
    RTCMatchNoHib(current_reading + time_value);
    putsUart0("\nCounter has started!\r\n");
}
