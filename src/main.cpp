#include <ESP8266WiFi.h>
#include <DHT.h>
#include <Firebase_ESP_Client.h>
#include "time_utils.h"
#include "secrets.h"
#include "Array_Utils.h"

// Bibliothèques auxiliaires Firebase
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ------------------------- CONFIGURATION -------------------------
#define DHT_PIN 12   // D6
#define DHT_TYPE DHT11
#define SOIL_MOISTURE_PIN A0
#define RELAY_PIN 14  // D5

const bool TEST_MODE = false;
const char* BASE_URL = TEST_MODE ? "test_data" : "cultures";

const int HISTORY_SIZE = 10;
bool RELAY_STATE = false;
bool RELAY_STATE_CHANGED = false;

// Tableaux de mesures
float humidite_air_array[HISTORY_SIZE] = {0};
float humidite_soil_array[HISTORY_SIZE] = {0};
float temperature_array[HISTORY_SIZE] = {0};
char timestamps_array[HISTORY_SIZE][25];

// Structure de mesure
struct Measure {
  const char* key;
  float value;
};

// Objets capteur / Firebase
DHT dht(DHT_PIN, DHT_TYPE);
FirebaseData fbdo;        // Pour les requêtes classiques (set/get)
FirebaseData fbdoStream;  // Pour le streaming
FirebaseAuth auth;
FirebaseConfig config;

// ------------------------- VARIABLES GLOBALES -------------------------
unsigned long lastMeasureTime = 0;
const unsigned long MEASURE_INTERVAL = 5000;
unsigned long lastStreamCheck = 0;
const unsigned long STREAM_CHECK_INTERVAL = 5000; // Réduit à 5 secondes
bool firebaseInitialized = false;
bool streamStarted = false;

// ------------------------- FONCTIONS -------------------------
void initializeTimestamp();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void checkWiFiConnection();
void reconnectWiFi();
void initializeFirebase();
void handleRelayStateChange();
void debugPrint(const String& message);
void startFirebaseStream();
void checkStreamConnection();

// ------------------------- DÉBOGAGE -------------------------
void debugPrint(const String& message) {
  Serial.println("[DEBUG] " + message);
}

// ------------------------- INITIALISATION -------------------------
void initializeTimestamp() {
  initTime();
  waitForNTP();
}

void reconnectWiFi() {
  debugPrint("Tentative de reconnexion WiFi...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("WiFi reconnecté ! IP: " + WiFi.localIP().toString());
    firebaseInitialized = false;
    streamStarted = false;
  } else {
    debugPrint("Impossible de se reconnecter au WiFi. Redémarrage...");
    ESP.restart();
  }
}

void initializeFirebase() {
  debugPrint("Initialisation de Firebase...");
  
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Configuration pour le streaming
  config.timeout.serverResponse = 15 * 1000;
  config.timeout.tokenGenerationError = 10 * 1000;
  
  // Configurer les callbacks de token
  config.token_status_callback = tokenStatusCallback;
  
  Firebase.reconnectNetwork(true);
  Firebase.begin(&config, &auth);

  debugPrint("Attente de la connexion Firebase...");
  int attempts = 0;
  while (!Firebase.ready() && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts % 10 == 0) Serial.println();
  }

  if (Firebase.ready()) {
    firebaseInitialized = true;
    debugPrint("Firebase initialisé avec succès");
    
    // Démarrer le stream
    startFirebaseStream();
  } else {
    debugPrint("Échec de l'initialisation Firebase: " + fbdo.errorReason());
    firebaseInitialized = false;
    streamStarted = false;
  }
}

void startFirebaseStream() {
  if (!Firebase.ready()) {
    debugPrint("Firebase non prêt, impossible de démarrer le stream");
    return;
  }
  
  debugPrint("Démarrage du stream Firebase...");
  String streamPath = String(BASE_URL) + "/dernieres_mesures";
  
  // Arrêter le stream existant s'il y en a un
  if (streamStarted) {
    Firebase.RTDB.endStream(&fbdoStream);
    streamStarted = false;
    delay(1000);
  }
  
  // Configurer le fbdoStream pour le streaming
  fbdoStream.setBSSLBufferSize(2048, 2048); // Buffer plus grand pour le streaming
  fbdoStream.setResponseSize(2048);
  
  if (!Firebase.RTDB.beginStream(&fbdoStream, streamPath.c_str())) {
    debugPrint("Échec du début du stream: " + fbdoStream.errorReason());
    streamStarted = false;
    
    // Tentative de récupération de l'état initial
    debugPrint("Tentative de récupération de l'état initial...");
    String valvePath = streamPath + "/etat_electrovanne";
    if (Firebase.RTDB.getBool(&fbdo, valvePath.c_str())) {
      if (fbdo.dataType() == "boolean") {
        RELAY_STATE = fbdo.boolData();
        digitalWrite(RELAY_PIN, RELAY_STATE ? LOW : HIGH);
        debugPrint("État initial électrovanne: " + String(RELAY_STATE ? "OUVERT" : "FERMÉ"));
      }
    }
    
  } else {
    Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
    debugPrint("Stream Firebase démarré avec succès sur: " + streamPath);
    streamStarted = true;
  }
}

void checkStreamConnection() {
  if (!Firebase.ready()) {
    debugPrint("Firebase non prêt, stream impossible");
    return;
  }
  
  if (!streamStarted) {
    debugPrint("Stream non démarré, tentative de redémarrage...");
    startFirebaseStream();
    return;
  }
  
  // Vérifier si le stream est toujours connecté
  if (!fbdoStream.httpConnected()) {
    debugPrint("Stream déconnecté, reconnexion...");
    streamStarted = false;
    startFirebaseStream();
  } else {
    // Lire le stream pour le maintenir actif
    if (!Firebase.RTDB.readStream(&fbdoStream)) {
      debugPrint("Erreur lecture stream: " + fbdoStream.errorReason());
      if (fbdoStream.httpCode() == 401 || fbdoStream.httpCode() == 403) {
        // Token invalide, besoin de réauthentifier
        debugPrint("Token invalide, réinitialisation Firebase...");
        firebaseInitialized = false;
        initializeFirebase();
      }
    }
  }
}

// ------------------------- LECTURE DES MESURES -------------------------
void readMeasures(Measure* measures) {
  float temperature = dht.readTemperature();
  float humidity_air = dht.readHumidity();
  float soil_moisture_raw = analogRead(SOIL_MOISTURE_PIN);
  float soil_moisture = map(constrain(soil_moisture_raw, 0, 1023), 0, 1023, 0, 100);
  
  if (isnan(temperature) || isnan(humidity_air)) {
    debugPrint("Erreur de lecture DHT !");
    temperature = -1;
    humidity_air = -1;
  }

  measures[0] = {"humidite_air", humidity_air};
  measures[1] = {"temperature", temperature};
  measures[2] = {"humidite_sol", soil_moisture};
  
  debugPrint("Mesures - Temp: " + String(temperature) + 
             "°C, HumAir: " + String(humidity_air) + 
             "%, HumSol: " + String(soil_moisture) + "%");
}

// ------------------------- MISE À JOUR FIREBASE -------------------------
void updateDataBase(Measure* measures, int taille) {
  if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
    String path = String(BASE_URL) + "/dernieres_mesures";
    
    FirebaseJson json;
    for (int i = 0; i < taille; i++) {
      json.set(measures[i].key, measures[i].value);
    }
    json.set("timestamp", getIsoUtcTime());
    
    if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
      debugPrint("Mesures mises à jour avec succès");
    } else {
      debugPrint("Erreur update mesures: " + fbdo.errorReason());
    }
    
    // Mettre à jour l'historique moins fréquemment
    static unsigned long lastHistoryUpdate = 0;
    if (millis() - lastHistoryUpdate > 30000) {
      lastHistoryUpdate = millis();
      
      FirebaseJson historyJson;
      FirebaseJsonArray arrAir, arrSoil, arrTemp, tsArray;
      
      for (int i = 0; i < HISTORY_SIZE; i++) {
        arrAir.add(humidite_air_array[i]);
        arrSoil.add(humidite_soil_array[i]);
        arrTemp.add(temperature_array[i]);
        tsArray.add(String(timestamps_array[i]));
      }
      
      historyJson.set("humidite_air", arrAir);
      historyJson.set("humidite_sol", arrSoil);
      historyJson.set("temperature", arrTemp);
      historyJson.set("timestamps", tsArray);
      
      String historyPath = String(BASE_URL) + "/historiques";
      if (Firebase.RTDB.setJSON(&fbdo, historyPath.c_str(), &historyJson)) {
        debugPrint("Historique mis à jour avec succès");
      } else {
        debugPrint("Erreur update historique: " + fbdo.errorReason());
      }
    }
  }
}

// ------------------------- GESTION ÉLECTROVANNE -------------------------
void handleRelayStateChange() {
  if (RELAY_STATE_CHANGED) {
    digitalWrite(RELAY_PIN, RELAY_STATE ? LOW : HIGH);
    debugPrint("État électrovanne changé: " + String(RELAY_STATE ? "OUVERT" : "FERMÉ"));
    RELAY_STATE_CHANGED = false;
  }
}

// ------------------------- CALLBACKS STREAM -------------------------
void streamCallback(FirebaseStream data) {
  debugPrint("=== DONNÉES STREAM REÇUES ===");
  debugPrint("Chemin: " + data.streamPath());
  debugPrint("DataPath: " + data.dataPath());
  debugPrint("Type: " + data.dataType());
  debugPrint("Valeur: " + data.stringData());
  debugPrint("Event: " + String(data.eventType()));
  debugPrint("=============================");

  // Vérifier si c'est bien le champ etat_electrovanne qui a changé
  if (data.dataPath() == "/etat_electrovanne") {
    if (data.dataType() == "boolean") {
      bool newState = data.boolData();
      if (newState != RELAY_STATE) {
        RELAY_STATE = newState;
        RELAY_STATE_CHANGED = true;
        debugPrint("Nouvel état reçu de l'interface: " + String(newState ? "OUVERT" : "FERMÉ"));
      }
    }
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    debugPrint("⚠️ Timeout du stream Firebase !");
  }
  streamStarted = false;
}

// ------------------------- GESTION CONNEXION -------------------------
void checkWiFiConnection() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      debugPrint("WiFi perdu !");
      reconnectWiFi();
    } else {
      debugPrint("WiFi OK - Signal: " + String(WiFi.RSSI()) + " dBm");
    }
  }
}

// ------------------------- SETUP -------------------------
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  debugPrint("Démarrage du système Agrisense...");
  
  // Initialisation hardware
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Fermé par défaut
  pinMode(SOIL_MOISTURE_PIN, INPUT);

  // Connexion WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  debugPrint("Connexion au WiFi " + String(WIFI_SSID) + "...");
  
  int wifiRetries = 0;
  while (WiFi.status() != WL_CONNECTED && wifiRetries < 30) {
    delay(500);
    Serial.print(".");
    wifiRetries++;
    if (wifiRetries % 20 == 0) Serial.println();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("WiFi connecté! IP: " + WiFi.localIP().toString());
    
    initializeTimestamp();
    initializeFirebase();
    
    // Initialiser les tableaux
    for (int i = 0; i < HISTORY_SIZE; i++) {
      String ts = getIsoUtcTime();
      strncpy(timestamps_array[i], ts.c_str(), 24);
    }
    
  } else {
    debugPrint("Échec connexion WiFi. Redémarrage...");
    ESP.restart();
  }
}

// ------------------------- LOOP -------------------------
void loop() {
  checkWiFiConnection();
  
  // Réinitialiser Firebase si perte de connexion
  if (WiFi.status() == WL_CONNECTED && !Firebase.ready()) {
    debugPrint("Reconnexion Firebase nécessaire...");
    initializeFirebase();
  }

  // Vérifier le stream régulièrement
  if (millis() - lastStreamCheck > STREAM_CHECK_INTERVAL) {
    lastStreamCheck = millis();
    checkStreamConnection();
  }

  // Gérer l'électrovanne
  handleRelayStateChange();

  // Prise de mesures
  if (millis() - lastMeasureTime >= MEASURE_INTERVAL) {
    lastMeasureTime = millis();

    Measure measures[3];
    readMeasures(measures);

    // Mise à jour Firebase
    if (Firebase.ready()) {
      updateDataBase(measures, 3);
    }
  }

  // Lecture continue du stream (ESSENTIEL)
  if (streamStarted && Firebase.ready()) {
    if (!Firebase.RTDB.readStream(&fbdoStream)) {
      debugPrint("Erreur lecture stream loop: " + fbdoStream.errorReason());
      if (fbdoStream.httpCode() == 401) {
        debugPrint("Token expiré, réinitialisation...");
        firebaseInitialized = false;
      }
      streamStarted = false;
    }
  }

  delay(50); // Réduit le délai pour une meilleure réactivité
}