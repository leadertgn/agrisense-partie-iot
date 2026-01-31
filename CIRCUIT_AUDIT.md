# Système de Surveillance et Contrôle d'Environnement de Culture

## Vue d'ensemble
Ce projet vise à créer un système embarqué pour la surveillance et le contrôle d'un environnement de culture. Il mesure la température, l'humidité de l'air et l'humidité du sol, puis transmet ces données à une base de données Firebase en temps réel. Le système permet également de contrôler un relais (pour une électrovanne, par exemple) à distance via Firebase.

L'architecture logicielle repose sur un microcontrôleur ESP8266 qui interagit avec des capteurs physiques et une base de données cloud (Firebase Realtime Database) via Wi-Fi. Le code gère la connexion réseau, l'acquisition des données, la mise à jour de Firebase et la réception de commandes pour le relais.

## Composants Hardware
| Composant               | Pin ESP8266 | Fonction                                  | Notes                                      |
| :---------------------- | :---------- | :---------------------------------------- | :----------------------------------------- |
| Capteur DHT11           | GPIO12 (D6) | Mesure température et humidité de l'air   |                                            |
| Capteur Humidité du Sol | A0          | Mesure l'humidité du sol (analogique)     |                                            |
| Relais                  | GPIO14 (D5) | Contrôle d'un actionneur (ex: électrovanne) | `LOW` pour activer, `HIGH` pour désactiver |

## Configuration des Pins
Les pins sont définies au début du fichier `src/main.cpp` :

```cpp
// ------------------------- CONFIGURATION -------------------------
#define DHT_PIN 12   // D6
#define DHT_TYPE DHT11
#define SOIL_MOISTURE_PIN A0
#define RELAY_PIN 14  // D5
```
- `DHT_PIN` (GPIO12, D6) est utilisé pour le capteur DHT11.
- `SOIL_MOISTURE_PIN` (A0) est l'entrée analogique pour le capteur d'humidité du sol.
- `RELAY_PIN` (GPIO14, D5) est la sortie numérique pour contrôler le relais.

## Bibliothèques et Dépendances
Les bibliothèques suivantes sont utilisées dans le projet :

-   `ESP8266WiFi.h` : Gère la connectivité Wi-Fi de l'ESP8266.
-   `DHT.h` : Interface avec le capteur de température et d'humidité DHT11.
-   `Firebase_ESP_Client.h` : Client Firebase pour ESP8266/ESP32, permettant l'interaction avec Firebase Realtime Database.
-   `time_utils.h` : Utilitaire pour la gestion du temps et la génération de timestamps (non fourni, mais implicite).
-   `secrets.h` : Contient les identifiants sensibles (Wi-Fi, Firebase) pour des raisons de sécurité (non fourni, mais implicite).
-   `Array_Utils.h` : Utilitaire pour la manipulation des tableaux (non fourni, mais implicite).
-   `addons/TokenHelper.h` : Aide à la gestion des tokens d'authentification Firebase.
-   `addons/RTDBHelper.h` : Aide à l'interaction avec la Realtime Database de Firebase.

La bibliothèque `Firebase Arduino Client Library for ESP8266 and ESP32@4.4.17` est spécifiée dans `platformio.ini`.

## Logique du Code Principal
### `setup()`
1.  Initialise la communication série à 115200 bauds.
2.  Initialise le capteur DHT (`dht.begin()`).
3.  Configure la pin du relais en sortie et l'éteint (`digitalWrite(RELAY_PIN, HIGH)`).
4.  Configure la pin d'humidité du sol en entrée.
5.  Tente de se connecter au réseau Wi-Fi en utilisant les identifiants de `secrets.h`.
6.  Si la connexion Wi-Fi est établie, initialise le service de temps (`initializeTimestamp()`) et Firebase (`initializeFirebase()`).
7.  Initialise les tableaux d'historique avec le timestamp actuel.
8.  En cas d'échec de la connexion Wi-Fi, redémarre l'ESP8266.

### `loop()`
1.  Vérifie et gère la connexion Wi-Fi via `checkWiFiConnection()`.
2.  Si Firebase n'est pas prêt alors que le Wi-Fi est connecté, tente de le réinitialiser.
3.  Vérifie et maintient la connexion au stream Firebase via `checkStreamConnection()` toutes les `STREAM_CHECK_INTERVAL` (5 secondes).
4.  Gère les changements d'état du relais via `handleRelayStateChange()`.
5.  Toutes les `MEASURE_INTERVAL` (5 secondes) :
    *   Lit les mesures des capteurs (`readMeasures()`).
    *   Met à jour la base de données Firebase avec les dernières mesures et l'historique (`updateDataBase()`).
6.  Lit en continu le stream Firebase (`Firebase.RTDB.readStream(&fbdoStream)`) pour recevoir les commandes en temps réel.
7.  Un `delay(50)` est appliqué pour stabiliser la boucle.

### Fonctions critiques
-   `initializeFirebase()` : Configure les identifiants Firebase (API Key, URL, Email/Password), initialise le client Firebase et démarre le stream de données.
-   `startFirebaseStream()` : Tente de démarrer un stream Firebase sur le chemin `/dernieres_mesures` et configure les callbacks pour les données reçues et les timeouts.
-   `readMeasures(Measure* measures)` : Lit la température et l'humidité de l'air depuis le DHT11, et l'humidité du sol depuis la pin analogique A0, puis stocke les valeurs dans une structure `Measure`.
-   `updateDataBase(Measure* measures, int taille)` : Envoie les dernières mesures et un historique des données à Firebase Realtime Database.
-   `streamCallback(FirebaseStream data)` : Fonction de rappel appelée lorsqu'une nouvelle donnée est reçue via le stream Firebase. Elle est utilisée pour détecter les changements d'état de l'électrovanne (`/etat_electrovanne`) et mettre à jour l'état local du relais.
-   `handleRelayStateChange()` : Met à jour l'état physique du relais si `RELAY_STATE_CHANGED` est vrai.

## Schéma de Câblage
```mermaid
flowchart TD
ESP8266["ESP8266[NodeMCU)"]
DHT11["Capteur DHT11"]
SOL["Capteur Humidité Sol"]
RELAY["Module Relais"]
ELECTROVANNE[Electrovanne]

ESP8266 --> DHT11 : GPIO12[D6)
ESP8266 --> SOL : A0
ESP8266 --> RELAY : GPIO14[D5)
RELAY --> ELECTROVANNE : Commande
```

## Procédure d'Installation
1.  **Configuration de PlatformIO** :
    *   Assurez-vous d'avoir PlatformIO IDE installé (extension pour VS Code recommandée).
    *   Ouvrez le dossier du projet dans PlatformIO.
    *   Le fichier `platformio.ini` configure automatiquement l'environnement pour l'ESP8266 (`board = esp12e`) et la bibliothèque Firebase requise.

2.  **Installation des bibliothèques** :
    *   PlatformIO gérera automatiquement l'installation de `Firebase Arduino Client Library for ESP8266 and ESP32@4.4.17` et de ses dépendances lors de la première compilation.
    *   Assurez-vous que la bibliothèque DHT est également installée (généralement `DHT sensor library` par Adafruit).

3.  **Configuration des identifiants (secrets.h)** :
    *   Créez un fichier nommé `secrets.h` dans le répertoire `src/` (ou à la racine du projet si `src` est configuré pour l'inclure).
    *   Ce fichier doit contenir vos identifiants Wi-Fi et Firebase. Exemple de contenu :
        ```c++
        #ifndef SECRETS_H
        #define SECRETS_H

        #define WIFI_SSID "Votre_SSID_WiFi"
        #define WIFI_PASSWORD "Votre_Mot_De_Passe_WiFi"

        #define API_KEY "Votre_Cle_API_Firebase"
        #define DATABASE_URL "https://votre-projet-firebase.firebaseio.com/"
        #define USER_EMAIL "votre_email_firebase@example.com"
        #define USER_PASSWORD "votre_mot_de_passe_firebase"

        #endif
        ```
    *   Remplacez les valeurs d'exemple par vos propres identifiants Firebase et Wi-Fi.

4.  **Compilation et Téléchargement** :
    *   Connectez votre carte ESP8266 (ex: NodeMCU) à votre ordinateur via USB.
    *   Dans PlatformIO, utilisez le bouton "Upload" (flèche droite) pour compiler le code et le téléverser sur votre ESP8266.

## Tests et Dépannage
1.  **Vérification du câblage** :
    *   Assurez-vous que le DHT11 est correctement connecté à GPIO12 (D6), le capteur d'humidité du sol à A0, et le relais à GPIO14 (D5).
    *   Vérifiez l'alimentation des capteurs et du relais.

2.  **Moniteur Série** :
    *   Ouvrez le Moniteur Série de PlatformIO (icône de prise) à 115200 bauds.
    *   Observez les messages de débogage (`[DEBUG]`) pour suivre l'état de la connexion Wi-Fi, l'initialisation de Firebase, les lectures de capteurs et les mises à jour de la base de données.
    *   Recherchez les messages d'erreur liés au Wi-Fi, à Firebase ou aux capteurs.

3.  **Connexion Wi-Fi** :
    *   Si le Moniteur Série indique "Échec connexion WiFi", vérifiez `WIFI_SSID` et `WIFI_PASSWORD` dans `secrets.h`.
    *   Assurez-vous que l'ESP8266 est à portée de votre réseau Wi-Fi.

4.  **Connexion Firebase** :
    *   Si "Échec de l'initialisation Firebase" apparaît, vérifiez `API_KEY`, `DATABASE_URL`, `USER_EMAIL`, et `USER_PASSWORD` dans `secrets.h`.
    *   Vérifiez les règles de sécurité de votre base de données Firebase pour vous assurer que l'utilisateur a les permissions de lecture/écriture nécessaires.
    *   Un "Token expiré" ou "Token invalide" peut nécessiter une réinitialisation de Firebase, ce que le code tente de gérer.

5.  **Lectures des capteurs** :
    *   Si "Erreur de lecture DHT !" apparaît, vérifiez le câblage du DHT11 ou essayez un autre capteur.
    *   Les valeurs d'humidité du sol doivent varier lorsque le capteur est sec ou humide.

6.  **Contrôle du relais** :
    *   Modifiez la valeur de `etat_electrovanne` dans votre base de données Firebase (sous `/dernieres_mesures`).
    *   Le Moniteur Série devrait afficher "Nouvel état reçu de l'interface" et l'état du relais devrait changer physiquement.