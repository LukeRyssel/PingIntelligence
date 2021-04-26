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
#include <sQueue.h>
#include <Adafruit_Sensor.h>
#include "DHTesp.h"

//---DEFINITIONS---//
#define ONE_SEC 1000 / portTICK_PERIOD_MS
#define ONE_MIN ONE_SEC * 60
#define BAND 915E6
#define PORT 12345
#define WAIT_TIME 10 * 1000 / portTICK_PERIOD_MS
#define BROADCAST 5000
#define BOARD_NUM 3
#define NUM_OF_BOARDS 4
#define SYNC_TIME 59
#define INIT_SYNC 2
#define CRAFT_TIME ONE_MIN / 2
#define HOUR gps.time.hour()
#define MIN gps.time.minute()
#define SEC gps.time.second()
#define CLIENT_NODE "A050"
#define SERVER_NODE "A27C"

// Globals
static const int RXPin = 13, TXPin = 12;
static const uint32_t GPSBaud = 9600;

SemaphoreHandle_t SendSemaphore;
SemaphoreHandle_t CreateSemaphore;
SemaphoreHandle_t ListenSemaphore;

// The TinyGPS++ object
TinyGPSPlus gps;

// Temp/Humidity Sensor
DHTesp dht;

// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin);

//---QUEUES---//
queue TQ;
queue BQ;
queue VL;

//---TASK DECLARATIONS---//
void TaskScheduler(void *pvParameters);
void TaskCreate(void *pvParameters);

//---FUNCTIONS DECLARATIONS---//
//void printOLED(char *str); // print just a string
void printOLED(char *str, int value); // print a string with value
void initWifi();
void SendPackets();
void ListenPackets();

String pctID(String packet)
{
  String packetID;
  for (int i = 2; i < 5; i++)
    packetID += packet[i];
  return packetID;
}

String pctOrigin(String packet)
{
  String originAddr;
  for(int i = 6; i < 10; i++)
    originAddr += packet[i];
  return originAddr;
}

void SendPackets()
{
  String packet;
  int broadcast_time = millis() + BROADCAST;
  xSemaphoreTake(SendSemaphore, portMAX_DELAY);
  while (broadcast_time > millis())
  {
    if (!TQ.isEmpty())
    {
      packet = TQ.peek();
      LoRa.beginPacket();
      LoRa.setTxPower(14, RF_PACONFIG_PASELECT_PABOOST);
      LoRa.print(packet);
      LoRa.endPacket();
      Serial.print("Sent packet: ");
      Serial.println(packet);
      TQ.dequeue();
      vTaskDelay(ONE_SEC * 2); // maybe take this out
    }
  }
  xSemaphoreGive(SendSemaphore);
}

void ListenPackets()
{
  int packetSize = LoRa.parsePacket();
  String packet = "";
  String macAddr = WiFi.macAddress();
  String packetID;
  String originAddr;
  int i;
  String packetType;
  if (packetSize)
  {
    // received a packet
    Serial.print("Received packet ");
    // read packet
    while (LoRa.available())
      packet += (char)LoRa.read();
    packetType += packet[0];
    i = packet.length() - 4;
    packetID = pctID(packet); // returns packet ID for given packet
    originAddr = pctOrigin(packet);
    if (packetType == "0")
    {
      xSemaphoreTake(SendSemaphore, portMAX_DELAY);
      if (!TQ.isFull())
        TQ.enqueue(packet);
      packet = "1 ";
      packet += packetID;
      packet += " ";
      packet += originAddr;
      if (!TQ.isFull())
        TQ.enqueue(packet);
      xSemaphoreGive(SendSemaphore);
    }
    else if (packetType == "2")
    {
      for (int i = 2; i < 5; i++)
        packetID += packet[i];
      if (!VL.searchQueue(packetID))
      {
        if(VL.isFull())
          VL.dequeue();
        VL.enqueue(packet);
        if (!TQ.isFull())
          TQ.enqueue(packet);
      }
    }
    Serial.println(packet);
  }
}

void drawLogo()
{
  int i;
  Heltec.display->clear();
  // draws top circuit lines
  for (i = 2; i < 10; i++)
  {
    Heltec.display->setPixel(64, i);
    Heltec.display->setPixel(54, i);
    Heltec.display->setPixel(74, i);
  }
  // draws left circuit lines
  for (i = 34; i < 42; i++)
  {
    Heltec.display->setPixel(i, 32);
    Heltec.display->setPixel(i, 42);
    Heltec.display->setPixel(i, 22);
  }
  // draw right circuit lines
  for (i = 88; i < 96; i++)
  {
    Heltec.display->setPixel(i, 32);
    Heltec.display->setPixel(i, 42);
    Heltec.display->setPixel(i, 22);
  }
  // draws bottom circuit lines
  for (i = 56; i < 64; i++)
  {
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

void setup()
{
  char lat[20], lng[20];
  Serial.begin(115200);
  //WiFi.disconnect(WIFI_OFF); // set wifi to off, so we can use the core

  // begin communication with GPS
  ss.begin(GPSBaud);
  dht.setup(17, DHTesp::DHT22);

  // init LoRa
  Heltec.begin(true /*DisplayEnable Enable*/,
               true /*Heltec.Heltec.Heltec.LoRa Disable*/,
               true /*Serial Enable*/, true /*PABOOST Enable*/,
               BAND /*long BAND*/);
  // init display, turn off when power saving
  Heltec.display->init();
  Heltec.display->flipScreenVertically();
  drawLogo();
  delay(2000);
  WiFi.disconnect(WIFI_OFF);
  while (1)
  {
    while (ss.available() > 0)
    {
      if (gps.encode(ss.read()))
      {
        Serial.print(gps.time.hour());
        Serial.print(" : ");
        Serial.print(gps.time.minute());
        Serial.print(" : ");
        Serial.println(gps.time.second());
        if (gps.time.minute() != 0)
          break;
      }
    }
    if (gps.time.minute() != 0)
      break;
  }
  Serial.println("Connected to GPS");
  while (1)
  {
    while (ss.available() > 0)
    {
      if (gps.encode(ss.read()))
      {
        if (gps.time.minute() % INIT_SYNC == 0)
          break;
      }
    }
    if (gps.time.minute() % INIT_SYNC == 0)
      break;
  }
  Serial.println("Program is running and is synced with nodes");
  Serial.print("Lat: ");
  Serial.println(gps.location.lat());
  Serial.print("Lng: ");
  Serial.println(gps.location.lng());
  SendSemaphore = xSemaphoreCreateBinary();
  CreateSemaphore = xSemaphoreCreateBinary();
  ListenSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(SendSemaphore);
  xSemaphoreGive(CreateSemaphore);
  xSemaphoreGive(ListenSemaphore);
  xTaskCreatePinnedToCore(TaskScheduler, "Main task that schedules the program", 8192, NULL, configMAX_PRIORITIES, NULL, 1);
  xTaskCreatePinnedToCore(TaskCreate, "Create packets", 4096, NULL, configMAX_PRIORITIES - 23, NULL, 0);
}

void loop()
{
  // dont need this with tasks
}

void TaskScheduler(void *pvParameters)
{
  (void)pvParameters;
  //char lat[20], lng[20];
  int syncTimer, listenPrint = 0;
  int broadcast_time = millis() + BOARD_NUM * 10000;
  Heltec.display->clear();
  Heltec.display->display();
  xSemaphoreTake(ListenSemaphore, portMAX_DELAY);
  xSemaphoreTake(CreateSemaphore, portMAX_DELAY);
  for (;;)
  {
    // syncing....
    while (ss.available() > 0)
    {
      if (gps.encode(ss.read()))
      {
        if (gps.time.minute() % SYNC_TIME == 0)
        {
          broadcast_time = millis() + BOARD_NUM * 10000;
          syncTimer = millis() + 60000; // current time plus a minute
          Serial.println("Syncing...");
          while (syncTimer > millis()) ;
          Serial.println("Done syncing.");
          break;
        }
      }
    }

    // main controller for sending and receiving

    // SEND
    if (broadcast_time < millis())
    {
      SendPackets();
      broadcast_time = millis() + (NUM_OF_BOARDS - 1) * 10000;
    }

    // RECEIVE
    else
      ListenPackets();
  }
}

void TaskCreate(void *pvParameters)
{
  (void)pvParameters;
  String packet;
  String macAddr = WiFi.macAddress();
  Serial.println(macAddr);
  int packetID = 100 * BOARD_NUM;
  String temp_str;
  float temperature;
  for (;;)
  {
    vTaskDelay(CRAFT_TIME);
    temperature = dht.getTemperature();
    if (packetID == 100 * BOARD_NUM + 100)
      packetID = 100 * BOARD_NUM;
    packet = "0 "; // packet type
    temp_str = packetID;
    packet += temp_str;
    packet += " ";
    for (int i = 12; i < 17; i++)
    {
      if (macAddr[i] == ':')
        ; // dont add colon
      else
        packet += macAddr[i];
    }
    packet += " ";     // space
    packet += "1234 "; // dest addr
    packet += "Temp "; // variable decl
    temp_str = temperature;
    packet += temp_str;
    // add hash, do later
    packetID++;
    xSemaphoreTake(SendSemaphore, portMAX_DELAY);
    if (!TQ.isFull())
      TQ.enqueue(packet);
    xSemaphoreGive(SendSemaphore);
  }
}