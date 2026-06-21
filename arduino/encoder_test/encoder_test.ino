/*
 * encoder_test.ino  -  Standalone encoder bring-up / wiring test.
 *
 * Verifies the 17E1K-07 incremental encoder (via the AM26LS32AC line receiver)
 * on the same pins and with the same x4 quadrature decode as the main firmware,
 * so a good result here means the real firmware's encoder will work too.
 *
 * Board: Arduino UNO R4 Minima.  No external libraries required.
 *
 * Wiring (see docs/WIRING.md):
 *   AM26LS32AC 1Y (pin 3) -> D2   (encoder channel A, TTL)
 *   AM26LS32AC 2Y (pin 5) -> D3   (encoder channel B, TTL)
 *   receiver + encoder powered from 5 V, grounds common.
 *
 * Open the Serial Monitor at 115200. It prints the live count whenever it
 * changes, plus revolutions and degrees. Turn the shaft by hand:
 *   - count should rise turning one way, fall the other;
 *   - one full turn should change the count by ~4000 (CPR).
 * Send 'z' (newline) in the monitor to zero the count.
 * If the count jumps erratically or only changes by 1/2 per detent, check the
 * A/B wiring and the differential pairs / terminations.
 */
#include <Arduino.h>
#include <limits.h>   // LONG_MIN

// ---- Config (match the main firmware) -------------------------------------
constexpr uint8_t PIN_ENC_A = 2;
constexpr uint8_t PIN_ENC_B = 3;
constexpr long    ENCODER_CPR = 4000;      // 1000 PPR x4
constexpr int     ENC_FORWARD_SIGN = +1;   // flip to -1 if "forward" counts down
constexpr uint32_t PRINT_INTERVAL_MS = 100;

// ---- Fast, atomic both-channel read (UNO R4 Minima) -----------------------
// digitalRead() on the RA4M1 is slow (~1 us) and reads one pin at a time, which
// lengthens the ISR and can "tear" the A/B pair when they change close together
// (e.g. a geared encoder spun fast). Read both channels from the PORT1 input
// register in a single access instead.  D2 = P1_04, D3 = P1_05 -> bits 4 and 5.
static_assert(PIN_ENC_A == 2 && PIN_ENC_B == 3,
              "Fast read assumes D2/D3 (PORT1 bits 4/5); update readAB() if you move the pins");

static inline uint8_t readAB() {
  uint16_t p = R_PORT1->PIDR;                  // one atomic snapshot of the port
  return (uint8_t)((((p >> 4) & 1u) << 1) | ((p >> 5) & 1u));
}

// ---- x4 quadrature decode (same table as encoder.cpp) ---------------------
static const int8_t kTable[16] = {
   0, -1, +1,  0,
  +1,  0,  0, -1,
  -1,  0,  0, +1,
   0, +1, -1,  0
};

volatile long    g_count = 0;
volatile uint8_t g_prev = 0;
volatile long    g_errors = 0;   // invalid-transition counter (noise indicator)

inline void update() {
  uint8_t curr = readAB();
  uint8_t idx  = (uint8_t)((g_prev << 2) | curr);
  int8_t  delta = kTable[idx];
  // A "2" change (both bits flipped at once) lands on a 0 table entry and is a
  // merged/missed edge -> count it as an error for diagnostics.
  if (delta == 0 && curr != g_prev) g_errors++;
  g_count += (long)delta * ENC_FORWARD_SIGN;
  g_prev = curr;
}

void isr() { update(); }

long readCount() {
  return g_count;   // 32-bit load is atomic vs. the ISR on the Cortex-M4
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { /* wait briefly for the monitor */ }

  pinMode(PIN_ENC_A, INPUT);
  pinMode(PIN_ENC_B, INPUT);

  g_prev = readAB();

  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), isr, CHANGE);

  Serial.println(F("=== Encoder test ==="));
  // Cross-check the fast port read against digitalRead so a wrong port mapping
  // would be obvious here (the two A/B pairs must match).
  uint8_t fa = (g_prev >> 1) & 1, fb = g_prev & 1;
  Serial.print(F("Initial A=")); Serial.print(fa);
  Serial.print(F(" B=")); Serial.print(fb);
  Serial.print(F("  (digitalRead A=")); Serial.print(digitalRead(PIN_ENC_A));
  Serial.print(F(" B=")); Serial.print(digitalRead(PIN_ENC_B));
  Serial.println(F(")"));
  Serial.println(F("Turn the shaft. Send 'z' to zero. One rev ~= 4000 counts."));
}

void loop() {
  // Serial command: 'z' zeroes the count.
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'z' || c == 'Z') {
      noInterrupts(); g_count = 0; g_errors = 0; interrupts();
      Serial.println(F("-> zeroed"));
    }
  }

  static long lastShown = LONG_MIN;
  static uint32_t lastPrint = 0;
  uint32_t now = millis();

  long count = readCount();
  if (count != lastShown && (now - lastPrint) >= PRINT_INTERVAL_MS) {
    lastPrint = now;
    lastShown = count;

    float revs = (float)count / (float)ENCODER_CPR;
    float deg  = revs * 360.0f;

    Serial.print(F("count="));   Serial.print(count);
    Serial.print(F("  revs=")); Serial.print(revs, 3);
    Serial.print(F("  deg="));  Serial.print(deg, 1);
    if (g_errors) { Serial.print(F("  invalid_edges=")); Serial.print(g_errors); }
    Serial.println();
  }
}
