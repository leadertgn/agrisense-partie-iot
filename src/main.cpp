#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "time_utils.h"
// Bibliothèque auxiliaire pour le jeton d'authentification
#include "addons/TokenHelper.h"  
#include "addons/RTDBHelper.h" 

// Remplacez par votre propre jeton d'authentification
#define WIFI_SSID "youpilab_fibre"
#define WIFI_PASSWORD "i_l@v3_yl2021Fibre"

// Firebase
#define API_KEY "AIzaSyD934rncjxMR41u1KkusSskFs1Neh0FVvI"
#define DATABASE_URL "https://essai-iot-52c18-default-rtdb.firebaseio.com"

// Auth Firebase
#define USER_EMAIL "espuser@gmail.com"
#define USER_PASSWORD "monpassword123"

// Objet principal Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Fonction d'initialisation du timestamp en-tête 
void initializeTimestamp();
void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connexion au WiFi");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("\nWiFi connecté");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());
   // Configuration Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

   // Auth Email/Password
   auth.user.email = USER_EMAIL;
   auth.user.password = USER_PASSWORD;
  
  // Initialisation Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  // Initialisation du serveur NTP
  initializeTimestamp();

}

void loop() {
  if(Firebase.ready()){
    Serial.println("Firebase prêt !");
    String ts = getIsoUtcTime();
    Serial.println(ts);
    delay(5000);
  }

}

void initializeTimestamp() {
  initTime(); // initialisation NTP
  waitForNTP(); // attente synchro NTP
}