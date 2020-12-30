#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <periodic.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "rtc.h"


//extern void Burst();
extern int periodic_time_value;
extern int num_samples;

uint32_t clear_value = 0;

void isHibWriteComplete(){

    while((HIB_CTL_R & HIB_CTL_WRC) == 0); //wait until the RTC Complete
}

uint32_t getSecondsValue(){

    return HIB_RTCC_R;
}

void RTCModuleRCGCInit(){

    SYSCTL_RCGCHIB_R |= SYSCTL_RCGCHIB_R0; // turn on clocking to the Hibernation Module

}

void LoadRTCValue(uint32_t value){

    HIB_RTCLD_R = value;
    isHibWriteComplete();
}

void RTCMatchNoHib(uint32_t match_value)
{
    HIB_RTCM0_R  = match_value;
    isHibWriteComplete();
}
void RTCMatchSetupNoHib(uint32_t match_value, uint32_t load_value){

    /*
     * Make sure that timer has been configured and NOT been instructed to count before using this function!
     */
    HIB_RTCM0_R  = match_value;
    isHibWriteComplete();
    //LoadRTCValue(load_value);
    HIB_IM_R |= HIB_IM_RTCALT0;
    isHibWriteComplete();
    NVIC_EN1_R |= 1 << (INT_HIBERNATE - 16 - 32); //turn on interrupts
}

void RTCMatchSetupHib(uint32_t match_value, uint32_t load_value){

    /*
     * Make sure that timer has been configured and NOT been instructed to count before using this function!
     */

    HIB_RTCM0_R  = match_value;
    isHibWriteComplete();
    LoadRTCValue(load_value);
    HIB_IM_R |= HIB_IM_RTCALT0; // set interrupt mask
    isHibWriteComplete();
    SetRTCWEN(); // enable match exit hibernation
    // Will Need to Enable Hibernation
    // Will Need to Start Counting
    NVIC_EN1_R |= 1 << (INT_HIBERNATE - 16 - 32); //turn on interrupts
}

void EnableHibernation()
{
    HIB_CTL_R |= HIB_CTL_HIBREQ;
    isHibWriteComplete();
}

void HibernateMatchISR(){

    putsUart0("Timer Expired!");
//    Burst();
    NVIC_SYS_CTRL_R |= NVIC_SYS_CTRL_SEVONPEND;
    HIB_IC_R |= HIB_IC_RTCALT0;     //Clear the interrupt
    EnableNoHibWakeUpPeriodic(periodic_time_value);
}
void StartRTCCounting(){

    HIB_CTL_R |= HIB_CTL_RTCEN;
    isHibWriteComplete();
}

void SetRTCWEN()
{
    HIB_CTL_R |= HIB_CTL_RTCWEN; //Enable the RTCWEN bit
    isHibWriteComplete();
}

void RTCInit(){

    /*
     Debug Note: All register write accesses for the Hibernation Register are ignored until the HIB_CTL_WRC is set to 1, indicating writes have been completed.
     */

    HIB_CTL_R &= ~(HIB_CTL_RTCEN); // Turn off RTC
    isHibWriteComplete();
    HIB_CTL_R |= HIB_CTL_CLK32EN; //Assuming OSCBYP is set to 0, enabling the internal 32.768KHz internal oscillator
    isHibWriteComplete();  // Wait until the WC bit is set signalling a write complete    isHibWriteComplete();
    HIB_RTCLD_R &= clear_value; // Set Load value to 0
    isHibWriteComplete();

}

