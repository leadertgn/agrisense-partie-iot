#include <ESP8266WiFi.h>
#include "time_utils.h"

#define WIFI_SSID "youpilab_fibre"
#define WIFI_PASSWORD "i_l@v3_yl2021Fibre"



void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connexion au WiFi");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("\nWiFi connecté");

  initTime(); // initialisation NTP
  waitForNTP(); // attente synchro NTP
}

void loop() {
  String ts = getIsoUtcTime();
  Serial.println(ts);
  delay(5000);
}
