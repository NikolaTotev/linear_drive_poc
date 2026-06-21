#include "safety.h"
#include "config.h"

namespace {

// ---- Debounced digital input helper ---------------------------------------
struct Debounced {
  uint8_t pin;
  bool activeHigh;          // true => HIGH means "active/pressed"
  bool stable = false;      // debounced logical state (true = active)
  bool lastRaw = false;
  uint32_t lastChange = 0;

  void begin(uint8_t p, uint8_t mode, bool active_high) {
    pin = p; activeHigh = active_high;
    pinMode(pin, mode);
    bool raw = read_raw();
    stable = raw; lastRaw = raw; lastChange = millis();
  }
  bool read_raw() {
    bool hi = (digitalRead(pin) == HIGH);
    return activeHigh ? hi : !hi;
  }
  // returns true if the stable state changed this call
  bool update(uint32_t now) {
    bool raw = read_raw();
    if (raw != lastRaw) { lastRaw = raw; lastChange = now; }
    if ((now - lastChange) >= SWITCH_DEBOUNCE_MS && stable != raw) {
      stable = raw;
      return true;
    }
    return false;
  }
};

Debounced g_min;     // limit MIN  (NC wiring: pressed/fault => HIGH)
Debounced g_max;     // limit MAX
Debounced g_btn;     // e-stop button (pressed => LOW)

// ---- E-stop latch state ----------------------------------------------------
bool g_swLatched   = false;
bool g_physLatched = false;
bool g_pressConsumed = false;     // this physical press already acted
uint32_t g_pressStart = 0;

bool g_prevCombined = false;
bool g_engagedPending = false;
bool g_clearedPending = false;
safety::EstopSource g_engageSrc = safety::EstopSource::SW;

void refreshCombinedEdges(safety::EstopSource lastEngageSrc) {
  bool combined = g_swLatched || g_physLatched;
  if (combined && !g_prevCombined) { g_engagedPending = true; g_engageSrc = lastEngageSrc; }
  if (!combined && g_prevCombined) { g_clearedPending = true; }
  g_prevCombined = combined;
}

} // namespace

namespace safety {

void begin() {
  g_min.begin(PIN_LIMIT_MIN, INPUT_PULLUP, /*activeHigh=*/true);
  g_max.begin(PIN_LIMIT_MAX, INPUT_PULLUP, /*activeHigh=*/true);
  g_btn.begin(PIN_ESTOP_BTN, INPUT_PULLUP, /*activeHigh=*/false); // pressed = LOW
  g_prevCombined = (g_swLatched || g_physLatched);
}

void update(uint32_t now) {
  g_min.update(now);
  g_max.update(now);

  bool changed = g_btn.update(now);
  bool pressed = g_btn.stable;

  EstopSource lastSrc = g_physLatched ? EstopSource::BTN : EstopSource::SW;

  if (changed) {
    if (pressed) {                       // released -> pressed edge
      g_pressStart = now;
      g_pressConsumed = false;
      if (!g_physLatched) {              // first press latches
        g_physLatched = true;
        g_pressConsumed = true;          // don't let this same hold clear it
        lastSrc = EstopSource::BTN;
      }
    } else {                             // pressed -> released edge
      g_pressConsumed = false;
    }
  }

  // Long-press clear: only when held, latched, and this press hasn't acted yet.
  if (pressed && g_physLatched && !g_pressConsumed &&
      (now - g_pressStart) >= ESTOP_LONGPRESS_MS) {
    g_physLatched = false;
    g_pressConsumed = true;
  }

  refreshCombinedEdges(lastSrc);
}

bool limitMinPressed() { return g_min.stable; }
bool limitMaxPressed() { return g_max.stable; }

bool estopLatched() { return g_swLatched || g_physLatched; }

void commandSoftwareEstop(bool on) {
  if (on == g_swLatched) return;
  g_swLatched = on;
  refreshCombinedEdges(EstopSource::SW);
}

bool consumeEngaged(EstopSource& src) {
  if (!g_engagedPending) return false;
  g_engagedPending = false;
  src = g_engageSrc;
  return true;
}

bool consumeCleared() {
  if (!g_clearedPending) return false;
  g_clearedPending = false;
  return true;
}

} // namespace safety
