// Ethernet Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6

// Pinning for IoT projects with wireless modules:
// N24L01+ RF transceiver
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS on PE0
//   INT on PB2
// Xbee module
//   DIN (UART1TX) on PC5
//   DOUT (UART1RX) on PC4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "eth0.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "math.h"
#include "timer.h"
#include "time.h"
#include "rtc.h"
#include "periodic.h"

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4

#define MAX_CHARS 100


uint8_t f_dhcp=2;
uint8_t f_discover =2;
uint8_t f_offer =2;
uint8_t f_request =2;
uint8_t f_ack =2;
uint8_t f_unicast=0;
uint8_t f_pub =0;
uint8_t temp_flag=0;
uint8_t time_flag=0;
uint8_t f_uart=0;
uint8_t g_led =0;
//uint8_t topic_length=0;
//uint8_t d_length=0;
bool isOffer =0;
bool isRenewed =0;
bool isRebind =0;
bool isack =0;
bool isarp=0;
bool isled =0;
bool istemp=0;
bool istime =0;
char topics[10][20]={'\0'};
uint8_t topic_count;
int periodic_time_value;
#define AIN3_MASK 1
struct stringStuff
{
    char strInput[MAX_CHARS];
    uint8_t fieldCount;
    uint8_t pos[20];
    uint8_t type[20];

    char argument[20];
    uint8_t w;
    uint8_t x;
    uint8_t y;
    uint8_t z;
    char data[20];

    char comparator_1[10];
    char input_1[10];
    char then_1[10];
    char data_2[20];
    char input_2[10];
    char then_2[10];
    char output[10];
    char data_3[10];


};

struct Time set_time = {0,0,0,0};
struct Date set_date = {0,0,0};

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Enable clocks

    SYSCTL_GPIOHBCTL_R = 0;

    // Enable clocks
    SYSCTL_RCGCADC_R |= SYSCTL_RCGCADC_R0;
    SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOE;

    // Configure AIN3 as an analog input
    GPIO_PORTE_AFSEL_R |= AIN3_MASK;                 // select alternative functions for AN3 (PE0)
    GPIO_PORTE_DEN_R &= ~AIN3_MASK;                  // turn off digital operation on pin PE0
    GPIO_PORTE_AMSEL_R |= AIN3_MASK;                 // turn on analog operation on pin PE0

    // Configure ADC
    ADC0_ACTSS_R &= ~ADC_ACTSS_ASEN3;                // disable sample sequencer 3 (SS3) for programming
    ADC0_CC_R = ADC_CC_CS_SYSPLL;                    // select PLL as the time base (not needed, since default value)
    ADC0_PC_R = ADC_PC_SR_1M;                        // select 1Msps rate
    ADC0_EMUX_R = ADC_EMUX_EM3_PROCESSOR;            // select SS3 bit in ADCPSSI as trigger
    ADC0_SSMUX3_R = 3;                               // set first sample to AIN3
    ADC0_SSCTL3_R = ADC_SSCTL3_END0;                 // mark first sample as the end
    ADC0_ACTSS_R |= ADC_ACTSS_ASEN3;

    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);

}

int16_t readAdc0Ss3()
{
    ADC0_PSSI_R |= ADC_PSSI_SS3;                     // set start bit
    while (ADC0_ACTSS_R & ADC_ACTSS_BUSY);           // wait until SS3 is not busy
    return ADC0_SSFIFO3_R;                           // get single result from the FIFO
}

void EnableSleepClocking()
{
    SYSCTL_SCGC0_R |=SYSCTL_SCGC0_HIB;
}
void displayConnectionInfo()
{
    uint8_t i;
    char str[10];
    uint8_t mac[6];
    uint8_t ip[4];
    etherGetMacAddress(mac);
    putsUart0("HW: ");
    for (i = 0; i < 6; i++)
    {
        sprintf(str, "%02x", mac[i]);
        putsUart0(str);
        if (i < 6-1)
            putcUart0(':');
    }
    putcUart0('\r\n');
    etherGetIpAddress(ip);
    putsUart0("IP: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }
    if (etherIsDhcpEnabled())
        putsUart0(" (dhcp)");
    else
        putsUart0(" (static)");
    putcUart0('\r\n');
    etherGetIpSubnetMask(ip);
    putsUart0("SN: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }
    putcUart0('\r\n');
    etherGetIpGatewayAddress(ip);
    putsUart0("GW: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }
    putcUart0('\r\n');
    etherGetdnsAddress(ip);
    putsUart0("DNS: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }
    putcUart0('\r\n');

    if (etherIsLinkUp())
        putsUart0("Link is up\n");
    else
        putsUart0("Link is down\n");
}

int strlnt(char *str1)
{
    uint16_t Length = 0;



    while (str1[Length] != '\0')
    {
        Length++;
    }
    return Length;
}

int strComp(const char str1[], const char str2[])
{
    int i = 0;
    while(str1[i] == str2[i])
    {
        if(str1[i] == '\0' && str2[i] == '\0')
            break;
        i++;
    }
    return str1[i] - str2[i];

}

void strCopy(char* str2, char* str1)
{
    int i ;

    for (i = 0; str1[i]!='\0'; ++i)
    {
        str2[i] = str1[i];
    }
    str2[i] = '\0';

}

int isDelim(char c)
{
    if(c==' '|| c== ','||c==':'||c==';'||c==0||c=='.')
        return 1;

    else
        return 0;
}

int isNumber(char c)
{
    if ((c>=48&&c<=57) || c=='<'||c=='='||c=='>'||c=='!')
        return 1;
    else
        return 0;
}

int isAlpha(char c)
{
    if(c>=97&&c<=122)
        return 1;
    else
        return 0;
}
struct stringStuff tokenizeString(struct stringStuff string1)
{
    int i =0;
    char c=0;
    int j=strlnt(string1.strInput);

    for(i=0;i<j;i++)
    {
        char a=string1.strInput[i];
        if (isAlpha(string1.strInput[i])==1 && isDelim(c))
        {
            string1.pos[string1.fieldCount]=i;
            string1.type[string1.fieldCount]='a';
            string1.fieldCount++;

        }
        if (isNumber(string1.strInput[i])==1 && isDelim(c))
        {
            string1.pos[string1.fieldCount]=i;
            string1.type[string1.fieldCount]='n' ;
            string1.fieldCount++;

        }
        if(a=='&'&& isDelim(c))
        {
            string1.pos[string1.fieldCount]=i;
            string1.type[string1.fieldCount]='&';
            string1.fieldCount++;
        }
        if (isDelim(a))
        {
            string1.strInput[i]='\0';
        }
        c= string1.strInput[i];

    }
    strCopy(string1.argument,&string1.strInput[string1.pos[1]]);
    strCopy(string1.data,&string1.strInput[string1.pos[2]]);
    string1.w = atoi(&string1.strInput[string1.pos[2]]);
    string1.x = atoi(&string1.strInput[string1.pos[3]]);
    string1.y = atoi(&string1.strInput[string1.pos[4]]);
    string1.z = atoi(&string1.strInput[string1.pos[5]]);
    strCopy(string1.comparator_1,&string1.strInput[string1.pos[2]]);
    strCopy(string1.input_1,&string1.strInput[string1.pos[3]]);
    strCopy(string1.then_1,&string1.strInput[string1.pos[4]]);
    strCopy(string1.data_2,&string1.strInput[string1.pos[5]]);
    strCopy(string1.input_2,&string1.strInput[string1.pos[6]]);
    strCopy(string1.then_2,&string1.strInput[string1.pos[7]]);
    strCopy(string1.output,&string1.strInput[string1.pos[8]]);
    strCopy(string1.data_3,&string1.strInput[string1.pos[9]]);
    return string1;

    return string1;
}

bool isCommand (char* strCmd, uint8_t minArgs,struct stringStuff string1)
{
    if (strComp(string1.strInput,strCmd)==0)
    {
        if(minArgs<=string1.fieldCount)
        {
            return true;
        }
    }

    return false;
}

uint16_t getValue(uint8_t argNumber,struct stringStuff string1)
{
    uint16_t num=atoi(&string1.strInput[string1.pos[argNumber+1]]);
    return num;
}

void getsUart0(char* str,uint8_t maxChars)
{
    int count = 0;
    while(true)
    {
        if (count == maxChars)
        {
            str[count]='\0';
            return;
        }
        char c = getcUart0();
        if (c == 8||c==127)
        {
            if(count != 0)
                count--;


        }

        if (c == 13)
        {
            str[count] = '\0';
            return;
        }
        if (c >= 32)
        {
            if (c >= 65 && c <=90 )
            {
                c=c+32;
                str[count] = c;
            }
            else
                str[count]=c;

            count++;

        }
    }
}


void initEeprom()
{
    SYSCTL_RCGCEEPROM_R = 1;
    _delay_cycles(3);
    while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING);
}

void writeEeprom(uint16_t add, uint32_t data)
{
    EEPROM_EEBLOCK_R = add >> 4;
    EEPROM_EEOFFSET_R = add & 0xF;
    EEPROM_EERDWR_R = data;
    while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING);
}

uint32_t readEeprom(uint16_t add)
{
    EEPROM_EEBLOCK_R = add >> 4;
    EEPROM_EEOFFSET_R = add & 0xF;
    return EEPROM_EERDWR_R;
}



void discover_flag_check()
{
    if(etherIsDhcpEnabled() ==1 &&  isOffer==0)
    {
        etherSendDiscoverMessage();
    }
}

void set_offer_flag_false()
{
    isOffer =0;
}

void set_offer_flag_true()
{
    isOffer =1;
}

void readconfig()
{
    uint32_t temp;
    uint8_t ip;

    if(readEeprom(1)==0xFFFFFFFF)
    {
        etherEnableDhcpMode();
        etherSendDiscoverMessage();
        startPeriodicTimer(discover_flag_check,15);
        set_offer_flag_false();


    }

    else

    {
        etherDisableDhcpMode();
    }
}

void set_renewal_flag_true()
{
    isRenewed =1;
}

void set_renewal_flag_false()
{
    isRenewed =0;
}



void testip()
{
    if(isack == 1 && isarp==1)
    {
        etherSendDeclineMessage();
        stopTimer(testip);
        startPeriodicTimer(discover_flag_check,15);

    }
    isack =0;
}

void t1()
{
    if(isRenewed ==0)
    {
        set_to_unicast();
        startPeriodicTimer(etherSendDHCPRequest,15);
        set_renewal_flag_true();
    }

    if(isRenewed ==1)
    {
        set_to_unicast();
        restartTimer(etherSendDHCPRequest);
    }


}

void set_is_rebind_true()
{
    isRebind=0;
}

void set_is_rebind_false()
{
    isRebind =1;
}

void t2()
{
    stopTimer(etherSendDHCPRequest);
    if(isRebind == 0)
    {
        startPeriodicTimer(etherSendDHCPRebind,15);
        set_is_rebind_true();
    }

    if(isRebind == 1)
    {
        restartTimer(etherSendDHCPRebind);
    }

}

bool check(char* comparator)
{
    if(strComp("&&",comparator)==0)
        return true;
    else
        return false;
}
//

uint8_t identifier(char* comparator)
{
    if(strComp("==",comparator)==0)
        return 1;
    else if (strComp("!=",comparator)==0)
        return 2;
    else if (strComp(">",comparator)==0)
        return 3;
    else if (strComp(">=",comparator)==0)
        return 4;
    else if (strComp("<",comparator)==0)
        return 5;
    else if (strComp("<=",comparator)==0)
        return 6;
    else
        return 0;

}

void add_topic(char name[20])
{ uint8_t i=0,j=0;
   for(i=0;i<10;i++)
   {
       if(topics[i][0]=='\0')
       {
           for(j=0;j<20;j++)
           {
               topics[i][j]=name[j];
           }
               break;
       }
   }
}

void delete_topic(char name[20])
{
    uint8_t i=0,j=0;
    for(i=0;i<10;i++)
      {
          if(strComp(topics[i],name)==0)
          {
              for(j=0;j<20;j++)
              {
                  topics[i][j]='\0';
              }
                  break;
          }
      }
}

void view_topics()
{

    uint8_t i=0,j=0;
    for(i=0;i<10;i++)
    {
        if(topics[i][0]!='\0')
        {
            putsUart0(topics[i]);
            putsUart0("\n\r");

        }
    }
}
bool search_topics(char name[20])
{

    uint8_t i=0,j=0;
    for(i=0;i<10;i++)
    {
        if(strComp(topics[i],name)==0)
        {

            return 1;
        }
    }

    return 0;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522



int main(void)
{
    uint8_t* udpData;
    uint8_t data[MAX_PACKET_SIZE];

    // Init controller
    initHw();
    RTCModuleRCGCInit();
    RTCInit();
    StartRTCCounting();
    EnableSleepClocking();
    initTimer();
    // Setup UART0
    initUart0();
    initEeprom();
    setUart0BaudRate(115200, 40e6);

    // Init ethernet interface (eth0)
    putsUart0("\nStarting eth0\n");
    etherSetMacAddress(2, 3, 4, 5, 6, 118);
    etherInit(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);

    etherDisableDhcpMode();
    etherSetIpAddress(192, 168, 1, 118);
    etherSetIpSubnetMask(255, 255, 255, 0);
    etherSetIpGatewayAddress(192, 168, 1, 1);
    etherSet_g_DNS(0,0,0,0);
    waitMicrosecond(100000);
    displayConnectionInfo();
    readconfig();
    //    // Flash LED
//    setPinValue(GREEN_LED, 1);
//    waitMicrosecond(100000);
//    setPinValue(GREEN_LED, 0);
//    waitMicrosecond(100000);
    char strInput[MAX_CHARS];
    ///  startPeriodicTimer(flash,1);
    //stopTimer(flash);
    //    waitMicrosecond(100000);
    //startPeriodicTimer(flash2,1);
    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    //  etherSendDiscoverMessage();

    //send_syn();
    while (true)
    {

        struct stringStuff string1, *string_test;
        string_test = &string1;
        //Put terminal processing here
        if (kbhitUart0())
        {
            getsUart0(string1.strInput, MAX_CHARS);
            putsUart0(string1.strInput);
            putsUart0("\n \r");
            string1.fieldCount=0;
            string1 = tokenizeString(string1);
            char* str;
            char* data;

            str=string_test->argument;
            int w,x,y,z;
            w = string_test-> w;
            x = string_test-> x;
            y = string_test-> y;
            z = string_test-> z;
            data = string_test->data;

            if(isCommand("if",5,string1))
            {
                if(check(string_test->then_1))
                {
                    //&&
                    //comparator_1 has comparator for first thing bef
                    //argument has 'temp'
                    //input_1 has temp value
                    //data_2 has 'time'
                    //input_2 has on/off
                    //output has "pub"
                    //data_3 has topic for publish
                    if(strComp(string_test->argument,"temp")==0)
                    {
                        //send_mqtt_subreq("temp",4);
                        uint16_t raw;
                        float instantTemp;
                        raw = readAdc0Ss3();
                        instantTemp = -(((raw+0.5) / 4096.0 * 3.3) - 0.424) / 0.00625;

                        if(identifier(string_test->comparator_1)==4)
                        {

                            if(instantTemp>=(atoi(string_test->input_1)))
                            {
                                temp_flag=1;
                            }
                            else
                            {
                                temp_flag =0;
                            }
                        }

                        if(identifier(string_test->comparator_1)==1)
                        {

                            if(instantTemp==(atoi(string_test->input_1)))
                            {
                                temp_flag=1;
                            }

                            else
                            {
                                temp_flag =0;
                            }

                        }

                        if(identifier(string_test->comparator_1)==2)
                        {

                            if(instantTemp!=(atoi(string_test->input_1)))
                            {
                                temp_flag=1;
                            }

                            else
                            {
                                temp_flag =0;
                            }

                        }

                        if(identifier(string_test->comparator_1)==3)
                        {

                            if(instantTemp>(atoi(string_test->input_1)))
                            {
                                temp_flag=1;
                            }

                            else
                            {
                                temp_flag =0;
                            }

                        }

                        if(identifier(string_test->comparator_1)==5)
                        {

                            if(instantTemp<(atoi(string_test->input_1)))
                            {
                                temp_flag=1;
                            }

                            else
                            {
                                temp_flag =0;
                            }

                        }

                        if(identifier(string_test->comparator_1)==6)
                        {

                            if(instantTemp<=(atoi(string_test->input_1)))
                            {
                                temp_flag=1;
                            }

                            else
                            {
                                temp_flag =0;
                            }
                        }

                        if(strComp(string_test->data_2,"time")==0)
                        {
                            //send_mqtt_subreq("time",4);
                            if(strComp(string_test->input_2,"on")==0)
                            {
                                time_flag =1;
                            }
                            else
                            {
                                time_flag =0;
                            }
                        }

                        if(strComp(string_test->output,"pub")==0)
                        {
                            if(strComp(string_test->data_3,"temp")==0)
                            {
                                send_mqtt_subreq("temp",4);
                                //if(istemp==0)
                                    add_topic("temp");

                                f_pub =1;


                            }

                            if(strComp(string_test->data_3,"time")==0)
                            {
                                send_mqtt_subreq("time",4);
                                add_topic("time");
                                f_pub =1;
                                time_flag =1;

                            }


                        }

                        if(strComp(string_test->output,"uart")==0)
                        {
                            if(strComp(string_test->data_3,"temp")==0)
                            {
                                send_mqtt_subreq("temp",4);
                                add_topic("temp");
                                f_uart =1;

                            }

                            if(strComp(string_test->data_3,"time")==0)
                            {
                                send_mqtt_subreq("time",4);
                                add_topic("time");
                                f_uart =1;
                                time_flag =1;

                            }
                        }

                    }






                }
                else
                {
                    if(identifier(string_test->comparator_1))
                    {
                        //for temp
                        //comparator_1 has comparator for first thing bef
                        //argument has 'temp'
                        //input_1 has temp value
                        // data_2 has then "topic"
                        //input_2 has "topic" value

                        if(strComp(string_test->argument,"temp")==0)
                        {
                            //send_mqtt_subreq("temp",4);
                            uint16_t raw;
                            float instantTemp;
                            raw = readAdc0Ss3();
                            instantTemp = -(((raw+0.5) / 4096.0 * 3.3) - 0.424) / 0.00625;

                            if(identifier(string_test->comparator_1)==4)
                            {

                                if(instantTemp>=(atoi(string_test->input_1)))
                                {
                                    temp_flag=1;
                                }
                                else
                                {
                                    temp_flag =0;
                                }
                            }

                            if(identifier(string_test->comparator_1)==1)
                            {

                                if(instantTemp==(atoi(string_test->input_1)))
                                {
                                    temp_flag=1;
                                }

                                else
                                {
                                    temp_flag =0;
                                }

                            }

                            if(identifier(string_test->comparator_1)==2)
                            {

                                if(instantTemp!=(atoi(string_test->input_1)))
                                {
                                    temp_flag=1;
                                }

                                else
                                {
                                    temp_flag =0;
                                }

                            }

                            if(identifier(string_test->comparator_1)==3)
                            {

                                if(instantTemp>(atoi(string_test->input_1)))
                                {
                                    temp_flag=1;
                                }

                                else
                                {
                                    temp_flag =0;
                                }

                            }

                            if(identifier(string_test->comparator_1)==5)
                            {

                                if(instantTemp<(atoi(string_test->input_1)))
                                {
                                    temp_flag=1;
                                }

                                else
                                {
                                    temp_flag =0;
                                }

                            }

                            if(identifier(string_test->comparator_1)==6)
                            {

                                if(instantTemp<=(atoi(string_test->input_1)))
                                {
                                    temp_flag=1;
                                }

                                else
                                {
                                    temp_flag =0;
                                }
                            }



                            if(strComp(string_test->data_2,"pub")==0)
                            {
                                if(strComp(string_test->input_2,"temp")==0)
                                {
                                    send_mqtt_subreq("temp",4);
                                    add_topic("temp");
                                    f_pub =1;


                                }




                            }

                            if(strComp(string_test->data_2,"led")==0)
                            {
                                if(strComp(string_test->input_2,"on")==0)
                                {
                                    //send_mqtt_subreq("temp",4);
                                    //f_pub =1;
                                    setPinValue(GREEN_LED, 1);
                                    g_led =1;


                                }

                                if(strComp(string_test->input_2,"off")==0)
                                {
                                    //send_mqtt_subreq("temp",4);
                                    //f_pub =1;
                                    setPinValue(GREEN_LED, 0);
                                    g_led =0;


                                }



                            }





                            if(strComp(string_test->data_2,"uart")==0)
                            {
                                if(strComp(string_test->input_2,"temp")==0)
                                {
                                    send_mqtt_subreq("temp",4);
                                    add_topic("temp");
                                    f_uart =1;

                                }


                            }

                        }









                    }
                    else
                    {
                        //for led
                        //argument has led
                        //comparator_1 has on/off
                        //then_1 has then "topic"
                        //data_2 has "topic" value
                        if(strComp(string_test->argument,"time")==0)
                        {
                            //send_mqtt_subreq(string_test->argument,strlnt(string_test->argument));
                            //check LED
                              //getPinValue(GREEN_LED);
                            if(strComp(string_test->comparator_1,"on")==0)
                            {
                                //if(g_led == 1)
                                //{
                                  send_mqtt_pubmsg("time","led on",4,6);
                                //add_topic("led on");
                                //}
                            }

                            if(strComp(string_test->comparator_1,"off")==0)
                            {
                                //if(g_led == 0)
                                //{
                                    send_mqtt_pubmsg("time","led off",4,6);
                                    add_topic("led off");

                                //}
                            }
                            //if(check_LED() == 1)
                            //{
                            //send_mqtt_pubmsg(string_test->then_1,string_test->data_2,strlnt(string_test->then_1),strlnt(string_test->data_2));
                            //}
                        }

                        // for  buttonpressed
                        //argument has buttonpressed
                        //comparator_1 has "then"
                        //input_1 has then "topic"

                        //putsUart0("yeezy");
                    }

                }


            }

            else if(isCommand("pub_time",0,string1))
            {
                if(f_pub == 1 && time_flag==1)
                {
                    uint32_t difference_value = CalculateCurrentDifference(&set_time);
                    UpdateDateandTimeValues(&set_date, &set_time, difference_value);
                    sprintf(str,"H:%d M:%d S:%d \n", set_time.hour, set_time.minute, set_time.second);
                    putsUart0(str);
                    send_mqtt_pubmsg("time",str,4,strlnt(str));
                }

            }

            //                        else if(isCommand("pub_time",0,string1))
            //                        {
            //                            send_mqtt_pubmsg("time","blah",4,4);
            //                        }
            else if(isCommand("set_time",3,string1))
            {
                InitializeTimeStructure(&set_time, w, x, y);
                send_mqtt_subreq("time",4);
                add_topic("time");

            }
            else if(isCommand("date",0,string1))
            {
                uint32_t difference_value = CalculateCurrentDifference(&set_time);
                UpdateDateandTimeValues(&set_date, &set_time, difference_value);
                sprintf(str,"The date is D:%d M:%d Y:%d \n", set_date.day, set_date.month, set_date.year);
                putsUart0(str);
            }

            else if(isCommand("set_date",3,string1))
            {
                InitializeDateStructure(&set_date, w, x, y);

            }

            else if(isCommand("pub_led_on",0,string1))
            {
                send_mqtt_pubmsg("led","on",3,2);
            }
            else if(isCommand("connect",0,string1))
            {
                send_syn();
            }
            else if(isCommand("pub_temp",0,string1))
            {
                if(f_pub==1 && temp_flag == 1)
                {
                    uint16_t raw;
                    float instantTemp;
                    raw = readAdc0Ss3();
                    instantTemp = -(((raw+0.5) / 4096.0 * 3.3) - 0.424) / 0.00625;

                    char temp_temp[20];
                    sprintf(temp_temp, "%4.1f",instantTemp);
                    putsUart0("temp");
                    putsUart0(temp_temp);
                    putsUart0("\r\n");
                    send_mqtt_pubmsg("temp",temp_temp,4,strlnt(temp_temp));
                }

                f_pub =0 ;
                temp_flag = 0;
                // ltoa(instantTemp,temp_temp);
                // putsUart0(temp_temp);
            }

            else if(isCommand("uart_temp",0,string1))
            {
                if(temp_flag==1 && f_uart == 1)
                {
                    uint16_t raw;
                    float instantTemp;
                    raw = readAdc0Ss3();
                    instantTemp = -(((raw+0.5) / 4096.0 * 3.3) - 0.424) / 0.00625;

                    char temp_temp[20];
                    sprintf(temp_temp, "%4.1f",instantTemp);
                    putsUart0("temp");
                    putsUart0(temp_temp);
                    putsUart0("\r\n");
                    //send_mqtt_pubmsg("temp",temp_temp,4,strlnt(temp_temp));
                }
                temp_flag =0;
                f_uart =0;
                // ltoa(instantTemp,temp_temp);
                // putsUart0(temp_temp);
            }

            else if(isCommand("temp",0,string1))
            {
                // if(temp_flag==1 && f_uart == 1)
                //{
                uint16_t raw;
                float instantTemp;
                raw = readAdc0Ss3();
                instantTemp = -(((raw+0.5) / 4096.0 * 3.3) - 0.424) / 0.00625;

                char temp_temp[20];
                sprintf(temp_temp, "%4.1f",instantTemp);
                putsUart0("temp");
                putsUart0(temp_temp);
                putsUart0("\r\n");
                //send_mqtt_pubmsg("temp",temp_temp,4,strlnt(temp_temp));
                //}
                //temp_flag =0;
                //f_uart =0;
                // ltoa(instantTemp,temp_temp);
                // putsUart0(temp_temp);
            }

            else if(isCommand("help_subs",0,string1))
            {
                view_topics();
            }

            else if(isCommand("uart_time",0,string1))
            {
                if(time_flag == 1 && f_uart == 1)
                {
                    uint32_t difference_value = CalculateCurrentDifference(&set_time);
                    UpdateDateandTimeValues(&set_date, &set_time, difference_value);
                    sprintf(str,"H:%d M:%d S:%d \n", set_time.hour, set_time.minute, set_time.second);
                    putsUart0(str);
                    //send_mqtt_pubmsg("time",str,4,strlnt(str));
                }

            }


            else if(isCommand("current_time",0,string1))
            {


                uint32_t difference_value = CalculateCurrentDifference(&set_time);
                UpdateDateandTimeValues(&set_date, &set_time, difference_value);
                sprintf(str,"H:%d M:%d S:%d \n", set_time.hour, set_time.minute, set_time.second);
                putsUart0(str);
                send_mqtt_pubmsg("time",str,4,strlnt(str));


            }

            else if(isCommand("time1",0,string1))
            {


                uint32_t difference_value = CalculateCurrentDifference(&set_time);
                UpdateDateandTimeValues(&set_date, &set_time, difference_value);
                sprintf(str,"H:%d M:%d S:%d \n", set_time.hour, set_time.minute, set_time.second);
                putsUart0(str);
                //send_mqtt_pubmsg("time",str,4,strlnt(str));


            }





            else if(isCommand("dhcp",1,string1))
            {
                if(strComp("on",str)==0)
                {
                    putsUart0("DHCP is now on");
                    f_discover =1;
                    f_offer =0;
                    f_dhcp =1;
                    etherSendDiscoverMessage();
                    writeEeprom(1,0xFFFFFFFF);
                    set_offer_flag_false();
                    etherEnableDhcpMode();

                    startPeriodicTimer(discover_flag_check,15);

                    putsUart0("\r\n");
                }

                else if(strComp("off",str)==0)
                {
                    putsUart0("DHCP is now off");
                    f_discover = 1;
                    etherDisableDhcpMode();
                    stopTimer(discover_flag_check);
                    stopTimer(etherSendDHCPRequest);
                    stopTimer(t1);
                    stopTimer(t2);
                    stopTimer(testip);
                    stopTimer(etherSendDHCPRequest);
                    stopTimer(etherSendDHCPRebind);
                    etherSetIpAddress(192, 168, 1, 118);
                    etherSetIpSubnetMask(255, 255, 255, 0);
                    etherSetIpGatewayAddress(192, 168, 1, 1);
                    etherSet_g_DNS(0,0,0,0);
                    etherSendDHCPRelease();
                    f_dhcp =0;
                    writeEeprom(1,0);
                    putsUart0("\r\n");
                }

                else if(strComp("refresh",str)==0)
                {
                    putsUart0("DHCP refreshed");
                    // f_dhcp = 0;
                    if(etherIsDhcpEnabled()==1)
                    {
                        set_to_unicast();
                        etherSendDHCPRequest();
                    }

                    else
                    {
                        putsUart0("DHCP Off Nothing to refresh");
                        putsUart0("\r\n");
                    }

                    putsUart0("\r\n");
                }

                else if(strComp("decline",str)==0)
                {
                    etherSendDeclineMessage();
                    putsUart0("DHCP Declined");
                    // f_dhcp = 0;

                    putsUart0("\r\n");
                }

                else if(strComp("release",str)==0)
                {

                    f_dhcp = 0;
                    //etherSendDHCPRelease();
                    if(etherIsDhcpEnabled()==1)
                    {
                        etherDisableDhcpMode();
                        stopTimer(discover_flag_check);
                        stopTimer(etherSendDHCPRequest);
                        stopTimer(t1);
                        stopTimer(t2);
                        stopTimer(testip);
                        stopTimer(etherSendDHCPRequest);
                        stopTimer(etherSendDHCPRebind);
                        etherSetIpAddress(192, 168, 1, 118);
                        etherSetIpSubnetMask(255, 255, 255, 0);
                        etherSetIpGatewayAddress(192, 168, 1, 1);
                        etherSet_g_DNS(0,0,0,0);
                        etherSendDHCPRelease();
                        putsUart0("DHCP RELEASED");
                        //etherSendDiscoverMessage();
                        // f_discover =1;
                        f_offer =0;
                        f_request =0;
                        f_ack = 0;
                        // flash();
                    }

                    else
                    {
                        putsUart0("DHCP Off Nothing to release");
                        putsUart0("\r\n");
                    }

                    putsUart0("\r\n");

                }
            }

            if(strcmp(strInput,"set ip")==0)
            {
                char w[3],x[3],y[1],z[3];

                putsUart0("Enter W");
                getsUart0(w,1);
                uint16_t i_w = atoi(w);
                writeEeprom(2,i_w);
                // f_dhcp = 0;

                putsUart0("Enter X");
                getsUart0(x,1);
                uint16_t i_x = atoi(x);
                writeEeprom(3,i_x);

                putsUart0("Enter Y");
                getsUart0(y,1);
                uint16_t i_y = atoi(y);
                writeEeprom(4,i_y);

                putsUart0("Enter Z");
                getsUart0(z,1);
                uint16_t i_z = atoi(z);
                writeEeprom(5,i_z);

                int a,b,c,d;

                a = readEeprom(2);
                b = readEeprom(3);
                c = readEeprom(4);
                d = readEeprom(5);

                etherSetIpAddress(i_w, i_x, i_y, i_z);
                putsUart0("\r\n");
            }


            else if(strcmp(strInput,"set gw")==0)
            {
                char e[3],f[3],g[1],h[3];

                putsUart0("Enter W");
                getsUart0(e,1);
                uint16_t i_e = atoi(e);
                writeEeprom(6,i_e);
                // f_dhcp = 0;

                putsUart0("Enter X");
                getsUart0(f,1);
                uint16_t i_f = atoi(f);
                writeEeprom(7,i_f);

                putsUart0("Enter Y");
                getsUart0(g,1);
                uint16_t i_g = atoi(g);
                writeEeprom(8,i_g);

                putsUart0("Enter Z");
                getsUart0(h,1);
                uint16_t i_h = atoi(h);
                writeEeprom(9,i_h);

                int aa,bb,cc,dd;

                aa = readEeprom(6);
                bb = readEeprom(7);
                cc = readEeprom(8);
                dd = readEeprom(9);

                etherSetIpGatewayAddress(i_e, i_f, i_g, i_h);
                putsUart0("\r\n");
            }




            //           if(strcmp(strInput,"start timer")==0)
            //           {
            //               startOneshotTimer(flash3, 5);
            //               startOneshotTimer(flash4, 10);
            //
            //
            //           }

            else if(strcmp(strInput,"set sn")==0)
            {
                char i[3],j[3],k[1],l[3];

                putsUart0("Enter W");
                getsUart0(i,1);
                uint16_t i_i = atoi(i);
                writeEeprom(10,i_i);
                // f_dhcp = 0;

                putsUart0("Enter X");
                getsUart0(j,1);
                uint16_t i_j = atoi(j);
                writeEeprom(11,i_j);

                putsUart0("Enter Y");
                getsUart0(k,1);
                uint16_t i_k = atoi(k);
                writeEeprom(12,i_k);

                putsUart0("Enter Z");
                getsUart0(l,1);
                uint16_t i_l = atoi(l);
                writeEeprom(13,i_l);

                int a_i,b_i,c_i,d_i;

                a_i = readEeprom(10);
                b_i = readEeprom(11);
                c_i = readEeprom(12);
                d_i = readEeprom(13);

                etherSetIpSubnetMask(i_i, i_j, i_k, i_l);

                putsUart0("\r\n");

            }

            else if(strcmp(strInput,"set dns")==0)
            {
                char i[3],j[3],k[1],l[3];

                putsUart0("Enter W");
                getsUart0(i,1);
                uint16_t i_i = atoi(i);
                writeEeprom(14,i_i);
                // f_dhcp = 0;

                putsUart0("Enter X");
                getsUart0(j,1);
                uint16_t i_j = atoi(j);
                writeEeprom(15,i_j);

                putsUart0("Enter Y");
                getsUart0(k,1);
                uint16_t i_k = atoi(k);
                writeEeprom(16,i_k);

                putsUart0("Enter Z");
                getsUart0(l,1);
                uint16_t i_l = atoi(l);
                writeEeprom(17,i_l);

                int a_i,b_i,c_i,d_i;

                //                      a_i = readEeprom(10);
                //                      b_i = readEeprom(11);
                //                      c_i = readEeprom(12);
                //                      d_i = readEeprom(13);

                etherSet_g_DNS(i_i, i_j, i_k, i_l);

                putsUart0("\r\n");

            }

            else if(isCommand("ifconfig",0,string1))
            {
                //ifconfig

                displayConnectionInfo();
            }

//            else if(isCommand("connect",0,string1))
//            {
//                send_mqtt_connect();
//            }

            else if(isCommand("subscribe",0,string1))
            {
                //str = topic;
                topic_length = strlnt(str);
                add_topic(str);
                send_mqtt_subreq(str,topic_length);
                add_topic(str);
            }

            else if(isCommand("publish",0,string1))
            { //str = topic;
                //data = data;
                //strlnt for str length

                topic_length = strlnt(str);
                d_length = strlnt(data);

                send_mqtt_pubmsg(str,data,topic_length,d_length);
            }

            else if(isCommand("ping",0,string1))
            {
                send_mqtt_ping();
            }

            else if(isCommand("disconnect",0,string1))
            {
                send_mqtt_disconnect();
            }

            else if(isCommand("unsubscribe",0,string1))
            {
                topic_length = strlnt(str);
                send_mqtt_unsub(str,topic_length);
                delete_topic(str);
            }

            else if(isCommand("reboot",0,string1))
            {
                putsUart0("working \n \r");
                putsUart0("\n \r");
                NVIC_APINT_R = NVIC_APINT_VECTKEY|NVIC_APINT_VECT_RESET;
            }


            //if(strcmp(strInput("Connect")))

            //           else
            //           {
            //               putsUart0("Invalid Command");
            //               putsUart0("\r\n");
            //           }

        }

        //        if(readEeprom(1)==1&& f_discover==0)
        //        {
        //            etherSendDiscoverMessage();
        //            //startPeriodicTimer(discover_flag_check,15);
        //        }

        //        if(readEeprom)
        //        {
        //            etherSendDiscoverMessage();
        //
        //            putsUart0("Discover");
        //            f_discover =1;
        //            f_dhcp=0;
        //           // f_offer =0;
        //            //f_request =0;
        //            //f_ack = 0;
        //        }



        // Packet processing
        if (etherIsDataAvailable())
        {
            if (etherIsOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
            }

            // Get packet
            etherGetPacket(data, MAX_PACKET_SIZE);

            // Handle ARP request
            if (etherIsArpRequest(data))
            {   if(isack == 1)
            {
                isarp = 1;
                isack=0;
            }
            etherSendArpResponse(data);
            }

            // Handle IP datagram
            if (etherIsIp(data))
            {
                if (etherIsIpUnicast(data))
                {

                    if(etherIsAck(data))
                    {
                        // in renew
                        isack = 1;
                        etherSendGratuitousArpRequest();
                        restartTimer(testip);
                        etherSet_g_IP();
                        set_renewal_flag_true();
                        stopTimer(etherSendDHCPRequest);
                        stopTimer(etherSendDHCPRebind);
                        stopTimer(t1);
                        restartTimer(t1);
                        stopTimer(t2);
                        restartTimer(t2);

                        //stopTimer(etherSendDHCPRequest);
                        // startOneshotTimer(t1,30);
                        //restartTimer(t1);
                    }
                    // handle icmp ping request
                    if (etherIsPingRequest(data))
                    {
                        etherSendPingResponse(data);
                    }

                    if(etherIsTcp(data))
                    {
                        // waitMicrosecond(1000);
                        // setPinValue(GREEN_LED, 1);

                        //                        if(get_tcp_flag(data)==htons(0x8012))
                        //                        {
                        //                            send_mqtt_ack();
                        //                        }

                        etherFrame* ether = (etherFrame*)data;
                        ipFrame* ip = (ipFrame*)&ether->data;
                        tcpMQTTFrame* tcp = (tcpMQTTFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
                        MQTTPubFrame *mqtt = (MQTTPubFrame*)&tcp->options;
                        uint32_t tcp_seqno = tcp->seq_no;
                        uint32_t tcp_ackno = tcp->ack_no;


                        //
                        //                        if(get_tcp_flag(data)==htons(0x8002))
                        //                        {
                        //                        send_syn_ack(data);
                        //                        }
                        //                        uint16_t flag_tcp = get_tcp_flag(data);
                        //                        char str[100];
                        //                        sprintf(str, "%02x",flag_tcp);
                        //                        putsUart0("flag_tcp");
                        //                        putsUart0(str);
                        //                        putsUart0("\r\n");

                        //                         if(get_tcp_flag(data)==htons(0x5018))
                        //                        {
                        //                            send_ack(data);
                        //                        }

                        //                        else if(get_tcp_flag(data)==htons(0x5011))
                        //                        {
                        //                            send_fin_ack(data);
                        //                        }

                        if(get_mqtt_tcp_flag(data)==htons(0x8012))
                        {

                            tcp_seqno = tcp->ack_no;
                            tcp_ackno = htonl(htonl(tcp->seq_no)+1);
                            send_mqtt_ack(tcp_ackno,tcp_seqno);

                                send_mqtt_connect(data);
                        }

                        else if(get_mqtt_tcp_flag(data)==htons(0x5018))
                        {
                            uint32_t datasize = htons(ip->length)-40;
                            tcp_seqno = tcp->ack_no;
                            tcp_ackno = htonl(htonl(tcp->seq_no)+datasize);
                            //if(search_topics(mqtt->topicname))
                            //{
                            char string[100];
                            uint8_t length = get_topic_length();
                            uint8_t i=0;

                             putsUart0(mqtt->topicname);
//                             putsUart0(mqtt->data);

//                            for(i=0;i<length;i++)
//                            {
//                                if(mqtt->topicname[i]=='0')
//                                {
//                                    continue;
//                                }
//                                else
//                                {
//                                    putcUart0(mqtt->topicname[i]);
//                                }
//                            }
//
//                            for(i=0;i<d_length;i++)
//                            {
//                                if(mqtt->data[i]=='0')
//                                {
//                                    continue;
//                                }
//                                else
//                                {
//                                    putcUart0(mqtt->data[i]);
//                                }
//                            }


                            putsUart0("\n\r");
                            putsUart0("\n\r");

//                            for(i=0;i<d_length;i++)
//                            {
//                                putcUart0(mqtt->data[i]);
//                            }
//                            putsUart0("\n\r");
//                            putsUart0("\n\r");
                                //putsUart0(mqtt->data);
//                                putsUart0("\n\r");
                            //}
                            send_mqtt_ack(tcp_ackno,tcp_seqno);

                        }

                        else if(get_mqtt_tcp_flag(data)==htons(0x5011))
                        {
                            uint32_t datasize = htons(ip->length)-40;
                            tcp_seqno = tcp->ack_no;
                            tcp_ackno = htonl(htonl(tcp->seq_no)+datasize);

                            send_mqtt_finack(tcp_ackno,tcp_seqno);

                        }

                        //setPinValue(GREEN_LED, 0);
                        //  setPinValue(RED_LED, 1);
                    }

                    // Process UDP datagram
                    // test this with a udp send utility like sendip
                    //   if sender IP (-is) is 192.168.1.198, this will attempt to
                    //   send the udp datagram (-d) to 192.168.1.199, port 1024 (-ud)
                    // sudo sendip -p ipv4 -is 192.168.1.198 -p udp -ud 1024 -d "on" 192.168.1.199
                    // sudo sendip -p ipv4 -is 192.168.1.198 -p udp -ud 1024 -d "off" 192.168.1.199
                    if (etherIsUdp(data))
                    {
                        udpData = etherGetUdpData(data);
                        if (strcmp((char*)udpData, "on") == 0)
                            setPinValue(GREEN_LED, 1);
                        if (strcmp((char*)udpData, "off") == 0)
                            setPinValue(GREEN_LED, 0);
                        etherSendUdpResponse(data, (uint8_t*)"Nikita", 9);

                    }


                }

                if (etherIsIpBroadcast(data))
                {


                    if(etherIsOffer(data))
                    {
                        //stop timer discover
                        stopTimer(discover_flag_check);
                        set_offer_flag_true();
                        etherSetOffer(data);
                        f_offer=1;
                        f_dhcp =0;
                        etherSendDHCPRequest();
                        f_discover =  1;

                        f_request=1;
                        //  putsUart0("offer");

                        putsUart0("\r\n");


                    }

                    if(etherIsAck(data))
                    {
                        //start one shot timer to test ip (Gratuitios arp for 2 seconds wait for response)
                        f_request =1;
                        f_offer =1;
                        f_discover =1;
                        f_ack =1;
                        etherSetAck(data);
                        isack=1;
                        // startOneshotTimer(flash3, 5);
                        //startOneshotTimer(flash4, 10);

                        //uint8_t ip_g[4];
                        // etherGet_g_IpAddress(ip_g);



                        //  etherSendDHCPRebind();
                        //if(f_ack ==1)
                        //{   etherSet_g_IP();
                        //startOneshotTimer(t1, 15);
                        //}

                        uint32_t ip_lease_time=get_ip_lease_time();

                        //                              char str[100];
                        //                              sprintf(str, "%02x",ip_lease_time);
                        //                              putsUart0("ip_lease_time");
                        //                              putsUart0(str);
                        //                              putsUart0("\r\n");

                        uint32_t half_lease_time = 0.5*ip_lease_time;
                        uint32_t t2_lease_time = 87.5*ip_lease_time;

                        etherSendGratuitousArpRequest();
                        startOneshotTimer(testip, 2);
                        etherSet_g_IP();
                        stopTimer(t2);
                        startOneshotTimer(t1, 15);
                        startOneshotTimer(t2, 100);
                        startOneshotTimer(discover_flag_check,120);





                    }
                }




            }
        }
    }
}
