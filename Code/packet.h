/* LakeNet: Ping Intelligence
 * packet.h
 * Based on the packet structure of Doug Park's LoRa Thesis
 * 11-8-20
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Arduino.h"
#include "heltec.h"
#include "WiFi.h"

// Constant Size Values
const int PAYLOAD_SIZE = 20; // 20 bytes for payload (based on similation, can be increased based on demand)
const int HASH_SIZE = 32; // hash value of payload (always 32 bytes)
const int PACKET_ID_SIZE = 4; // 4 bytes for unique identifier
const int NODE_ADDR_SIZE = 2; // first 2 bytes of MAC Address of device, can be adjusted based on size of network and conflicting address values
const int LAND_NODE_ADDR = 56; // Not the actual land node receipt

// Enumeration of packet type
typedef enum {DataPacket, NodeReceipt, EndReceipt} PacketType;

// header, total space of 7 bytes
typedef struct Header {
  PacketType packetType; // enumeration of packet type
  String originNodeAddr; // origin node address
  unsigned int packetID; // packet identifier
  String destNodeAddr; // destination node (server)
} header;

// data packet, total space of 39 + the size of the payload. currently is set to 20, so 59 bytes total (subject to change)
typedef struct DataPacket {
  header *head;
  char *payload;
  int hash[HASH_SIZE];
} dataPacket;

// receipt packet, total space of 13 bytes
typedef struct ReceiptPacket {
  header *head;
  String nodeGettingReceipt;
  unsigned int IPNR; // ID of Packet Needing Receipt, should be 4 bytes, however this is based on number of nodes in the network
} receiptPacket;

// probably not int here, but this returns the device mac address (maybe do last couple bytes)
String deviceMac()
{
    return WiFi.macAddress();
}


char *data()
{
  //collect sensor data
  return "42\n456789\n36";
}


dataPacket createDataPacket(dataPacket packet)
{
  //data packets are only made by lake devices, not land node

  //header
  packet.head->packetType = DataPacket;
  packet.head->originNodeAddr = deviceMac(); // Figure out hex stuff here, maybe changes each device jump
  packet.head->packetID = deviceMac()[0] + 2; //array of boolean, placement of array is addition to mac, 
                                           //bool value whether to packetID is being used
  packet.head->destNodeAddr = LAND_NODE_ADDR; // Figure out hex stuff here
  
  //payload
  packet.payload = (char *)malloc(PAYLOAD_SIZE + 1);
  packet.payload = strdup(data());
  
  //hash
  //just use Dougs CryptoAlg
  return packet;
}