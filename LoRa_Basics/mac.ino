/*
 * LakeNet
 * 1-20-21
 * Mac Address Print
 * OLED Display
*/


#include "Arduino.h"
#include "heltec.h"
#include "WiFi.h"

String macAddr;

void setup() {
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Disable*/, true /*Serial Enable*/);
  Heltec.display->flipScreenVertically();
  Heltec.display->setFont(ArialMT_Plain_10);
  macAddr = WiFi.macAddress();
}

void printMac() {
    Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(0, 0, macAddr);
}


void loop() {
  // clear the display
  Heltec.display->clear();
  // draw the current demo method
  printMac();

  Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);
  Heltec.display->drawString(10, 128, String(millis()));
  // write the buffer to the display
  Heltec.display->display();

  delay(10);
}
