#include <ESP8266WiFi.h>
#include <DHT.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include "secrets.h"  // Contient WIFI_SSID, WIFI_PASSWORD, API_KEY, DATABASE_URL, USER_EMAIL, USER_PASSWORD
#include "time_utils.h" // Pour NTP et timestamp ISO 8601

// Broches
#define DHT_PIN 12 // D6
#define DHT_TYPE DHT11
#define RELAY_PIN 14 // D5

// Capteur et relais
DHT dht(DHT_PIN, DHT_TYPE);
bool RELAY_STATE = false;

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// -----------------
// Fonctions
// -----------------
//String collectSensorData();
void checkWiFiConnection();
//void sendAllData(float temperature, float humiditeAir, float humiditeSol);

// -----------------
// Setup
// -----------------
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relais fermé par défaut

  // Connexion WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connexion au WiFi");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("\nWiFi connecté : " + WiFi.localIP().toString());

  // Initialisation NTP
  initTime();  
  waitForNTP();
  Serial.println("Heure synchronisée : " + getIsoUtcTime());

  // Config Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.cert.data = NULL; // Optionnel, pour SSL
  //config.cert.data = FIREBASE_ROOT_CA; // Certificat SSL pour sécurité
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);

  // Attendre que Firebase soit prêt
  int attempts = 0;
  while (!Firebase.ready() && attempts < 10) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  if (Firebase.ready()) {
    Serial.println("\nFirebase prêt !");
  } else {
    Serial.println("\nErreur connexion Firebase : " + fbdo.errorReason());
  }
}

// -----------------
// Loop
// -----------------
void loop() {
  checkWiFiConnection();

  if (Firebase.ready()) {

  } else {
    Serial.println("Firebase non prêt !");
    delay(5000);
  }
}

// -----------------
// Fonctions
// -----------------

void sendAllData(float temperature, float humiditeAir, float humiditeSol) {
    String timestamp = getIsoUtcTime();

    FirebaseJson updateJson;

    // ----- Dernières mesures (sans toucher à etat_electrovanne) -----
    FirebaseJson lastMeasures;
    lastMeasures.set("temperature", temperature);
    lastMeasures.set("humidite_air", humiditeAir);
    lastMeasures.set("humidite_sol", humiditeSol);
    lastMeasures.set("timestamp", timestamp);
    updateJson.set("cultures/dernieres_mesures", lastMeasures);

    // ----- Historiques -----
    struct { const char* key; float value; } measures[] = {
        {"temperature", temperature},
        {"humidite_air", humiditeAir},
        {"humidite_sol", humiditeSol}
    };

    for (int i = 0; i < 3; i++) {
        String path = String("cultures/historiques/") + measures[i].key;

        // Lire l'historique actuel
        FirebaseJsonArray arr;
        if (Firebase.RTDB.getArray(&fbdo, path)) {
            arr = fbdo.jsonArray();
        }

        // Limiter à 10 éléments
        if (arr.size() >= 10) arr.remove(0);
        arr.add(measures[i].value);

        // Ajouter au JSON global
        updateJson.set(path.c_str(), arr);
    }

    // ----- Timestamps -----
    String tsPath = "cultures/historiques/timestamps";
    FirebaseJsonArray tsArr;
    if (Firebase.RTDB.getArray(&fbdo, tsPath)) tsArr = fbdo.jsonArray();
    if (tsArr.size() >= 10) tsArr.remove(0);
    tsArr.add(timestamp);
    updateJson.set(tsPath.c_str(), tsArr);

    // ----- Envoi unique -----
    if (Firebase.RTDB.updateNode(&fbdo, "/", &updateJson)) {
        Serial.println("Toutes les données envoyées avec succès !");
    } else {
        Serial.println("Erreur envoi global : " + fbdo.errorReason());
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

