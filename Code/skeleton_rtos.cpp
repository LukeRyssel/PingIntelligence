/* Group:
 * LakeNet LoRa Mesh Network Program
 *
 * Authors:
 * Ronald Keating, Nate Osterberg, Luke Ryssel
 * 
 * 
 * Description:
 *    Program uses some aspects from Doug Park's Master Thesis
 *    on the Stateless Broadcast Network Protocol for LoRa;
 *    however, we have implemented an RTOS for this protocol
 *    to work. This was a future expectation of Doug to be implemented.
 *    
*/

//---LIBRARIES---//
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include "heltec.h"
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

//---DEFINITIONS---//
#define ONE_SEC 1000 / portTICK_PERIOD_MS
#define BAND 915E6

static const int RXPin = 13, TXPin = 12;
static const uint32_t GPSBaud = 9600;

// The TinyGPS++ object
TinyGPSPlus gps;

// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin);

//---PACKET TYPES---//
enum PacketType {DataPacket, NodeReceipt, EndReceipt};

//---PACKETS---//
typedef struct PacketHeader {
  int packetType; // packet type, as specified in the packet type enum above
  byte originAddr[6]; // 6 byte mac address of THIS device, should only be 2 bytes
  byte packetID[4]; // unique 4 byte packet identifier
  byte destAddr[6]; // 6 byte mac address of destination device, should only be 2 bytes
} packetHeader;

typedef struct DataPacket {
  packetHeader header;
  char payload[20]; // 20 byte payload, data to be transmitted
  char hash[32]; // 32 byte hash of the payload, SHA256 in simulation
} dataPacket;

typedef struct ReceiptPacket {
  packetHeader header;
  byte destAddr[6]; // destination address of node needing receipt
  byte packetID[4]; // ID of packet needing receipt (to drop from queue)
} receiptPacket;

//---QUEUES---//
QueueHandle_t BQ; // backup queue
QueueHandle_t TQ; // transmit queue
QueueHandle_t VL; // visitor list

//---TASK DECLARATIONS---//
void TaskScheduler(void *pvParameters);
void TaskWiFi(void *pvParameters);

//---FUNCTIONS DECLARATIONS---//
//void printOLED(char *str); // print just a string
void printOLED(char *str, int value); // print a string with value

void drawLogo() {
    int i;
    Heltec.display->clear();
    // draws top circuit lines
    for(i = 2; i < 10; i++) {
      Heltec.display->setPixel(64, i);
      Heltec.display->setPixel(54, i);
      Heltec.display->setPixel(74, i);
    }
    // draws left circuit lines
    for(i = 34; i < 42; i++) {
      Heltec.display->setPixel(i, 32);
      Heltec.display->setPixel(i, 42);
      Heltec.display->setPixel(i, 22);
    }
    // draw right circuit lines
    for(i = 88; i < 96; i++) {
      Heltec.display->setPixel(i, 32);
      Heltec.display->setPixel(i, 42);
      Heltec.display->setPixel(i, 22);
    }
    // draws bottom circuit lines
    for(i = 56; i < 64; i++) {
      Heltec.display->setPixel(64, i);
      Heltec.display->setPixel(54, i);
      Heltec.display->setPixel(74, i);
    }
    Heltec.display->drawRect(42, 10, 45, 45);
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
    Heltec.display->drawString(64, 11, "L");
    Heltec.display->drawString(64, 21, "A");
    Heltec.display->drawString(64, 31, "K");
    Heltec.display->drawString(64, 41, "E");
    Heltec.display->drawString(56, 41, "N");
    Heltec.display->drawString(72, 41, "T");
    Heltec.display->display();
}

void setup() {
  Serial.begin(115200);
  WiFi.disconnect(WIFI_OFF); // set wifi to off, so we can use the core

  // begin communication with GPS
  ss.begin(GPSBaud);

  // init LoRa
  Heltec.begin(true /*DisplayEnable Enable*/, 
  true /*Heltec.Heltec.Heltec.LoRa Disable*/, 
  true /*Serial Enable*/, true /*PABOOST Enable*/, 
  BAND /*long BAND*/);
  // init display, turn off when power saving
  Heltec.display->init();
  Heltec.display->flipScreenVertically();
  drawLogo();
  delay(5000);
  xTaskCreatePinnedToCore(TaskScheduler, "Main task that schedules the program", 4096, NULL, configMAX_PRIORITIES, NULL, 1);
  xTaskCreatePinnedToCore(TaskWiFi, "WiFi manager", 2048, NULL, configMAX_PRIORITIES - 1, NULL, 0);
}

void loop() {
  // dont need this with tasks
}

// prints a string to OLED, for development purposes, dont run on core 0
void printOLED(char *str)
{
  Heltec.display->clear();
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawString(0, 0, str);
  Heltec.display->display();
}

// prints a string and value to OLED, for development purposes, dont run on core 0
void printOLED(char *str, int value)
{
  Heltec.display->clear();
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawString(0, 0, str);
  Heltec.display->drawString(35, 0, " : ");
  Heltec.display->drawString(45, 0, String(value));
  Heltec.display->display();
}

void sendDataPacket(dataPacket *packet)
{
  ;
}

void sendReceiptPacket(receiptPacket *packet)
{
  LoRa.beginPacket();
  LoRa.setTxPower(14,RF_PACONFIG_PASELECT_PABOOST);
  LoRa.print("hello");
  LoRa.endPacket();
}

void TaskScheduler(void *pvParameters)
{
  (void)pvParameters;
  char lat[20], lng[20];
  for(;;) {
    while (ss.available() > 0) {
      if(gps.encode(ss.read())) {
        snprintf(lat, sizeof(lat), "%g", gps.location.lat());
        snprintf(lng, sizeof(lng), "%g", gps.location.lng());
        Heltec.display->clear();
        Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
        Heltec.display->setFont(ArialMT_Plain_10);
        Heltec.display->drawString(0, 0, lat);
        Heltec.display->drawString(0, 10, lng);
        Serial.print(gps.time.hour());
        Serial.print(" : ");
        Serial.print(gps.time.minute());
        Serial.print(" : ");
        Serial.println(gps.time.second());
        Heltec.display->display();
        vTaskDelay(5 * ONE_SEC);
      }
    }
    //vTaskDelay(5 * ONE_SEC);
  }
}

void TaskWiFi(void *pvParameters)
{
  (void)pvParameters;
  //dataPacket *pack = (dataPacket*)malloc(sizeof(dataPacket));
  for(;;) {
    Serial.println("WiFi Task");
    //vTaskDelay(ONE_SEC);
    delay(1000);
  }
}