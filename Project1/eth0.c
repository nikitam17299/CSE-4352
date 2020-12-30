// ETH0 Library
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

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <eth0.h>
#include <stdint.h>
#include <stdbool.h>
#include<stdio.h>
#include<string.h>
#include "tm4c123gh6pm.h"
#include "wait.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "timer.h"

// Pins
#define CS PORTA,3
#define WOL PORTB,3
#define INT PORTC,6

// Ether registers
#define ERDPTL      0x00
#define ERDPTH      0x01
#define EWRPTL      0x02
#define EWRPTH      0x03
#define ETXSTL      0x04
#define ETXSTH      0x05
#define ETXNDL      0x06
#define ETXNDH      0x07
#define ERXSTL      0x08
#define ERXSTH      0x09
#define ERXNDL      0x0A
#define ERXNDH      0x0B
#define ERXRDPTL    0x0C
#define ERXRDPTH    0x0D
#define ERXWRPTL    0x0E
#define ERXWRPTH    0x0F
#define EIE         0x1B
#define EIR         0x1C
#define RXERIF  0x01
#define TXERIF  0x02
#define TXIF    0x08
#define PKTIF   0x40
#define ESTAT       0x1D
#define CLKRDY  0x01
#define TXABORT 0x02
#define ECON2       0x1E
#define PKTDEC  0x40
#define ECON1       0x1F
#define RXEN    0x04
#define TXRTS   0x08
#define ERXFCON     0x38
#define EPKTCNT     0x39
#define MACON1      0x40
#define MARXEN  0x01
#define RXPAUS  0x04
#define TXPAUS  0x08
#define MACON2      0x41
#define MARST   0x80
#define MACON3      0x42
#define FULDPX  0x01
#define FRMLNEN 0x02
#define TXCRCEN 0x10
#define PAD60   0x20
#define MACON4      0x43
#define MABBIPG     0x44
#define MAIPGL      0x46
#define MAIPGH      0x47
#define MACLCON1    0x48
#define MACLCON2    0x49
#define MAMXFLL     0x4A
#define MAMXFLH     0x4B
#define MICMD       0x52
#define MIIRD   0x01
#define MIREGADR    0x54
#define MIWRL       0x56
#define MIWRH       0x57
#define MIRDL       0x58
#define MIRDH       0x59
#define MAADR1      0x60
#define MAADR0      0x61
#define MAADR3      0x62
#define MAADR2      0x63
#define MAADR5      0x64
#define MAADR4      0x65
#define MISTAT      0x6A
#define MIBUSY  0x01
#define ECOCON      0x75

// Ether phy registers
#define PHCON1      0x00
#define PDPXMD 0x0100
#define PHSTAT1     0x01
#define LSTAT  0x0400
#define PHCON2      0x10
#define HDLDIS 0x0100
#define PHLCON      0x14

// Packets
#define IP_ADD_LENGTH 4
#define HW_ADD_LENGTH 6

#define MAX_PACKET_SIZE 1522;

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------

uint8_t nextPacketLsb = 0x00;
uint8_t nextPacketMsb = 0x00;
uint8_t sequenceId = 1;
uint32_t sum;
uint8_t macAddress[HW_ADD_LENGTH] = {2,3,4,5,6,7};
uint8_t ipAddress[IP_ADD_LENGTH] = {0,0,0,0};
uint8_t ipSubnetMask[IP_ADD_LENGTH] = {255,255,255,0};
uint8_t ipGwAddress[IP_ADD_LENGTH] = {0,0,0,0};
bool dhcpEnabled = true;
uint8_t g_yiaddr[4];
uint8_t g_siaddr[4];
uint8_t g_subnet[4];
uint8_t g_gateway[4];
uint8_t g_dns[4];
uint8_t g_ether_server[6];
uint8_t ack_ip_lease[4];

bool isUnicast =0;
//bool isOffer =0;
//bool isRenewed =0;
//bool isRebind =0;
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

// This M4F is little endian (TI hardwired it this way)
// Network byte order is big endian
// Must interpret uint16_t in reverse order

typedef struct _enc28j60Frame // 4-bytes
{
    uint16_t size;
    uint16_t status;
    uint8_t data;
} enc28j60Frame;

typedef struct _etherFrame // 14-bytes
{
    uint8_t destAddress[6];
    uint8_t sourceAddress[6];
    uint16_t frameType;
    uint8_t data;
} etherFrame;

typedef struct _ipFrame // minimum 20 bytes
{
    uint8_t revSize;
    uint8_t typeOfService;
    uint16_t length;
    uint16_t id;
    uint16_t flagsAndOffset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t headerChecksum;
    uint8_t sourceIp[4];
    uint8_t destIp[4];
} ipFrame;

typedef struct _icmpFrame
{
    uint8_t type;
    uint8_t code;
    uint16_t check;
    uint16_t id;
    uint16_t seq_no;
    uint8_t data;
} icmpFrame;

typedef struct _arpFrame
{
    uint16_t hardwareType;
    uint16_t protocolType;
    uint8_t hardwareSize;
    uint8_t protocolSize;
    uint16_t op;
    uint8_t sourceAddress[6];
    uint8_t sourceIp[4];
    uint8_t destAddress[6];
    uint8_t destIp[4];
} arpFrame;

typedef struct _udpFrame // 8 bytes
{
    uint16_t sourcePort;
    uint16_t destPort;
    uint16_t length;
    uint16_t check;
    uint8_t  data;
} udpFrame;

typedef struct _dhcpFrame
{
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t  xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[16];
    uint8_t data[192];
    uint32_t magicCookie;
    uint8_t options[0];
} dhcpFrame;

typedef struct _discoveroptions
{
    uint8_t msgtype[3];
    uint8_t clientid[9];
    uint8_t parareqlist[6];
    //uint8_t dns[6];
    uint8_t endoption;


}discoveroptions;

typedef struct _dhcprequestoptions
{
    uint8_t msgtype[3];
    uint8_t clientid[9];
    uint8_t parareqlist[6];
    uint8_t req_ip[6];
    uint8_t ser_ip[6];
    uint8_t endoption;


}dhcprequestoptions;

typedef struct _tcpFrame
{
    uint16_t srcport;
    uint16_t destport;
    uint32_t seq_no;
    uint32_t ack_no;
    uint16_t data_offset;
    uint16_t win_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
    //uint16_t options[0];
    uint16_t data;

}tcpFrame;


typedef struct _dhcp_ack_requestoptions
{
    uint8_t msgtype[3];
    uint8_t clientid[9];
    uint8_t parareqlist[6];
    //uint8_t req_ip[6];
    //uint8_t ser_ip[6];
    uint8_t endoption;


}dhcp_ack_requestoptions;


//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Buffer is configured as follows
// Receive buffer starts at 0x0000 (bottom 6666 bytes of 8K space)
// Transmit buffer at 01A0A (top 1526 bytes of 8K space)

void etherCsOn()
{
    setPinValue(CS, 0);
    __asm (" NOP");                    // allow line to settle
    __asm (" NOP");
    __asm (" NOP");
    __asm (" NOP");
}

void etherCsOff()
{
    setPinValue(CS, 1);
}

void etherWriteReg(uint8_t reg, uint8_t data)
{
    etherCsOn();
    writeSpi0Data(0x40 | (reg & 0x1F));
    readSpi0Data();
    writeSpi0Data(data);
    readSpi0Data();
    etherCsOff();
}

uint8_t etherReadReg(uint8_t reg)
{
    uint8_t data;
    etherCsOn();
    writeSpi0Data(0x00 | (reg & 0x1F));
    readSpi0Data();
    writeSpi0Data(0);
    data = readSpi0Data();
    etherCsOff();
    return data;
}

void etherSetReg(uint8_t reg, uint8_t mask)
{
    etherCsOn();
    writeSpi0Data(0x80 | (reg & 0x1F));
    readSpi0Data();
    writeSpi0Data(mask);
    readSpi0Data();
    etherCsOff();
}

void etherClearReg(uint8_t reg, uint8_t mask)
{
    etherCsOn();
    writeSpi0Data(0xA0 | (reg & 0x1F));
    readSpi0Data();
    writeSpi0Data(mask);
    readSpi0Data();
    etherCsOff();
}

void etherSetBank(uint8_t reg)
{
    etherClearReg(ECON1, 0x03);
    etherSetReg(ECON1, reg >> 5);
}

void etherWritePhy(uint8_t reg, uint16_t data)
{
    etherSetBank(MIREGADR);
    etherWriteReg(MIREGADR, reg);
    etherWriteReg(MIWRL, data & 0xFF);
    etherWriteReg(MIWRH, (data >> 8) & 0xFF);
}

uint16_t etherReadPhy(uint8_t reg)
{
    uint16_t data, dataH;
    etherSetBank(MIREGADR);
    etherWriteReg(MIREGADR, reg);
    etherWriteReg(MICMD, MIIRD);
    waitMicrosecond(11);
    etherSetBank(MISTAT);
    while ((etherReadReg(MISTAT) & MIBUSY) != 0);
    etherSetBank(MICMD);
    etherWriteReg(MICMD, 0);
    data = etherReadReg(MIRDL);
    dataH = etherReadReg(MIRDH);
    data |= (dataH << 8);
    return data;
}

void etherWriteMemStart()
{
    etherCsOn();
    writeSpi0Data(0x7A);
    readSpi0Data();
}

void etherWriteMem(uint8_t data)
{
    writeSpi0Data(data);
    readSpi0Data();
}

void etherWriteMemStop()
{
    etherCsOff();
}

void etherReadMemStart()
{
    etherCsOn();
    writeSpi0Data(0x3A);
    readSpi0Data();
}

uint8_t etherReadMem()
{
    writeSpi0Data(0);
    return readSpi0Data();
}

void etherReadMemStop()
{
    etherCsOff();
}

// Initializes ethernet device
// Uses order suggested in Chapter 6 of datasheet except 6.4 OST which is first here
void etherInit(uint16_t mode)
{
    // Initialize SPI0
    initSpi0(USE_SSI0_RX);
    setSpi0BaudRate(4e6, 40e6);
    setSpi0Mode(0, 0);

    // Enable clocks
    enablePort(PORTA);
    enablePort(PORTB);
    enablePort(PORTC);

    // Configure pins for ethernet module
    selectPinPushPullOutput(CS);
    selectPinDigitalInput(WOL);
    selectPinDigitalInput(INT);

    // make sure that oscillator start-up timer has expired
    while ((etherReadReg(ESTAT) & CLKRDY) == 0) {}

    // disable transmission and reception of packets
    etherClearReg(ECON1, RXEN);
    etherClearReg(ECON1, TXRTS);

    // initialize receive buffer space
    etherSetBank(ERXSTL);
    etherWriteReg(ERXSTL, LOBYTE(0x0000));
    etherWriteReg(ERXSTH, HIBYTE(0x0000));
    etherWriteReg(ERXNDL, LOBYTE(0x1A09));
    etherWriteReg(ERXNDH, HIBYTE(0x1A09));

    // initialize receiver write and read ptrs
    // at startup, will write from 0 to 1A08 only and will not overwrite rd ptr
    etherWriteReg(ERXWRPTL, LOBYTE(0x0000));
    etherWriteReg(ERXWRPTH, HIBYTE(0x0000));
    etherWriteReg(ERXRDPTL, LOBYTE(0x1A09));
    etherWriteReg(ERXRDPTH, HIBYTE(0x1A09));
    etherWriteReg(ERDPTL, LOBYTE(0x0000));
    etherWriteReg(ERDPTH, HIBYTE(0x0000));

    // setup receive filter
    // always check CRC, use OR mode
    etherSetBank(ERXFCON);
    etherWriteReg(ERXFCON, (mode | ETHER_CHECKCRC) & 0xFF);

    // bring mac out of reset
    etherSetBank(MACON2);
    etherWriteReg(MACON2, 0);

    // enable mac rx, enable pause control for full duplex
    etherWriteReg(MACON1, TXPAUS | RXPAUS | MARXEN);

    // enable padding to 60 bytes (no runt packets)
    // add crc to tx packets, set full or half duplex
    if ((mode & ETHER_FULLDUPLEX) != 0)
        etherWriteReg(MACON3, FULDPX | FRMLNEN | TXCRCEN | PAD60);
    else
        etherWriteReg(MACON3, FRMLNEN | TXCRCEN | PAD60);

    // leave MACON4 as reset

    // set maximum rx packet size
    etherWriteReg(MAMXFLL, LOBYTE(1518));
    etherWriteReg(MAMXFLH, HIBYTE(1518));

    // set back-to-back inter-packet gap to 9.6us
    if ((mode & ETHER_FULLDUPLEX) != 0)
        etherWriteReg(MABBIPG, 0x15);
    else
        etherWriteReg(MABBIPG, 0x12);

    // set non-back-to-back inter-packet gap registers
    etherWriteReg(MAIPGL, 0x12);
    etherWriteReg(MAIPGH, 0x0C);

    // leave collision window MACLCON2 as reset

    // setup mac address
    etherSetBank(MAADR0);
    etherWriteReg(MAADR5, macAddress[0]);
    etherWriteReg(MAADR4, macAddress[1]);
    etherWriteReg(MAADR3, macAddress[2]);
    etherWriteReg(MAADR2, macAddress[3]);
    etherWriteReg(MAADR1, macAddress[4]);
    etherWriteReg(MAADR0, macAddress[5]);

    // initialize phy duplex
    if ((mode & ETHER_FULLDUPLEX) != 0)
        etherWritePhy(PHCON1, PDPXMD);
    else
        etherWritePhy(PHCON1, 0);

    // disable phy loopback if in half-duplex mode
    etherWritePhy(PHCON2, HDLDIS);

    // Flash LEDA and LEDB
    etherWritePhy(PHLCON, 0x0880);
    waitMicrosecond(100000);

    // set LEDA (link status) and LEDB (tx/rx activity)
    // stretch LED on to 40ms (default)
    etherWritePhy(PHLCON, 0x0472);
    // enable reception
    etherSetReg(ECON1, RXEN);
}

// Returns true if link is up
bool etherIsLinkUp()
{
    return (etherReadPhy(PHSTAT1) & LSTAT) != 0;
}

// Returns TRUE if packet received
bool etherIsDataAvailable()
{
    return ((etherReadReg(EIR) & PKTIF) != 0);
}

// Returns true if rx buffer overflowed after correcting the problem
bool etherIsOverflow()
{
    bool err;
    err = (etherReadReg(EIR) & RXERIF) != 0;
    if (err)
        etherClearReg(EIR, RXERIF);
    return err;
}

// Returns up to max_size characters in data buffer
// Returns number of bytes copied to buffer
// Contents written are 16-bit size, 16-bit status, payload excl crc
uint16_t etherGetPacket(uint8_t packet[], uint16_t maxSize)
{
    uint16_t i = 0, size, tmp16, status;

    // enable read from FIFO buffers
    etherReadMemStart();

    // get next packet information
    nextPacketLsb = etherReadMem();
    nextPacketMsb = etherReadMem();

    // calc size
    // don't return crc, instead return size + status, so size is correct
    size = etherReadMem();
    tmp16 = etherReadMem();
    size |= (tmp16 << 8);

    // get status (currently unused)
    status = etherReadMem();
    tmp16 = etherReadMem();
    status |= (tmp16 << 8);

    // copy data
    if (size > maxSize)
        size = maxSize;
    while (i < size)
        packet[i++] = etherReadMem();

    // end read from FIFO buffers
    etherReadMemStop();

    // advance read pointer
    etherSetBank(ERXRDPTL);
    etherWriteReg(ERXRDPTL, nextPacketLsb); // hw ptr
    etherWriteReg(ERXRDPTH, nextPacketMsb);
    etherWriteReg(ERDPTL, nextPacketLsb);   // dma rd ptr
    etherWriteReg(ERDPTH, nextPacketMsb);

    // decrement packet counter so that PKTIF is maintained correctly
    etherSetReg(ECON2, PKTDEC);

    return size;
}

// Writes a packet
bool etherPutPacket(uint8_t packet[], uint16_t size)
{
    uint16_t i;

    // clear out any tx errors
    if ((etherReadReg(EIR) & TXERIF) != 0)
    {
        etherClearReg(EIR, TXERIF);
        etherSetReg(ECON1, TXRTS);
        etherClearReg(ECON1, TXRTS);
    }

    // set DMA start address
    etherSetBank(EWRPTL);
    etherWriteReg(EWRPTL, LOBYTE(0x1A0A));
    etherWriteReg(EWRPTH, HIBYTE(0x1A0A));

    // start FIFO buffer write
    etherWriteMemStart();

    // write control byte
    etherWriteMem(0);

    // write data
    for (i = 0; i < size; i++)
        etherWriteMem(packet[i]);

    // stop write
    etherWriteMemStop();

    // request transmit
    etherWriteReg(ETXSTL, LOBYTE(0x1A0A));
    etherWriteReg(ETXSTH, HIBYTE(0x1A0A));
    etherWriteReg(ETXNDL, LOBYTE(0x1A0A+size));
    etherWriteReg(ETXNDH, HIBYTE(0x1A0A+size));
    etherClearReg(EIR, TXIF);
    etherSetReg(ECON1, TXRTS);

    // wait for completion
    while ((etherReadReg(ECON1) & TXRTS) != 0);

    // determine success
    return ((etherReadReg(ESTAT) & TXABORT) == 0);
}

// Calculate sum of words
// Must use getEtherChecksum to complete 1's compliment addition
void etherSumWords(void* data, uint16_t sizeInBytes)
{
    uint8_t* pData = (uint8_t*)data;
    uint16_t i;
    uint8_t phase = 0;
    uint16_t data_temp;
    for (i = 0; i < sizeInBytes; i++)
    {
        if (phase)
        {
            data_temp = *pData;
            sum += data_temp << 8;
        }
        else
            sum += *pData;
        phase = 1 - phase;
        pData++;
    }
}

void set_to_unicast()
{
    isUnicast = 1;
}

// Completes 1's compliment addition by folding carries back into field
uint16_t getEtherChecksum()
{
    uint16_t result;
    // this is based on rfc1071
    while ((sum >> 16) > 0)
        sum = (sum & 0xFFFF) + (sum >> 16);
    result = sum & 0xFFFF;
    return ~result;
}

uint16_t *get_g_ip_l_time()
{
    static uint16_t ip_l_time[4];
    int i=0;
    for (i = 0; i < 4; i++)
    {
       ip_l_time[i] = ack_ip_lease[i];
    }

    return ip_l_time;

}

void etherCalcIpChecksum(ipFrame* ip)
{
    // 32-bit sum over ip header
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
}

// Converts from host to network order and vice versa
uint16_t htons(uint16_t value)
{
    return ((value & 0xFF00) >> 8) + ((value & 0x00FF) << 8);
}
#define ntohs htons

// Determines whether packet is IP datagram
bool etherIsIp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    bool ok;
    ok = (ether->frameType == htons(0x0800));
    if (ok)
    {
        sum = 0;
        etherSumWords(&ip->revSize, (ip->revSize & 0xF) * 4);
        ok = (getEtherChecksum() == 0);
    }
    return ok;
}

// Determines whether packet is unicast to this ip
// Must be an IP packet
bool etherIsIpUnicast(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    uint8_t i = 0;
    bool ok = true;
    while (ok & (i < IP_ADD_LENGTH))
    {
        ok = (ip->destIp[i] == ipAddress[i]);
        i++;
    }
    return ok;
}

// Determines whether packet is broadcast to this ip
// Must be an IP packet
bool etherIsIpBroadcast(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    uint8_t i = 0;
    bool ok = true;
    while (ok & (i < IP_ADD_LENGTH))
    {
        ok = (ip->destIp[i] == 0xff);
        i++;
    }
    return ok;
}

// Determines whether packet is ping request
// Must be an IP packet
bool etherIsPingRequest(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    icmpFrame* icmp = (icmpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    return (ip->protocol == 0x01 & icmp->type == 8);
}


bool etherIsOffer(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*)&udp->data;


    discoveroptions* d_options = (void*)dhcp->options;

     if(d_options->msgtype[2] == 2 &&  dhcp->chaddr[0] == 0x02 && dhcp->chaddr[1] == 0x03 && dhcp->chaddr[2] ==0x04 && dhcp->chaddr[3]==0x05 && dhcp->chaddr[4]==0x06 && dhcp->chaddr[5] ==0x76)
     {
       return 1;
     }


     return 0;

}

void etherSetOffer(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*)&udp->data;

    uint8_t opt_size = ip->length - 8 - 20 -240;

    g_yiaddr[0] = dhcp->yiaddr[0];
    g_yiaddr[1] = dhcp->yiaddr[1];
    g_yiaddr[2] = dhcp->yiaddr[2];
    g_yiaddr[3] = dhcp->yiaddr[3];

    int i=0;
    for(i=0;i<opt_size;i++)
    {
        if(dhcp->options[i] == 54)
        {
            g_siaddr[0] = dhcp->options[i+2];
            g_siaddr[1] = dhcp->options[i+3];
            g_siaddr[2] = dhcp->options[i+4];
            g_siaddr[3] = dhcp->options[i+5];
            break;
        }

    }
}

// Sends a ping response given the request data
void etherSendPingResponse(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    icmpFrame* icmp = (icmpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    uint8_t i, tmp;
    uint16_t icmp_size;
    // swap source and destination fields
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        tmp = ether->destAddress[i];
        ether->destAddress[i] = ether->sourceAddress[i];
        ether->sourceAddress[i] = tmp;
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        tmp = ip->destIp[i];
        ip->destIp[i] = ip ->sourceIp[i];
        ip->sourceIp[i] = tmp;
    }
    // this is a response
    icmp->type = 0;
    // calc icmp checksum
    sum = 0;
    etherSumWords(&icmp->type, 2);
    icmp_size = ntohs(ip->length);
    icmp_size -= 24; // sub ip header and icmp code, type, and check
    etherSumWords(&icmp->id, icmp_size);
    icmp->check = getEtherChecksum();
    // send packet
    etherPutPacket(ether, 14 + ntohs(ip->length));
}

// Determines whether packet is ARP
bool etherIsArpRequest(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    arpFrame* arp = (arpFrame*)&ether->data;
    bool ok;
    uint8_t i = 0;
    ok = (ether->frameType == htons(0x0806));
    while (ok & (i < IP_ADD_LENGTH))
    {
        ok = (arp->destIp[i] == ipAddress[i]);
        i++;
    }
    if (ok)
        ok = (arp->op == htons(1));
    return ok;
}

// Sends an ARP response given the request data
void etherSendArpResponse(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    arpFrame* arp = (arpFrame*)&ether->data;
    uint8_t i, tmp;
    // set op to response
    arp->op = htons(2);
    // swap source and destination fields
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        arp->destAddress[i] = arp->sourceAddress[i];
        ether->destAddress[i] = ether->sourceAddress[i];
        ether->sourceAddress[i] = arp->sourceAddress[i] = macAddress[i];
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        tmp = arp->destIp[i];
        arp->destIp[i] = arp->sourceIp[i];
        arp->sourceIp[i] = tmp;
    }
    // send packet
    etherPutPacket(ether, 42);
}

// Sends an ARP request
void etherSendArpRequest(uint8_t packet[], uint8_t ip[])
{
    etherFrame* ether = (etherFrame*)packet;
    arpFrame* arp = (arpFrame*)&ether->data;
    uint8_t i;
    // fill ethernet frame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }
    ether->frameType = 0x0608;
    // fill arp frame
    arp->hardwareType = htons(1);
    arp->protocolType = htons(0x0800);
    arp->hardwareSize = HW_ADD_LENGTH;
    arp->protocolSize = IP_ADD_LENGTH;
    arp->op = htons(1);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        arp->sourceAddress[i] = macAddress[i];
        arp->destAddress[i] = 0xFF;
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        arp->sourceIp[i] = g_yiaddr[i];
        arp->destIp[i] = ip[i];
    }
    // send packet
    etherPutPacket(ether, 42);
}

void etherSendGratuitousArpRequest()
{
    uint8_t blah[1522];
    etherFrame* ether = (etherFrame*)blah;
    arpFrame* arp = (arpFrame*)&ether->data;
    uint8_t i;
    // fill ethernet frame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }
    ether->frameType = 0x0608;
    // fill arp frame
    arp->hardwareType = htons(1);
    arp->protocolType = htons(0x0800);
    arp->hardwareSize = HW_ADD_LENGTH;
    arp->protocolSize = IP_ADD_LENGTH;
    arp->op = htons(1);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        arp->sourceAddress[i] = macAddress[i];
        arp->destAddress[i] = 0xFF;
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        arp->sourceIp[i] = g_yiaddr[i];
        arp->destIp[i] = g_yiaddr[i];
    }
    // send packet
    etherPutPacket(ether, 42);
}

// Determines whether packet is UDP datagram
// Must be an IP packet
bool etherIsUdp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    bool ok;
    uint16_t tmp16;
    ok = (ip->protocol == 0x11);
    if (ok)
    {
        // 32-bit sum over pseudo-header
        sum = 0;
        etherSumWords(ip->sourceIp, 8);
        tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;
        etherSumWords(&udp->length, 2);
        // add udp header and data
        etherSumWords(udp, ntohs(udp->length));
        ok = (getEtherChecksum() == 0);
    }
    return ok;
}

bool etherIsAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*)&udp->data;
    bool found =0;
        uint8_t opt_size = ip->length - 8 - 20 -240;
        discoveroptions* d_options = (void*)dhcp->options;
        char str[100];

        if(d_options->msgtype[2] == 5  &&  dhcp->chaddr[0] == 0x02 && dhcp->chaddr[1] == 0x03 && dhcp->chaddr[2] ==0x04 && dhcp->chaddr[3]==0x05 && dhcp->chaddr[4]==0x06 && dhcp->chaddr[5] ==0x76)
            {   found =1;
            }

return found;
}



void etherSetAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;

     g_ether_server[0] = ether->sourceAddress[0];
     g_ether_server[1] = ether->sourceAddress[1];
     g_ether_server[2] = ether->sourceAddress[2];
     g_ether_server[3] = ether->sourceAddress[3];
     g_ether_server[4] = ether->sourceAddress[4];
     g_ether_server[5] = ether->sourceAddress[5];
//     g_ether_server[6] = ether->sourceAddress[6];
//     g_ether_server[7] = ether->sourceAddress[7];
       ipFrame* ip = (ipFrame*)&ether->data;
       udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
       dhcpFrame* dhcp = (dhcpFrame*)&udp->data;
       uint8_t f_subnet=0,f_server=0,f_lease_time=0,f_gateway=0,f_dns =0;

           uint8_t opt_size = ip->length - 8 - 20 -240;

           char str[100];


                   g_yiaddr[0] = dhcp->yiaddr[0];
                   g_yiaddr[1] = dhcp->yiaddr[1];
                   g_yiaddr[2] = dhcp->yiaddr[2];
                   g_yiaddr[3] = dhcp->yiaddr[3];

//                   sprintf(str, "%02x",g_yiaddr[0]);
//                   putsUart0("g_yiaddr[0]");
//                   putsUart0(str);
//                   putsUart0("\r\n");
//
//                   sprintf(str, "%02x",g_yiaddr[1]);
//                   putsUart0("g_yiaddr[1]");
//                   putsUart0(str);
//                   putsUart0("\r\n");
//
//                   sprintf(str, "%02x",g_yiaddr[2]);
//                   putsUart0("g_yiaddr[2]");
//                   putsUart0(str);
//                   putsUart0("\r\n");
//
//                   sprintf(str, "%02x",g_yiaddr[3]);
//                   putsUart0("g_yiaddr[3]");
//                   putsUart0(str);
//                   putsUart0("\r\n");

                   int i=0;
                   for(i=0;i<opt_size;i++)
                   {
                       if(dhcp->options[i]==1 && f_subnet ==0 )
                       {
                           g_subnet[0] = dhcp->options[i+4];
                           g_subnet[1] = dhcp->options[i+5];
                           g_subnet[2] = dhcp->options[i+6];
                           g_subnet[3] = dhcp->options[i+7];

//                           sprintf(str, "%02x",dhcp->options[i+4]);
//                           putsUart0("dhcp->options[i+4]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",dhcp->options[i+5]);
//                           putsUart0("dhcp->options[i+5]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",dhcp->options[i+6]);
//                           putsUart0("dhcp->options[i+6]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",dhcp->options[i+7]);
//                           putsUart0("dhcp->options[i+7]");
//                           putsUart0(str);
//                           putsUart0("\r\n");


//                           sprintf(str, "%02x",g_subnet[0]);
//                           putsUart0("g_subnet[0]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",g_subnet[1]);
//                           putsUart0("g_subnet[1]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",g_subnet[2]);
//                           putsUart0("g_subnet[2]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",g_subnet[3]);
//                           putsUart0("g_subnet[3]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//                           f_subnet =1;

                       }

                       if(dhcp->options[i] == 54 && f_server==0)
                       {
                       g_siaddr[0] = dhcp->options[i+2];
                       g_siaddr[1] = dhcp->options[i+3];
                       g_siaddr[2] = dhcp->options[i+4];
                       g_siaddr[3] = dhcp->options[i+5];

//                       sprintf(str, "%02x", g_siaddr[0]);
//                       putsUart0("g_siaddr[0]");
//                       putsUart0(str);
//                       putsUart0("\r\n");
//
//                       sprintf(str, "%02x", g_siaddr[1]);
//                       putsUart0("g_siaddr[1]");
//                       putsUart0(str);
//                       putsUart0("\r\n");
//
//                       sprintf(str, "%02x", g_siaddr[2]);
//                       putsUart0("g_siaddr[2]");
//                       putsUart0(str);
//                       putsUart0("\r\n");
//
//                       sprintf(str, "%02x", g_siaddr[3]);
//                       putsUart0("g_siaddr[3]");
//                       putsUart0(str);
//                       putsUart0("\r\n");




                        f_server =1;

                       }

                       if(dhcp->options[i]==51 && f_lease_time ==0)
                       {
                           ack_ip_lease[0] = dhcp->options[i+2];
                           ack_ip_lease[1] = dhcp->options[i+3];
                           ack_ip_lease[2] = dhcp->options[i+4];
                           ack_ip_lease[3] = dhcp->options[i+5];



//                           sprintf(str, "%02x",ack_ip_lease[0]);
//                           putsUart0("ack_ip_lease[0]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",ack_ip_lease[1]);
//                           putsUart0("ack_ip_lease[1]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",ack_ip_lease[2]);
//                           putsUart0("ack_ip_lease[2]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",ack_ip_lease[3]);
//                           putsUart0("ack_ip_lease[3]");
//                           putsUart0(str);
//                           putsUart0("\r\n");

                           f_lease_time =1;

                       }

                       if(dhcp->options[i]==3 && f_gateway ==0)
                       {
                           g_gateway[0] = dhcp->options[i+2];
                           g_gateway[1] = dhcp->options[i+3];
                           g_gateway[2] = dhcp->options[i+4];
                           g_gateway[3] = dhcp->options[i+5];

//                           sprintf(str, "%02x",dhcp->options[i+2]);
//                           putsUart0("dhcp->options[i+2]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",dhcp->options[i+3]);
//                           putsUart0("dhcp->options[i+3]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",dhcp->options[i+4]);
//                           putsUart0("dhcp->options[i+4]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",dhcp->options[i+5]);
//                           putsUart0("dhcp->options[i+5]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",g_gateway[0]);
//                           putsUart0("g_gateway[0]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",g_gateway[1]);
//                           putsUart0("g_gateway[1]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",g_gateway[2]);
//                           putsUart0("g_gateway[2]");
//                           putsUart0(str);
//                           putsUart0("\r\n");
//
//                           sprintf(str, "%02x",g_gateway[3]);
//                           putsUart0("g_gateway[3]");
//                           putsUart0(str);
//                           putsUart0("\r\n");

                           f_gateway =1;

                       }

                       if(dhcp->options[i] == 6 && f_dns==0)
                       {
                           g_dns[0] = dhcp->options[i+2];
                           g_dns[1] = dhcp->options[i+3];
                           g_dns[2] = dhcp->options[i+4];
                           g_dns[3] = dhcp->options[i+5];

                           f_dns=1;

                       }

                   }

}

bool etherIsTcp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    tcpFrame* tcp = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    //tcpFrame*
    bool ok;
    uint16_t tmp16;
    uint8_t tcp_length = 0;
    tcp_length = htons(ip->length) - ((ip->revSize & 0xF) * 4);
    ok = (ip->protocol == 0x06);
    if (ok)
    {
        // 32-bit sum over pseudo-header
        sum = 0;
        etherSumWords(ip->sourceIp, 8);
        tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;
        sum += htons(tcp_length);
        //etherSumWords(tcp_length, 2);
        // add udp header and data
        etherSumWords(tcp, (tcp_length));
        ok = (getEtherChecksum() == 0);
    }
    return ok;
}

uint16_t get_tcp_flag(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    tcpFrame* tcp = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    uint16_t tcp_state = tcp->data_offset;

    return tcp_state;

}

uint32_t get_ip_lease_time()
{
    uint32_t i_ip_lease_time=0;
   // for(int i=0;i<4;i++)
    //{
    i_ip_lease_time = ack_ip_lease[0]<< 24 | ack_ip_lease[1]<<16| ack_ip_lease[2]<<8| ack_ip_lease[3];

    //}

    return i_ip_lease_time;
}
// Gets pointer to UDP payload of frame
uint8_t* etherGetUdpData(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    return &udp->data;
}

// Send responses to a udp datagram
// destination port, ip, and hardware address are extracted from provided data
// uses destination port of received packet as destination of this packet

void etherSendUdpResponse(uint8_t packet[], uint8_t* udpData, uint8_t udpSize)
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    uint8_t *copyData;
    uint8_t i, tmp8;
    uint16_t tmp16;
    // swap source and destination fields
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        tmp8 = ether->destAddress[i];
        ether->destAddress[i] = ether->sourceAddress[i];
        ether->sourceAddress[i] = tmp8;
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        tmp8 = ip->destIp[i];
        ip->destIp[i] = ip->sourceIp[i];
        ip->sourceIp[i] = tmp8;
    }
    // set source port of resp will be dest port of req
    // dest port of resp will be left at source port of req
    // unusual nomenclature, but this allows a different tx
    // and rx port on other machine
    udp->sourcePort = udp->destPort;
    // adjust lengths
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + udpSize);
    // 32-bit sum over ip header
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    udp->length = htons(8 + udpSize);
    // copy data
    copyData = &udp->data;
    for (i = 0; i < udpSize; i++)
        copyData[i] = udpData[i];
    // 32-bit sum over pseudo-header
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    etherSumWords(&udp->length, 2);
    // add udp header except crc
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, udpSize);
    udp->check = getEtherChecksum();

    // send packet with size = ether + udp hdr + ip header + udp_size
    etherPutPacket(ether, 22 + ((ip->revSize & 0xF) * 4) + udpSize);
}



void etherSet_g_IP()
{
    // uint8_t g_yiaddr[4];
    // uint8_t g_siaddr[4];
//    uint8_t g_subnet[4];
//    uint8_t g_gateway[4];
//    uint8_t g_dns[4];
    etherSetIpAddress(g_yiaddr[0], g_yiaddr[1], g_yiaddr[2], g_yiaddr[3]);
    etherSetIpSubnetMask(g_subnet[0],g_subnet[1], g_subnet[2], g_subnet[3]);
    etherSetIpGatewayAddress(g_gateway[0], g_gateway[1], g_gateway[2], g_gateway[3]);
    etherSet_g_DNS(g_dns[0],g_dns[1],g_dns[2],g_dns[3]);

}



void etherSendDiscoverMessage()
{
    uint8_t blah[1522];
    etherFrame* ether = (etherFrame*)blah;
    ipFrame* ip = (ipFrame*)&ether->data;
    ip->revSize = 0x45;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    uint8_t i, tmp8;
    uint16_t tmp16;

    ether->sourceAddress[0]=0x02;
    ether->sourceAddress[1]=0x03;
    ether->sourceAddress[2]=0x04;
    ether->sourceAddress[3]=0x05;
    ether->sourceAddress[4]=0x06;
    ether->sourceAddress[5]=0x76;

    ether->destAddress[0]=0xff;
    ether->destAddress[1]=0xff;
    ether->destAddress[2]=0xff;
    ether->destAddress[3]=0xff;
    ether->destAddress[4]=0xff;
    ether->destAddress[5]=0xff;

    ether->frameType =  htons(0x0800);

    ip->sourceIp[0]=0;
    ip->sourceIp[1]=0;
    ip->sourceIp[2]=0;
    ip->sourceIp[3]=0;

    ip->destIp[0]=0xff;
    ip->destIp[1]=0xff;
    ip->destIp[2]=0xff;
    ip->destIp[3]=0xff;


    ip->typeOfService = 0x00;
    ip->id = htons(0x0000);
    ip->flagsAndOffset =htons(0x0000);
    ip->ttl=0xff;
    ip->protocol= 17;
    ip->headerChecksum =0x00;

    // set source port of resp will be dest port of req
    // dest port of resp will be left at source port of req
    // unusual nomenclature, but this allows a different tx
    // and rx port on other machine
    udp->sourcePort = htons(68);
    udp->destPort = htons(67);
    udp->check =0;

    dhcpFrame* dhcp = (dhcpFrame*)&udp->data;

    memset(dhcp, 0, 400);


    dhcp->op = 1;
    dhcp->htype = 1;
    dhcp->hlen = 6;
    dhcp->hops = 0;
    dhcp->xid = 0x12345678;
    dhcp->secs = 0;
    dhcp->flags = htons(0x8000);

    dhcp->ciaddr[0]= 0;
    dhcp->ciaddr[1]= 0;
    dhcp->ciaddr[2]= 0;
    dhcp->ciaddr[3]= 0;

    dhcp->yiaddr[0]= 0;
    dhcp->yiaddr[1]= 0;
    dhcp->yiaddr[2]= 0;
    dhcp->yiaddr[3]= 0;

    dhcp->siaddr[0]= 0;
    dhcp->siaddr[1]= 0;
    dhcp->siaddr[2]= 0;
    dhcp->siaddr[3]= 0;

    dhcp->giaddr[0] =0;
    dhcp->giaddr[1] =0;
    dhcp->giaddr[2]=0;
    dhcp->giaddr[3] =0;

    dhcp->chaddr[0] = 0x02;
    dhcp->chaddr[1] = 0x03;
    dhcp->chaddr[2] =0x04;
    dhcp->chaddr[3]=0x05;
    dhcp->chaddr[4]=0x06;
    dhcp->chaddr[5] =0x76;

  // int i=0;

   // int i=0;


    for(i=6;i<16;i++)
           {
               dhcp->chaddr[i] =0;

           }

           for(i=0;i<192;i++)
           {
               dhcp->data[i]=0x00;
           }

    dhcp->magicCookie = 0x63538263;

    discoveroptions* d_options = (void*)dhcp->options;

    d_options->msgtype[0]= 53;
    d_options->msgtype[1]= 1;
    d_options->msgtype[2]= 1;

    d_options->clientid[0]= 61;
    d_options->clientid[1]= 7;
    d_options->clientid[2]= 0x01;
    d_options->clientid[3]= 0xd0;
    d_options->clientid[4]= 0x37;
    d_options->clientid[5]= 0x45;
    d_options->clientid[6]= 0x6f;
    d_options->clientid[7]= 0x28;
    d_options->clientid[8]= 0xba;

    d_options->parareqlist[0]= 55;
    d_options->parareqlist[1]= 4;
    d_options->parareqlist[2]= 1;
    d_options->parareqlist[3]= 3;
    d_options->parareqlist[4]= 51;
    d_options->parareqlist[5]= 6;

    d_options->endoption = 255;


    udp->length=htons(8+240+19);
    //adjust lengths
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + 240+19);

    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();

    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    etherSumWords(&udp->length, 2);
    // add udp header except crc
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, 259);
    udp->check = getEtherChecksum();
    // send packet with size = ether + udp hdr + ip header + udp_size + dhcp size + options
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + 240+19);
}

void etherSendDeclineMessage()
{
    uint8_t blah[500];
    etherFrame* ether = (etherFrame*)blah;
    ipFrame* ip = (ipFrame*)&ether->data;
    ip->revSize = 0x45;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    uint8_t i, tmp8;
    uint16_t tmp16;

    ether->sourceAddress[0]=0x02;
    ether->sourceAddress[1]=0x03;
    ether->sourceAddress[2]=0x04;
    ether->sourceAddress[3]=0x05;
    ether->sourceAddress[4]=0x06;
    ether->sourceAddress[5]=0x76;

    ether->destAddress[0]=0xff;
    ether->destAddress[1]=0xff;
    ether->destAddress[2]=0xff;
    ether->destAddress[3]=0xff;
    ether->destAddress[4]=0xff;
    ether->destAddress[5]=0xff;

    ether->frameType =  htons(0x0800);

    ip->sourceIp[0]=0;
    ip->sourceIp[1]=0;
    ip->sourceIp[2]=0;
    ip->sourceIp[3]=0;

    ip->destIp[0]=0xff;
    ip->destIp[1]=0xff;
    ip->destIp[2]=0xff;
    ip->destIp[3]=0xff;


    ip->typeOfService = 0x00;
    ip->id = htons(0x0000);
    ip->flagsAndOffset =htons(0x0000);
    ip->ttl=0xff;
    ip->protocol= 17;
    ip->headerChecksum =0x00;

    // set source port of resp will be dest port of req
    // dest port of resp will be left at source port of req
    // unusual nomenclature, but this allows a different tx
    // and rx port on other machine
    udp->sourcePort = htons(68);
    udp->destPort = htons(67);
    udp->check =0;

    dhcpFrame* dhcp = (dhcpFrame*)&udp->data;

    memset(dhcp, 0, 400);


    dhcp->op = 1;
    dhcp->htype = 1;
    dhcp->hlen = 6;
    dhcp->hops = 0;
    dhcp->xid = 0x12345678;
    dhcp->secs = 0;
    dhcp->flags = htons(0x8000);

    dhcp->ciaddr[0]= 0;
    dhcp->ciaddr[1]= 0;
    dhcp->ciaddr[2]= 0;
    dhcp->ciaddr[3]= 0;

    dhcp->yiaddr[0]= 0;
    dhcp->yiaddr[1]= 0;
    dhcp->yiaddr[2]= 0;
    dhcp->yiaddr[3]= 0;

    dhcp->siaddr[0]= 0;
    dhcp->siaddr[1]= 0;
    dhcp->siaddr[2]= 0;
    dhcp->siaddr[3]= 0;

    dhcp->giaddr[0] =0;
    dhcp->giaddr[1] =0;
    dhcp->giaddr[2]=0;
    dhcp->giaddr[3] =0;

    dhcp->chaddr[0] = 0x02;
    dhcp->chaddr[1] = 0x03;
    dhcp->chaddr[2] =0x04;
    dhcp->chaddr[3]=0x05;
    dhcp->chaddr[4]=0x06;
    dhcp->chaddr[5] =0x76;

  // int i=0;

   // int i=0;


    for(i=6;i<16;i++)
           {
               dhcp->chaddr[i] =0;

           }

           for(i=0;i<192;i++)
           {
               dhcp->data[i]=0x00;
           }

    dhcp->magicCookie = 0x63538263;

    discoveroptions* d_options = (void*)dhcp->options;

    d_options->msgtype[0]= 53;
    d_options->msgtype[1]= 1;
    d_options->msgtype[2]= 4;

    d_options->clientid[0]= 61;
    d_options->clientid[1]= 7;
    d_options->clientid[2]= 0x01;
    d_options->clientid[3]= 0xd0;
    d_options->clientid[4]= 0x37;
    d_options->clientid[5]= 0x45;
    d_options->clientid[6]= 0x6f;
    d_options->clientid[7]= 0x28;
    d_options->clientid[8]= 0xba;

    d_options->parareqlist[0]= 55;
    d_options->parareqlist[1]= 4;
    d_options->parareqlist[2]= 1;
    d_options->parareqlist[3]= 3;
    d_options->parareqlist[4]= 51;
    d_options->parareqlist[5]= 6;

    d_options->endoption = 255;


    udp->length=htons(8+240+19);
    //adjust lengths
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + 240+19);

    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();

    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    etherSumWords(&udp->length, 2);
    // add udp header except crc
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, 259);
    udp->check = getEtherChecksum();
    // send packet with size = ether + udp hdr + ip header + udp_size + dhcp size + options
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + 240+19);
}


void etherSendDHCPRelease()
{
    uint8_t blah[500];
    etherFrame* ether = (etherFrame*)blah;
    ipFrame* ip = (ipFrame*)&ether->data;
    ip->revSize = 0x45;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    uint8_t i, tmp8;
    uint16_t tmp16;

    ether->sourceAddress[0]=0x02;
    ether->sourceAddress[1]=0x03;
    ether->sourceAddress[2]=0x04;
    ether->sourceAddress[3]=0x05;
    ether->sourceAddress[4]=0x06;
    ether->sourceAddress[5]=0x76;

    ether->destAddress[0]=0xff;
    ether->destAddress[1]=0xff;
    ether->destAddress[2]=0xff;
    ether->destAddress[3]=0xff;
    ether->destAddress[4]=0xff;
    ether->destAddress[5]=0xff;

    ether->frameType =  htons(0x0800);

    ip->sourceIp[0]=0;
    ip->sourceIp[1]=0;
    ip->sourceIp[2]=0;
    ip->sourceIp[3]=0;

    ip->destIp[0]=0xff;
    ip->destIp[1]=0xff;
    ip->destIp[2]=0xff;
    ip->destIp[3]=0xff;


    ip->typeOfService = 0x00;
    ip->id = htons(0x0000);
    ip->flagsAndOffset =htons(0x0000);
    ip->ttl=0xff;
    ip->protocol= 17;
    ip->headerChecksum =0x00;

    // set source port of resp will be dest port of req
    // dest port of resp will be left at source port of req
    // unusual nomenclature, but this allows a different tx
    // and rx port on other machine
    udp->sourcePort = htons(68);
    udp->destPort = htons(67);
    udp->check =0;

    dhcpFrame* dhcp = (dhcpFrame*)&udp->data;

    memset(dhcp, 0, 400);


    dhcp->op = 1;
    dhcp->htype = 1;
    dhcp->hlen = 6;
    dhcp->hops = 0;
    dhcp->xid = 0x12345678;
    dhcp->secs = 0;
    dhcp->flags = htons(0x0000);

    //??
    dhcp->ciaddr[0]= 0;
    dhcp->ciaddr[1]= 0;
    dhcp->ciaddr[2]= 0;
    dhcp->ciaddr[3]= 0;

    dhcp->yiaddr[0]= 0;
    dhcp->yiaddr[1]= 0;
    dhcp->yiaddr[2]= 0;
    dhcp->yiaddr[3]= 0;

    dhcp->siaddr[0]= 0;
    dhcp->siaddr[1]= 0;
    dhcp->siaddr[2]= 0;
    dhcp->siaddr[3]= 0;

    dhcp->giaddr[0] =0;
    dhcp->giaddr[1] =0;
    dhcp->giaddr[2]=0;
    dhcp->giaddr[3] =0;

    dhcp->chaddr[0] = 0x02;
    dhcp->chaddr[1] = 0x03;
    dhcp->chaddr[2] =0x04;
    dhcp->chaddr[3]=0x05;
    dhcp->chaddr[4]=0x06;
    dhcp->chaddr[5] =0x76;

  // int i=0;

   // int i=0;


    for(i=6;i<16;i++)
           {
               dhcp->chaddr[i] =0;

           }

           for(i=0;i<192;i++)
           {
               dhcp->data[i]=0x00;
           }

    dhcp->magicCookie = 0x63538263;

    discoveroptions* d_options = (void*)dhcp->options;

    d_options->msgtype[0]= 53;
    d_options->msgtype[1]= 1;
    d_options->msgtype[2]= 7;

    d_options->clientid[0]= 61;
    d_options->clientid[1]= 7;
    d_options->clientid[2]= 0x01;
    d_options->clientid[3]= 0xd0;
    d_options->clientid[4]= 0x37;
    d_options->clientid[5]= 0x45;
    d_options->clientid[6]= 0x6f;
    d_options->clientid[7]= 0x28;
    d_options->clientid[8]= 0xba;

    d_options->parareqlist[0]= 55;
    d_options->parareqlist[1]= 4;
    d_options->parareqlist[2]= 1;
    d_options->parareqlist[3]= 3;
    d_options->parareqlist[4]= 51;
    d_options->parareqlist[5]= 6;


    d_options->endoption = 255;


    udp->length=htons(8+240+19);
    //adjust lengths
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + 240+19);

    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();

    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    etherSumWords(&udp->length, 2);
    // add udp header except crc
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, 259);
    udp->check = getEtherChecksum();
    // send packet with size = ether + udp hdr + ip header + udp_size + dhcp size + options
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + 240+19);
}

void etherSendDHCPRebind()
{
    uint8_t blah[500];
    etherFrame* ether = (etherFrame*)blah;
    ipFrame* ip = (ipFrame*)&ether->data;
    ip->revSize = 0x45;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    uint8_t i, tmp8;
    uint16_t tmp16;
                     ether->sourceAddress[0]=0x02;
                     ether->sourceAddress[1]=0x03;
                     ether->sourceAddress[2]=0x04;
                     ether->sourceAddress[3]=0x05;
                     ether->sourceAddress[4]=0x06;
                     ether->sourceAddress[5]=0x76;

                     ether->destAddress[0]=0xff;
                     ether->destAddress[1]=0xff;
                     ether->destAddress[2]=0xff;
                     ether->destAddress[3]=0xff;
                     ether->destAddress[4]=0xff;
                     ether->destAddress[5]=0xff;

                     ether->frameType =  htons(0x0800);

                     ip->sourceIp[0]=g_yiaddr[0];
                     ip->sourceIp[1]=g_yiaddr[1];
                     ip->sourceIp[2]=g_yiaddr[2];
                     ip->sourceIp[3]=g_yiaddr[3];

                     ip->destIp[0]=0xff;
                     ip->destIp[1]=0xff;
                     ip->destIp[2]=0xff;
                     ip->destIp[3]=0xff;


                     ip->revSize = 0x45;

                     ip->typeOfService = 0x00;
                     ip->id = htons(0x0000);
                     ip->flagsAndOffset =htons(0x0000);
                     ip->ttl=0xff;
                     ip->protocol= 17;
                     ip->headerChecksum =0x0000;

                     // set source port of resp will be dest port of req
                     // dest port of resp will be left at source port of req
                     // unusual nomenclature, but this allows a different tx
                     // and rx port on other machine
                     udp->sourcePort = htons(68);
                     udp->destPort = htons(67);
                   //  udp->check =0x0000;

                     dhcpFrame* dhcp = (dhcpFrame*)&udp->data;

                     memset(dhcp, 0, 400);


                     dhcp->op = 1;
                     dhcp->htype = 0x01;
                     dhcp->hlen =6;
                     dhcp->hops =0;
                     dhcp->xid =0x12345678;
                     dhcp->secs =0;
                     dhcp->flags = htons(0x0000);
                     dhcp->ciaddr[0]=g_yiaddr[0];
                     dhcp->ciaddr[1]=g_yiaddr[1];
                     dhcp->ciaddr[2]=g_yiaddr[2];
                     dhcp->ciaddr[3]=g_yiaddr[3];

                        dhcp->yiaddr[0]=0;
                     dhcp->yiaddr[1]=0;
                     dhcp->yiaddr[2]=0;
                     dhcp->yiaddr[3]=0;

                     dhcp->siaddr[0]= 0;
                     dhcp->siaddr[1]=0;
                     dhcp->siaddr[2]=0;
                     dhcp->siaddr[3]=0;

                     dhcp->giaddr[0]=0;
                     dhcp->giaddr[1]=0;
                     dhcp->giaddr[2]=0;
                     dhcp->giaddr[3]=0;

                     dhcp->chaddr[0] = 0x02;
                     dhcp->chaddr[1] = 0x03;
                     dhcp->chaddr[2] =0x04;
                     dhcp->chaddr[3]=0x05;
                     dhcp->chaddr[4]=0x06;
                     dhcp->chaddr[5] =0x76;

                     for(i=6;i<16;i++)
                     {
                         dhcp->chaddr[i] =0;

                     }

                     for(i=0;i<193;i++)
                     {
                         dhcp->data[i]=0x00;
                     }


                     dhcp->magicCookie = 0x63538263;



                     dhcp_ack_requestoptions* r_options = (void*)dhcp->options;

                     r_options->msgtype[0]= 53;
                     r_options->msgtype[1]=1;
                     r_options->msgtype[2]=3;

                     r_options->clientid[0]=61;
                     r_options->clientid[1]=7;
                     r_options->clientid[2]=0x01;
                     r_options->clientid[3]=0x02;
                     r_options->clientid[4]=0x03;
                     r_options->clientid[5]=0x04;
                     r_options->clientid[6]=0x05;
                     r_options->clientid[7]=0x06;
                     r_options->clientid[8]=0x76;

                     r_options->parareqlist[0]=55;
                     r_options->parareqlist[1]=4;
                     r_options->parareqlist[2]=1;
                     r_options->parareqlist[3]=3;
                     r_options->parareqlist[4]=51;
                     r_options->parareqlist[5]=6;


                     r_options->endoption = 255;

//                     ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + 240+18);
//                    // sum = 0;
//                     //
//                     //adjust lengths
//
//
//                     sum = 0;
//                     etherSumWords(&ip->revSize, 10);
//                     etherSumWords(ip->sourceIp,((ip->revSize & 0xF)*4)-12);
//                     ip->headerChecksum = getEtherChecksum();
//                     udp->length=htons(8+240+18);
//                     sum = 0;
//                     //udp->check =0;
//                     etherSumWords(ip->sourceIp, 8);
//                     tmp16 = ip->protocol;
//
//                     sum += (tmp16 & 0xff) << 8;
//                     etherSumWords(&udp->length, 2);
//                     // add udp header except crc
//                     etherSumWords(udp, 6);
//                     etherSumWords(&udp->data, 240+18);
//                     udp->check = getEtherChecksum();
//                     // send packet with size = ether + udp hdr + ip header + udp_size + dhcp size + options
//                     etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + 240+18);

                     udp->length=htons(8+240+19);
                        //adjust lengths
                        ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + 240+19);

                        sum = 0;
                        etherSumWords(&ip->revSize, 20);
                        ip->headerChecksum = getEtherChecksum();

                        sum = 0;
                        etherSumWords(ip->sourceIp, 8);
                        tmp16 = ip->protocol;

                        sum += (tmp16 & 0xff) << 8;
                        etherSumWords(&udp->length, 2);
                        // add udp header except crc
                        etherSumWords(udp, 6);
                        etherSumWords(&udp->data, 240+19);
                        udp->check = getEtherChecksum();
                        // send packet with size = ether + udp hdr + ip header + udp_size + dhcp size + options
                        etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + 240+19);

              }


void etherSendDHCPRequest()
{
       uint8_t blah[500];
       etherFrame* ether = (etherFrame*)blah;
       ipFrame* ip = (ipFrame*)&ether->data;
       ip->revSize = 0x45;
       udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

       uint16_t tmp16;
if(isUnicast == 0)
       {
       ether->sourceAddress[0]=0x02;
       ether->sourceAddress[1]=0x03;
       ether->sourceAddress[2]=0x04;
       ether->sourceAddress[3]=0x05;
       ether->sourceAddress[4]=0x06;
       ether->sourceAddress[5]=0x76;

       ether->destAddress[0]=0xff;
       ether->destAddress[1]=0xff;
       ether->destAddress[2]=0xff;
       ether->destAddress[3]=0xff;
       ether->destAddress[4]=0xff;
       ether->destAddress[5]=0xff;

       ether->frameType =  htons(0x0800);

       ip->sourceIp[0]=0;
       ip->sourceIp[1]=0;
       ip->sourceIp[2]=0;
       ip->sourceIp[3]=0;

       ip->destIp[0]=0xff;
       ip->destIp[1]=0xff;
       ip->destIp[2]=0xff;
       ip->destIp[3]=0xff;


       ip->typeOfService = 0x00;
       ip->id = htons(0x0000);
       ip->flagsAndOffset =htons(0x0000);
       ip->ttl=0xff;
       ip->protocol= 17;
       ip->headerChecksum =0x0000;

       // set source port of resp will be dest port of req
       // dest port of resp will be left at source port of req
       // unusual nomenclature, but this allows a different tx
       // and rx port on other machine
       udp->sourcePort = htons(68);
       udp->destPort = htons(67);
       udp->check =0;

       dhcpFrame* dhcp = (dhcpFrame*)&udp->data;

       memset(dhcp, 0, 400);


       dhcp->op = 1;
       dhcp->htype = 1;
       dhcp->hlen =6;
       dhcp->hops =0;
       dhcp->xid =0x12345678;
       dhcp->secs =0;
       dhcp->flags = htons(0x8000);
       dhcp->ciaddr[0]=0;
       dhcp->ciaddr[1]=0;
       dhcp->ciaddr[2]=0;
       dhcp->ciaddr[3]=0;

       dhcp->yiaddr[0]=0;
       dhcp->yiaddr[1]=0;
       dhcp->yiaddr[2]=0;
       dhcp->yiaddr[3]=0;

       dhcp->siaddr[0]=g_siaddr[0];
       dhcp->siaddr[1]=g_siaddr[1];
       dhcp->siaddr[2]=g_siaddr[2];
       dhcp->siaddr[3]=g_siaddr[3];

       dhcp->giaddr[0]=0;
       dhcp->giaddr[1]=0;
       dhcp->giaddr[2]=0;
       dhcp->giaddr[3]=0;

       dhcp->chaddr[0] = 0x02;
       dhcp->chaddr[1] = 0x03;
       dhcp->chaddr[2] =0x04;
       dhcp->chaddr[3]=0x05;
       dhcp->chaddr[4]=0x06;
       dhcp->chaddr[5] =0x76;
       int i=0;
//       for(i=6;i<16;i++)
//       {
//           dhcp->chaddr[i] =0;
//
//       }

       for(i=6;i<16;i++)
       {
           dhcp->chaddr[i] =0;

       }

       for(i=0;i<192;i++)
       {
           dhcp->data[i]=0x00;
       }

       dhcp->magicCookie = 0x63538263;

//       for(i=0;i<193;i++)
//       {
//           dhcp->data[i]=0x00;
//       }



       dhcprequestoptions* r_options = (void*)dhcp->options;

       r_options->msgtype[0]= 53;
       r_options->msgtype[1]=1;
       r_options->msgtype[2]=3;

       r_options->clientid[0]=61;
       r_options->clientid[1]=7;
       r_options->clientid[2]=0x01;
       r_options->clientid[3]=0x02;
       r_options->clientid[4]=0x03;
       r_options->clientid[5]=0x04;
       r_options->clientid[6]=0x05;
       r_options->clientid[7]=0x05;
       r_options->clientid[8]=0x76;

       r_options->parareqlist[0]=55;
       r_options->parareqlist[1]=4;
       r_options->parareqlist[2]=1;
       r_options->parareqlist[3]=3;
       r_options->parareqlist[4]=51;
       r_options->parareqlist[5]=6;


       r_options->req_ip[0] = 50;
       r_options->req_ip[1] = 4;
       r_options->req_ip[2] = g_yiaddr[0];
       r_options->req_ip[3] = g_yiaddr[1];
       r_options->req_ip[4] = g_yiaddr[2];
       r_options->req_ip[5] = g_yiaddr[3];

       r_options->ser_ip[0] = 54;
       r_options->ser_ip[1] = 4;
       r_options->ser_ip[2] = g_siaddr[0];
       r_options->ser_ip[3] = g_siaddr[1];
       r_options->ser_ip[4] = g_siaddr[2];
       r_options->ser_ip[5] = g_siaddr[3];


       r_options->endoption = 255;


       udp->length=htons(8+240+31);
       //adjust lengths
       ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + 240+31);

       sum = 0;
       etherSumWords(&ip->revSize, 20);
       ip->headerChecksum = getEtherChecksum();

       sum = 0;
       etherSumWords(ip->sourceIp, 8);
       tmp16 = ip->protocol;

       sum += (tmp16 & 0xff) << 8;
       etherSumWords(&udp->length, 2);
       // add udp header except crc
       etherSumWords(udp, 6);
       etherSumWords(&udp->data, 240+31);
       udp->check = getEtherChecksum();
       // send packet with size = ether + udp hdr + ip header + udp_size + dhcp size + options
       etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + 240+31);

}

else if(isUnicast == 1)
              {
                     ether->sourceAddress[0]=0x02;
                     ether->sourceAddress[1]=0x03;
                     ether->sourceAddress[2]=0x04;
                     ether->sourceAddress[3]=0x05;
                     ether->sourceAddress[4]=0x06;
                     ether->sourceAddress[5]=0x76;

                     ether->destAddress[0]=g_ether_server[0];
                     ether->destAddress[1]=g_ether_server[1];
                     ether->destAddress[2]=g_ether_server[2];
                     ether->destAddress[3]=g_ether_server[3];
                     ether->destAddress[4]=g_ether_server[4];
                     ether->destAddress[5]=g_ether_server[5];

                     ether->frameType =  htons(0x0800);

                     ip->sourceIp[0]=g_yiaddr[0];
                     ip->sourceIp[1]=g_yiaddr[1];
                     ip->sourceIp[2]=g_yiaddr[2];
                     ip->sourceIp[3]=g_yiaddr[3];

                     ip->destIp[0]=g_siaddr[0];
                     ip->destIp[1]=g_siaddr[1];
                     ip->destIp[2]=g_siaddr[2];
                     ip->destIp[3]=g_siaddr[3];


                     ip->revSize = 0x45;

                     ip->typeOfService = 0x00;
                     ip->id = htons(0x0000);
                     ip->flagsAndOffset =htons(0x0000);
                     ip->ttl=0xff;
                     ip->protocol= 17;
                     ip->headerChecksum =0x0000;

                     // set source port of resp will be dest port of req
                     // dest port of resp will be left at source port of req
                     // unusual nomenclature, but this allows a different tx
                     // and rx port on other machine
                     udp->sourcePort = htons(68);
                     udp->destPort = htons(67);
                   //  udp->check =0x0000;

                     dhcpFrame* dhcp = (dhcpFrame*)&udp->data;

                     memset(dhcp, 0, 400);


                     dhcp->op = 1;
                     dhcp->htype = 0x01;
                     dhcp->hlen =6;
                     dhcp->hops =0;
                     dhcp->xid =0x12345678;
                     dhcp->secs =0;
                     dhcp->flags = htons(0x0000);
                     dhcp->ciaddr[0]=g_yiaddr[0];
                     dhcp->ciaddr[1]=g_yiaddr[1];
                     dhcp->ciaddr[2]=g_yiaddr[2];
                     dhcp->ciaddr[3]=g_yiaddr[3];

                        dhcp->yiaddr[0]=0;
                     dhcp->yiaddr[1]=0;
                     dhcp->yiaddr[2]=0;
                     dhcp->yiaddr[3]=0;

                     dhcp->siaddr[0]= 0;
                     dhcp->siaddr[1]=0;
                     dhcp->siaddr[2]=0;
                     dhcp->siaddr[3]=0;

                     dhcp->giaddr[0]=0;
                     dhcp->giaddr[1]=0;
                     dhcp->giaddr[2]=0;
                     dhcp->giaddr[3]=0;

                     dhcp->chaddr[0] = 0x02;
                     dhcp->chaddr[1] = 0x03;
                     dhcp->chaddr[2] =0x04;
                     dhcp->chaddr[3]=0x05;
                     dhcp->chaddr[4]=0x06;
                     dhcp->chaddr[5] =0x76;
                     int i;
                     for(i=6;i<16;i++)
                     {
                         dhcp->chaddr[i] =0;

                     }

                     for(i=0;i<193;i++)
                     {
                         dhcp->data[i]=0x00;
                     }


                     dhcp->magicCookie = 0x63538263;



                     dhcp_ack_requestoptions* r_options = (void*)dhcp->options;

                     r_options->msgtype[0]= 53;
                     r_options->msgtype[1]=1;
                     r_options->msgtype[2]=3;

                     r_options->clientid[0]=61;
                     r_options->clientid[1]=7;
                     r_options->clientid[2]=0x01;
                     r_options->clientid[3]=0x02;
                     r_options->clientid[4]=0x03;
                     r_options->clientid[5]=0x04;
                     r_options->clientid[6]=0x05;
                     r_options->clientid[7]=0x06;
                     r_options->clientid[8]=0x76;

                     r_options->parareqlist[0]=55;
                     r_options->parareqlist[1]=4;
                     r_options->parareqlist[2]=1;
                     r_options->parareqlist[3]=3;
                     r_options->parareqlist[4]=51;
                     r_options->parareqlist[5]=6;


                     r_options->endoption = 255;

//                     ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + 240+18);
//                    // sum = 0;
//                     //
//                     //adjust lengths
//
//
//                     sum = 0;
//                     etherSumWords(&ip->revSize, 10);
//                     etherSumWords(ip->sourceIp,((ip->revSize & 0xF)*4)-12);
//                     ip->headerChecksum = getEtherChecksum();
//                     udp->length=htons(8+240+18);
//                     sum = 0;
//                     //udp->check =0;
//                     etherSumWords(ip->sourceIp, 8);
//                     tmp16 = ip->protocol;
//
//                     sum += (tmp16 & 0xff) << 8;
//                     etherSumWords(&udp->length, 2);
//                     // add udp header except crc
//                     etherSumWords(udp, 6);
//                     etherSumWords(&udp->data, 240+18);
//                     udp->check = getEtherChecksum();
//                     // send packet with size = ether + udp hdr + ip header + udp_size + dhcp size + options
//                     etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + 240+18);

                     udp->length=htons(8+240+19);
                        //adjust lengths
                        ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + 240+19);

                        sum = 0;
                        etherSumWords(&ip->revSize, 20);
                        ip->headerChecksum = getEtherChecksum();

                        sum = 0;
                        etherSumWords(ip->sourceIp, 8);
                        tmp16 = ip->protocol;

                        sum += (tmp16 & 0xff) << 8;
                        etherSumWords(&udp->length, 2);
                        // add udp header except crc
                        etherSumWords(udp, 6);
                        etherSumWords(&udp->data, 240+19);
                        udp->check = getEtherChecksum();
                        // send packet with size = ether + udp hdr + ip header + udp_size + dhcp size + options
                        etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + 240+19);

              }






}

uint32_t htonl(uint32_t x) {
  //  unsigned char *s = (unsigned char *)&x;
    return (((0x000000ff & x) << 24) + ((0x0000ff00 & x) << 8) + ((0x00ff0000 & x) >> 8) + ((0xff000000 & x) >> 24));
}
void send_syn_ack(uint8_t packet[])
{
    uint8_t blah;
    etherFrame* ether = (etherFrame*)packet;
        ipFrame* ip = (ipFrame*)&ether->data;
        tcpFrame* tcp = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
       uint8_t tcp_length = htons(ip->length) - ((ip->revSize & 0xF) * 4);
        uint8_t *copyData;
        uint8_t i, tmp8;
        uint16_t tmp16,tmps;
        // swap source and destination fields
        for (i = 0; i < HW_ADD_LENGTH; i++)
        {
            tmp8 = ether->destAddress[i];
            ether->destAddress[i] = ether->sourceAddress[i];
            ether->sourceAddress[i] = tmp8;
        }
        for (i = 0; i < IP_ADD_LENGTH; i++)
        {
            tmp8 = ip->destIp[i];
            ip->destIp[i] = ip->sourceIp[i];
            ip->sourceIp[i] = tmp8;
        }
        // set source port of resp will be dest port of req
        // dest port of resp will be left at source port of req
        // unusual nomenclature, but this allows a different tx
        // and rx port on other machine
        tmps = tcp->srcport;
        tcp->srcport = tcp->destport;
        tcp->destport = tmps;

        tcp->data_offset = htons(0x8012);
        uint32_t temp_seq = tcp->seq_no;

        tcp->seq_no = (temp_seq);
        tcp->ack_no = htonl((htonl(temp_seq)+1));
        tcp->checksum = 0x00;
        tcp->urgent_pointer = 0x00;
       // tcp->


        // adjust lengths
        ip->length = htons(((ip->revSize & 0xF) * 4) + tcp_length);
        // 32-bit sum over ip header
        sum = 0;
        //etherSumWords(&ip->revSize, 10);
        //etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
        //ip->headerChecksum = getEtherChecksum();
        //udp->length = htons(8 + udpSize);
        // copy data
     //   copyData = &udp->data;
       // for (i = 0; i < udpSize; i++)
         //   copyData[i] = udpData[i];
        // 32-bit sum over pseudo-header
        sum = 0;
               etherSumWords(ip->sourceIp, 8);
               tmp16 = ip->protocol;
               sum += (tmp16 & 0xff) << 8;
               sum += htons(tcp_length);
               //etherSumWords(tcp_length, 2);
               // add udp header and data
               etherSumWords(tcp, (tcp_length));
               tcp->checksum = getEtherChecksum();

        // send packet with size = ether + udp hdr + ip header + udp_size
        etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + tcp_length);
}

void send_ack(uint8_t packet[])
{
        uint8_t blah;
        etherFrame* ether = (etherFrame*)packet;
           ipFrame* ip = (ipFrame*)&ether->data;
           tcpFrame* tcp = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
          uint8_t tcp_length = htons(ip->length) - ((ip->revSize & 0xF) * 4);
          uint8_t tcp_data_length = tcp_length - 20 ;
           uint8_t *copyData;
           uint8_t i, tmp8;
           uint16_t tmp16,tmps;
           // swap source and destination fields
           for (i = 0; i < HW_ADD_LENGTH; i++)
           {
               tmp8 = ether->destAddress[i];
               ether->destAddress[i] = ether->sourceAddress[i];
               ether->sourceAddress[i] = tmp8;
           }
           for (i = 0; i < IP_ADD_LENGTH; i++)
           {
               tmp8 = ip->destIp[i];
               ip->destIp[i] = ip->sourceIp[i];
               ip->sourceIp[i] = tmp8;
           }
           // set source port of resp will be dest port of req
           // dest port of resp will be left at source port of req
           // unusual nomenclature, but this allows a different tx
           // and rx port on other machine
           tmps = tcp->srcport;
           tcp->srcport = tcp->destport;
           tcp->destport = tmps;

           tcp->data_offset = htons(0x5018);
           uint32_t temp_seq = tcp->seq_no;

           tcp->seq_no = tcp->ack_no;
           tcp->ack_no = htonl((htonl(temp_seq)+tcp_data_length));
           tcp->checksum = 0x00;
           tcp->urgent_pointer = 0x00;
          // tcp->


           // adjust lengths
           ip->length = htons(((ip->revSize & 0xF) * 4) + tcp_length);
           // 32-bit sum over ip header
           sum = 0;
           //etherSumWords(&ip->revSize, 10);
           //etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
           //ip->headerChecksum = getEtherChecksum();
           //udp->length = htons(8 + udpSize);
           // copy data
        //   copyData = &udp->data;
          // for (i = 0; i < udpSize; i++)
            //   copyData[i] = udpData[i];
           // 32-bit sum over pseudo-header
           sum = 0;
                  etherSumWords(ip->sourceIp, 8);
                  tmp16 = ip->protocol;
                  sum += (tmp16 & 0xff) << 8;
                  sum += htons(tcp_length);
                  //etherSumWords(tcp_length, 2);
                  // add udp header and data
                  etherSumWords(tcp, (tcp_length));
                  tcp->checksum = getEtherChecksum();

           // send packet with size = ether + udp hdr + ip header + udp_size
           etherPutPacket(ether, 14 + htons(ip->length));
}


void send_fin_ack(uint8_t packet[])
{
    uint8_t blah;
            etherFrame* ether = (etherFrame*)packet;
               ipFrame* ip = (ipFrame*)&ether->data;
               tcpFrame* tcp = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
              uint8_t tcp_length = htons(ip->length) - ((ip->revSize & 0xF) * 4);
              uint8_t tcp_data_length = tcp_length - 20 ;
               uint8_t *copyData;
               uint8_t i, tmp8;
               uint16_t tmp16,tmps;
               // swap source and destination fields
               for (i = 0; i < HW_ADD_LENGTH; i++)
               {
                   tmp8 = ether->destAddress[i];
                   ether->destAddress[i] = ether->sourceAddress[i];
                   ether->sourceAddress[i] = tmp8;
               }
               for (i = 0; i < IP_ADD_LENGTH; i++)
               {
                   tmp8 = ip->destIp[i];
                   ip->destIp[i] = ip->sourceIp[i];
                   ip->sourceIp[i] = tmp8;
               }
               // set source port of resp will be dest port of req
               // dest port of resp will be left at source port of req
               // unusual nomenclature, but this allows a different tx
               // and rx port on other machine
               tmps = tcp->srcport;
               tcp->srcport = tcp->destport;
               tcp->destport = tmps;

               tcp->data_offset = htons(0x5011);
               uint32_t temp_seq = tcp->seq_no;

               tcp->seq_no = tcp->ack_no;
               tcp->ack_no = htonl((htonl(temp_seq)+1));
               tcp->checksum = 0x00;
               tcp->urgent_pointer = 0x00;
              // tcp->


               // adjust lengths
               ip->length = htons(((ip->revSize & 0xF) * 4) + tcp_length);
               // 32-bit sum over ip header
               sum = 0;
               //etherSumWords(&ip->revSize, 10);
               //etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
               //ip->headerChecksum = getEtherChecksum();
               //udp->length = htons(8 + udpSize);
               // copy data
            //   copyData = &udp->data;
              // for (i = 0; i < udpSize; i++)
                //   copyData[i] = udpData[i];
               // 32-bit sum over pseudo-header
               sum = 0;
                      etherSumWords(ip->sourceIp, 8);
                      tmp16 = ip->protocol;
                      sum += (tmp16 & 0xff) << 8;
                      sum += htons(tcp_length);
                      //etherSumWords(tcp_length, 2);
                      // add udp header and data
                      etherSumWords(tcp, (tcp_length));
                      tcp->checksum = getEtherChecksum();

               // send packet with size = ether + udp hdr + ip header + udp_size
               etherPutPacket(ether, 14 + htons(ip->length));
}


uint16_t etherGetId()
{
    return htons(sequenceId);
}

void etherIncId()
{
    sequenceId++;
}

// Enable or disable DHCP mode
void etherEnableDhcpMode()
{
    dhcpEnabled = true;
}

void etherDisableDhcpMode()
{
    dhcpEnabled = false;
}

bool etherIsDhcpEnabled()
{
    return dhcpEnabled;
}
// Determines if the IP address is valid
bool etherIsIpValid()
{
    return ipAddress[0] || ipAddress[1] || ipAddress[2] || ipAddress[3];
}

// Sets IP address
void etherSetIpAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3)
{
    ipAddress[0] = ip0;
    ipAddress[1] = ip1;
    ipAddress[2] = ip2;
    ipAddress[3] = ip3;
}

// Gets IP address
void etherGetIpAddress(uint8_t ip[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        ip[i] = ipAddress[i];
}

void etherGet_g_IpAddress(uint8_t ip[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        ip[i] = g_yiaddr[i];
}

void etherGetdnsAddress(uint8_t ip[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        ip[i] = g_dns[i];
}

// Sets IP subnet mask
void etherSetIpSubnetMask(uint8_t mask0, uint8_t mask1, uint8_t mask2, uint8_t mask3)
{
    ipSubnetMask[0] = mask0;
    ipSubnetMask[1] = mask1;
    ipSubnetMask[2] = mask2;
    ipSubnetMask[3] = mask3;
}

// Gets IP subnet mask
void etherGetIpSubnetMask(uint8_t mask[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        mask[i] = ipSubnetMask[i];
}

// Sets IP gateway address
void etherSetIpGatewayAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3)
{
    ipGwAddress[0] = ip0;
    ipGwAddress[1] = ip1;
    ipGwAddress[2] = ip2;
    ipGwAddress[3] = ip3;
}

void etherSet_g_DNS(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3)
{
   g_dns[0] = ip0;
   g_dns[1] = ip1;
   g_dns[2] = ip2;
   g_dns[3] = ip3;

}

// Gets IP gateway address
void etherGetIpGatewayAddress(uint8_t ip[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        ip[i] = ipGwAddress[i];
}

// Sets MAC address
void etherSetMacAddress(uint8_t mac0, uint8_t mac1, uint8_t mac2, uint8_t mac3, uint8_t mac4, uint8_t mac5)
{
    macAddress[0] = mac0;
    macAddress[1] = mac1;
    macAddress[2] = mac2;
    macAddress[3] = mac3;
    macAddress[4] = mac4;
    macAddress[5] = mac5;
}

// Gets MAC address
void etherGetMacAddress(uint8_t mac[6])
{
    uint8_t i;
    for (i = 0; i < 6; i++)
        mac[i] = macAddress[i];
}


