## 1. Vue d'ensemble
Ce projet vise à créer un système de surveillance et de contrôle intelligent pour l'agriculture, nommé Agrisense, basé sur un microcontrôleur ESP8266. Il mesure la température de l'air, l'humidité de l'air et l'humidité du sol, puis envoie ces données à une base de données Firebase Realtime. Le système permet également de contrôler à distance une électrovanne via Firebase pour gérer l'arrosage.

L'architecture repose sur un ESP8266 qui agit comme un client WiFi, se connectant à Firebase pour la lecture et l'écriture de données. Il utilise un mécanisme de "stream" Firebase pour recevoir des commandes en temps réel, notamment pour l'état de l'électrovanne, et met à jour les mesures et un historique dans la base de données.

## 2. Composants Hardware
Le tableau suivant détaille les composants matériels essentiels pour ce projet :

| Composant | Pin | Fonction | Notes |
|---|---|---|---|
| ESP8266 (ESP-12E) | N/A | Microcontrôleur principal | Gère la logique, la connexion WiFi et la communication Firebase. |
| Capteur DHT11 | D6 (GPIO12) | Mesure la température et l'humidité de l'air | Le type est défini comme DHT11 dans le code. |
| Capteur d'humidité du sol | A0 (ADC) | Mesure l'humidité du sol | Capteur analogique, connecté à l'entrée analogique de l'ESP8266. |
| Module Relais 1 canal | D5 (GPIO14) | Contrôle l'électrovanne | Permet d'activer ou de désactiver l'électrovanne. |
| Électrovanne | N/A | Actionneur d'arrosage | Connectée au module relais pour l'irrigation. |

**Liste de courses :**
- Module ESP8266 (ex: NodeMCU ESP-12E)
- Capteur de température et d'humidité DHT11 (le code utilise ce type)
- Capteur d'humidité du sol (type capacitif ou résistif)
- Module relais 1 canal 5V
- Électrovanne (tension compatible avec votre alimentation, ex: 12V)
- Alimentation 5V pour l'ESP8266 et, si nécessaire, une alimentation séparée pour l'électrovanne.
- Fils de connexion (Dupont mâle-mâle, mâle-femelle)
- Breadboard (pour le prototypage)

## 3. Configuration des Pins
Les broches GPIO de l'ESP8266 sont configurées comme suit dans le fichier `src/main.cpp` :
- `DHT_PIN` est défini à `12` (correspondant à la broche D6 de l'ESP8266).
- `DHT_TYPE` est défini à `DHT11`.
- `SOIL_MOISTURE_PIN` est défini à `A0`.
- `RELAY_PIN` est défini à `14` (correspondant à la broche D5 de l'ESP8266).

## 4. Bibliothèques
Le projet utilise les bibliothèques suivantes, incluses dans `src/main.cpp` :
- `ESP8266WiFi.h` : Gère la connexion WiFi de l'ESP8266.
- `DHT.h` : Fournit les fonctions nécessaires pour interagir avec le capteur DHT11.
- `Firebase_ESP_Client.h` : Client Firebase pour ESP8266 et ESP32, permettant la communication avec la Realtime Database.
- `time_utils.h` : Contient des fonctions utilitaires pour l'initialisation du temps via NTP et la génération de timestamps ISO UTC.
- `secrets.h` : Fichier personnalisé (non fourni) qui doit contenir les identifiants sensibles comme le SSID WiFi, le mot de passe, la clé API Firebase, l'URL de la base de données et les informations d'authentification utilisateur.
- `Array_Utils.h` : Bibliothèque utilitaire pour la manipulation de tableaux (incluse mais non utilisée directement pour les tableaux d'historique dans le code fourni).
- `addons/TokenHelper.h` : Aide à la gestion des tokens d'authentification Firebase.
- `addons/RTDBHelper.h` : Aide à l'interaction avec la Realtime Database Firebase.

## 5. Logique du Code
Le code est structuré autour des fonctions `setup()` et `loop()` d'Arduino, complétées par plusieurs fonctions utilitaires :

- **`setup()`** :
    - Initialise la communication série à 115200 bauds.
    - Configure les broches `RELAY_PIN` en sortie et `SOIL_MOISTURE_PIN` en entrée. Le relais est initialisé en position "fermé" (HIGH).
    - Démarre le capteur DHT.
    - Tente de se connecter au réseau WiFi en utilisant les identifiants de `secrets.h`. En cas d'échec, l'ESP redémarre.
    - Une fois connecté au WiFi, initialise le temps via NTP (`initializeTimestamp()`).
    - Initialise la connexion Firebase (`initializeFirebase()`) et démarre le stream Firebase.
    - Initialise les tableaux d'historique avec un timestamp initial.

- **`loop()`** :
    - Appelle `checkWiFiConnection()` pour surveiller et gérer la connexion WiFi.
    - Vérifie si Firebase est prêt et tente de le réinitialiser si la connexion est perdue.
    - Appelle `checkStreamConnection()` à intervalles réguliers (`STREAM_CHECK_INTERVAL`) pour s'assurer que le stream Firebase est actif et le redémarrer si nécessaire.
    - Appelle `handleRelayStateChange()` pour mettre à jour l'état physique du relais si une commande a été reçue via Firebase.
    - À intervalles réguliers (`MEASURE_INTERVAL`), la fonction `readMeasures()` est appelée pour lire les données des capteurs.
    - Si Firebase est prêt, `updateDataBase()` est appelée pour envoyer les mesures actuelles et l'historique à la base de données.
    - Lit en continu le stream Firebase (`Firebase.RTDB.readStream()`) pour recevoir les commandes en temps réel, ce qui est essentiel pour la réactivité du contrôle de l'électrovanne.
    - Un `delay(50)` est utilisé pour réduire la charge CPU.

- **Fonctions critiques :**
    - **`readMeasures(Measure* measures)`** : Lit la température et l'humidité de l'air depuis le DHT11, ainsi que l'humidité du sol depuis la broche analogique A0. Les valeurs sont stockées dans une structure `Measure`.
    - **`updateDataBase(Measure* measures, int taille)`** : Construit un objet JSON avec les dernières mesures et l'état de l'électrovanne, puis l'envoie à Firebase. Met également à jour un historique des mesures toutes les 30 secondes.
    - **`streamCallback(FirebaseStream data)`** : Fonction de rappel déclenchée lors de la réception de données via le stream Firebase. Elle est spécifiquement configurée pour détecter les changements sur le chemin `/etat_electrovanne` et met à jour la variable `RELAY_STATE` en conséquence.
    - **`handleRelayStateChange()`** : Met à jour l'état de la broche `RELAY_PIN` en fonction de `RELAY_STATE` lorsque `RELAY_STATE_CHANGED` est vrai.
    - **`initializeFirebase()` / `startFirebaseStream()` / `checkStreamConnection()`** : Gèrent l'authentification, la connexion et le maintien du stream Firebase.

## 6. Schéma de Câblage
```mermaid
flowchart TD
    ESP8266["ESP8266 (NodeMCU/ESP-12E)"]

    DHT11["Capteur DHT11"]
    CapteurSol["Capteur Humidité Sol"]
    Relais["Module Relais 1 canal"]
    Electrovanne["Électrovanne"]

    ESP8266 -->|GPIO12 (D6) - Data| DHT11
    ESP8266 -->|A0 - Analog Out| CapteurSol
    ESP8266 -->|GPIO14 (D5) - IN| Relais
    Relais --> Electrovanne
    DHT11 ---|VCC/GND| ESP8266
    CapteurSol ---|VCC/GND| ESP8266
    Relais ---|VCC/GND| ESP8266
```

## 7. Installation
Pour installer et exécuter ce projet, suivez les étapes ci-dessous :

1.  **Installer PlatformIO** : Si ce n'est pas déjà fait, installez l'IDE PlatformIO (via VS Code ou en standalone).
2.  **Cloner le dépôt GitHub** : Récupérez le code source du projet sur votre machine locale.
    `git clone https://github.com/leadertgn/agrisense-partie-iot.git`
    `cd agrisense-partie-iot`
3.  **Créer le fichier `secrets.h`** : Dans le dossier `src`, créez un nouveau fichier nommé `secrets.h` et ajoutez-y vos identifiants. Ce fichier n'est pas inclus dans le dépôt pour des raisons de sécurité.
    ```cpp
    #ifndef SECRETS_H
    #define SECRETS_H

    #define WIFI_SSID "Votre_SSID_WiFi"
    #define WIFI_PASSWORD "Votre_Mot_De_Passe_WiFi"

    #define API_KEY "Votre_Cle_API_Firebase"
    #define DATABASE_URL "Votre_URL_Base_De_Donnees_Firebase"
    #define USER_EMAIL "Votre_Email_Firebase"
    #define USER_PASSWORD "Votre_Mot_De_Passe_Firebase"

    #endif
    ```
    Remplacez les valeurs par vos propres informations d'identification Firebase et WiFi.
4.  **Vérifier `platformio.ini`** : Le fichier `platformio.ini` est déjà configuré pour la carte `esp12e` et inclut la bibliothèque Firebase nécessaire. Aucune modification n'est généralement requise.
5.  **Câbler les composants** : Suivez le schéma de câblage fourni dans la section 6 pour connecter le DHT11, le capteur d'humidité du sol et le module relais à votre ESP8266.
6.  **Téléverser le code** : Connectez votre ESP8266 à votre ordinateur via USB et utilisez PlatformIO pour téléverser le code sur la carte.

## 8. Tests et Dépannage
Voici des points de contrôle et des solutions pour les problèmes potentiels, incluant les bugs détectés automatiquement :

-   **Connexion WiFi** :
    -   **Problème** : L'ESP8266 ne se connecte pas au WiFi ou perd la connexion fréquemment.
    -   **Vérification** : Assurez-vous que `WIFI_SSID` et `WIFI_PASSWORD` dans `secrets.h` sont corrects. Vérifiez la force du signal WiFi à l'emplacement de l'ESP.
    -   **Solution** : Redémarrez l'ESP. Si le problème persiste, vérifiez le routeur WiFi. Le code inclut une logique de reconnexion automatique (`reconnectWiFi()` appelée par `checkWiFiConnection()`).

-   **Connexion Firebase** :
    -   **Problème** : Les données ne sont pas envoyées à Firebase ou le stream ne démarre pas.
    -   **Vérification** : Vérifiez `API_KEY`, `DATABASE_URL`, `USER_EMAIL`, et `USER_PASSWORD` dans `secrets.h`. Assurez-vous que les règles de sécurité de votre base de données Firebase permettent les opérations de lecture/écriture pour l'utilisateur configuré.
    -   **Solution** : Vérifiez les messages de débogage sur le moniteur série. Les erreurs comme "Échec de l'initialisation Firebase" ou "Échec du début du stream" indiqueront un problème d'authentification ou de configuration. Le code tente de réinitialiser Firebase si la connexion est perdue.

-   **Lecture des capteurs DHT11 et humidité du sol** :
    -   **Problème** : Les valeurs de température, humidité de l'air ou du sol sont incorrectes (`-1`, `nan`, ou valeurs fixes).
    -   **Vérification** :
        -   **DHT11** : Vérifiez le câblage du DHT11 à la broche D6 (GPIO12). Assurez-vous que le capteur est correctement alimenté.
        -   **Humidité du sol** : Vérifiez le câblage du capteur d'humidité du sol à la broche A0. Assurez-vous que le capteur est bien inséré dans le sol et qu'il est alimenté.
    -   **Solution** : Le code gère les erreurs de lecture DHT (`isnan`). Vérifiez physiquement les capteurs et leurs connexions.

-   **Contrôle de l'électrovanne** :
    -   **Problème** : L'électrovanne ne s'active pas ou ne se désactive pas correctement.
    -   **Vérification** :
        -   Vérifiez le câblage du module relais à la broche D5 (GPIO14) et à l'électrovanne.
        -   Assurez-vous que le module relais est correctement alimenté.
        -   Vérifiez