/*
  ============================================================
  TPI 2026 - Horloge LED avec capteurs environnementaux
  Auteur: Ryan Bersier
  Fichier: main.cpp
  ============================================================

  Objectif du programme:
  - Afficher l'heure via RTC DS1307 sur un ring NeoPixel 60 LEDs
  - Lire l'environnement via BME680 (T/H/P + estimation PPM)
  - Afficher les donnees sur un afficheur 4x14 segments HT16K33
  - Gerer 6 boutons pour configuration, affichage, luminosite, alerte

  Materiel:
  - Arduino UNO Rev3
  - RTC DS1307 (I2C)
  - BME680 (I2C, adresse 0x76)
  - HT16K33 4x14 segments (I2C, adresse 0x70)
  - Ring NeoPixel WS2812 60 LEDs (DIN sur D6)
  - Boutons sur D2, D3, D4, D5, D7, D8

  Notes importantes:
  - Le BME680 avec la librairie Adafruit renvoie une resistance de gaz.
    Ici, on en deduit une estimation simple de "PPM" pour respecter
    le cahier des charges. Ce n'est pas une mesure CO2 absolue certifiee.
  - Tous les boutons sont en INPUT_PULLUP: appui = LOW.
*/

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME680.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_NeoPixel.h>

// ============================================================
// Mapping materiel (broches)
// ============================================================
static const uint8_t PIN_BTN_MENU = 2;   // D2
static const uint8_t PIN_BTN_UP = 3;     // D3
static const uint8_t PIN_BTN_DOWN = 4;   // D4
static const uint8_t PIN_BTN_NEXT = 5;   // D5
static const uint8_t PIN_RING = 6;       // D6
static const uint8_t PIN_BTN_BRIGHT = 7; // D7
static const uint8_t PIN_BTN_ALERT = 8;  // D8

static const uint8_t BTN_PINS[6] = {
  PIN_BTN_MENU, PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_NEXT, PIN_BTN_BRIGHT, PIN_BTN_ALERT
};

// Constantes de temporisation et taille du ring
static const uint8_t RING_LED_COUNT = 60;
static const uint16_t DEBOUNCE_MS = 40;
static const uint32_t ENV_READ_MS = 2000;
static const uint32_t DISPLAY_ROTATE_MS = 10000;
static const float TEMP_OFFSET_C = -2.5f;

// ============================================================
// Objets librairies
// ============================================================
RTC_DS1307 rtc;
Adafruit_BME680 bme;
Adafruit_AlphaNum4 alpha4;
Adafruit_NeoPixel ring(RING_LED_COUNT, PIN_RING, NEO_GRB + NEO_KHZ800);

// ============================================================
// Gestion des boutons (anti-rebond logiciel)
// ============================================================
bool btnStableState[6] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
bool btnLastReading[6] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
uint32_t btnLastChangeMs[6] = {0, 0, 0, 0, 0, 0};
bool btnPressedEvent[6] = {false, false, false, false, false, false};

// ============================================================
// Donnees environnementales mises en cache
// ============================================================
float tempRawC = 0.0f;
float tempCompC = 0.0f;
float envHumPct = 0.0f;
float envPressureHpa = 0.0f;
uint32_t envGasOhm = 0;
int envPpmEstimate = 400;
uint32_t lastEnvReadMs = 0;
uint32_t lastEnvSuccessMs = 0;
uint8_t envFailCount = 0;
bool envValid = false;
float gasBaselineOhm = 0.0f;
bool gasBaselineValid = false;
float gasFilteredOhm = 0.0f;
bool gasFilterValid = false;
uint32_t baselineStartMs = 0;

// ============================================================
// Gestion des pages affichees sur le 14-segments
// ============================================================
enum DisplayItem : uint8_t {
  DISP_AIR = 0,
  DISP_TEMP = 1,
  DISP_HUM = 2,
  DISP_ATMO = 3,
  DISP_DATE = 4,
  DISP_COUNT = 5
};
DisplayItem currentDisplayItem = DISP_AIR;
uint32_t lastDisplaySwitchMs = 0;
bool showTagForCurrentItem = true;
bool tagDisplayActive = false;
uint32_t tagDisplayUntilMs = 0;

// ============================================================
// Mode configuration horaire/date
// ============================================================
bool configMode = false;
uint8_t configField = 0; // 0=day,1=month,2=hour,3=min,4=sec
int cfgHour = 0;
int cfgMinute = 0;
int cfgSecond = 0;
int cfgDay = 1;
int cfgMonth = 1;
int cfgYear = 2026;

// ============================================================
// Comportement visuel de l'anneau
// ============================================================
uint8_t brightnessStep = 0; // 0=100,1=75,2=50,3=25 (version limitee pour stabilite)
const uint8_t brightnessValues[4] = {100, 75, 50, 25};

// Intensites LED reduites pour eviter scintillement/corruption data due au courant.
const uint8_t COL_R = 100;
const uint8_t COL_G = 100;
const uint8_t COL_B = 100;

bool alertSuppressed = false;
int lastRenderedSecond = -1;
int lastRenderedMinute = -1;
int lastRenderedHour = -1;
bool lastRenderedAlertOn = false;
uint32_t alertBlinkPhase = 0;

static inline uint8_t daysInMonth(int month, int year) {
  // Renvoie le nombre de jours du mois en tenant compte des annees bissextiles.
  if (month == 2) {
    bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    return leap ? 29 : 28;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

void display4(const char *txt) {
  // Affiche exactement 4 caracteres (ou espaces) sur le HT16K33.
  alpha4.clear();
  for (uint8_t i = 0; i < 4; i++) {
    char c = txt[i] ? txt[i] : ' ';
    alpha4.writeDigitAscii(i, c);
  }
  alpha4.writeDisplay();
}

void displayNumber4(int value) {
  // Formate un entier sur 4 caracteres (aligne a droite).
  char buf[5];
  snprintf(buf, sizeof(buf), "%4d", value);
  display4(buf);
}

void displayTemp(float tempC) {
  // Affiche la temperature sous la forme XX.XC (ex: 23.4C).
  int scaled = (int)round(tempC * 10.0f);
  int absScaled = abs(scaled);
  int tens = absScaled / 10;
  int tenths = absScaled % 10;

  alpha4.clear();
  if (scaled < 0) {
    alpha4.writeDigitAscii(0, '-');
    alpha4.writeDigitAscii(1, (char)('0' + (tens % 10)), true);
  } else {
    alpha4.writeDigitAscii(0, (char)('0' + ((tens / 10) % 10)));
    alpha4.writeDigitAscii(1, (char)('0' + (tens % 10)), true);
  }
  alpha4.writeDigitAscii(2, (char)('0' + tenths));
  alpha4.writeDigitAscii(3, 'C');
  alpha4.writeDisplay();
}

void displayHumidity(float hum) {
  // Affiche l'humidite sous la forme XX.X%
  int scaled = (int)round(hum * 10.0f);
  int tens = scaled / 10;
  int tenths = abs(scaled % 10);

  alpha4.clear();
  alpha4.writeDigitAscii(0, (char)('0' + ((tens / 10) % 10)));
  alpha4.writeDigitAscii(1, (char)('0' + (tens % 10)), true);
  alpha4.writeDigitAscii(2, (char)('0' + tenths));
  alpha4.writeDigitAscii(3, '%');
  alpha4.writeDisplay();
}

void displayDateDDMM(const DateTime &now) {
  // Affiche la date au format DD.MM (point via decimal point).
  static uint8_t lastValidDay = 1;
  static uint8_t lastValidMonth = 1;

  uint8_t day = now.day();
  uint8_t month = now.month();

  // Protection contre les lectures transitoires invalides (ex: 00.00).
  if (day >= 1 && day <= 31 && month >= 1 && month <= 12) {
    lastValidDay = day;
    lastValidMonth = month;
  } else {
    day = lastValidDay;
    month = lastValidMonth;
  }

  alpha4.clear();
  alpha4.writeDigitAscii(0, (char)('0' + (day / 10)));
  alpha4.writeDigitAscii(1, (char)('0' + (day % 10)), true);
  alpha4.writeDigitAscii(2, (char)('0' + (month / 10)));
  alpha4.writeDigitAscii(3, (char)('0' + (month % 10)));
  alpha4.writeDisplay();
}

void showTagThenValue(const char *tag) {
  // Affiche le TAG (ex: TEMP) sans bloquer la boucle principale.
  display4(tag);
  tagDisplayActive = true;
  tagDisplayUntilMs = millis() + 600;
}

int estimatePpmFromGasModel(float gasOhmFiltered, float gasOhmBaseline, float tempC, float humPct) {
  // Heuristique eCO2:
  // - ratio gaz baseline / gaz courant
  // - conversion non-lineaire pour mieux suivre les degradations
  // - compensation temperature/humidite a faible amplitude
  if (gasOhmFiltered <= 1.0f || gasOhmBaseline <= 1.0f) return 400;

  float ratio = gasOhmBaseline / gasOhmFiltered;
  if (ratio < 0.5f) ratio = 0.5f;
  if (ratio > 6.0f) ratio = 6.0f;

  // Courbe non-lineaire plus sensible:
  // eCO2 ~= 400 * ratio^k avec k ajuste pour mieux suivre les hausses marquées.
  // Ex: ratio 1.25 donne environ ~1150 ppm.
  const float k = 5.0f;
  float ppm = 400.0f * powf(ratio, k);

  // Compensation T/H moderee (on garde un impact faible).
  ppm += (humPct - 40.0f) * 1.6f;
  ppm += (tempC - 22.0f) * 3.0f;

  if (ppm < 400.0f) ppm = 400.0f;
  if (ppm > 5000.0f) ppm = 5000.0f;
  return (int)round(ppm);
}

void updateButtons() {
  // Lit les boutons avec debounce et genere des evenements "front descendant".
  uint32_t nowMs = millis();
  for (uint8_t i = 0; i < 6; i++) {
    bool reading = digitalRead(BTN_PINS[i]);
    if (reading != btnLastReading[i]) {
      btnLastChangeMs[i] = nowMs;
      btnLastReading[i] = reading;
    }

    if ((nowMs - btnLastChangeMs[i]) > DEBOUNCE_MS) {
      if (btnStableState[i] != reading) {
        btnStableState[i] = reading;
        if (btnStableState[i] == LOW) {
          btnPressedEvent[i] = true;
        }
      }
    }
  }
}

bool consumePress(uint8_t idx) {
  // Consomme un evenement d'appui unique (une fois par clic).
  if (!btnPressedEvent[idx]) return false;
  btnPressedEvent[idx] = false;
  return true;
}

void loadConfigFromRtc(const DateTime &now) {
  // Copie l'heure RTC courante dans les variables de configuration.
  cfgYear = now.year();
  cfgMonth = now.month();
  cfgDay = now.day();
  cfgHour = now.hour();
  cfgMinute = now.minute();
  cfgSecond = now.second();
}

void applyConfigToRtc() {
  // Ecrit les valeurs configurees dans le module RTC.
  uint8_t maxDay = daysInMonth(cfgMonth, cfgYear);
  if (cfgDay > maxDay) cfgDay = maxDay;
  rtc.adjust(DateTime(cfgYear, cfgMonth, cfgDay, cfgHour, cfgMinute, cfgSecond));
}

void displayConfigField() {
  // Affiche le champ en cours de reglage:
  // DD--, MM--, HH--, --MM, SS--
  alpha4.clear();
  switch (configField) {
    case 0: { // DD--
      alpha4.writeDigitAscii(0, (char)('0' + (cfgDay / 10)));
      alpha4.writeDigitAscii(1, (char)('0' + (cfgDay % 10)));
      alpha4.writeDigitAscii(2, '-');
      alpha4.writeDigitAscii(3, '-');
      break;
    }
    case 1: { // MM--
      alpha4.writeDigitAscii(0, (char)('0' + (cfgMonth / 10)));
      alpha4.writeDigitAscii(1, (char)('0' + (cfgMonth % 10)));
      alpha4.writeDigitAscii(2, '-');
      alpha4.writeDigitAscii(3, '-');
      break;
    }
    case 2: { // HH--
      alpha4.writeDigitAscii(0, (char)('0' + (cfgHour / 10)));
      alpha4.writeDigitAscii(1, (char)('0' + (cfgHour % 10)));
      alpha4.writeDigitAscii(2, '-');
      alpha4.writeDigitAscii(3, '-');
      break;
    }
    case 3: { // --MM
      alpha4.writeDigitAscii(0, '-');
      alpha4.writeDigitAscii(1, '-');
      alpha4.writeDigitAscii(2, (char)('0' + (cfgMinute / 10)));
      alpha4.writeDigitAscii(3, (char)('0' + (cfgMinute % 10)));
      break;
    }
    case 4: { // SS--
      alpha4.writeDigitAscii(0, (char)('0' + (cfgSecond / 10)));
      alpha4.writeDigitAscii(1, (char)('0' + (cfgSecond % 10)));
      alpha4.writeDigitAscii(2, '-');
      alpha4.writeDigitAscii(3, '-');
      break;
    }
  }
  alpha4.writeDisplay();
}

void updateEnvironmentRead() {
  // Fait une lecture BME680 periodique (toutes les ENV_READ_MS).
  uint32_t nowMs = millis();
  if (nowMs - lastEnvReadMs < ENV_READ_MS) return;
  lastEnvReadMs = nowMs;

  if (!bme.performReading()) {
    // Si plusieurs lectures consecutives echouent, on reinitialise le BME680.
    envFailCount++;
    if (envFailCount >= 3) {
      bool ok = bme.begin(0x76);
      if (ok) {
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150);
      }
      envValid = false;
      envFailCount = 0;
    }
    return;
  }

  envFailCount = 0;
  lastEnvSuccessMs = nowMs;
  envValid = true;

  tempRawC = bme.temperature;
  tempCompC = tempRawC + TEMP_OFFSET_C;
  envHumPct = bme.humidity;
  envPressureHpa = bme.pressure / 100.0f;
  envGasOhm = bme.gas_resistance;

  // 1) Filtrage du signal gaz (anti-bruit)
  if (!gasFilterValid) {
    gasFilteredOhm = (float)envGasOhm;
    gasFilterValid = true;
  } else {
    gasFilteredOhm = (gasFilteredOhm * 0.85f) + ((float)envGasOhm * 0.15f);
  }

  // 2) Baseline adaptative "air propre observe"
  //    - Warmup: pendant ~3 min, on construit une baseline stable
  //    - Ensuite: baseline ne redescend plus (evite la chute artificielle des PPM)
  if (!gasBaselineValid) {
    gasBaselineOhm = gasFilteredOhm;
    gasBaselineValid = true;
  } else {
    uint32_t ageMs = millis() - baselineStartMs;
    if (ageMs < 180000UL) {
      // Warmup: suit plus vite les meilleures valeurs observees.
      if (gasFilteredOhm > gasBaselineOhm) {
        gasBaselineOhm = (gasBaselineOhm * 0.70f) + (gasFilteredOhm * 0.30f);
      }
    } else {
      // Regime normal: baseline monotone (pas de derive descendante).
      if (gasFilteredOhm > gasBaselineOhm) {
        gasBaselineOhm = (gasBaselineOhm * 0.95f) + (gasFilteredOhm * 0.05f);
      }
    }
  }

  // 3) Estimation eCO2
  envPpmEstimate = estimatePpmFromGasModel(gasFilteredOhm, gasBaselineOhm, tempCompC, envHumPct);

  // Si l'air redevient bon, on rearme l'alerte.
  if (envPpmEstimate < 1000) {
    alertSuppressed = false;
  }
}

void renderRingClock(const DateTime &now) {
  // Dessine l'horloge analogique sur 60 LEDs:
  // - seconde (bleu, trainée cumulee de 0 a seconde courante)
  // - minute (vert)
  // - heure (rouge)
  // Couleurs de chevauchement:
  // - heure + minute = jaune
  // - heure + seconde = magenta
  // - minute + seconde = cyan
  // - heure + minute + seconde = blanc
  uint8_t hourPos = (uint8_t)((now.hour() % 12) * 5);
  uint8_t minPos = (uint8_t)now.minute();
  uint8_t sec = (uint8_t)now.second();

  ring.clear();
  for (uint8_t i = 0; i < RING_LED_COUNT; i++) {
    bool secOn = (i <= sec);       // trainée seconde: 0..sec (reset naturel a 00)
    bool minOn = (i == minPos);
    bool hourOn = (i == hourPos);

    uint8_t mask = (hourOn ? 0x1 : 0x0) | (minOn ? 0x2 : 0x0) | (secOn ? 0x4 : 0x0);
    switch (mask) {
      case 0x1: ring.setPixelColor(i, ring.Color(COL_R, 0, 0)); break;              // heure
      case 0x2: ring.setPixelColor(i, ring.Color(0, COL_G, 0)); break;              // minute
      case 0x4: ring.setPixelColor(i, ring.Color(0, 0, COL_B)); break;              // seconde
      case 0x3: ring.setPixelColor(i, ring.Color(COL_R, COL_G, 0)); break;            // heure+minute
      case 0x5: ring.setPixelColor(i, ring.Color(COL_R, 0, COL_B)); break;            // heure+seconde
      case 0x6: ring.setPixelColor(i, ring.Color(0, 110, 35)); break;             // minute+seconde (cyan tire vers vert)
      case 0x7: ring.setPixelColor(i, ring.Color(COL_R, COL_G, COL_B)); break;          // les 3
      default: break;
    }
  }

  ring.show();
}

void renderRingAlertBlink() {
  // Alerte visuelle: ring entier rouge clignotant ON/OFF chaque seconde.
  bool on = ((millis() / 200UL) % 2UL) == 0UL;
  ring.setBrightness(255);
  if (on) {
    for (uint8_t i = 0; i < RING_LED_COUNT; i++) {
      ring.setPixelColor(i, ring.Color(255, 0, 0));
    }
  } else {
    ring.clear();
  }
  ring.show();
}

bool shouldRenderRing(const DateTime &now, bool alertActive) {
  if (alertActive) {
    uint32_t phase = (millis() / 200UL) % 2UL;
    if (phase != alertBlinkPhase) {
      alertBlinkPhase = phase;
      return true;
    }
    return false;
  }

  if (lastRenderedSecond != now.second() ||
      lastRenderedMinute != now.minute() ||
      lastRenderedHour != now.hour()) {
    lastRenderedSecond = now.second();
    lastRenderedMinute = now.minute();
    lastRenderedHour = now.hour();
    lastRenderedAlertOn = false;
    ring.setBrightness(brightnessValues[brightnessStep]);
    return true;
  }

  return false;
}

void updateDisplay(const DateTime &now) {
  // En mode config: afficher uniquement le champ de reglage.
  if (configMode) {
    displayConfigField();
    return;
  }

  // En mode normal: rotation automatique toutes les 10s.
  uint32_t nowMs = millis();
  if ((nowMs - lastDisplaySwitchMs) >= DISPLAY_ROTATE_MS) {
    lastDisplaySwitchMs = nowMs;
    currentDisplayItem = (DisplayItem)((currentDisplayItem + 1) % DISP_COUNT);
    showTagForCurrentItem = true;
    tagDisplayActive = false;
  }

  // Si un TAG est en cours d'affichage, on attend sa fin (non bloquant).
  if (tagDisplayActive) {
    if ((int32_t)(nowMs - tagDisplayUntilMs) < 0) {
      return;
    }
    tagDisplayActive = false;
  }

  switch (currentDisplayItem) {
    case DISP_AIR:
      if (showTagForCurrentItem) {
        showTagThenValue("eCO2");
        showTagForCurrentItem = false;
        return;
      }
      if (envValid) displayNumber4(envPpmEstimate);
      else display4("----");
      break;
    case DISP_TEMP:
      if (showTagForCurrentItem) {
        showTagThenValue("TEMP");
        showTagForCurrentItem = false;
        return;
      }
      displayTemp(tempCompC);
      break;
    case DISP_HUM:
      if (showTagForCurrentItem) {
        showTagThenValue("HUMD");
        showTagForCurrentItem = false;
        return;
      }
      displayHumidity(envHumPct);
      break;
    case DISP_ATMO:
      if (showTagForCurrentItem) {
        showTagThenValue("ATMO");
        showTagForCurrentItem = false;
        return;
      }
      if (envValid) displayNumber4((int)round(envPressureHpa));
      else display4("----");
      break;
    case DISP_DATE:
      if (showTagForCurrentItem) {
        showTagThenValue("DATE");
        showTagForCurrentItem = false;
        return;
      }
      displayDateDDMM(now);
      break;
    default:
      display4("----");
      break;
  }
}

void onMenuButton(const DateTime &now) {
  // D2:
  // - entree mode config
  // - validation champ suivant
  // - sortie mode config apres le mois
  if (!configMode) {
    configMode = true;
    configField = 0;
    loadConfigFromRtc(now);
    return;
  }

  if (configField < 4) {
    configField++;
  } else {
    applyConfigToRtc();
    configMode = false;
    configField = 0;
    lastDisplaySwitchMs = millis();
    showTagForCurrentItem = true;
    tagDisplayActive = false;
  }
}

void adjustCurrentField(int delta) {
  // D3/D4: incremente/decremente le champ actif
  // en respectant les bornes du cahier des charges.
  switch (configField) {
    case 0: { // day
      uint8_t maxDay = daysInMonth(cfgMonth, cfgYear);
      cfgDay += delta;
      if (cfgDay < 1) cfgDay = 1;
      if (cfgDay > maxDay) cfgDay = maxDay;
      break;
    }
    case 1: // month
      cfgMonth += delta;
      if (cfgMonth < 1) cfgMonth = 1;
      if (cfgMonth > 12) cfgMonth = 12;
      if (cfgDay > daysInMonth(cfgMonth, cfgYear)) {
        cfgDay = daysInMonth(cfgMonth, cfgYear);
      }
      break;
    case 2: // hour
      cfgHour += delta;
      if (cfgHour < 0) cfgHour = 0;
      if (cfgHour > 23) cfgHour = 23;
      break;
    case 3: // minute
      cfgMinute += delta;
      if (cfgMinute < 0) cfgMinute = 0;
      if (cfgMinute > 59) cfgMinute = 59;
      break;
    case 4: // second
      cfgSecond += delta;
      if (cfgSecond < 0) cfgSecond = 0;
      if (cfgSecond > 59) cfgSecond = 59;
      break;
  }
}

void cycleBrightness() {
  // D7: cycle 100% -> 75% -> 50% -> 25% -> ...
  brightnessStep = (uint8_t)((brightnessStep + 1) % 4);
  ring.setBrightness(brightnessValues[brightnessStep]);
}

void handleButtons(const DateTime &now) {
  // Route chaque bouton vers son action metier.
  if (consumePress(0)) {
    onMenuButton(now);
  }

  if (consumePress(1) && configMode) {
    adjustCurrentField(+1);
  }

  if (consumePress(2) && configMode) {
    adjustCurrentField(-1);
  }

  if (consumePress(3) && !configMode) {
    currentDisplayItem = (DisplayItem)((currentDisplayItem + 1) % DISP_COUNT);
    lastDisplaySwitchMs = millis();
    showTagForCurrentItem = true;
    tagDisplayActive = false;
  }

  if (consumePress(4)) {
    cycleBrightness();
  }

  if (consumePress(5)) {
    alertSuppressed = true;
  }
}

void setup() {
  // Initialisation generale du systeme.
  Serial.begin(9600);
  Wire.begin();

  for (uint8_t i = 0; i < 6; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
  }

  ring.begin();
  ring.setBrightness(brightnessValues[brightnessStep]);
  ring.clear();
  ring.show();

  alpha4.begin(0x70);
  display4("BOOT");

  if (!rtc.begin()) {
    // Blocage volontaire: sans RTC, l'horloge ne peut pas fonctionner correctement.
    display4("RTC!");
    while (true) { delay(100); }
  }

  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  if (!bme.begin(0x76)) {
    // Blocage volontaire: sans capteur, le mode environnement n'est pas exploitable.
    display4("BME!");
    while (true) { delay(100); }
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  lastDisplaySwitchMs = millis();
  lastEnvReadMs = 0;
  lastEnvSuccessMs = millis();
  baselineStartMs = millis();
  showTagForCurrentItem = true;
  tagDisplayActive = false;
}

void loop() {
  // Boucle principale non bloquante (sauf petit delay de stabilite).
  updateButtons();
  DateTime now = rtc.now();

  handleButtons(now);
  updateEnvironmentRead();
  updateDisplay(now);

  bool alertActive = (!configMode) && (envPpmEstimate >= 1000) && (!alertSuppressed);
  if (shouldRenderRing(now, alertActive)) {
    if (alertActive) {
      renderRingAlertBlink();
    } else {
      renderRingClock(now);
    }
  }

  delay(10);
}
