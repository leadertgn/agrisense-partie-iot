#include <ESP8266WiFi.h>
#include <DHT.h>
#include <Firebase_ESP_Client.h>
#include "time_utils.h"
#include "secrets.h" 
// Bibliothèque auxiliaire pour le jeton d'authentification
#include "addons/TokenHelper.h"  
#include "addons/RTDBHelper.h" 

// Brochage des capteurs
#define DHT_PIN 12  // D6 
#define DHT_TYPE DHT11 
#define SOIL_MOISTURE_PIN A0



// Objet DHT 
DHT dht(DHT_PIN,DHT_TYPE);

// Objet principal Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// En-tête des fonctions
void initializeTimestamp();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);

// setup
void setup() {
  Serial.begin(115200);
  dht.begin();

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
  //Démarrage de l'écoute 
  if(!Firebase.RTDB.beginStream(&fbdo,"cultures/dernieres_mesures")){
    Serial.println("Echec stream : " + fbdo.errorReason());
  }
  // Initialisation de l'écoute avec les callbacks 
  Firebase.RTDB.setStreamCallback(&fbdo,streamCallback,streamTimeoutCallback);
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
  else {
    Serial.println("Firebase non prêt !");
  }
}

void initializeTimestamp() {
  initTime(); // initialisation NTP
  waitForNTP(); // attente synchro NTP
}

void streamCallback(FirebaseStream data) {
  Serial.println("Données reçues depuis Firebase !");
  Serial.print("Chemin écouté :");
  Serial.println(data.streamPath());
  Serial.print("Valeur : ");
  Serial.println(data.stringData());
  Serial.print("Chemin de la donnée changée : ");
  Serial.println(data.dataPath());
}

void streamTimeoutCallback(bool timeout) {
  if(timeout){
    Serial.println("Timeout du stream !");
  }
  if(!fbdo.httpConnected()) {
    Serial.println("Erreur : " + fbdo.errorReason());
  }
}