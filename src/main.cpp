#include <ESP8266WiFi.h>
#include <DHT.h>
#include <Firebase_ESP_Client.h>
#include "time_utils.h"
#include "secrets.h" 
#include "certs.h"

// Bibliothèque auxiliaire pour le jeton d'authentification
#include "addons/TokenHelper.h"  
#include "addons/RTDBHelper.h" 

// Brochage des capteurs
#define DHT_PIN 12  // D6 
#define DHT_TYPE DHT11 
#define SOIL_MOISTURE_PIN A0


// Brochage des actionneurs et état

#define RELAY_PIN 14 // D5
bool RELAY_STATE = false;
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
void getInitialRelayState();
void checkWiFiConnection();
// setup
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Fermé par défaut

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
  config.cert.data = FIREBASE_ROOT_CA;

   // Auth Email/Password
   auth.user.email = USER_EMAIL;
   auth.user.password = USER_PASSWORD;
  
  // Initialisation Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);

  // Attendre que Firebase soit prêt
  int attempts = 0;
  while (!Firebase.ready() && attempts < 10) {
    delay(1000);
    attempts++;
    Serial.print(".");
  }
  if(Firebase.ready()) {
    getInitialRelayState();
    
    Serial.println("Démarrage de l'écoute");
    if(!Firebase.RTDB.beginStream(&fbdo,"cultures/dernieres_mesures/etat_electrovanne")){
      Serial.println("Echec stream : " + fbdo.errorReason());
    }
    Firebase.RTDB.setStreamCallback(&fbdo,streamCallback,streamTimeoutCallback);
  } else {
    Serial.println("Échec de la connexion Firebase" + fbdo.errorReason());
  }

  initializeTimestamp();

}

void loop() {
  checkWiFiConnection();  // Vérifier WiFi régulièrement
  
  if(Firebase.ready()){
    Serial.println("Firebase prêt !");
    String ts = getIsoUtcTime();
    Serial.println(ts);
  }
  else {
    Serial.println("Firebase non prêt !");
    Serial.printf("Mémoire libre: %d bytes\n", ESP.getFreeHeap());
  }
  delay(5000);
}

void initializeTimestamp() {
  initTime(); // initialisation NTP
  waitForNTP(); // attente synchro NTP
}

void streamCallback(FirebaseStream data) {
  Serial.println("\n=== Données reçues depuis Firebase ===");
  Serial.print("Chemin écouté complet: ");
  Serial.println(data.streamPath());
  Serial.print("Type de donnée: ");
  Serial.println(data.dataType());
  Serial.print("Valeur: ");
  Serial.println(data.stringData());
  
  // Maintenant on reçoit directement la valeur booléenne
  if(data.dataType() == "boolean"){
    RELAY_STATE = data.boolData();
    digitalWrite(RELAY_PIN, RELAY_STATE ? LOW : HIGH);
    Serial.print("Nouvel état de l'électrovanne : ");
    Serial.println(RELAY_STATE ? "OUVERT" : "FERME");
    Serial.println("=== Changement appliqué ===");
  }
  else {
    Serial.println("=== Type de donnée inattendu ===");
  }
}

void streamTimeoutCallback(bool timeout) {
  if(timeout){
    Serial.println("Timeout du stream !");
  }
  if(!fbdo.httpConnected()) {
    Serial.println("Erreur : " + fbdo.errorReason());
  }
}

void getInitialRelayState(){
  if(Firebase.RTDB.getBool(&fbdo,"cultures/dernieres_mesures/etat_electrovanne")){
    if(fbdo.dataType() == "boolean"){
      bool initial_state = fbdo.boolData();
      RELAY_STATE = initial_state;
      digitalWrite(RELAY_PIN, RELAY_STATE ? LOW : HIGH);
      Serial.print("Etat initial de l'électrovanne : ");
      Serial.println(RELAY_STATE ? "OUVERT" : "FERME");
      Serial.print("Valeur pin: ");
      int state = digitalRead(RELAY_PIN);
      Serial.println(state);
      Serial.print("Etat : ");
      Serial.println(state == LOW ? "OUVERT" : "FERME");
    }else{
      Serial.print("Type de donnée inconnue: ");
      Serial.println(fbdo.dataType());
      Serial.print("Valeur reçue: ");
      Serial.println(fbdo.stringData());
    }
  }else{
    Serial.print("Erreur lors de la récupération: ");
    Serial.println(fbdo.errorReason());
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi perdu, tentative de reconnexion...");
    WiFi.disconnect();
    WiFi.reconnect();
    delay(3000);
  }
}