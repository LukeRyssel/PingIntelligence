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
#include <WiFiUdp.h>
#include <string.h>
#include "heltec.h"
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <sQueue.h>

//---DEFINITIONS---//
#define ONE_SEC 1000 / portTICK_PERIOD_MS
#define BAND 915E6
#define PORT 12345
#define WAIT_TIME 10 * 1000 / portTICK_PERIOD_MS
#define BROADCAST 5000
#define BOARD_NUM 1
#define NUM_OF_BOARDS 3

// Globals
static const int RXPin = 13, TXPin = 12;
static const uint32_t GPSBaud = 9600;
const char ssid[] = "";
const char passwd[] = "";

static const char address[] = "192.168.0.24";

SemaphoreHandle_t SendSemaphore;
SemaphoreHandle_t CreateSemaphore;
SemaphoreHandle_t ListenSemaphore;

// Wifi packet sender
WiFiUDP Udp;

// The TinyGPS++ object
TinyGPSPlus gps;

// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin);

//---QUEUES---//
QueueHandle_t BQ; // backup queue
//QueueHandle_t TQ; // transmit queue
QueueHandle_t VL; // visitor list

queue TQ;

//---TASK DECLARATIONS---//
void TaskScheduler(void *pvParameters);
void TaskCreate(void *pvParameters);

//---FUNCTIONS DECLARATIONS---//
//void printOLED(char *str); // print just a string
void printOLED(char *str, int value); // print a string with value
void initWifi();
void SendPackets();
void ListenPackets();

void SendPackets()
{
  String packet;
  int broadcast_time = millis() + BROADCAST;
  xSemaphoreTake(SendSemaphore, portMAX_DELAY);
  while(broadcast_time > millis())
  {
    if(!TQ.isEmpty())
    {
      packet = TQ.peek();
      LoRa.beginPacket();
      LoRa.setTxPower(14,RF_PACONFIG_PASELECT_PABOOST);
      LoRa.print(packet);
      LoRa.endPacket();
      Serial.print("Sent packet: ");
      Serial.println(packet);
      TQ.dequeue();
      vTaskDelay(ONE_SEC * 2);
    }
  }
  xSemaphoreGive(SendSemaphore);
}

void ListenPackets()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print("Received packet '");
    // read packet
    while (LoRa.available())  
      Serial.print((char)LoRa.read());
    Serial.println();
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
  Serial.begin(115200);
  //WiFi.disconnect(WIFI_OFF); // set wifi to off, so we can use the core

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
  delay(2000);

  WiFi.disconnect(WIFI_OFF);
  while (ss.available() > 0) {
    if(gps.encode(ss.read())) {
      while(gps.time.minute() == 0) ;
      Serial.println("Connected to GPS");
      uint8_t timeToStart = gps.time.minute() + 1;
      Serial.print("Start minute: ");
      Serial.println(gps.time.minute());
      Serial.print("Suppose end time: ");
      Serial.println(timeToStart);
      while(gps.time.minute() < timeToStart) ;
      Serial.println("Program is running and is synced with nodes");
      Serial.print("Lat: ");
      Serial.println(gps.location.lat());
      Serial.print("Lng: ");
      Serial.println(gps.location.lng());
      break;
    }
  }
  Serial.println("Connected to GPS");
  uint8_t timeToStart = gps.time.minute() + 1;
  Serial.print("Start minute: ");
  Serial.println(gps.time.minute());
  Serial.print("Suppose end time: ");
  Serial.println(timeToStart);
  while(gps.time.minute() < timeToStart) ;
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
  //TQ = xQueueCreate(10, sizeof(String));
  xTaskCreatePinnedToCore(TaskScheduler, "Main task that schedules the program", 8192, NULL, configMAX_PRIORITIES, NULL, 1);
  xTaskCreatePinnedToCore(TaskCreate, "Create packets", 4096, NULL, configMAX_PRIORITIES - 23, NULL, 0);
}

void loop()
{
  // dont need this with tasks
}

// prints a string to OLED, for development purposes, dont run on core 0
void printOLED(String str)
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

void initWifi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.disconnect();
    vTaskDelay(1000);
    WiFi.mode(WIFI_MODE_STA);
    vTaskDelay(1000);
    WiFi.begin(ssid, passwd);
    vTaskDelay(1000);
    while (WiFi.status() != WL_CONNECTED)
    {
      vTaskDelay(ONE_SEC / 2);
      Serial.print(".");
    }
  }
  vTaskDelay(1000);
}

void TaskScheduler(void *pvParameters)
{
  (void)pvParameters;
  char lat[20], lng[20];
  int broadcast_time = millis() + BOARD_NUM * 10000;
  Heltec.display->clear();
  Heltec.display->display();
  xSemaphoreTake(ListenSemaphore, portMAX_DELAY);
  xSemaphoreTake(CreateSemaphore, portMAX_DELAY);
  for (;;)
  { 
    if (broadcast_time < millis())
    {
      //broadcast_time = millis() + BROADCAST;
      SendPackets();
      broadcast_time = millis() + (NUM_OF_BOARDS - 1) * 10000;
    }
    // listening
    else {
        ListenPackets();
    }
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
  for (;;)
  {
    vTaskDelay(ONE_SEC * 9);
    if(packetID == 100 * BOARD_NUM + 100)
      packetID = 100 * BOARD_NUM;
    packet = "0 "; // packet type
    temp_str = packetID;
    packet += temp_str;
    packet += " ";
    for(int i = 12; i < 17; i++)
    {
      if(macAddr[i] == ':') ; // dont add colon
      else
        packet += macAddr[i];
    }
    packet += " "; // space
    packet += "1234 "; // dest addr
    packet += "Temp "; // variable decl
    packet += "420 "; // put temp here
    // add hash, do later
    packetID++;
    xSemaphoreTake(SendSemaphore, portMAX_DELAY);
    if(!TQ.isFull())
      TQ.enqueue(packet);
    xSemaphoreGive(SendSemaphore);
  }
}