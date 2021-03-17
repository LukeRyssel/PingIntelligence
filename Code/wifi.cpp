#include <WiFi.h>
#include <WiFiUdp.h>

// set some port number here, I just used 12345
#define PORT 12345

WiFiUDP Udp;

const char ssid[] = "YOUR_WIFI_NAME";
const char passwd[] = "YOUR_WIFI_PASSWORD";

static const char address[] = "YOUR_COMPUTER_IP_ADDRESS"; 

void setup() {
  WiFi.disconnect();
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, passwd);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
  }
}

void loop() {
  uint8_t buffer[10] = "hello ";
  Udp.beginPacket(address, PORT);
  Udp.write(buffer, 10);
  Udp.endPacket();
  delay(10000); // delay 10 seconds, its really annoying to get a bunch of data at once
}