```markdown
# Agrisense - Partie IoT

## Vue d'ensemble

Ce projet vise à créer un système de surveillance et de contrôle automatisé pour l'agriculture, spécifiquement axé sur la gestion de l'irrigation et le suivi des conditions environnementales. Il utilise un microcontrôleur ESP8266 pour collecter des données de capteurs (température, humidité de l'air, humidité du sol) et interagir avec une électrovanne. Les données sont envoyées en temps réel vers une base de données Firebase pour visualisation et contrôle à distance.

*   **Objectif :** Système de surveillance et de contrôle automatisé pour l'agriculture.
*   **Fonctionnalités principales :**
    *   Lecture des données de température et d'humidité de l'air via un capteur DHT11.
    *   Lecture de l'humidité du sol via un capteur analogique.
    *   Contrôle d'une électrovanne (ouverture/fermeture) via un relais.
    *   Connexion au réseau Wi-Fi.
    *   Communication avec Firebase Realtime Database pour :
        *   Envoyer les mesures périodiques.
        *   Recevoir des commandes pour l'état de l'électrovanne (streaming).
        *   Stocker un historique des mesures.
    *   Synchronisation horaire via NTP.
    *   Gestion de la reconnexion Wi-Fi et Firebase.
    *   Mode de test pour simuler des données.
*   **Architecture globale :**
    *   **Hardware :** Microcontrôleur ESP8266 (module ESP-12E), capteur DHT11, capteur d'humidité du sol analogique, module relais, électrovanne.
    *   **Software :** Firmware embarqué sur l'ESP8266 utilisant le framework Arduino, avec des bibliothèques pour le Wi-Fi, Firebase, la gestion du temps et les capteurs DHT. Communication via MQTT (implicitement géré par la bibliothèque Firebase) vers Firebase Realtime Database.

## Composants Hardware

| Composant             | Pin ESP8266 | Fonction                               | Notes                                      |
| :-------------------- | :---------- | :------------------------------------- | :----------------------------------------- |
| ESP8266 (ESP-12E)     | N/A         | Microcontrôleur principal              |                                            |
| Capteur DHT11         | D6 (GPIO12) | Mesure température et humidité de l'air | Nécessite une résistance pull-up (souvent intégrée) |
| Capteur humidité sol  | A0          | Mesure humidité du sol (analogique)    | Lecture brute à convertir/mapper           |
| Module Relais         | D5 (GPIO14) | Commande de l'électrovanne             | Piloté par un signal GPIO, LOW active l'électrovanne |
| Électrovanne          | N/A         | Contrôle du flux d'eau                 | Pilotée par le relais                      |
| Alimentation          | N/A         | Alimentation des composants            | ESP8266 (5V/3.3V), Relais (5V), Capteurs (3.3V/5V) |

## Configuration des Pins (Code Source)

Les définitions des pins sont situées au début du fichier `src/main.cpp`.

```cpp
#define DHT_PIN 12   // Connecté au GPIO12 (D6 sur certaines cartes de développement)
#define DHT_TYPE DHT11 // Type de capteur DHT
#define SOIL_MOISTURE_PIN A0 // Broche analogique pour le capteur d'humidité du sol
#define RELAY_PIN 14  // Connecté au GPIO14 (D5 sur certaines cartes de développement)
```

*   **`DHT_PIN` (GPIO12 / D6) :** Connecté à la broche de données du capteur DHT11.
*   **`SOIL_MOISTURE_PIN` (A0) :** Connecté à la sortie analogique du capteur d'humidité du sol.
*   **`RELAY_PIN` (GPIO14 / D5) :** Connecté à la broche de contrôle du module relais. Le code active le relais avec `LOW` et le désactive avec `HIGH`, ce qui correspond à une logique "LOW active" pour le relais.

## Bibliothèques et Dépendances

Les bibliothèques nécessaires sont incluses via `#include` dans `src/main.cpp` et listées dans `platformio.ini`.

*   **`ESP8266WiFi.h` :** Pour la gestion de la connexion Wi-Fi.
*   **`DHT.h` :** Pour interagir avec le capteur DHT11/DHT22.
*   **`Firebase_ESP_Client.h` :** Bibliothèque principale pour la communication avec Firebase.
    *   `addons/TokenHelper.h` : Utilitaire pour la gestion des tokens d'authentification Firebase.
    *   `addons/RTDBHelper.h` : Utilitaires pour les opérations sur la Realtime Database.
*   **`time_utils.h` :** Bibliothèque personnalisée (non fournie dans l'extrait) pour la synchronisation horaire NTP et la génération de timestamps ISO UTC.
*   **`secrets.h` :** Fichier personnalisé (non fourni) contenant les identifiants Wi-Fi (SSID, mot de passe) et les informations d'authentification Firebase (API Key, URL, email, mot de passe utilisateur). **Il est crucial de ne pas commiter ce fichier sur un dépôt public.**
*   **`Array_Utils.h` :** Bibliothèque personnalisée (non fournie) pour la manipulation de tableaux.
*   **`FirebaseJson.h` :** Pour construire et manipuler des objets JSON pour les requêtes Firebase.

**Dépendance dans `platformio.ini` :**
*   `Firebase Arduino Client Library for ESP8266 and ESP32@4.4.17`

## Logique du Code Principal

*   **`setup()` :**
    1.  Initialisation de la communication série (`Serial.begin`).
    2.  Initialisation du capteur DHT (`dht.begin()`).
    3.  Configuration des pins GPIO : `RELAY_PIN` en sortie (initialisé à `HIGH` - électrovanne fermée), `SOIL_MOISTURE_PIN` en entrée.
    4.  Tentative de connexion au réseau Wi-Fi en utilisant les identifiants de `secrets.h`.
    5.  Si la connexion Wi-Fi réussit :
        *   Initialisation de l'heure système via NTP (`initializeTimestamp()`).
        *   Initialisation de la connexion Firebase (`initializeFirebase()`).
        *   Initialisation des tableaux d'historique avec des timestamps.
    6.  Si la connexion Wi-Fi échoue après plusieurs tentatives, redémarrage du module.

*   **`loop()` :**
    1.  **Vérification de la connexion Wi-Fi (`checkWiFiConnection()`):** Vérifie périodiquement si le Wi-Fi est toujours connecté, tente de reconnecter si nécessaire.
    2.  **Réinitialisation Firebase :** Si le Wi-Fi est connecté mais Firebase n'est pas prêt, tente de réinitialiser la connexion Firebase.
    3.  **Vérification du Stream Firebase (`checkStreamConnection()`):** Vérifie périodiquement si le stream Firebase est actif et tente de le redémarrer si nécessaire.
    4.  **Gestion de l'électrovanne (`handleRelayStateChange()`):** Met à jour l'état physique du relais si un changement a été détecté via le stream Firebase.
    5.  **Prise de mesures (`readMeasures()`):** Toutes les `MEASURE_INTERVAL` millisecondes (5 secondes), lit les données des capteurs DHT et d'humidité du sol.
    6.  **Mise à jour de la base de données (`updateDataBase()`):** Si Firebase est prêt, envoie les mesures actuelles et l'état de l'électrovanne à Firebase. Met également à jour l'historique toutes les 30 secondes.
    7.  **Lecture continue du Stream (`Firebase.RTDB.readStream()`):** Essentiel pour maintenir la connexion du stream et recevoir les mises à jour en temps réel. Gère les erreurs de token (401).
    8.  Petit délai (`delay(50)`) pour améliorer la réactivité.

*   **Fonctions critiques :**
    *   **`initializeFirebase()` :** Gère la configuration et l'authentification avec Firebase.
    *   **`startFirebaseStream()` :** Démarre l'écoute des changements sur la base de données Firebase, spécifiquement pour l'état de l'électrovanne.
    *   **`streamCallback()` :** Fonction appelée lorsqu'une donnée est reçue via le stream. Met à jour l'état `RELAY_STATE` si le chemin `/etat_electrovanne` change.
    *   **`readMeasures()` :** Collecte les données brutes des capteurs.
    *   **`updateDataBase()` :** Envoie les données mesurées et l'historique vers Firebase.
    *   **`handleRelayStateChange()` :** Applique les changements d'état de l'électrovanne au matériel.
    *   **`checkWiFiConnection()` / `reconnectWiFi()` :** Assure la résilience de la connexion réseau.
    *   **`checkStreamConnection()` :** Assure la résilience de la connexion au stream Firebase.

## Schéma de Câblage (Mermaid)

```mermaid
flowchart TD
    subgraph ESP8266_Module["ESP8266 (ESP-12E)"]
        GPIO12["GPIO12 (D6)"]
        GPIO14["GPIO14 (D5)"]
        A0["Pin A0"]
        VCC["3.3V / 5V"]
        GND["GND"]
    end

    subgraph Capteurs
        DHT11["DHT11"]
        SoilSensor["Capteur Humidité Sol"]
    end

    subgraph Actionneurs
        RelayModule["Module Relais"]
        SolenoidValve["Électrovanne"]
    end

    subgraph Alimentation
        PowerSupply["Alimentation Externe"]
    end

    subgraph Cloud
        Firebase["Firebase Realtime DB"]
    end

    %% Connexions Capteurs
    GPIO12 --> DHT11
    DHT11 -- "Data" --> GPIO12
    A0 --> SoilSensor
    SoilSensor -- "Analog Out" --> A0

    %% Connexions Actionneurs
    GPIO14 -- "Signal IN" --> RelayModule
    RelayModule -- "NO/NC" --> SolenoidValve

    %% Alimentations
    PowerSupply -- "5V" --> RelayModule
    PowerSupply -- "VCC" --> SolenoidValve
    ESP8266_Module -- "3.3V/5V" --> VCC
    ESP8266_Module -- "GND" --> GND
    DHT11 -- "VCC" --> VCC
    DHT11 -- "GND" --> GND
    SoilSensor -- "VCC" --> VCC
    SoilSensor -- "GND" --> GND
    RelayModule -- "VCC" --> VCC
    RelayModule -- "GND" --> GND

    %% Connexion Réseau
    ESP8266_Module -- "Wi-Fi" --> Firebase

    %% Notes
    note right of RelayModule : Le relais contrôle l'alimentation de l'électrovanne.
    note right of ESP8266_Module : La broche D5 (GPIO14) contrôle le relais.
    note right of DHT11 : La broche D6 (GPIO12) est pour les données du DHT11.
```

## Procédure d'Installation

1.  **Configuration IDE :**
    *   Utiliser **PlatformIO** avec VS Code est recommandé (comme indiqué dans `platformio.ini`).
    *   Alternativement, utiliser l'IDE Arduino.
2.  **Installation des bibliothèques :**
    *   **Avec PlatformIO :** La bibliothèque `Firebase Arduino Client Library for ESP8266 and ESP32` sera automatiquement téléchargée et gérée lors de la compilation si elle est correctement listée dans `platformio.ini`. Les autres bibliothèques (`ESP8266WiFi.h`, `DHT.h`) sont généralement incluses dans le SDK ESP8266 du framework Arduino.
    *   **Avec l'IDE Arduino :**
        *   Installer le gestionnaire de cartes pour ESP8266 : Aller dans `Fichier > Préférences`, ajouter `http://arduino.esp8266.com/stable/package_esp8266com_index.json` dans "URL supplémentaires pour le gestionnaire de cartes". Installer ensuite le paquet "esp8266" via le Gestionnaire de cartes (`Outils > Type de carte > Gestionnaire de cartes...`).
        *   Installer la bibliothèque `DHT sensor library` par Adafruit via le Gestionnaire de bibliothèques (`Croquis > Inclure une bibliothèque > Gérer les bibliothèques...`).
        *   Installer la bibliothèque `Firebase Arduino Client Library for ESP8266 and ESP32` par Firebase. Il est souvent préférable de la cloner depuis GitHub ou de l'ajouter manuellement si le gestionnaire ne la trouve pas facilement.
        *   Les bibliothèques `time_utils.h`, `secrets.h`, et `Array_Utils.h` sont des fichiers personnalisés. Vous devrez les créer dans le dossier de votre projet (ou dans le dossier `libraries` de l'IDE Arduino) et copier le code source correspondant.
3.  **Configuration des Identifiants :**
    *   Créez un fichier nommé `secrets.h` à la racine de votre projet (ou dans le même répertoire que `main.cpp` si vous n'utilisez pas de structure de projet PlatformIO spécifique).
    *   Ajoutez-y vos identifiants Wi-Fi et Firebase :
        ```c++
        #ifndef SECRETS_H
        #define SECRETS_H

        // Wi-Fi Credentials
        const char* WIFI_SSID = "VOTRE_SSID_WIFI";
        const char* WIFI_PASSWORD = "VOTRE_MOT_DE_PASSE_WIFI";

        // Firebase Credentials
        const char* API_KEY = "VOTRE_API_KEY";
        const char* DATABASE_URL = "https://VOTRE_PROJET_ID.firebaseio.com"; // Sans le / at end
        const char* USER_EMAIL = "votre_email@example.com";
        const char* USER_PASSWORD = "votre_mot_de_passe_firebase";

        #endif
        ```
    *   **IMPORTANT :** Assurez-vous que le fichier `secrets.h` est ajouté à votre fichier `.gitignore` pour éviter de le commiter accidentellement sur GitHub.
    *   Configurez votre projet Firebase : Créez un projet sur la console Firebase, activez la Realtime Database, et configurez l'authentification (Email/Password). Récupérez votre API Key et l'URL de la base de données.
4.  **Compilation et Upload :**
    *   **Avec PlatformIO :** Utilisez les commandes `pio run` pour compiler et `pio run -t upload` pour téléverser le code sur votre ESP8266. `pio run -t monitor` pour ouvrir le moniteur série.
    *   **Avec l'IDE Arduino :** Sélectionnez la bonne carte (`ESP8266 Boards > NodeMCU 1.0 (ESP-12E Module)` ou similaire), le bon port COM, puis cliquez sur "Téléverser". Ouvrez le moniteur série (`Outils > Moniteur série`) à la vitesse de 115200 bauds.

## Tests et Dépannage

*   **Points de contrôle Hardware :**
    *   Vérifiez que toutes les connexions sont correctes et sécurisées.
    *   Assurez-vous que l'alimentation est suffisante pour tous les composants, en particulier le module relais et l'électrovanne.
    *   Testez le relais séparément pour confirmer son bon fonctionnement.
    *   Vérifiez que le capteur DHT est correctement câblé (VCC, GND, Data).
    *   Testez le capteur d'humidité du sol avec de l'eau pour voir si la valeur analogique change.
*   **Vérifications Serial Monitor :**
    *   Le moniteur série est votre outil principal de diagnostic. Surveillez les messages `[DEBUG]` pour suivre le déroulement du programme.
    *   Vérifiez les messages de connexion Wi-Fi et Firebase.
    *   Examinez les erreurs retournées par Firebase (`fbdo.errorReason()`).
    *   Observez les valeurs des capteurs lors de la prise de mesures.
*   **Erreurs courantes et solutions :**
    *   **Échec de connexion Wi-Fi :** Vérifiez `WIFI_SSID` et `WIFI_PASSWORD` dans `secrets.h`. Assurez-vous que le réseau est accessible. Redémarrez le module ESP8266.
    *   **Échec d'initialisation Firebase :** Vérifiez `API_KEY`, `DATABASE_URL`, `USER_EMAIL`, `USER_PASSWORD`. Assurez-vous que les règles de sécurité de votre base de données Firebase autorisent l'accès (au moins en mode test initialement). Vérifiez que le token d'authentification est correctement géré. Redémarrez le module.
    *   **Le relais ne s'active pas :** Vérifiez la logique `digitalWrite(RELAY_PIN, RELAY_STATE ? LOW : HIGH);`. Si votre relais est "HIGH active", inversez la logique. Vérifiez le câblage du relais. Assurez-vous que `RELAY_STATE` est correctement mis à jour via le stream.
    *   **Données de capteurs incohérentes :** Vérifiez le câblage. Pour l'humidité du sol, la fonction `map()` pourrait nécessiter un ajustement des valeurs min/max lues (`constrain`) en fonction de votre capteur et du milieu (terre sèche vs. eau).
    *   **Perte de connexion Stream :** Le code inclut des mécanismes de vérification (`checkStreamConnection`) et de redémarrage du stream. Si le problème persiste, vérifiez la stabilité de votre connexion Wi-Fi et la configuration de Firebase. Une réinitialisation complète de Firebase (`initializeFirebase()`) est tentée en cas d'erreurs de token (HTTP 401).
    *   **Heure système incorrecte :** Assurez-vous que la fonction `initTime()` et `waitForNTP()` dans `time_utils.h` fonctionnent correctement et que l'ESP8266 peut atteindre les serveurs NTP.
```