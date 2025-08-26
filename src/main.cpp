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
String collectSensorData();
void sendToFirebase(String path, String jsonData);
void checkWiFiConnection();
String getISOTime();

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
    String jsonData = collectSensorData();
    Serial.println("JSON généré : " + jsonData);

    sendToFirebase("/test_data", jsonData);
    delay(10000); // toutes les 10 secondes
  } else {
    Serial.println("Firebase non prêt !");
    delay(5000);
  }
}

// -----------------
// Fonctions
// -----------------

String collectSensorData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Erreur lecture DHT !");
      temperature = 0;
      humidity = 0;
  }

  DynamicJsonDocument doc(256);
  doc["temperature"] = temperature;
  doc["humidite"] = humidity;
  doc["timestamp"] = getIsoUtcTime(); // Timestamp ISO 8601

  String jsonStr;
  serializeJson(doc, jsonStr);
  return jsonStr;
}

void sendToFirebase(String path, String jsonData) {
  // Convertir le JSON string en objet
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    Serial.println("Erreur parsing JSON : " + String(error.c_str()));
    return;
  }

  // Préparer FirebaseJson pour mise à jour partielle
  FirebaseJson fbJson;
  JsonObject obj = doc.as<JsonObject>();
  for (JsonPair kv : obj) {
    // Ajouter chaque champ individuellement
    if (kv.value().is<float>()) {
      fbJson.set(kv.key().c_str(), kv.value().as<float>());
    } else if (kv.value().is<bool>()) {
      fbJson.set(kv.key().c_str(), kv.value().as<bool>());
    } else {
      fbJson.set(kv.key().c_str(), kv.value().as<String>());
    }
  }

  // Envoyer la mise à jour sans écraser les autres champs
  if (Firebase.RTDB.updateNode(&fbdo, path.c_str(), &fbJson)) {
    Serial.println("Données envoyées avec succès !");
  } else {
    Serial.println("Erreur d'envoi : " + fbdo.errorReason());
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

