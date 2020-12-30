//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL with MPU 9250
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz
// Hibernation Module Clock : 32768 Hz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef RTC_H_
#define RTC_H_


//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void isHibWriteComplete();
uint32_t getSecondsValue();
void RTCModuleRCGCInit();
void LoadRTCValue(uint32_t value);
void RTCMatchSetupNoHib(uint32_t match_value, uint32_t load_value);
void RTCMatchNoHib(uint32_t match_value);
void RTCMatchSetupHib(uint32_t match_value, uint32_t load_value);
void EnableHibernationandRTCCount();
void SetRTCWEN();
void HibernateMatchISR();
void StartRTCCounting();
void RTCMatchSetNoHib(uint32_t match_value);
void RTCInit();

#endif
