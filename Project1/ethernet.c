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

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4

#define MAX_CHARS 81


uint8_t f_dhcp=2;
uint8_t f_discover =2;
uint8_t f_offer =2;
uint8_t f_request =2;
uint8_t f_ack =2;
uint8_t f_unicast=0;
bool isOffer =0;
bool isRenewed =0;
bool isRebind =0;
bool isack =0;
bool isarp=0;


//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
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

void getsUart0(char* str, uint8_t maxChars)
{

    int count =0;


    char c;

   while(count<MAX_CHARS)
{

  c = getcUart0();


  if(count == MAX_CHARS-2)
                  {
                    str[count] = c;
                    str[count+1]=0;
                    return ;
                  }

  if((c == 8 || c == 127) && (count>0))
      {
          count--;
      }


  if(c == 13)
 {

     str[count]= 0;
     return ;
 }

  if((c >= 65) && (c <=90))
         {
            c = c + 32;
            str[count] = c;
            count++;
         }

  else if(c>=32)
     {
         str[count] = c;
         count++;
     }


}

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
    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);
    char strInput[MAX_CHARS];
  ///  startPeriodicTimer(flash,1);
    //stopTimer(flash);
//    waitMicrosecond(100000);
    //startPeriodicTimer(flash2,1);
    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
 //  etherSendDiscoverMessage();
    while (true)
    {
         //Put terminal processing here
        if (kbhitUart0())
        {
            getsUart0(strInput,MAX_CHARS);
            putsUart0("\r\n");
            putsUart0(strInput);
            putsUart0("\r\n");
            putsUart0("\r\n");

//            if(strcmp(strInput,"dhcp on\r\n")==0)
//            {
//                putsUart0("DHCP is now on");
//                f_discover =1;
//                f_offer =0;
//                f_dhcp =1;
//                etherSendDiscoverMessage();
//                writeEeprom(1,1);
//                set_offer_flag_false();
//                etherEnableDhcpMode();
//
//                startPeriodicTimer(discover_flag_check,15);
//
//                putsUart0("\r\n");
//            }

           if(strcmp(strInput,"dhcp on")==0)
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

           if(strcmp(strInput,"dhcp off")==0)
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

           if(strcmp(strInput,"dhcp refresh")==0)
           {
               putsUart0("DHCP refreshed");
              // f_dhcp = 0;
               set_to_unicast();
               etherSendDHCPRequest();
               putsUart0("\r\n");
           }

           if(strcmp(strInput,"dhcp decline")==0)
           {
               etherSendDeclineMessage();
               putsUart0("DHCP Declined");
               // f_dhcp = 0;

               putsUart0("\r\n");
           }

           if(strcmp(strInput,"dhcp release")==0)
           {

               f_dhcp = 0;
               //etherSendDHCPRelease();
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

               putsUart0("\r\n");

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


           if(strcmp(strInput,"set gw")==0)
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

           if(strcmp(strInput,"set sn")==0)
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

           if(strcmp(strInput,"set dns")==0)
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

           if(strcmp(strInput,"ifconfig")==0)
           {
               //if(readEeprom(1) == 0)
               //{
                   displayConnectionInfo();
               //}
           }

           if(strcmp(strInput,"reboot")==0)
           {
               putsUart0("Reeboted");
               NVIC_APINT_R = NVIC_APINT_VECTKEY|NVIC_APINT_VECT_RESET;
               putsUart0("\r\n");
           }

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
                        setPinValue(GREEN_LED, 1);

                        if(get_tcp_flag(data)==htons(0x8002))
                        {
                        send_syn_ack(data);
                        }
                        uint16_t flag_tcp = get_tcp_flag(data);
//                        char str[100];
//                        sprintf(str, "%02x",flag_tcp);
//                        putsUart0("flag_tcp");
//                        putsUart0(str);
//                        putsUart0("\r\n");

                        if(get_tcp_flag(data)==htons(0x5018))
                        {
                            send_ack(data);
                        }

                        if(get_tcp_flag(data)==htons(0x5011))
                        {
                            send_fin_ack(data);
                        }
                        setPinValue(GREEN_LED, 0);
                        setPinValue(RED_LED, 1);
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
                              startOneshotTimer(t1, half_lease_time);
                              startOneshotTimer(t2, t2_lease_time);
                              startOneshotTimer(discover_flag_check,ip_lease_time);





                    }
                }




            }
        }
    }
}
