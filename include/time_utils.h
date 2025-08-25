#pragma once
#include <Arduino.h>
#include <time.h>
#include <ESP8266WiFi.h>  // ou <WiFi.h> pour ESP32

// ----------------------
// Configuration NTP
// ----------------------
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600; // 1h * 3600 secondes
const int daylightOffset_sec = 0; // pas d'heure d'été
// ----------------------
// Initialisation NTP
// ----------------------
inline void initTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2); // UTC +1
}

inline void waitForNTP() {
    Serial.print("Synchronisation NTP");
    time_t now = time(nullptr);
    int tries = 0;
    while (now < 8 * 3600 * 2) { // temps avant synchro NTP
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        tries++;
        if (tries > 20) break;
    }
    Serial.println("\nHeure synchronisée !");
}

inline String getIsoUtcTime() {
    time_t now;
    time(&now);
    struct tm *utc = gmtime(&now);

    char buffer[32];
    snprintf(buffer, sizeof(buffer),
             "%04d-%02d-%02dT%02d:%02d:%02dZ",
             utc->tm_year + 1900,
             utc->tm_mon + 1,
             utc->tm_mday,
             utc->tm_hour,
             utc->tm_min,
             utc->tm_sec);

    return String(buffer);
}
