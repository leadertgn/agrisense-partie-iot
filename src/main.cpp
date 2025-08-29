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
char timestamps_array[HISTORY_SIZE][25]; // Tableau de chaînes fixes

// Structure de mesure
struct Measure {
  const char* key;
  float value;
};

// Objets capteur / Firebase
DHT dht(DHT_PIN, DHT_TYPE);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ------------------------- VARIABLES GLOBALES -------------------------
unsigned long lastMeasureTime = 0;
const unsigned long MEASURE_INTERVAL = 5000;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000;
bool firebaseInitialized = false;

// ------------------------- FONCTIONS -------------------------
void initializeTimestamp();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void getInitialRelayState();
void checkWiFiConnection();
void reconnectWiFi();
void reconnectFirebase();
void initializeFirebase();
void handleRelayStateChange();
void debugPrint(const String& message);

// ------------------------- DÉBOGAGE -------------------------
void debugPrint(const String& message) {
  Serial.println("[DEBUG] " + message);
}

// ------------------------- INITIALISATION -------------------------
void initializeTimestamp() {
  initTime();    // initialisation NTP
  waitForNTP();  // attente synchro
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
    // Réinitialiser Firebase après reconnexion WiFi
    firebaseInitialized = false;
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

  // Configuration des timeouts (version corrigée)
  //config.timeout.serverResponse = 30 * 1000;
  // La propriété sslRequest n'existe pas, elle a été supprimée
  
  Firebase.reconnectNetwork(true);
  Firebase.begin(&config, &auth);

  int attempts = 0;
  while (!Firebase.ready() && attempts < 15) {
    delay(1000);
    attempts++;
    Serial.print(".");
  }

  if (Firebase.ready()) {
    firebaseInitialized = true;
    debugPrint("Firebase initialisé avec succès");
    
    // Récupérer l'état initial
    getInitialRelayState();
    
    // Démarrer le stream sur le bon chemin
    String streamPath = String(BASE_URL) + "/dernieres_mesures";
    debugPrint("Démarrage du stream sur: " + streamPath);
    
    if (!Firebase.RTDB.beginStream(&fbdo, streamPath.c_str())) {
      debugPrint("Échec du début du stream: " + fbdo.errorReason());
    } else {
      Firebase.RTDB.setStreamCallback(&fbdo, streamCallback, streamTimeoutCallback);
      debugPrint("Stream Firebase démarré avec succès");
    }
  } else {
    debugPrint("Échec de l'initialisation Firebase: " + fbdo.errorReason());
    firebaseInitialized = false;
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
FirebaseJson buildMeasuresJson(Measure* measures, int taille) {
  FirebaseJson json;
  for (int i = 0; i < taille; i++) {
    json.add(measures[i].key, measures[i].value);
  }
  json.add("timestamp", getIsoUtcTime());
  json.add("etat_electrovanne", RELAY_STATE);
  return json;
}

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
    // Mettre à jour seulement les mesures
    FirebaseJson mesuresJson = buildMeasuresJson(measures, taille);
    String path = String(BASE_URL) + "/dernieres_mesures";
    
    if (Firebase.RTDB.setJSON(&fbdo, path, &mesuresJson)) {
      debugPrint("Mesures mises à jour avec succès");
    } else {
      debugPrint("Erreur update mesures: " + fbdo.errorReason());
    }

    // Mettre à jour l'historique moins fréquemment
    static unsigned long lastHistoryUpdate = 0;
    if (millis() - lastHistoryUpdate > 30000) { // Toutes les 30 secondes
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
    
    // Confirmer l'état à Firebase
    if (Firebase.ready()) {
      String path = String(BASE_URL) + "/dernieres_mesures/etat_electrovanne";
      if (Firebase.RTDB.setBool(&fbdo, path, RELAY_STATE)) {
        debugPrint("État électrovanne confirmé dans Firebase");
      } else {
        debugPrint("Erreur confirmation état: " + fbdo.errorReason());
      }
    }
    
    RELAY_STATE_CHANGED = false;
  }
}

void getInitialRelayState() {
  String path = String(BASE_URL) + "/dernieres_mesures/etat_electrovanne";
  if (Firebase.RTDB.getBool(&fbdo, path.c_str())) {
    if (fbdo.dataType() == "boolean") {
      RELAY_STATE = fbdo.boolData();
      digitalWrite(RELAY_PIN, RELAY_STATE ? LOW : HIGH);
      debugPrint("État initial électrovanne: " + String(RELAY_STATE ? "OUVERT" : "FERMÉ"));
    } else {
      debugPrint("Type de données inattendu: " + String(fbdo.dataType()));
    }
  } else {
    debugPrint("Erreur récupération état relais: " + fbdo.errorReason());
  }
}

// ------------------------- CALLBACKS STREAM -------------------------
void streamCallback(FirebaseStream data) {
  debugPrint("Données reçues depuis Firebase");
  debugPrint("Chemin: " + data.streamPath());
  debugPrint("DataPath: " + data.dataPath());
  debugPrint("Type: " + data.dataType());
  debugPrint("Valeur: " + data.stringData());

  // Vérifier si c'est bien le champ etat_electrovanne qui a changé
  if (data.dataPath() == "/etat_electrovanne") {
    if (data.dataType() == "boolean" || data.dataType() == "int") {
      bool newState = data.dataType() == "boolean" ? data.boolData() : (data.intData() == 1);
      
      if (newState != RELAY_STATE) {
        RELAY_STATE = newState;
        RELAY_STATE_CHANGED = true;
        debugPrint("Nouvel état reçu: " + String(newState ? "OUVERT" : "FERMÉ"));
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
    debugPrint("Timeout du stream Firebase");
  }
  if (!fbdo.httpConnected()) {
    debugPrint("Erreur connexion Firebase: " + fbdo.errorReason());
  }
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
  if (millis() - lastAttempt > RECONNECT_INTERVAL) {
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
    
    // Initialisation timestamp et Firebase
    initializeTimestamp();
    initializeFirebase();
    
    // Initialiser les tableaux avec des valeurs par défaut
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
  
  // Gérer la reconnexion Firebase si nécessaire
  if (WiFi.status() == WL_CONNECTED && (!Firebase.ready() || !firebaseInitialized)) {
    reconnectFirebase();
  }

  // Gérer les changements d'état de l'électrovanne
  handleRelayStateChange();

  // Prise de mesures périodique
  if (millis() - lastMeasureTime >= MEASURE_INTERVAL) {
    lastMeasureTime = millis();

    Measure measures[3];
    readMeasures(measures);

    // Ajouter à l'historique
    addMesureToHistory(humidite_air_array, HISTORY_SIZE, measures[0].value);
    addMesureToHistory(temperature_array, HISTORY_SIZE, measures[1].value);
    addMesureToHistory(humidite_soil_array, HISTORY_SIZE, measures[2].value);

    // Mise à jour du timestamp
    String ts = getIsoUtcTime();
    decalageGauche(timestamps_array, HISTORY_SIZE);
    strncpy(timestamps_array[HISTORY_SIZE - 1], ts.c_str(), 24);

    // Mise à jour Firebase si connecté
    if (Firebase.ready()) {
      updateDataBase(measures, 3);
    }
  }

  // Petite pause pour éviter de surcharger le CPU
  delay(100);
}