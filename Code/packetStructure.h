/* LakeNet: Ping Intelligence
 * packetStructure.h
 * Based on the packet structure of Doug Park's LoRa Thesis
 * 11-8-20
 */

// Constant Size Values
const int PAYLOAD_SIZE = 20; 				// 20 bytes for payload (based on similation, can be increased based on demand)
const int HASH_SIZE = 32; 				// hash value of payload (always 32 bytes)
const int PACKET_ID_SIZE = 4; 			// 4 bytes for unique identifier
const int NODE_ADDR_SIZE = 2; 			// first 2 bytes of MAC Address of device, can be adjusted based on size of network and conflicting address values

// Enumeration of packet type
typedef enum {DataPacket, NodeReceipt, EndReceipt} PacketType;

// header, total space of 7 bytes
typedef struct Header {
  PacketType packetType; 				// enumeration of packet type
  char originNodeAddr[NODE_ADDR_SIZE]; 		// first 2 bytes of MAC address of original packet sender
  unsigned int packetID; 				// unique ID that changes based on node that is sending packet
  char* destNodeAddr[NODE_ADDR_SIZE]; 		// first 2 bytes of MAC address of the destination node
} header;

// data packet, total space of 39 + the size of the payload. currently is set to 20, so 59 bytes total (subject to change)
typedef struct DataPacket {
  header *head; 					// include header (7 bytes)
  char payload[PAYLOAD_SIZE]; 			// payload of the packet, meaning data that is collected
  int hash[HASH_SIZE]; 				// hash of the payload, each array spot only holds 1 byte of an int, should make this smaller
} dataPacket;

// receipt packet, total space of 13 bytes
typedef struct ReceiptPacket {
  header *head; 					// include header (7 bytes)
  char nodeGettingReceipt[NODE_ADDR_SIZE]; 		// address of node getting receipt (another destNodeAddr essentially)
  unsigned int IPNR; 					// ID of Packet Needing Receipt, should be 4 bytes, however this is based on number of nodes in the network
} receiptPacket;
