# Documentation du Projet Agrisense IoT

## 1. Vue d'ensemble

Ce projet vise à automatiser et surveiller un système d'irrigation agricole en utilisant un microcontrôleur ESP8266. Il mesure la température, l'humidité de l'air, l'humidité du sol et contrôle une électrovanne pour l'irrigation. Les données collectées sont envoyées en temps réel à une base de données Firebase, et l'état de l'électrovanne peut être contrôlé à distance via cette même base de données. L'architecture repose sur un ESP8266 connecté au WiFi, communiquant avec Firebase pour la synchronisation des données et le contrôle.

## 2. Composants Hardware

| Composant             | Pin ESP8266 | Fonction                               | Notes                                     |
| :-------------------- | :---------- | :------------------------------------- | :---------------------------------------- |
| Capteur DHT (DHT11)   | D6 (GPIO12) | Mesure température et humidité de l'air | Connecté à la broche numérique D6         |
| Capteur d'humidité sol | A0          | Mesure l'humidité du sol               | Connecté à la broche analogique A0        |
| Relais                | D5 (GPIO14) | Contrôle de l'électrovanne             | Piloté par le GPIO14, LOW = Ouvert, HIGH = Fermé |
| ESP8266 (Module ESP-12E) | N/A         | Microcontrôleur principal              | Connectivité WiFi intégrée                |

## 3. Configuration des Pins

```cpp
#define DHT_PIN 12   // D6
#define DHT_TYPE DHT11
#define SOIL_MOISTURE_PIN A0
#define RELAY_PIN 14  // D5
```

## 4. Bibliothèques

*   `ESP8266WiFi.h`: Gestion de la connexion WiFi.
*   `DHT.h`: Lecture des données du capteur DHT.
*   `Firebase_ESP_Client.h`: Communication avec Firebase Realtime Database.
*   `time_utils.h`: Gestion de l'heure et synchronisation NTP (non fourni dans l'extrait).
*   `secrets.h`: Stockage des identifiants WiFi et Firebase (non fourni dans l'extrait).
*   `Array_Utils.h`: Utilitaires pour la manipulation de tableaux (non fourni dans l'extrait).
*   `addons/TokenHelper.h`: Gestion des tokens d'authentification Firebase.
*   `addons/RTDBHelper.h`: Fonctions spécifiques pour Firebase Realtime Database.

## 5. Logique du Code

*   **`setup()`**:
    *   Initialisation de la communication série.
    *   Initialisation du capteur DHT.
    *   Configuration des broches pour le relais (sortie) et le capteur d'humidité du sol (entrée).
    *   Connexion au réseau WiFi en utilisant les identifiants de `secrets.h`.
    *   Si la connexion WiFi réussit :
        *   Initialisation de l'heure système (`initializeTimestamp()`).
        *   Initialisation de Firebase (`initializeFirebase()`).
        *   Initialisation des tableaux d'historique.
    *   Si la connexion WiFi échoue, redémarrage de l'ESP.

*   **`loop()`**:
    *   Vérification périodique de la connexion WiFi (`checkWiFiConnection()`).
    *   Si le WiFi est connecté mais Firebase n'est pas prêt, réinitialisation de Firebase (`initializeFirebase()`).
    *   Vérification régulière de la connexion au stream Firebase (`checkStreamConnection()`).
    *   Gestion du changement d'état de l'électrovanne (`handleRelayStateChange()`).
    *   Prise de mesures (température, humidité air/sol) toutes les `MEASURE_INTERVAL` millisecondes (`readMeasures()`).
    *   Mise à jour de la base de données Firebase avec les mesures et l'état de l'électrovanne (`updateDataBase()`).
    *   Lecture continue du stream Firebase pour maintenir la connexion et recevoir les mises à jour (`Firebase.RTDB.readStream()`).
    *   Petit délai (`delay(50)`) pour améliorer la réactivité.

*   **Fonctions Critiques**:
    *   `initializeFirebase()`: Configure et démarre la connexion à Firebase.
    *   `startFirebaseStream()`: Démarre le flux de données en temps réel depuis Firebase.
    *   `streamCallback()`: Traite les données reçues via le stream Firebase, notamment les changements d'état de l'électrovanne.
    *   `checkStreamConnection()`: Vérifie et maintient la connexion au stream Firebase.
    *   `updateDataBase()`: Envoie les mesures actuelles et l'historique vers Firebase.
    *   `handleRelayStateChange()`: Applique les changements d'état de l'électrovanne demandés via Firebase.

## 6. Schema de Cablage

```mermaid
flowchart TD
    A[ESP8266] -->|GPIO12 (D6)| B(Capteur DHT11)
    A -->|A0| C(Capteur Humidité Sol)
    A -->|GPIO14 (D5)| D(Module Relais)
    D -->|Commande| E(Électrovanne)
    A -->|WiFi| F(Routeur WiFi)
    F -->|Internet| G(Firebase)
    G -->|Stream/Set| A
```

## 7. Installation

1.  **Configuration IDE**: Assurez-vous d'utiliser PlatformIO avec VS Code ou l'IDE Arduino.
2.  **Installation des Bibliothèques**:
    *   Ouvrez `platformio.ini` et laissez PlatformIO installer les dépendances listées dans `lib_deps`. Pour l'IDE Arduino, installez manuellement les bibliothèques via le gestionnaire de bibliothèques :
        *   `Firebase Arduino Client Library for ESP8266 and ESP32`
        *   `ESP8266WiFi`
        *   `DHT sensor library`
        *   Les bibliothèques `time_utils.h`, `secrets.h`, `Array_Utils.h` doivent être créées et placées dans le répertoire approprié (`src/` ou `include/`).
3.  **Configuration des Identifiants**:
    *   Créez un fichier `secrets.h` dans le répertoire `src/` avec le contenu suivant :
        ```cpp
        #ifndef SECRETS_H
        #define SECRETS_H

        #define WIFI_SSID "VOTRE_SSID_WIFI"
        #define WIFI_PASSWORD "VOTRE_MOT_DE_PASSE_WIFI"

        #define API_KEY "VOTRE_API_KEY_FIREBASE"
        #define DATABASE_URL "https://VOTRE_PROJET_FIREBASE.firebaseio.com/" // Sans le '/' final
        #define USER_EMAIL "votre_email@example.com"
        #define USER_PASSWORD "votre_mot_de_passe_firebase"

        #endif
        ```
    *   Remplacez les placeholders par vos informations réelles de connexion WiFi et Firebase. Assurez-vous que votre projet Firebase est configuré pour permettre l'authentification par email/mot de passe et que les règles de sécurité autorisent les écritures.
4.  **Compilation et Upload**: Compilez le projet et téléversez-le sur votre carte ESP8266 via le port série approprié.

## 8. Tests et Depannage

*   **Connexion WiFi**: Vérifiez que les identifiants WiFi dans `secrets.h` sont corrects et que le réseau est accessible. Le `Serial.println` dans `setup()` et `loop()` indiquera l'état de la connexion.
*   **Connexion Firebase**: Assurez-vous que l'URL de la base de données, l'API Key, l'email et le mot de passe utilisateur sont corrects dans `secrets.h`. Vérifiez les règles de sécurité de votre base de données Firebase. Les messages de débogage indiqueront les échecs d'initialisation.
*   **Capteurs**:
    *   **DHT11**: Vérifiez le câblage. Si les lectures sont `NaN` (Not a Number), le capteur pourrait être mal connecté ou défectueux. Le code utilise `DHT11`, assurez-vous que c'est bien le modèle utilisé.
    *   **Humidité Sol**: Vérifiez le câblage de la broche analogique. Les valeurs brutes (`analogRead`) doivent varier lorsque la sonde est dans l'eau ou à l'air sec. La fonction `map` convertit ces valeurs en pourcentage.
*   **Relais**: Le relais est activé lorsque `RELAY_STATE` est `true` (correspondant à `LOW` sur la broche `RELAY_PIN`). Vérifiez le câblage du module relais et de l'électrovanne.
*   **Stream Firebase**: Si le stream ne fonctionne pas (`streamStarted` reste `false` ou `checkStreamConnection` signale des déconnexions), vérifiez la connexion internet, la configuration Firebase et les tokens. Le `streamTimeoutCallback` et les erreurs dans `readStream` sont des indicateurs clés.
*   **Bugs Hardware Détectés**:
    *   **Critique**: Le code utilise `ESP8266WiFi.h` mais la documentation mentionne `ESP32` dans la section "PLATEFORME DÉTECTÉE". Si vous utilisez un ESP32, la bibliothèque `WiFi.h` et d'autres fonctions spécifiques à l'ESP8266 devront être adaptées. Le code source fourni est explicitement pour ESP8266.
    *   **Avertissement**: Le code utilise `DHT_TYPE DHT11` mais la liste de courses suggère `DHT22`. Assurez-vous que le type de DHT défini dans le code (`DHT_TYPE`) correspond au capteur physique utilisé. Le DHT11 est moins précis que le DHT22.
    *   **Avertissement**: La liste de courses mentionne `ESP32` et `ESP8266` comme alternatives, mais le code source est spécifiquement écrit pour `ESP8266` (utilisation de `ESP8266WiFi.h` et configuration `platform = espressif8266` dans `platformio.ini`). L'utilisation d'un ESP32 nécessiterait des modifications significatives du code WiFi et potentiellement d'autres bibliothèques.

---

## 🛒 Liste de Courses

| Composant | Prix (USD) | Liens d'Achat | Alternatives |
|-----------|-------------|----------------|---------------|
| **DHT22** | **$9.43** | [Adafruit ($9.95)](https://www.adafruit.com/product/386) • [DFRobot (via Mouser) ($8.90)](https://www.mouser.fr/ProductDetail/DFRobot/SEN0137) | DHT11, AHT20 |
| **ESP32** | **$5.50** | [ElectroPeak ($2.75)](https://electropeak.com/products/esp-wroom-32-esp32-wi-fi-bluetooth-development-board) • [AliExpress ($3.95)](https://www.aliexpress.com/item/1005004652285149.html) | ESP32-S2, ESP32-S3 |
| **ESP8266** | **$4.00** | [AliExpress ($4.00)](https://s.click.aliexpress.com/e/_EITua4Q) • [Amazon ($6.99)](https://www.amazon.com/HiLetgo-ESP8266-ESP-12E-NodeMCU-Module/dp/B010O1G1ES) | ESP32, Wemos D1 Mini |
| **Relay Module** | **$10.00** | [Amazon ($2.99)](https://www.amazon.com/SMAKN-Channel-Module-Arduino-Raspberry/dp/B00VR1E5C6) • [SparkFun ($18.42)](https://www.sparkfun.com/products/15093) | Solid State Relays (SSR), Multi-channel Relay Modules |

**💰 Total Estimate: ~$28.93**