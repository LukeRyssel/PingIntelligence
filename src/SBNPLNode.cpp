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
#define BROADCAST 20000 // broadcast for 20 seconds
#define BUFFER ONE_SEC * 3
#define BOARD_NUM 1       // change value for board number
#define STR_BOARD_NUM '1' // this one too
#define NUM_OF_BOARDS 4 // change this to increase number of boards or to increase time to broadcast
#define SYNC_TIME 4
#define INIT_SYNC 2
#define CRAFT_TIME ONE_MIN // set to 10 minutes
#define SERVER_NODE "A27C" // server node mac address (gateway node)

// Globals
static const int RXPin = 13, TXPin = 12;
static const uint32_t GPSBaud = 9600;

// Controls creating packet and sending packet so no interference with queue
SemaphoreHandle_t SendSemaphore;

// The TinyGPS++ object
TinyGPSPlus gps;

// Temp/Humidity Sensor
DHTesp dht;

// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin);

//---QUEUES---//
queue TQ; // transmit queue
queue BQ; // backup queue
queue VL; // visitor list

//---TASK DECLARATIONS---//
void TaskScheduler(void *pvParameters);
void TaskCreate(void *pvParameters);

//---FUNCTIONS DECLARATIONS---//
void initWifi();
void SendPackets();
void ListenPackets();

/*
  Function that syncs the nodes based on the SYNC_TIME definition.
  Reads the current gps.minute time and if a multiple of SYNC_TIME,
  then start syncing nodes. If it is not a multiple, then return 
  false. Simplifies the TaskScheduler / Main task code.
*/
bool syncNodes()
{
  // Boolean to hold if it is time to sync
  bool timeToSync = false;

  // packet to transfer from BQ to TQ (had to do it at some point)
  String packet;

  // while ss.available means that we can start reading from gps
  while (ss.available() > 0)
  {
    if (gps.encode(ss.read())) // this is the actual reading process
    {
      if (gps.time.minute() % SYNC_TIME == 0) // so if this is true (minute is multiple of SYNC_TIME), then sync
      {
        timeToSync = true; // break out of this loop (I know, confusing, but works)
        break;
      }
      else
        break; // break out anyways but timeToSync is set to false
    }
  }
  if (timeToSync) // if timeToSync...
  {
    Serial.println("Syncing...");
    while (1) // infinite while loop, constantly checking minute, and sometimes ss.available isnt available...
    {
      while (ss.available() > 0)
      {
        if (gps.encode(ss.read()))
        {
          if (gps.time.minute() % SYNC_TIME != 0) // sync until next minute, then break
            break;

          // lets add from BQ to TQ here
          else // this is what we do while syncing
          {
            if (!BQ.isEmpty())
            {
              packet = BQ.peek();
              Serial.println(packet);
              BQ.dequeue();
              if (!TQ.isFull())
              {
                TQ.enqueue(packet);
              }
            }
          }
        }
      }
      if (gps.time.minute() % SYNC_TIME != 0) // break out of the infinite while loop
        break;
    }
    Serial.println("Done syncing.");
    return true; // return true to reset broadcast time
  }
  else
    return false;
}

// parses the packet ID
String pctID(String packet)
{
  String packetID;
  for (int i = 2; i < 5; i++) // packet ID starts with 2 ends with 4 (3 chars)
    packetID += packet[i];
  return packetID;
}

// parses end receipt packet to-be-remove-from-queues ID
String pctEndID(String packet)
{
  String packetID;
  for (int i = 6; i < 9; i++)
    packetID += packet[i];
  return packetID;
}

// parses the source address of data packets
String pctOrigin(String packet)
{
  String originAddr;
  for (int i = 6; i < 10; i++)
    originAddr += packet[i];
  return originAddr;
}

// parses the destination address (used for node receipts)
// all data packets are appended with whatever nodes mac address
// and this is what this function parses.
// Example:
// node 1 sends data packet to node 2.
// node 2 sends a node receipt to node 1.
// node 2 appends its mac address to the data packet
// so that when node 3 receives it, it can send
// a node receipt back to node 2.
String pctDestAddr(String packet)
{
  String destAddr;
  for (int i = packet.length() - 4; i < packet.length(); i++)
    destAddr += packet[i];
  return destAddr;
}

// returns the last 4 HEX values of mac address (w/o ':')
String pctMacAddr(String fullMacAddr)
{
  String macAddr;
  for (int i = 12; i < 17; i++)
  {
    if (fullMacAddr[i] == ':')
      ; // dont add colon
    else
      macAddr += fullMacAddr[i];
  }
  return macAddr;
}

// function to send all packets in the Transmit Queue
void SendPackets()
{
  String packet;                             // stores items from queue and sends
  int broadcast_time = millis() + BROADCAST; // this is how long we broadcast for
  String pctArr[SIZE];
  xSemaphoreTake(SendSemaphore, portMAX_DELAY); // take send semaphore (dont want multiple tasks to read from TQ at same time)
  int i = 0;
  vTaskDelay(BUFFER); // slight buffer to let devices who were previously sending catch up and listen
  Serial.println("Sending...");
  TQ.printQueue("Transmit Queue");
  while (broadcast_time > millis()) // basically broadcast for 17 seconds (buffer reduces time)
  {
    if (!TQ.isEmpty()) // check that the transmit queue is not empty
    {
      packet = TQ.peek(); // fetch item / packet from queue
      LoRa.beginPacket(); // start LoRa packet
      LoRa.setTxPower(14, RF_PACONFIG_PASELECT_PABOOST);
      LoRa.print(packet); // add the packet from the transmit queue to the LoRa packet
      LoRa.endPacket();   // send packet
      TQ.dequeue();       // remove the packet from the queue
      pctArr[i] = packet; // store the packet in the string array to later add to backup queue
      i++;
      vTaskDelay(ONE_SEC * 2); // delay to (hopefully) ensure delivery
    }
  }
  Serial.println("Done Sending.");

  // good thing we have buffer
  for (int j = 0; j < i; j++) // for all values in the string array (packets we just sent), add to backup queue
  {
    if (!BQ.isFull()) // check backup queue is full
    {
      if (pctArr[j][0] == '0') // if data packet only, add to BQ
      {
        BQ.enqueue(pctArr[j]); // enqueue to BQ
      }
    }
  }
  TQ.printQueue("Transmit Queue");
  BQ.printQueue("Backup Queue");
  xSemaphoreGive(SendSemaphore); // free shared resource (only conflicts with Create packet task)
}

// function to listen for packets
void ListenPackets()
{
  int packetSize = LoRa.parsePacket();            // receives packet size if any
  String packet = "";                             // init packet
  String macAddr = pctMacAddr(WiFi.macAddress()); // get last 4 chars of mac address
  String packetID;                                // the packet ID
  String temp_packet;                             // temp packet to hold previous packets
  String endID;                                   // end receipt packet to-be-removed ID
  String originAddr; //
  String destAddr;   // appended packet mac address in each data packet
  String packetType; // the first character
  if (packetSize) // if packet size isnt zero, then listen
  {
    // received a packet

    // read packet
    while (LoRa.available()) // read in each character from LoRa transmission
      packet += (char)LoRa.read(); // append the characters to the packet
    packetType += packet[0]; // store packet type
    packetID = pctID(packet); // returns packet ID for given packet
    originAddr = pctOrigin(packet);

    // handle data packets
    if (packetType == "0")
    {
      // take semaphore to ensure using TQ does not conflict with Create packet task using TQ
      xSemaphoreTake(SendSemaphore, portMAX_DELAY);
      // if it is in any queue, we have already handle this packet
      if (VL.searchQueue(packetID) || BQ.searchQueue(packetID) || TQ.searchQueue(packetID))
      {
        Serial.print("In VL || BQ || TQ, dropping packet: ");
        Serial.println(packet);
      }
      
      // if it is not our own data packet, handle it. Packet ID corresponds to board number in definitions
      else if (packetID[0] != STR_BOARD_NUM)
      {
        Serial.print("Received data packet: ");
        Serial.println(packet);
        destAddr = pctDestAddr(packet); // gets the end mac address to be put on the node receipt
        // append my mac address for node receipt accceptance
        packet += " "; // space

        // append my own mac address to the data packet for future node receipts
        packet += pctMacAddr(WiFi.macAddress());
        // add packet to TQ and VL
        if (!TQ.isFull())
          TQ.enqueue(packet);
        if (!VL.isFull())
          VL.enqueue(packet);
        
        // create node receipt and add to TQ
        packet = "1 ";
        packet += packetID;
        packet += " ";
        packet += destAddr;
        if (!TQ.isFull())
          TQ.enqueue(packet);
      }

      // we have received our own packet, dont need it
      else
        Serial.println("Dropping my own packet");

      // let create packets run
      xSemaphoreGive(SendSemaphore);
    }

    // handle node receipts
    else if (packetType == "1")
    {
      if (pctMacAddr(WiFi.macAddress()) == pctDestAddr(packet)) // only want to receive node receipts for my packets
      {
        // fetch the packet from the TQ to later be stored in BQ (this is how to handle node receipts)
        // end receipts remove from both queues
        temp_packet = TQ.removeFromQueue(packetID);
        Serial.print("Received node receipt: ");
        Serial.println(packet);

        // add to backup queue
        if (BQ.isFull())
          BQ.dequeue();
        Serial.println("Adding to backup queue...");
        if (!BQ.searchQueue(packetID))
          BQ.enqueue(temp_packet);
      }
      else
        Serial.println("Received node receipt, but not for me");
    }

    // handle end receipts (remove from BQ and TQ here)
    else if (packetType == "2")
    {
      // if we have never received this end receipt before (probably will never end up in BQ but check anyways)
      if (!VL.searchQueue(packetID) && !BQ.searchQueue(packetID) && !TQ.searchQueue(packetID))
      {
        Serial.print("Received end receipt: ");
        Serial.println(packet);
        if (VL.isFull())
          VL.dequeue();
        VL.enqueue(packet);
        VL.printQueue("Visitor List");
        if (!TQ.isFull())
          TQ.enqueue(packet);
        TQ.printQueue("Transmit Queue");
        endID = pctEndID(packet);
        Serial.println("Removing from TQ: ");
        Serial.println(TQ.removeFromQueue(endID));
        TQ.printQueue("Transmit Queue");
        Serial.print("Removing from BQ: ");
        Serial.println(BQ.removeFromQueue(endID));
        BQ.printQueue("Backup Queue");
      }
      else
        Serial.println("End packet is in VL || BQ || TQ");
    }
  }
}

// prints the logo
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
  char lat[20], lng[20]; // latitude and longitude, values arent exactly used currently, just time
  Serial.begin(115200); // Serial communication, not necessary in real world application, just for testing

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

  // give time to boot up all devices, exactly the same pretty much as syncNodes functions
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

  // dump some sys info to show that GPS is working and running
  Serial.println("Program is running and is synced with nodes");
  Serial.print("Lat: ");
  Serial.println(gps.location.lat());
  Serial.print("Lng: ");
  Serial.println(gps.location.lng());

  // FreeeRTOS stuff to create semaphores and tasks (PIN TO CORE!!!!)
  SendSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(SendSemaphore);
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
  int broadcast_time = millis() + BOARD_NUM * BROADCAST; // time to start broadcasting
  for (;;)
  {
    // syncing....
    if (syncNodes())
      broadcast_time = millis() + BOARD_NUM * BROADCAST;

    // main controller for sending and receiving

    // SEND
    if (broadcast_time < millis())
    {
      SendPackets();
      broadcast_time = millis() + (NUM_OF_BOARDS - 1) * BROADCAST;
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
    packet += " ";         // space
    packet += SERVER_NODE; // dest addr
    packet += " ";
    packet += "Temp "; // variable decl
    temp_str = temperature;
    packet += temp_str;
    packet += " ";
    packet += pctMacAddr(WiFi.macAddress());
    packetID++;
    xSemaphoreTake(SendSemaphore, portMAX_DELAY);
    if (!TQ.isFull())
      TQ.enqueue(packet);
    xSemaphoreGive(SendSemaphore);
  }
}