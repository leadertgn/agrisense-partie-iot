## 1. Vue d'ensemble

Ce projet vise à créer un système IoT pour la surveillance et le contrôle agricole. Il utilise un microcontrôleur ESP8266 pour collecter des données environnementales (température, humidité de l'air, humidité du sol) via des capteurs, les envoyer à une base de données Firebase en temps réel, et contrôler une électrovanne (relais) à distance via Firebase. L'architecture repose sur une connexion Wi-Fi pour la communication avec Firebase, permettant une gestion à distance des cultures.

## 2. Composants Hardware

| Composant          | Pin        | Fonction                                   | Notes                                                              |
| :----------------- | :--------- | :----------------------------------------- | :----------------------------------------------------------------- |
| DHT11              | D6 (GPIO12) | Mesure la température et l'humidité de l'air | Capteur numérique.                                                 |
| Capteur Humidité Sol | A0         | Mesure l'humidité du sol                   | Capteur analogique.                                                |
| Module Relais      | D5 (GPIO14) | Contrôle l'électrovanne (arrosage)         | Active/désactive une charge externe (électrovanne).                |
| ESP8266            | -          | Microcontrôleur principal                  | Gère les capteurs, la communication Wi-Fi et Firebase.             |

## 3. Configuration des Pins

```cpp
#define DHT_PIN 12   // D6
#define DHT_TYPE DHT11
#define SOIL_MOISTURE_PIN A0
#define RELAY_PIN 14  // D5
```

## 4. Bibliothèques

*   `ESP8266WiFi.h`: Gère la connexion Wi-Fi de l'ESP8266.
*   `DHT.h`: Interface avec le capteur de température et d'humidité DHT11.
*   `Firebase_ESP_Client.h`: Permet la communication avec la base de données Firebase Realtime Database.
*   `time_utils.h`: Fournit des utilitaires pour la gestion du temps et des horodatages (non fourni, mais déduit).
*   `secrets.h`: Contient les informations d'identification sensibles (Wi-Fi, Firebase) (non fourni, mais déduit).
*   `Array_Utils.h`: Fournit des utilitaires pour la manipulation des tableaux (non fourni, mais déduit).
*   `addons/TokenHelper.h`: Aide à la gestion des tokens d'authentification Firebase.
*   `addons/RTDBHelper.h`: Fournit des fonctions d'aide pour la Realtime Database de Firebase.

## 5. Logique du Code

*   **`setup()`**:
    *   Initialise la communication série à 115200 bauds.
    *   Configure les broches du DHT, du relais (initialisé à `HIGH` pour être fermé par défaut), et du capteur d'humidité du sol.
    *   Tente de se connecter au réseau Wi-Fi défini dans `secrets.h`. En cas d'échec, redémarre l'ESP.
    *   Initialise le temps (NTP) et Firebase après une connexion Wi-Fi réussie.
    *   Initialise les tableaux d'historique avec l'horodatage actuel.

*   **`loop()`**:
    *   Vérifie et gère la connexion Wi-Fi via `checkWiFiConnection()`.
    *   Si la connexion Firebase est perdue, tente de la réinitialiser.
    *   Vérifie et maintient la connexion au stream Firebase via `checkStreamConnection()` toutes les `STREAM_CHECK_INTERVAL` (5 secondes).
    *   Appelle `handleRelayStateChange()` pour actualiser l'état du relais si une modification a été reçue via Firebase.
    *   Toutes les `MEASURE_INTERVAL` (5 secondes), lit les capteurs via `readMeasures()` et met à jour Firebase via `updateDataBase()`.
    *   Lit en continu le stream Firebase pour recevoir les commandes en temps réel.

*   **Fonctions critiques**:
    *   **`initializeFirebase()`**: Configure les informations d'authentification et de base de données Firebase, puis initialise la connexion. Tente de démarrer le stream Firebase après une initialisation réussie.
    *   **`startFirebaseStream()`**: Établit une connexion de streaming avec Firebase sur le chemin `BASE_URL/dernieres_mesures`. Configure les callbacks pour les données reçues (`streamCallback`) et les timeouts (`streamTimeoutCallback`). En cas d'échec, tente de récupérer l'état initial de l'électrovanne.
    *   **`readMeasures(Measure* measures)`**: Lit la température et l'humidité de l'air du DHT11, et l'humidité du sol via la broche analogique A0. Convertit la lecture analogique de l'humidité du sol en pourcentage.
    *   **`updateDataBase(Measure* measures, int taille)`**: Envoie les dernières mesures (température, humidité air/sol, état de l'électrovanne et timestamp) à Firebase sous le chemin `BASE_URL/dernieres_mesures`. Met également à jour un historique des mesures toutes les 30 secondes sous `BASE_URL/historiques`.
    *   **`streamCallback(FirebaseStream data)`**: Est appelée lorsqu'une nouvelle donnée est reçue via le stream Firebase. Si le chemin de la donnée est `/etat_electrovanne` et que le type est booléen, elle met à jour l'état local du relais (`RELAY_STATE`) et active le drapeau `RELAY_STATE_CHANGED`.
    *   **`handleRelayStateChange()`**: Vérifie si `RELAY_STATE_CHANGED` est vrai. Si oui, met à jour l'état physique du relais (`digitalWrite`) et réinitialise le drapeau.

## 6. Schéma de Câblage

```mermaid
flowchart TD
    ESP8266["ESP8266 (NodeMCU)"]

    DHT11["Capteur DHT11"]
    SOIL_MOISTURE["Capteur Humidité Sol"]
    RELAY_MODULE["Module Relais"]
    ELECTROVANNE["Électrovanne"]

    ESP8266 -->|D6 (GPIO12)| DHT11
    ESP8266 -->|A0| SOIL_MOISTURE
    ESP8266 -->|D5 (GPIO14)| RELAY_MODULE
    RELAY_MODULE --> ELECTROVANNE
```

## 7. Installation

1.  **Prérequis PlatformIO**: Assurez-vous d'avoir PlatformIO IDE installé (VS Code extension ou CLI).
2.  **Cloner le dépôt**:
    ```bash
    git clone https://github.com/leadertgn/agrisense-partie-iot.git
    cd agrisense-partie-iot
    ```
3.  **Fichier `secrets.h`**: Créez un fichier nommé `secrets.h` dans le dossier `src/` avec le contenu suivant, en remplaçant les placeholders par vos propres informations d'identification Firebase et Wi-Fi :
    ```cpp
    #ifndef SECRETS_H
    #define SECRETS_H

    #define WIFI_SSID "VOTRE_SSID_WIFI"
    #define WIFI_PASSWORD "VOTRE_MOT_DE_PASSE_WIFI"

    #define API_KEY "VOTRE_CLE_API_FIREBASE"
    #define DATABASE_URL "VOTRE_URL_BASE_DE_DONNEES_FIREBASE"
    #define USER_EMAIL "VOTRE_EMAIL_UTILISATEUR_FIREBASE"
    #define USER_PASSWORD "VOTRE_MOT_DE_PASSE_UTILISATEUR_FIREBASE"

    #endif
    ```
4.  **Bibliothèques**: PlatformIO gérera automatiquement les dépendances listées dans `platformio.ini`. Assurez-vous que la bibliothèque "Firebase Arduino Client Library for ESP8266 and ESP32" est installée (version 4.4.17 ou compatible).
5.  **Compilation et Téléchargement**: Connectez votre ESP8266 à votre ordinateur et utilisez PlatformIO pour compiler et téléverser le code sur la carte.

## 8. Tests et Dépannage

*   **Bug critique 1 : Utilisation de DHT11 avec `DHT_TYPE DHT22` dans le code.**
    *   **Description**: Le code utilise `#define DHT_TYPE DHT11` mais la détection automatique a identifié un DHT22. Si un DHT22 est réellement utilisé, le type de capteur est incorrectement défini, ce qui peut entraîner des lectures erronées ou des échecs de lecture.
    *   **Solution**: Vérifiez physiquement le capteur DHT utilisé. Si c'est un DHT22, changez `#define DHT_TYPE DHT11` en `#define DHT_TYPE DHT22` dans `src/main.cpp`. Si c'est bien un DHT11, ignorez cet avertissement.

*   **Avertissement 1 : Problème de compatibilité entre ESP8266 et ESP32.**
    *   **Description**: Le code est spécifiquement écrit pour ESP8266 (`ESP8266WiFi.h`, `platform = espressif8266`, `board = esp12e`). La détection a mentionné ESP32, ce qui pourrait prêter à confusion.
    *   **Solution**: Assurez-vous d'utiliser une carte ESP8266 (comme un NodeMCU ESP-12E) pour ce projet. Le code n'est pas directement compatible avec un ESP32 sans modifications (notamment pour les bibliothèques Wi-Fi et certaines configurations de broches).

*   **Avertissement 2 : `time_utils.h` et `Array_Utils.h` non fournis.**
    *   **Description**: Le code inclut des fichiers d'en-tête `time_utils.h` et `Array_Utils.h` qui ne sont pas présents dans le dépôt fourni. Cela entraînera des erreurs de compilation.
    *   **Solution**: Vous devez créer ces fichiers et y implémenter les fonctions manquantes (`initTime()`, `waitForNTP()`, `getIsoUtcTime()`, et toute fonction d'utilitaire de tableau si nécessaire). Pour `time_utils.h`, une implémentation courante pour ESP8266 implique l'utilisation de `NTPClient` ou `sntp` pour synchroniser l'heure et formater les timestamps.

*   **Connexion Wi-Fi instable**:
    *   **Symptôme**: L'ESP8266 se déconnecte fréquemment du Wi-Fi ou ne parvient pas à se connecter.
    *   **Vérification**: Vérifiez le `WIFI_SSID` et `WIFI_PASSWORD` dans `secrets.h`. Assurez-vous que le signal Wi-Fi est fort à l'emplacement de l'appareil. Redémarrez votre routeur Wi-Fi.

*   **Problèmes de connexion Firebase**:
    *   **Symptôme**: Les données ne sont pas envoyées à Firebase, ou le stream ne fonctionne pas.
    *   **Vérification**: Vérifiez `API_KEY`, `DATABASE_URL`, `USER_EMAIL`, et `USER_PASSWORD` dans `secrets.h`. Assurez-vous que les règles de sécurité de votre base de données Firebase permettent l'accès en lecture/écriture pour l'utilisateur configuré. Vérifiez la console série pour les messages d'erreur Firebase.

*   **Lectures de capteurs incorrectes**:
    *   **Symptôme**: Température, humidité de l'air ou du sol affichées comme `-1` ou valeurs irréalistes.
    *   **Vérification**: Vérifiez le câblage du DHT11 et du capteur d'humidité du sol. Assurez-vous que le DHT11 est correctement alimenté et que la broche de données est connectée à `DHT_PIN` (D6). Pour le capteur d'humidité du sol, assurez-vous qu'il est correctement alimenté et que sa sortie analogique est connectée à `SOIL_MOISTURE_PIN` (A0).

*   **Relais ne s'active pas**:
    *   **Symptôme**: L'électrovanne ne s'ouvre/ferme pas lorsque l'état est modifié dans Firebase.
    *   **Vérification**: Vérifiez le câblage du module relais à la broche `RELAY_PIN` (D5) et son alimentation. Assurez-vous que le module relais est un type compatible avec l'ESP8266 (souvent actif à l'état bas, comme configuré par `digitalWrite(RELAY_PIN, RELAY_STATE ? LOW : HIGH)`). Vérifiez que l'électrovanne est correctement connectée au module relais et qu'elle reçoit l'alimentation nécessaire.

## 9. Shopping List


```PYTHON
concise_search("prix DHT11", max_num_results=1)
concise_search("prix capteur humidité sol arduino", max_num_results=1)
concise_search("prix module relais 5V arduino", max_num_results=1)
concise_search("prix ESP8266 NodeMCU", max_num_results=1)

```

```
Looking up information on Google Search.

```

```
Looking up information on Google Search.

```

```
Looking up information on Google Search.

```

```
Looking up information on Google Search.

```
Voici une liste des composants nécessaires pour ce projet, avec des estimations de prix basées sur des recherches récentes :

*   **DHT11 (Capteur de température et d'humidité)**: Environ 2€ - 8€ (selon le vendeur et si c'est un module ou le capteur seul).
    *   [Exemple d'achat (eBay)](https://www.ebay.com/sch/i.html?_nkw=Dht11+Dht-11+Digital+Temperature+and+Humidity+Sensor+for+Arduino)
*   **Capteur d'humidité du sol (hygromètre)**: Environ 2€ - 5€
    *   [Exemple d'achat (RS Components)](https://fr.rs-online.com/web/p/kits-et-cartes-compatibles-arduino/1721067)
*   **Module Relais 5V (1 canal)**: Environ 2€ - 6€
    *   [Exemple d'achat (Arduino Store)](https://store.arduino.cc/products/1-relay-module-5-vdc-10a-assembled)
*   **ESP8266 NodeMCU (ESP-12E)**: Environ 3€ - 12€ (selon le modèle et le vendeur)
    *   [Exemple d'achat (AliExpress)](https://www.aliexpress.com/w/wholesale-esp8266-wifi-nodemcu.html)