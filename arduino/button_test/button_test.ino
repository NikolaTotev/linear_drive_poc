/*
 * button_test.ino  -  Standalone test for the limit switches and e-stop button.
 *
 * Board: Arduino UNO R4 Minima. Same pins, debounce, and e-stop latch logic as
 * the main firmware (config.h / safety.cpp), so this validates both the wiring
 * and the e-stop UX before running the full firmware. No external resistors
 * needed - everything uses the internal pull-ups.
 *
 * Wiring (see docs/WIRING.md):
 *   Limit MIN -> D4 : D3V COM->GND, NC->D4   (INPUT_PULLUP)
 *   Limit MAX -> D5 : D3V COM->GND, NC->D5   (INPUT_PULLUP)
 *   E-stop    -> D10: button to GND          (INPUT_PULLUP)
 *
 * Limits use fail-safe NC wiring: a healthy/closed switch reads "clear";
 * pressing it OR a broken/disconnected wire reads "TRIGGERED".
 *
 * E-stop latch: a first press latches; while latched, a separate long press
 * (>= 1.5 s) clears it (a single long press from idle only latches).
 *
 * --- Serial Monitor: 115200 baud --- (send any character to print a snapshot)
 */
#include <Arduino.h>

// ---- Config (match config.h) ----------------------------------------------
constexpr uint8_t  PIN_LIMIT_MIN = 4;
constexpr uint8_t  PIN_LIMIT_MAX = 5;
constexpr uint8_t  PIN_ESTOP_BTN = 10;
constexpr uint32_t SWITCH_DEBOUNCE_MS = 20;
constexpr uint32_t ESTOP_LONGPRESS_MS = 1500;

// ---- Debounced input helper -----------------------------------------------
struct Debounced {
  uint8_t pin;
  bool activeHigh;          // true => HIGH means active
  bool stable = false;      // debounced logical state (true = active)
  bool lastRaw = false;
  uint32_t lastChange = 0;

  void begin(uint8_t p, bool active_high) {
    pin = p; activeHigh = active_high;
    pinMode(pin, INPUT_PULLUP);
    bool raw = readRaw();
    stable = raw; lastRaw = raw; lastChange = millis();
  }
  bool readRaw() {
    bool hi = (digitalRead(pin) == HIGH);
    return activeHigh ? hi : !hi;
  }
  bool update(uint32_t now) {        // returns true if the stable state changed
    bool raw = readRaw();
    if (raw != lastRaw) { lastRaw = raw; lastChange = now; }
    if ((now - lastChange) >= SWITCH_DEBOUNCE_MS && stable != raw) {
      stable = raw; return true;
    }
    return false;
  }
};

Debounced g_min, g_max, g_btn;

// ---- E-stop latch state (mirrors safety.cpp) ------------------------------
bool g_latched = false;
bool g_pressConsumed = false;
uint32_t g_pressStart = 0;

void printSnapshot() {
  Serial.print(F("[ MIN="));   Serial.print(g_min.stable ? F("TRIGGERED") : F("clear"));
  Serial.print(F("  MAX="));   Serial.print(g_max.stable ? F("TRIGGERED") : F("clear"));
  Serial.print(F("  BTN="));   Serial.print(g_btn.stable ? F("pressed") : F("released"));
  Serial.print(F("  ESTOP=")); Serial.print(g_latched ? F("LATCHED") : F("clear"));
  Serial.println(F(" ]"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  g_min.begin(PIN_LIMIT_MIN, /*activeHigh=*/true);   // NC fail-safe: pressed = HIGH
  g_max.begin(PIN_LIMIT_MAX, /*activeHigh=*/true);
  g_btn.begin(PIN_ESTOP_BTN, /*activeHigh=*/false);  // pressed = LOW

  Serial.println(F("=== Button / switch test ==="));
  Serial.println(F("Press the limits and the e-stop; send any char for a snapshot."));
  printSnapshot();
}

void loop() {
  uint32_t now = millis();

  if (g_min.update(now))
    Serial.println(g_min.stable ? F("LIMIT MIN: TRIGGERED") : F("LIMIT MIN: clear"));
  if (g_max.update(now))
    Serial.println(g_max.stable ? F("LIMIT MAX: TRIGGERED") : F("LIMIT MAX: clear"));

  // ---- E-stop button + latch state machine ----
  bool changed = g_btn.update(now);
  bool pressed = g_btn.stable;

  if (changed) {
    if (pressed) {                       // released -> pressed
      g_pressStart = now;
      g_pressConsumed = false;
      if (!g_latched) {                  // first press latches
        g_latched = true;
        g_pressConsumed = true;          // this same hold can't also clear it
        Serial.println(F(">>> E-STOP LATCHED (button)"));
      } else {
        Serial.println(F("button pressed (hold >= 1.5 s to clear)"));
      }
    } else {                             // pressed -> released
      g_pressConsumed = false;
      Serial.println(F("button released"));
    }
  }

  // long-press clear: held, latched, and this press hasn't acted yet
  if (pressed && g_latched && !g_pressConsumed &&
      (now - g_pressStart) >= ESTOP_LONGPRESS_MS) {
    g_latched = false;
    g_pressConsumed = true;
    Serial.println(F("<<< E-STOP CLEARED (long press)"));
  }

  // snapshot on any serial input
  while (Serial.available()) { Serial.read(); printSnapshot(); }
}
