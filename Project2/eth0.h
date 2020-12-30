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

#ifndef ETH0_H_
#define ETH0_H_

#include <stdint.h>
#include <stdbool.h>

#define ETHER_UNICAST        0x80
#define ETHER_BROADCAST      0x01
#define ETHER_MULTICAST      0x02
#define ETHER_HASHTABLE      0x04
#define ETHER_MAGICPACKET    0x08
#define ETHER_PATTERNMATCH   0x10
#define ETHER_CHECKCRC       0x20

#define ETHER_HALFDUPLEX     0x00
#define ETHER_FULLDUPLEX     0x100

#define LOBYTE(x) ((x) & 0xFF)
#define HIBYTE(x) (((x) >> 8) & 0xFF)

uint16_t topic_length;
uint8_t d_length;

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
    //uint16_t options;
    uint16_t data;

}tcpFrame;

typedef struct _tcpMQTTFrame
{
    uint16_t srcport;
    uint16_t destport;
    uint32_t seq_no;
    uint32_t ack_no;
    uint16_t data_offset;
    uint16_t win_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
    uint16_t options;
    //uint16_t data;

}tcpMQTTFrame;


typedef struct _MQTTFrame
{
    uint8_t msgtype;
    uint8_t msglength;
    uint16_t protocol_length;
    char protocol_name[4];
    uint8_t mqtt_v;
    uint8_t mqtt_flags;
    uint16_t mqtt_ttl;
    uint16_t client_length;
    char clientID[6];


}MQTTFrame;

typedef struct _MQTTSubFrame
{
    uint8_t msgtype;
    uint8_t msglength;
    uint16_t msgid;
    uint16_t topiclength;
    char topicname[20];
    uint8_t req_qos;


}MQTTSubFrame;

typedef struct _MQTTUnSubFrame
{
    uint8_t msgtype;
    uint8_t msglength;
    uint16_t msgid;
    uint16_t topiclength;
    char topicname[20];
  //  uint8_t req_qos;


}MQTTUnSubFrame;

typedef struct _MQTTPubFrame
{
    uint8_t msgtype;
    uint8_t msglength;
    //uint16_t msgid;
    uint16_t topiclength;
    char topicname[40];
//    char data[20];
    uint16_t req_qos;


}MQTTPubFrame;

typedef struct _MQTTPingFrame
{
    uint8_t msgtype;
    uint8_t msglength;

}MQTTPingFrame;


//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void etherInit(uint16_t mode);
bool etherIsLinkUp();

bool etherIsDataAvailable();
bool etherIsOverflow();
uint16_t etherGetPacket(uint8_t packet[], uint16_t maxSize);
bool etherPutPacket(uint8_t packet[], uint16_t size);

bool etherIsIp(uint8_t packet[]);
bool etherIsIpUnicast(uint8_t packet[]);

bool etherIsPingRequest(uint8_t packet[]);
void etherSendPingResponse(uint8_t packet[]);

bool etherIsArpRequest(uint8_t packet[]);
void etherSendArpResponse(uint8_t packet[]);
void etherSendArpRequest(uint8_t packet[], uint8_t ip[]);

bool etherIsUdp(uint8_t packet[]);
uint8_t* etherGetUdpData(uint8_t packet[]);
void etherSendUdpResponse(uint8_t packet[], uint8_t* udpData, uint8_t udpSize);

void etherSendDiscoverMessage();
void etherSendDHCPRequest();

void etherEnableDhcpMode();
void etherDisableDhcpMode();
bool etherIsDhcpEnabled();
bool etherIsIpValid();
void etherSetIpAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void etherGetIpAddress(uint8_t ip[4]);
void etherSetIpGatewayAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void etherGetIpGatewayAddress(uint8_t ip[4]);
void etherSetIpSubnetMask(uint8_t mask0, uint8_t mask1, uint8_t mask2, uint8_t mask3);
void etherGetIpSubnetMask(uint8_t mask[4]);
void etherSetMacAddress(uint8_t mac0, uint8_t mac1, uint8_t mac2, uint8_t mac3, uint8_t mac4, uint8_t mac5);
void etherGetMacAddress(uint8_t mac[6]);
bool etherIsOffer(uint8_t packet[]);
bool etherIsIpBroadcast(uint8_t packet[]);
bool etherIsTcp(uint8_t packet[]);
bool etherIsAck(uint8_t packet[]);
uint16_t* get_g_ip_l_time();
void etherSetOffer(uint8_t packet[]);
void etherSetAck(uint8_t packet[]);
uint32_t htonl(uint32_t x);
uint16_t htons(uint16_t value);
void set_to_unicast();
void etherGet_g_IpAddress(uint8_t ip[4]);
void etherSet_g_IP();
void etherSendDHCPRelease();
void etherSendDeclineMessage();
void etherSendGratuitousArpRequest();
void etherSendDHCPRebind();
void send_syn_ack(uint8_t packet[]);
void send_ack(uint8_t packet[]);
uint16_t get_tcp_flag(uint8_t packet[]);
void send_fin_ack(uint8_t packet[]);
void send_syn();
//void send_mqtt_ack(uint8_t packet[]);
void send_mqtt_connect();
void send_mqtt_subreq(char topic[topic_length],uint16_t topic_length);
void send_mqtt_pubmsg(char topic[topic_length],char data[d_length],uint16_t topic_length,uint16_t d_length);
void send_mqtt_ping();
void send_mqtt_disconnect();
void send_mqtt_finack(uint32_t tcp_ack, uint32_t tcp_seq);
void send_mqtt_unsub(char topic[topic_length],uint16_t topic_length);
uint16_t get_mqtt_tcp_flag(uint8_t packet[]);
void send_mqtt_ack(uint32_t tcp_ack, uint32_t tcp_seq);
uint32_t get_ip_lease_time();
void etherSet_g_DNS(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void etherGetdnsAddress(uint8_t ip[4]);
uint8_t get_topic_length();
#define ntohs htons

#endif
