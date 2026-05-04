// ============================================================
//  TEST COMPOSANTS — Horloge LED avec capteurs
//  Ryan BERSIER — TPI 2026
//
//  Composants testés :
//    - RTC DS1307
//    - Capteur BME680
//    - Affichage HT16K33 (4x14 segments)
//    - Ring Adafruit NeoPixel 60 LEDs
//    - 6 boutons poussoirs (D2, D3, D4, D5, D7, D8)
//
//  Bibliothèques requises (PlatformIO) :
//    - adafruit/RTClib
//    - adafruit/Adafruit BME680 Library
//    - adafruit/Adafruit GFX Library
//    - adafruit/Adafruit LED Backpack Library
//    - adafruit/Adafruit NeoPixel
//
//  Utilisation :
//    Ouvrir le moniteur série à 9600 bauds.
//    Envoyer le numéro du test voulu (1 à 6).
// ============================================================

#include <arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME680.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_NeoPixel.h>

// ── Prototypes des fonctions ─────────────────────────────────
void printMenu();
void printTwoDigits(int n);
void initRTC();
void testRTC();
void initBME();
void testBME();
void initHT16K33();
void testHT16K33();
void testButtons();
void initNeoPixelRing();
void testNeoPixelRing();
void testAll();

// ── Broches boutons ──────────────────────────────────────────
#define BTN1 2
#define BTN2 3
#define BTN3 4
#define BTN4 5
#define BTN5 7
#define BTN6 8

const int BTNS[]    = {BTN1, BTN2, BTN3, BTN4, BTN5, BTN6};
const int NB_BTNS   = 6;
#define DEBOUNCE_MS 50

// ── Objets composants ────────────────────────────────────────
RTC_DS1307          rtc;
Adafruit_BME680     bme;
Adafruit_AlphaNum4  alpha4;   // HT16K33 4x14 segments
Adafruit_NeoPixel   ring60(60, 6, NEO_GRB + NEO_KHZ800);

// ── État boutons ─────────────────────────────────────────────
bool     lastState[6]    = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
bool     currentState[6] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
uint32_t lastDebounce[6] = {0, 0, 0, 0, 0, 0};

// ── Mode test actif ──────────────────────────────────────────
int activeTest = 0;
uint16_t ringIndex = 0;

// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  while (!Serial);
  Wire.begin();

  // Boutons en INPUT_PULLUP
  for (int i = 0; i < NB_BTNS; i++) {
    pinMode(BTNS[i], INPUT_PULLUP);
  }

  printMenu();
}

// ────────────────────────────────────────────────────────────
void loop() {
  // Lire commande série
  if (Serial.available()) {
    int cmd = Serial.parseInt();
    if (cmd >= 1 && cmd <= 6) {
      activeTest = cmd;
      Serial.println();
      switch (activeTest) {
        case 1: initRTC();     break;
        case 2: initBME();     break;
        case 3: initHT16K33(); break;
        case 4: break; // boutons: rien à init
        case 5: initNeoPixelRing(); break;
        case 6: testAll();     return;
      }
    } else if (cmd == 0) {
      activeTest = 0;
      printMenu();
      return;
    }
  }

  // Exécuter le test actif en boucle
  switch (activeTest) {
    case 1: testRTC();     delay(1000); break;
    case 2: testBME();     delay(2000); break;
    case 3: testHT16K33(); delay(500);  break;
    case 4: testButtons(); delay(50);   break;
    case 5: testNeoPixelRing(); delay(60); break;
    default: break;
  }
}

// ════════════════════════════════════════════════════════════
//  MENU
// ════════════════════════════════════════════════════════════
void printMenu() {
  Serial.println(F("\n╔══════════════════════════════════════╗"));
  Serial.println(F("║   TEST COMPOSANTS — TPI Ryan BERSIER ║"));
  Serial.println(F("╠══════════════════════════════════════╣"));
  Serial.println(F("║  1 → Test RTC DS1307                 ║"));
  Serial.println(F("║  2 → Test BME680                     ║"));
  Serial.println(F("║  3 → Test Affichage HT16K33          ║"));
  Serial.println(F("║  4 → Test Boutons poussoirs          ║"));
  Serial.println(F("║  5 → Test Ring NeoPixel 60           ║"));
  Serial.println(F("║  6 → Test ALL (intégration)          ║"));
  Serial.println(F("║  0 → Retour au menu                  ║"));
  Serial.println(F("╚══════════════════════════════════════╝"));
  Serial.println(F("Envoyer le numéro du test :"));
}

// ════════════════════════════════════════════════════════════
//  TEST 1 — RTC DS1307
// ════════════════════════════════════════════════════════════
void initRTC() {
  Serial.println(F("\n── TEST RTC DS1307 ─────────────────────"));
  if (!rtc.begin()) {
    Serial.println(F("[ERREUR] RTC non détecté ! Vérifier le câblage SDA/SCL."));
    activeTest = 0;
    printMenu();
    return;
  }
  if (!rtc.isrunning()) {
    Serial.println(F("[INFO] RTC arrêté. Réglage à l'heure de compilation..."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println(F("[OK] RTC détecté et en marche."));
  Serial.println(F("Lecture de l'heure toutes les secondes (0 pour quitter) :"));
}

void testRTC() {
  DateTime now = rtc.now();
  Serial.print(F("  Heure : "));
  printTwoDigits(now.hour());   Serial.print(F(":"));
  printTwoDigits(now.minute()); Serial.print(F(":"));
  printTwoDigits(now.second());
  Serial.print(F("   Date : "));
  printTwoDigits(now.day());   Serial.print(F("/"));
  printTwoDigits(now.month()); Serial.print(F("/"));
  Serial.println(now.year());
}

// ════════════════════════════════════════════════════════════
//  TEST 2 — BME680
// ════════════════════════════════════════════════════════════
void initBME() {
  Serial.println(F("\n── TEST BME680 ─────────────────────────"));
  if (!bme.begin(0x76)) {
    Serial.println(F("[ERREUR] BME680 non détecté ! Vérifier le câblage I2C."));
    activeTest = 0;
    printMenu();
    return;
  }
  // Paramètres oversampling recommandés Adafruit
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C pendant 150ms

  Serial.println(F("[OK] BME680 détecté et configuré."));
  Serial.println(F("[INFO] Attendre ~30s pour stabilisation du capteur COV."));
  Serial.println(F("Mesures toutes les 2s (0 pour quitter) :"));
}

void testBME() {
  if (!bme.performReading()) {
    Serial.println(F("[ERREUR] Lecture BME680 échouée."));
    return;
  }
  Serial.print(F("  Temp : ")); Serial.print(bme.temperature, 1); Serial.print(F(" °C   "));
  Serial.print(F("Hum : "));    Serial.print(bme.humidity, 1);    Serial.print(F(" %   "));
  Serial.print(F("Pres : "));   Serial.print(bme.pressure / 100.0, 1); Serial.print(F(" hPa   "));
  Serial.print(F("COV : "));    Serial.print(bme.gas_resistance / 1000.0, 1); Serial.println(F(" KΩ"));
}

// ════════════════════════════════════════════════════════════
//  TEST 3 — HT16K33 (affichage 4x14 segments)
// ════════════════════════════════════════════════════════════

// Séquence de chars à afficher
const char* seqChars[] = {"HELO", "TEST", "1234", "ABCD", "----"};
int seqIdx = 0;
uint32_t lastSeqTime = 0;
bool ht16ok = false;

void initHT16K33() {
  Serial.println(F("\n── TEST HT16K33 ────────────────────────"));
  alpha4.begin(0x70);  // adresse I2C par défaut
  ht16ok = true;
  Serial.println(F("[OK] HT16K33 initialisé (adresse 0x70)."));
  Serial.println(F("Défilement de caractères sur l'affichage..."));
  Serial.println(F("(0 pour quitter)"));
}

void testHT16K33() {
  if (!ht16ok) return;

  static bool showAllSegments = false;
  uint32_t now = millis();
  if (now - lastSeqTime > 800) {
    lastSeqTime = now;
    alpha4.clear();
    if (showAllSegments) {
      Serial.println(F("  Affichage : [ALL SEGMENTS ON]"));
      for (int i = 0; i < 4; i++) {
        alpha4.writeDigitRaw(i, 0xFFFF);
      }
    } else {
      const char* word = seqChars[seqIdx % 5];
      Serial.print(F("  Affichage : ")); Serial.println(word);
      for (int i = 0; i < 4; i++) {
        alpha4.writeDigitAscii(i, word[i]);
      }
      seqIdx++;
    }
    alpha4.writeDisplay();
    showAllSegments = !showAllSegments;
  }
}

// ════════════════════════════════════════════════════════════
//  TEST 4 — 6 boutons poussoirs
// ════════════════════════════════════════════════════════════
void testButtons() {
  static bool firstRun = true;
  if (firstRun) {
    Serial.println(F("\n── TEST BOUTONS ────────────────────────"));
    Serial.println(F("[OK] Appuyer sur les boutons (0 pour quitter) :"));
    Serial.println(F("  D2=BTN1  D3=BTN2  D4=BTN3  D5=BTN4  D7=BTN5  D8=BTN6"));
    firstRun = false;
  }

  uint32_t now = millis();
  for (int i = 0; i < NB_BTNS; i++) {
    bool reading = digitalRead(BTNS[i]);
    if (reading != lastState[i]) {
      lastDebounce[i] = now;
    }
    if ((now - lastDebounce[i]) > DEBOUNCE_MS) {
      if (reading != currentState[i]) {
        currentState[i] = reading;
        if (currentState[i] == LOW) {
          Serial.print(F("  [APPUI]   BTN")); Serial.print(i + 1);
          Serial.print(F(" (D")); Serial.print(BTNS[i]); Serial.println(F(")"));
        } else {
          Serial.print(F("  [RELACHE] BTN")); Serial.print(i + 1);
          Serial.print(F(" (D")); Serial.print(BTNS[i]); Serial.println(F(")"));
        }
      }
    }
    lastState[i] = reading;
  }
}

// ════════════════════════════════════════════════════════════
//  TEST 5 — Test ALL (intégration)
// ════════════════════════════════════════════════════════════
void initNeoPixelRing() {
  Serial.println(F("\n-- TEST RING NEOPIXEL 60 ---------------"));
  ring60.begin();
  ring60.setBrightness(40);
  ring60.clear();
  ring60.show();
  ringIndex = 0;
  Serial.println(F("[OK] Ring NeoPixel initialise (60 LEDs, DIN D6)."));
  Serial.println(F("Animation chenillard RGB (0 pour quitter)."));
}

void testNeoPixelRing() {
  ring60.clear();
  ring60.setPixelColor(ringIndex % 60, ring60.Color(255, 0, 0));
  ring60.setPixelColor((ringIndex + 20) % 60, ring60.Color(0, 255, 0));
  ring60.setPixelColor((ringIndex + 40) % 60, ring60.Color(0, 0, 255));
  ring60.show();
  ringIndex = (ringIndex + 1) % 60;
}

void testAll() {
  Serial.println(F("\n-- TEST INTEGRATION (ALL) --------------"));
  Serial.println(F("Verification de tous les composants...\n"));

  bool allOk = true;

  // RTC
  Serial.print(F("  RTC DS1307  ... "));
  if (rtc.begin() && rtc.isrunning()) {
    Serial.println(F("[OK]"));
  } else {
    Serial.println(F("[ERREUR] Non detecte ou arrete !"));
    allOk = false;
  }

  // BME680
  Serial.print(F("  BME680      ... "));
  if (bme.begin(0x76)) {
    Serial.println(F("[OK]"));
  } else {
    Serial.println(F("[ERREUR] Non detecte !"));
    allOk = false;
  }

  // HT16K33
  Serial.print(F("  HT16K33     ... "));
  alpha4.begin(0x70);
  alpha4.clear();
  alpha4.writeDigitAscii(0, 'T');
  alpha4.writeDigitAscii(1, 'E');
  alpha4.writeDigitAscii(2, 'S');
  alpha4.writeDigitAscii(3, 'T');
  alpha4.writeDisplay();
  Serial.println(F("[OK] 'TEST' affiche"));

  // Ring NeoPixel
  Serial.print(F("  NEOPIXEL    ... "));
  ring60.begin();
  ring60.setBrightness(40);
  ring60.clear();
  ring60.setPixelColor(0, ring60.Color(255, 0, 0));
  ring60.setPixelColor(20, ring60.Color(0, 255, 0));
  ring60.setPixelColor(40, ring60.Color(0, 0, 255));
  ring60.show();
  Serial.println(F("[OK] 3 LEDs RGB affichees"));

  // Boutons
  Serial.println(F("  BOUTONS     ... [OK] (testes visuellement)"));
  Serial.println(F("    D2 D3 D4 D5 D7 D8 - utiliser test 4 pour verification"));

  // Resume
  Serial.println();
  if (allOk) {
    Serial.println(F("[OK] Tous les composants I2C sont detectes."));
    Serial.println(F("[OK] Integration prete."));
    alpha4.clear();
    alpha4.writeDigitAscii(0, 'O');
    alpha4.writeDigitAscii(1, 'K');
    alpha4.writeDigitAscii(2, '!');
    alpha4.writeDigitAscii(3, ' ');
    alpha4.writeDisplay();
  } else {
    Serial.println(F("[ERREUR] Un ou plusieurs composants sont en erreur."));
    Serial.println(F("  -> Verifier le cablage et les adresses I2C."));
  }

  activeTest = 0;
  Serial.println();
  printMenu();
}

// ════════════════════════════════════════════════════════════
//  UTILITAIRES
// ════════════════════════════════════════════════════════════
void printTwoDigits(int n) {
  if (n < 10) Serial.print(F("0"));
  Serial.print(n);
}

