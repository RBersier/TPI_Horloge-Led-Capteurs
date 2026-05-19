# TPI_Horloge-Led-Capteurs

## Résumé du projet
Ce projet est une horloge murale basée sur Arduino qui combine:
- un affichage analogique de l’heure sur un anneau NeoPixel 60 LEDs,
- un affichage alphanumérique 4x14 segments pour les données environnementales,
- une mesure de l’environnement (température, humidité, pression, gaz/eCO2 estimé),
- un système d’alerte visuelle qualité d’air.

Le système est piloté par un RTC DS1307 pour garder l’heure, et par des boutons physiques pour la configuration et l’utilisation quotidienne.

## Manuel d’installation (depuis zéro)
### 1) Prérequis matériel
- 1x Arduino Uno Rev3
- 1x BME680 (I2C)
- 1x RTC DS1307 (I2C) + pile bouton
- 1x afficheur Adafruit 4x14 segments HT16K33 (I2C)
- 4x quarts NeoPixel 60 (assemblés en 1 ring 60 LEDs) ou 1 anneau équivalent WS2812B
- 6x boutons poussoirs (D2, D3, D4, D5, D7, D8)
- Câbles Dupont, breadboard (ou soudure finale)
- Câble USB A/B pour Arduino Uno

### 2) Prérequis logiciel
- Installer [VS Code](https://code.visualstudio.com/) ou [CLion](https://www.jetbrains.com/clion/)
- Installer l’extension **PlatformIO IDE**
- Installer les drivers Arduino Uno si nécessaire (Windows)

### 3) Récupérer le projet
1. Télécharger/Cloner le dépôt dans un dossier local.
2. Ouvrir le dossier du projet dans VS Code.
3. Ouvrir le sous-dossier `Code` comme projet PlatformIO.

### 4) Vérifier la configuration PlatformIO
Le fichier `Code/platformio.ini` contient les dépendances nécessaires (`lib_deps`), notamment:
- RTClib
- Adafruit BME680 Library
- Adafruit GFX Library
- Adafruit LED Backpack Library
- Adafruit NeoPixel

### 5) Câblage minimal
- **I2C commun** (SDA/SCL) pour DS1307, BME680, HT16K33
- **Ring NeoPixel data** sur `D6`
- **Boutons**:
  - `D2` Menu/Valider
  - `D3` Up
  - `D4` Down
  - `D5` Changer affichage
  - `D7` Luminosité
  - `D8` Désactiver alerte
- Masse (GND) commune sur tous les modules

### 6) Compiler et flasher
1. Brancher l’Arduino en USB.
2. Dans PlatformIO, lancer **Build**.
3. Sélectionner le port série correct (ex: `COM8`).
4. Lancer **Upload**.
5. (Optionnel) Ouvrir le moniteur série à `9600` bauds.

> Si le port ne fonctionne pas: lancer `pio device list` et mettre à jour `monitor_port` dans `Code/platformio.ini`.

### 7) Premier démarrage
- L’afficheur montre le cycle des infos (`eCO2`, `TEMP`, `HUMD`, `ATMO`, `DATE`).
- Le ring affiche l’heure.
- Si `RTC` ou `BME680` est absent, le système l’indique sur l’afficheur.

## Manuel d’utilisation
### A) Rôle de chaque composant
#### Arduino Uno Rev3
- Rôle: microcontrôleur principal.
- Données: centralise toutes les entrées/sorties et exécute la logique.

#### RTC DS1307
- Rôle: conserver l’heure/date même hors tension (avec pile).
- Données fournies: heure, minute, seconde, jour, mois, année.

#### BME680
- Rôle: mesure environnementale.
- Données fournies:
  - Température (`Traw`, `Tcomp` affichée)
  - Humidité relative (%)
  - Pression atmosphérique (hPa)
  - Résistance gaz (Ohm), convertie en `eCO2/PPM~` estimé

#### HT16K33 4x14 segments
- Rôle: affichage texte/valeurs.
- Données affichées:
  - `eCO2` (estimation ppm)
  - `TEMP`
  - `HUMD`
  - `ATMO`
  - `DATE`

#### Anneau NeoPixel 60 LEDs
- Rôle: horloge analogique + alerte visuelle.
- Affichage:
  - Heure en rouge
  - Minute en vert
  - Seconde en bleu cumulatif
  - Couleurs de mélange selon superposition
- Alerte:
  - Ring rouge clignotant rapide si qualité d’air dégradée

#### 6 boutons poussoirs
- Rôle: interface utilisateur locale (navigation/configuration/alertes).

### B) Fonction de chaque bouton
#### D2 — Menu / Valider
- Entre en mode configuration de l’heure/date.
- Cycle de validation des champs:
  - Jour -> Mois -> Heure -> Minute -> Seconde -> retour mode normal.

#### D3 — Up
- Incrémente le champ actif en mode configuration.

#### D4 — Down
- Décrémente le champ actif en mode configuration.

#### D5 — Changer affichage
- Passe manuellement à l’information suivante sur l’afficheur.
- Redémarre le timer de rotation automatique.

#### D7 — Luminosité
- Change la luminosité du ring par paliers:
  - 100% -> 75% -> 50% -> 25% -> 100% ...

#### D8 — Désactiver alerte
- Coupe temporairement l’alerte visuelle rouge.
- L’alerte peut se réarmer automatiquement si la qualité d’air repasse sous seuil puis remonte.

### C) Comportement général
- Rotation automatique des données sur le 14-segments (avec TAG avant valeur).
- Horloge analogique continue sur le ring.
- Alerte visuelle prioritaire sur le ring quand seuil qualité d’air dépassé.
- Réglage date/heure possible sans PC via boutons.
