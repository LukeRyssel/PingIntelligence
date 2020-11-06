/* Ping Intelligence
 * LoRa Basics for ESP32
 * 11-5-20
 * Design Review
 */

#include "heltec.h"
#define BAND 915E6

int counter = 0;
int MAXSIZE = 15; // message size

void setup() {
  Heltec.begin(true /*DisplayEnable Enable*/, true /*Heltec.LoRa Disable*/, true /*Serial Enable*/, true /*PABOOST Enable*/, BAND /*long BAND*/);
  Heltec.display->init();
  Heltec.display->flipScreenVertically();
  Heltec.display->setFont(ArialMT_Plain_10);

}

void loop() {
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  int packetSize;
  // load received packet into a message
  char msg[MAXSIZE];
  int i = 0; // iterator
  //sender
  if(counter%2 == 0)
  {
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "Sending Packet");
    Heltec.display->display();
    // start building packet
    LoRa.beginPacket();
    // not sure what this does
    LoRa.setTxPower(14,RF_PACONFIG_PASELECT_PABOOST);
    // write to packet I believe
    LoRa.print("PACKET FROM SENDER");
    LoRa.endPacket();
  }
  //receiver
  else
  {
    packetSize = LoRa.parsePacket();
    // if received
    if (packetSize) {
      Heltec.display->clear();
      Heltec.display->drawString(0, 0, "Received Packet");
      Heltec.display->display();
      // read packet into message
      while(LoRa.available()) {
        msg[i] = (char)LoRa.read();
        i++;
      }
      Heltec.display->clear();
      // print message
      Heltec.display->drawString(0, 0, msg);
      Heltec.display->display();
    }
  }
  counter++;
  delay(10000);
}
