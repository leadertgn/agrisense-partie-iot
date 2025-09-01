#pragma once
#include <Arduino.h>
#include <string.h> // pour strncpy

// =========================
// Fonctions génériques (pour int, float, double, etc.)
// =========================

// Affiche un tableau
template <typename T>
void afficherTableau(T tab[], int taille) {
  for (int i = 0; i < taille; i++) {
    Serial.print("[" + String(i) + "] = ");
    Serial.println(tab[i]);
  }
}

// Somme des éléments
template <typename T>
T sommeTableau(const T tab[], int taille) {
  T somme = 0;
  for (int i = 0; i < taille; i++) somme += tab[i];
  return somme;
}

// Moyenne
template <typename T>
float moyenneTableau(const T tab[], int taille) {
  if (taille == 0) return 0;
  return static_cast<float>(sommeTableau(tab, taille)) / taille;
}

// Minimum
template <typename T>
T minTableau(const T tab[], int taille) {
  T minVal = tab[0];
  for (int i = 1; i < taille; i++) if (tab[i] < minVal) minVal = tab[i];
  return minVal;
}

// Maximum
template <typename T>
T maxTableau(const T tab[], int taille) {
  T maxVal = tab[0];
  for (int i = 1; i < taille; i++) if (tab[i] > maxVal) maxVal = tab[i];
  return maxVal;
}

// Copier un tableau
template <typename T>
void copierTableau(const T source[], T dest[], int taille) {
  for (int i = 0; i < taille; i++) dest[i] = source[i];
}

// Décalage à gauche
template <typename T>
void decalageGauche(T tab[], int taille) {
  for (int i = 0; i < taille - 1; i++) {
    tab[i] = tab[i + 1];
  }
}

// Décalage à droite
template <typename T>
void decalageDroite(T tab[], int taille) {
  for (int i = taille - 1; i > 0; i--) {
    tab[i] = tab[i - 1];
  }
}

// Insertion (remplace à index donné)
template <typename T>
void insererValeur(T tab[], int taille, int index, T valeur) {
  if (index >= 0 && index < taille) tab[index] = valeur;
}

// Suppression (décale puis remet valeur neutre)
template <typename T>
void supprimerValeur(T tab[], int taille, int index) {
  if (index < 0 || index >= taille) return;
  for (int i = index; i < taille - 1; i++) {
    tab[i] = tab[i + 1];
  }
  tab[taille - 1] = T(); // valeur par défaut (0, 0.0, ou "")
}

// Recherche d’une valeur → retourne index ou -1
template <typename T>
int rechercherValeur(const T tab[], int taille, T valeur) {
  for (int i = 0; i < taille; i++) {
    if (tab[i] == valeur) return i;
  }
  return -1;
}

// Tri à bulles
template <typename T>
void trierTableau(T tab[], int taille) {
  for (int i = 0; i < taille - 1; i++) {
    for (int j = 0; j < taille - i - 1; j++) {
      if (tab[j] > tab[j + 1]) {
        T tmp = tab[j];
        tab[j] = tab[j + 1];
        tab[j + 1] = tmp;
      }
    }
  }
}

// =========================
// Spécialisations pour String et char[]
// =========================

// Décalage gauche pour String[]
inline void decalageGauche(String tab[], int taille) {
  for (int i = 0; i < taille - 1; i++) {
    tab[i] = tab[i + 1];
  }
  tab[taille - 1] = ""; // efface la dernière case
}

// Décalage droite pour String[]
inline void decalageDroite(String tab[], int taille) {
  for (int i = taille - 1; i > 0; i--) {
    tab[i] = tab[i - 1];
  }
  tab[0] = "";
}

// Décalage gauche pour char[][] (ex: timestamps)
inline void decalageGauche(char tab[][25], int taille) {
  for (int i = 0; i < taille - 1; i++) {
    strncpy(tab[i], tab[i + 1], 25);
    tab[i][24] = '\0'; // sécurité
  }
  tab[taille - 1][0] = '\0';
}

// Décalage droite pour char[][]
inline void decalageDroite(char tab[][25], int taille) {
  for (int i = taille - 1; i > 0; i--) {
    strncpy(tab[i], tab[i - 1], 25);
    tab[i][24] = '\0';
  }
  tab[0][0] = '\0';
}
