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
const unsigned long STREAM_CHECK_INTERVAL = 10000;
bool firebaseInitialized = false;
bool streamStarted = false;

// ------------------------- FONCTIONS -------------------------
void initializeTimestamp();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void checkWiFiConnection();
void reconnectWiFi();
void reconnectFirebase();
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

  // Configuration importante pour le streaming
  config.timeout.serverResponse = 10 * 1000;
  config.timeout.tokenGenerationError = 10 * 1000;
  config.timeout.socketConnection = 10 * 1000;
  //config.debug = true; // Activer le debug Firebase

  Firebase.reconnectNetwork(true);
  Firebase.begin(&config, &auth);

  int attempts = 0;
  while (!Firebase.ready() && attempts < 10) {
    delay(1000);
    attempts++;
    Serial.print(".");
  }

  if (Firebase.ready()) {
    firebaseInitialized = true;
    debugPrint("Firebase initialisé avec succès");
    
    // Démarrer le stream après un court délai
    delay(2000);
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
  
  if (!Firebase.RTDB.beginStream(&fbdoStream, streamPath.c_str())) {
    debugPrint("Échec du début du stream: " + fbdoStream.errorReason());
    streamStarted = false;
  } else {
    Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
    debugPrint("Stream Firebase démarré avec succès sur: " + streamPath);
    streamStarted = true;
  }
}

void checkStreamConnection() {
  if (!streamStarted || !Firebase.ready()) {
    debugPrint("Stream non actif, tentative de redémarrage...");
    startFirebaseStream();
    return;
  }

  // IMPORTANT : lire le stream pour garder la connexion vivante
  if (!Firebase.RTDB.readStream(&fbdoStream)) {
    debugPrint("Erreur stream: " + fbdoStream.errorReason());
    streamStarted = false;
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

// ------------------------- CONSTRUCTION JSON -------------------------
template <typename T>
void addMesureToHistory(T history[], int size, T mesure) {
  decalageGauche(history, size);
  history[size - 1] = mesure;
}

FirebaseJson buildHistoryJson() {
  FirebaseJson json;
  FirebaseJsonArray arrAir, arrSoil, arrTemp;

  for (int i = 0; i < HISTORY_SIZE; i++) {
    arrAir.add(humidite_air_array[i]);
    arrSoil.add(humidite_soil_array[i]);
    arrTemp.add(temperature_array[i]);
  }

  json.add("humidite_air", arrAir);
  json.add("humidite_sol", arrSoil);
  json.add("temperature", arrTemp);

  FirebaseJsonArray tsArray;
  for (int i = 0; i < HISTORY_SIZE; i++) {
    tsArray.add(String(timestamps_array[i]));
  }
  json.add("timestamps", tsArray);

  return json;
}

// ------------------------- MISE À JOUR FIREBASE -------------------------
void updateDataBase(Measure* measures, int taille) {
  if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
    String path = String(BASE_URL) + "/dernieres_mesures";
    
    // Mettre à jour chaque mesure
    for (int i = 0; i < taille; i++) {
      String measurePath = path + "/" + measures[i].key;
      if (!Firebase.RTDB.setFloat(&fbdo, measurePath, measures[i].value)) {
        debugPrint("Erreur update " + String(measures[i].key) + ": " + fbdo.errorReason());
      }
    }

    // Mettre à jour le timestamp
    String timestampPath = path + "/timestamp";
    if (!Firebase.RTDB.setString(&fbdo, timestampPath, getIsoUtcTime())) {
      debugPrint("Erreur update timestamp: " + fbdo.errorReason());
    }

    debugPrint("Mesures mises à jour avec succès");
    
    // Mettre à jour l'historique moins fréquemment
    static unsigned long lastHistoryUpdate = 0;
    if (millis() - lastHistoryUpdate > 10000) {
      lastHistoryUpdate = millis();
      
      FirebaseJson historyJson = buildHistoryJson();
      String historyPath = String(BASE_URL) + "/historiques";
      
      if (Firebase.RTDB.setJSON(&fbdo, historyPath, &historyJson)) {
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
  debugPrint("=============================");

  // Vérifier si c'est bien le champ etat_electrovanne qui a changé
  if (data.dataPath() == "/etat_electrovanne") {
    if (data.dataType() == "boolean" || data.dataType() == "int") {
      bool newState = data.dataType() == "boolean" ? data.boolData() : (data.intData() == 1);
      
      if (newState != RELAY_STATE) {
        RELAY_STATE = newState;
        RELAY_STATE_CHANGED = true;
        debugPrint("Nouvel état reçu de l'interface: " + String(newState ? "OUVERT" : "FERMÉ"));
      } else {
        debugPrint("État identique reçu, ignoré");
      }
    } else {
      debugPrint("Type inattendu pour électrovanne: " + data.dataType());
    }
  } else {
    debugPrint("Changement sur autre champ: " + data.dataPath());
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    debugPrint("⚠️ Timeout du stream Firebase !");
  }
  if (!fbdoStream.httpConnected()) {
    debugPrint("❌ Erreur connexion Firebase: " + fbdoStream.errorReason());
  }
  streamStarted = false; // Forcer la reconnexion
}

// ------------------------- GESTION CONNEXION -------------------------
void checkWiFiConnection() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      debugPrint("WiFi perdu !");
      reconnectWiFi();
    }
  }
}

void reconnectFirebase() {
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt > 30000) {
    lastAttempt = millis();
    
    debugPrint("Tentative de reconnexion Firebase...");
    if (!firebaseInitialized) {
      initializeFirebase();
    } else if (!Firebase.ready()) {
      Firebase.reconnectWiFi(true);
      delay(2000);
    }
  }
}

// ------------------------- SETUP -------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  debugPrint("Démarrage du système Agrisense...");
  
  // Initialisation hardware
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Fermé par défaut
  pinMode(SOIL_MOISTURE_PIN, INPUT);

  // Connexion WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  debugPrint("Connexion au WiFi...");
  
  int wifiRetries = 0;
  while (WiFi.status() != WL_CONNECTED && wifiRetries < 20) {
    delay(500);
    Serial.print(".");
    wifiRetries++;
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
  
  // Vérifier et reconnecter Firebase si nécessaire
  if (WiFi.status() == WL_CONNECTED && (!Firebase.ready() || !firebaseInitialized)) {
    reconnectFirebase();
  }

  // Vérifier régulièrement l'état du stream
  if (millis() - lastStreamCheck > STREAM_CHECK_INTERVAL) {
    lastStreamCheck = millis();
    if (Firebase.ready()) {
      checkStreamConnection();
    }
  }

  // Gérer les changements d'état de l'électrovanne
  handleRelayStateChange();

  // Prise de mesures périodique
  if (millis() - lastMeasureTime >= MEASURE_INTERVAL) {
    lastMeasureTime = millis();

    Measure measures[3];
    readMeasures(measures);

    addMesureToHistory(humidite_air_array, HISTORY_SIZE, measures[0].value);
    addMesureToHistory(temperature_array, HISTORY_SIZE, measures[1].value);
    addMesureToHistory(humidite_soil_array, HISTORY_SIZE, measures[2].value);

    String ts = getIsoUtcTime();
    decalageGauche(timestamps_array, HISTORY_SIZE);
    strncpy(timestamps_array[HISTORY_SIZE - 1], ts.c_str(), 24);

    if (Firebase.ready()) {
      updateDataBase(measures, 3);
    }
  }

  // Lire le stream en continu (indispensable pour garder la connexion vivante)
  if (streamStarted) {
    if (!Firebase.RTDB.readStream(&fbdoStream)) {
      debugPrint("Erreur stream loop: " + fbdoStream.errorReason());
      streamStarted = false;
    }
  }

  delay(100);
}
