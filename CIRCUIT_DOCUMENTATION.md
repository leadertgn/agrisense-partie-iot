# Documentation du Projet Agrisense IoT

## 1. Vue d'ensemble

Ce projet vise à automatiser la surveillance et le contrôle d'un système d'irrigation. Il utilise un microcontrôleur ESP8266 pour lire les données de capteurs d'humidité de l'air, de température et d'humidité du sol, puis les transmet à une base de données Firebase. L'état d'une électrovanne (pour l'irrigation) peut être contrôlé à distance via Firebase. L'architecture repose sur un ESP8266 connecté au Wi-Fi, communiquant avec Firebase pour la synchronisation des données et le contrôle.

## 2. Composants Hardware

| Composant             | Pin ESP8266 | Fonction                               | Notes                                  |
| :-------------------- | :---------- | :------------------------------------- | :------------------------------------- |
| Capteur DHT (DHT11)   | D6 (GPIO12) | Mesure température et humidité de l'air | Utilise la broche numérique            |
| Capteur d'humidité sol | A0          | Mesure l'humidité du sol               | Broche analogique, valeur brute lue    |
| Relais                | D5 (GPIO14) | Contrôle de l'électrovanne             | Piloté par l'ESP8266, LOW = Ouvert     |
| ESP8266 (NodeMCU)     | N/A         | Microcontrôleur principal              | Connecté au Wi-Fi, exécute le code     |

## 3. Configuration des Pins

```cpp
#define DHT_PIN 12   // D6
#define DHT_TYPE DHT11
#define SOIL_MOISTURE_PIN A0
#define RELAY_PIN 14  // D5
```

## 4. Bibliothèques

*   **ESP8266WiFi.h**: Gestion de la connexion Wi-Fi.
*   **DHT.h**: Lecture des données du capteur DHT.
*   **Firebase_ESP_Client.h**: Communication avec la base de données Firebase.
*   **time\_utils.h**: Utilitaires pour la gestion du temps et des timestamps (NTP).
*   **secrets.h**: Contient les identifiants Wi-Fi et Firebase (à créer par l'utilisateur).
*   **Array\_Utils.h**: Utilitaires pour la manipulation de tableaux (non utilisé directement dans le code fourni).
*   **addons/TokenHelper.h**: Aide à la gestion des tokens d'authentification Firebase.
*   **addons/RTDBHelper.h**: Aide à la gestion des requêtes Realtime Database Firebase.

## 5. Logique du Code

*   **`setup()`**:
    1.  Initialisation de la communication série (`Serial.begin`).
    2.  Initialisation du capteur DHT (`dht.begin`).
    3.  Configuration des broches pour le relais (sortie) et le capteur d'humidité du sol (entrée).
    4.  Connexion au réseau Wi-Fi en utilisant les identifiants de `secrets.h`.
    5.  Si la connexion Wi-Fi réussit :
        *   Initialisation de l'heure système via NTP (`initializeTimestamp`).
        *   Initialisation de la connexion Firebase (`initializeFirebase`).
        *   Initialisation des tableaux d'historique avec des timestamps.
    6.  Si la connexion Wi-Fi échoue, redémarrage de l'ESP.

*   **`loop()`**:
    1.  Vérification périodique de la connexion Wi-Fi (`checkWiFiConnection`).
    2.  Si le Wi-Fi est connecté mais Firebase n'est pas prêt, réinitialisation de Firebase (`initializeFirebase`).
    3.  Vérification périodique de la connexion au stream Firebase (`checkStreamConnection`).
    4.  Gestion du changement d'état de l'électrovanne (`handleRelayStateChange`).
    5.  Si l'intervalle de mesure est atteint :
        *   Lecture des mesures des capteurs (`readMeasures`).
        *   Mise à jour de la base de données Firebase (`updateDataBase`).
    6.  Lecture continue du stream Firebase pour maintenir la connexion et recevoir les mises à jour (`Firebase.RTDB.readStream`).
    7.  Petit délai (`delay(50)`) pour améliorer la réactivité.

*   **Fonctions Critiques**:
    *   `initializeFirebase()`: Configure et établit la connexion avec Firebase.
    *   `startFirebaseStream()`: Démarre l'écoute des changements sur la base de données Firebase.
    *   `streamCallback()`: Traite les données reçues via le stream Firebase, notamment les changements d'état de l'électrovanne.
    *   `checkStreamConnection()`: Vérifie et relance le stream si nécessaire.
    *   `readMeasures()`: Lit les données des capteurs DHT et d'humidité du sol.
    *   `updateDataBase()`: Envoie les mesures actuelles et l'historique vers Firebase.
    *   `handleRelayStateChange()`: Applique les changements d'état de l'électrovanne demandés via le stream.

## 6. Schema de Cablage

```mermaid
flowchart TD
    A[ESP8266] -->|GPIO12 (D6)| B(DHT Sensor Data)
    A -->|A0| C(Soil Moisture Sensor Analog Output)
    A -->|GPIO14 (D5)| D(Relay Module Input)
    D -->|Control Signal| E[Relay]
    E -->|Switched Output| F(Electrovanne)
    G[Alimentation 5V] --> H(Relay Module Power)
    G --> I(ESP8266 Power)
    J[Alimentation 3.3V] --> K(DHT Sensor VCC)
    J --> L(Soil Moisture Sensor VCC)
    M[GND] --> N(DHT Sensor GND)
    M --> O(Soil Moisture Sensor GND)
    M --> P(Relay Module GND)
    M --> Q(ESP8266 GND)
```

## 7. Installation

1.  **Configuration de l'IDE**: Assurez-vous d'avoir PlatformIO installé dans votre éditeur de code (VS Code recommandé) ou l'IDE Arduino avec le support ESP8266.
2.  **Installation des Bibliothèques**:
    *   Via PlatformIO : La bibliothèque `Firebase Arduino Client Library for ESP8266 and ESP32` est spécifiée dans `platformio.ini`. PlatformIO l'installera automatiquement.
    *   Via IDE Arduino : Installez manuellement les bibliothèques : "ESP8266WiFi", "DHT sensor library", "Firebase Arduino Client Library for ESP8266 and ESP32", et potentiellement les bibliothèques pour la gestion du temps si elles ne sont pas incluses par défaut.
3.  **Configuration des Identifiants**: Créez un fichier `secrets.h` à la racine du projet (ou dans `src/` selon la structure) avec le contenu suivant :
    ```cpp
    #ifndef SECRETS_H
    #define SECRETS_H

    // WiFi
    const char* WIFI_SSID = "VOTRE_SSID_WIFI";
    const char* WIFI_PASSWORD = "VOTRE_MOT_DE_PASSE_WIFI";

    // Firebase
    const char* API_KEY = "VOTRE_API_KEY";
    const char* DATABASE_URL = "VOTRE_DATABASE_URL"; // Ex: "https://votre-projet-id.firebaseio.com/"
    const char* USER_EMAIL = "votre_email@example.com";
    const char* USER_PASSWORD = "votre_mot_de_passe_firebase";

    #endif
    ```
    Remplacez les valeurs par vos propres identifiants. Assurez-vous que votre projet Firebase est configuré pour l'authentification par email/mot de passe et que les règles de sécurité de la Realtime Database autorisent les écritures/lectures pour ces identifiants.
4.  **Compilation et Upload**: Compilez le projet et téléversez-le sur votre carte ESP8266.

## 8. Tests et Depannage

*   **Connexion Série**: Ouvrez le moniteur série (à 115200 bauds) pour observer les messages de débogage, les statuts de connexion Wi-Fi et Firebase, ainsi que les valeurs des capteurs.
*   **État du Wi-Fi**: Vérifiez que l'ESP8266 se connecte correctement à votre réseau Wi-Fi. Les messages "WiFi connecté!" et l'adresse IP attribuée sont de bons indicateurs.
*   **État Firebase**: Assurez-vous que Firebase s'initialise correctement. Les messages "Firebase initialisé avec succès" et le démarrage du stream sont cruciaux.
*   **Fonctionnement des Capteurs**:
    *   Le capteur DHT doit afficher des valeurs de température et d'humidité de l'air raisonnables.
    *   Le capteur d'humidité du sol doit afficher une valeur entre 0 et 100%. **Bug Avertissement 1**: La fonction `map` est utilisée pour convertir la lecture analogique brute (0-1023) en pourcentage (0-100). Si la plage de lecture brute du capteur est différente (par exemple, si le capteur ne descend pas à 0 ou ne monte pas à 1023), la mise à l'échelle pourrait être inexacte. Il est recommandé de calibrer ces valeurs min/max.
*   **Contrôle de l'Électrovanne**:
    *   Vérifiez que l'électrovanne s'active (clic du relais) lorsque l'état change dans Firebase. **Bug Critique 1**: Le code utilise `digitalWrite(RELAY_PIN, RELAY_STATE ? LOW : HIGH);`. Cela signifie que `RELAY_STATE = true` (correspondant à "OUVERT" dans le code) active le relais en mettant la broche à `LOW`. Si le module relais attend un signal `HIGH` pour s'activer, ou si l'électrovanne s'active avec un signal `HIGH`, le comportement sera inversé. Il faut s'assurer de la logique de commande du module relais et de l'électrovanne.
    *   L'état initial de l'électrovanne est défini à `HIGH` (fermé) dans `setup()`.
*   **Stream Firebase**:
    *   Vérifiez que les données sont bien envoyées à Firebase et que les mises à jour de l'état de l'électrovanne depuis Firebase sont bien reçues et traitées par le `streamCallback`.
    *   **Bug Avertissement 2**: Le code tente de récupérer l'état initial de l'électrovanne (`etat_electrovanne`) si le début du stream échoue. Cependant, cette récupération se fait via `Firebase.RTDB.getBool`, qui peut être bloquante. Si Firebase n'est pas accessible, cela pourrait retarder le démarrage du reste du code. Une gestion d'erreur plus robuste pour la récupération de l'état initial pourrait être nécessaire.
*   **Stabilité**: Surveillez les redémarrages inattendus de l'ESP8266. Des problèmes de connexion Wi-Fi, d'épuisement de la mémoire ou des erreurs Firebase non gérées peuvent en être la cause. La réduction de `MEASURE_INTERVAL` et `STREAM_CHECK_INTERVAL` peut augmenter la charge sur l'ESP et Firebase.

---

## 🛒 Liste de Courses

| Composant | Prix (USD) | Liens d'Achat | Alternatives |
|-----------|-------------|----------------|---------------|
| **DHT22** | **$10.98** | [PiShop.us ($9.95)](https://www.pishop.us/product/dht22-am2302-digital-temperature-humidity-sensor-for-arduino/) • [Starfall Labs ($12.00)](https://starfalllabs.com/products/dht22-temperature-humidity-sensor) | DHT11, BME280 |
| **ESP32** | **$10.00** | [Mouser ($8.76)](https://www.mouser.com/ProductDetail/Espressif-Systems/ESP32-DevKitC-VE?qs=sGAEpiMZZMukxY%252B0R9bX41xU2p69H012) • [Newegg ($5.99)](https://www.newegg.com/p/354-009N-00096) | ESP32-S2, ESP32-C3 |
| **ESP8266** | **$9.04** | [Mouser ($8.08)](https://www.mouser.com/ProductDetail/Espressif-Systems/ESP8266-DevKitS?qs=sGAEpiMZZMukxY%252B0R9bX41eY0oN03L021) • [Arduino Store ($10.00)](https://store.arduino.cc/products/nodemcu-esp8266) | ESP32, ESP8266-01 |
| **Relay Module (1-Channel 5V)** | **$2.73** | [Goliath Automation & Robotics ($2.50)](https://goliathautomation.com/products/5v-1-channel-relay-module-for-arduino) • [PiShop.us ($2.95)](https://www.pishop.us/product/1-channel-relay-module-for-arduino-raspberry-pi-keyes/) | 2-Channel Relay Module, Solid State Relay (SSR) |

**💰 Total Estimate: ~$32.75**