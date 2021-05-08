#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "heltec.h"
#include "sQueue.h"

// set some port number here, I just used 12345
#define PORT 1337
#define BAND 915E6
#define BROADCAST 20000
#define BUFFER 3000
#define BOARD_NUM 4
#define NUM_OF_BOARDS 4
#define SYNC_TIME 4
#define INIT_SYNC 2
int broadcast_time;
int printTime = 1000;
#define LED 13
int endID = BOARD_NUM*100;
WiFiUDP Udp;

NTPClient timeClient(Udp);
const char ssid[] = "SPR-2G";
const char passwd[] = "soccerchorus041";
bool listenPrint;
String myMac;

static const char address[] = "98.146.150.148"; 

void SendToSQL(String DataType,String Value, String SourceNode, String PacketID);
void parsethePacket(String packet);
void sending();
void listening();
void printOLED(String str);
void craftEndReceipt(String packetID);
void countSeconds();

queue TQ;
queue VL;

void setup() {
  Heltec.begin(true /*DisplayEnable Enable*/, true /*Heltec.LoRa Disable*/, true /*Serial Enable*/, true /*PABOOST Enable*/, BAND /*long BAND*/);
  listenPrint = true;
  Serial.begin(115200);
  Serial.println("Starting");
  WiFi.disconnect();
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, passwd);
  String macAddr = WiFi.macAddress();
  for (int i = 12; i < 17; i++)
  {
    if (macAddr[i] == ':') ; // dont add colon
    else
      myMac += macAddr[i];
  }

  int timeToReboot = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
    // reboot here
    if(timeToReboot == 20) ESP.restart();
    timeToReboot++;
  }
  Serial.println("Connected");
  pinMode(LED, OUTPUT);
  timeClient.begin();
  
  Serial.println("Initializing");
  while(1){
    timeClient.update();
    //Serial.println(timeClient.getMinutes());
    if(timeClient.getMinutes() % INIT_SYNC == 0 && timeClient.getMinutes() != 0){
      while(timeClient.getMinutes() % INIT_SYNC == 0);
        //countSeconds();
      broadcast_time = millis() + (BOARD_NUM * BROADCAST);
      Serial.println("Done initializing");
      break;
    }
  }
}

void loop() {
  //countSeconds();
  
  if(timeClient.getMinutes() % SYNC_TIME == 0){
    Serial.println("Syncing...");
    listenPrint = true;
    while(timeClient.getMinutes() % SYNC_TIME == 0);
      //countSeconds();
    Serial.println("Done Syncing");
    broadcast_time = millis() + (BOARD_NUM *BROADCAST);
  }
  if(broadcast_time < millis()){
    broadcast_time = millis() + BROADCAST;
    
    Serial.println("Waiting to send");
    //Serial.println(millis());
    delay(BUFFER);
    //Serial.println(millis());
    while(1){
      //countSeconds();
      //if(myWait < timeClient.getSeconds()){

        sending();
        if(listenPrint == false){
          Serial.println("Sending...");
          listenPrint = true;
        }
        printOLED("Sending");
        if(broadcast_time < millis()){
          break;
        }
      //}
    }  
    
    broadcast_time = millis()+(NUM_OF_BOARDS-1)* BROADCAST;
  }
  
  //Serial.println(timeClient.getMinutes());
  else{
    if(listenPrint == true){
      Serial.println("Listening...");
      printOLED("Listening");
      listenPrint = false;
    }
    listening();
  }
  
}
void SendToSQL(String DataType,String Value,String SourceNode, String PacketID){
  

  String Time = timeClient.getFormattedTime();
  String MyMessage = "INSERT INTO Data VALUES ('";
  
  MyMessage = MyMessage + DataType;
  MyMessage = MyMessage + "', '";
  MyMessage = MyMessage + Value;
  MyMessage = MyMessage + "', '";
  MyMessage = MyMessage + SourceNode;
  MyMessage = MyMessage + "', '";
  MyMessage = MyMessage + Time;
  MyMessage = MyMessage + "', '";
  MyMessage = MyMessage + PacketID;
  MyMessage = MyMessage + "');";
  
  char buffer[100];
  MyMessage.toCharArray(buffer, 100);
  //strcpy((char*)MyMessageArray, MyMessage.c_str());
  Udp.beginPacket(address, PORT);
  Udp.print(MyMessage);
  Udp.endPacket();
}
void parsethePacket(String packet){
  int i;
  int x = 0;
  String packetType, packetID, orgAddr, desAddr, dataType, payload, packetLast;
  //Serial.println(packet);
  for(i = 0; i < packet.length(); i++){
    if(packet[i] == ' '){
      x++;
    }
    else{
      switch(x){
        case 0:
          packetType += packet[i];
        break;
        
        case 1:
          packetID += packet[i];
        break;
        
        case 2:
          orgAddr += packet[i];
        break;
        
        case 3:
          desAddr += packet[i];
        break;
        
        case 4:
          dataType += packet[i];
        break;
        
        case 5:
          payload += packet[i];
        break;
      }
    }
    
  }
  /*i = packet.length()-4;
  for(int j = 0; j < 4; j++){
    packetLast += packet[i];
    i++;
  }*/
  //Serial.print("PacketType: ");
  //Serial.println(packetType);
  //if (packetLast == "BC70"){//This if statment allows us to simiulate distance
  if(packetType == "0"){
    if (VL.searchQueue(packetID)){
      Serial.print("I already have this packet: ");
      Serial.println(packetID);
      //craftEndReceipt(packetID);
    }
    else{
      if(VL.isFull()){
        VL.dequeue();
      }
      VL.enqueue(packet);
      craftEndReceipt(packetID);
      Serial.print("PacketId: ");
      Serial.println(packetID);
      SendToSQL(dataType, payload, orgAddr, packetID);
    }
  }  
}

void sending(){
  String packet;
  if(!TQ.isEmpty()){
    packet = TQ.peek();
    TQ.dequeue();
    LoRa.beginPacket();
    LoRa.setTxPower(14, RF_PACONFIG_PASELECT_PABOOST);
    LoRa.print(packet);
    
    LoRa.endPacket();
    Serial.print("Sent packet: ");
    Serial.println(packet);
    delay(1000);
  }
}
void listening(){
  
  String myPacket;
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.println("Received packet ");
    // read packet
    while (LoRa.available()) {
      myPacket += ((char)LoRa.read());
    }
    parsethePacket(myPacket);
    
  }
}
void craftEndReceipt(String packetID){
  String packet;
  String temp;
  temp = endID;


  packet += "2 ";
  packet += temp;
  packet += " ";
  packet += packetID;
  if(endID == (BOARD_NUM*100)+99){
    endID = BOARD_NUM * 100;
  }
  else
    endID++;
  
  if (!TQ.isFull())
    TQ.enqueue(packet);
}
void printOLED(String str)
{
  Heltec.display->clear();
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawString(0, 0, str);
  Heltec.display->display();
}
void countSeconds(){
  if(printTime == 1000 || printTime == timeClient.getSeconds()){
    if (timeClient.getSeconds() == 59){
      printTime = 0;
    }
    else{
    printTime = timeClient.getSeconds() + 1;
    }
    Serial.println(timeClient.getSeconds());
  }
}