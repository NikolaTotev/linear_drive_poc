/*
 * current_sensor_test.ino  -  ACHS-7121 current-sensor bring-up test, driven
 * from the Arduino IDE Serial Monitor over USB.
 *
 * Sensor: Broadcom ACHS-7121-000E, ±10 A, 185 mV/A, 5 V single supply,
 * ratiometric output (zero current ≈ VDD/2). Board: Arduino UNO R4 Minima.
 *
 * Wiring (see the datasheet's typical application circuit):
 *   VDD (pin 8) -> 5 V, with a 0.1 µF bypass cap to GND (pin 5)
 *   GND (pin 5) -> GND  (common with the Arduino)
 *   VOUT(pin 7) -> Arduino A0
 *   FILTER(pin6)-> filter cap to GND (1 nF = 80 kHz; use ~47 nF for a quieter,
 *                  slower reading suited to motor current)
 *   IP+ (1,2) and IP- (3,4): the current path, in SERIES with the load you
 *                  want to measure (e.g. the DM320T's +Vdc supply line).
 *
 * The conduction path is isolated from the signal side, so it is safe to put it
 * in the motor-supply line while the Arduino reads VOUT.
 *
 * Because the sensor is ratiometric to VDD and the R4 ADC reference is also the
 * 5 V rail, current is computed in ADC counts so VDD drift cancels out.
 *
 * --- Serial Monitor: 115200 baud, "Newline" line ending ---
 * Commands:
 *   z   re-zero (call with NO current flowing through the sensor)
 *   ?   help
 */
#include <Arduino.h>

// ---- Config ---------------------------------------------------------------
constexpr uint8_t PIN_ISENSE = A0;          // VOUT -> A0
constexpr int     ADC_BITS   = 14;          // R4 ADC: up to 14-bit
constexpr long    ADC_MAX    = (1L << ADC_BITS) - 1;   // 16383
constexpr float   VREF       = 5.0f;        // ADC full-scale = 5 V rail
constexpr float   SENS_V_PER_A = 0.185f;    // ACHS-7121: 185 mV/A

// Ratiometric scale: ADC counts per amp (independent of exact VDD).
constexpr float COUNTS_PER_AMP = SENS_V_PER_A * (float)(ADC_MAX + 1) / VREF; // ~606

constexpr int      SAMPLES_PER_READ = 64;   // averaged per reported value
constexpr int      ZERO_SAMPLES     = 1000; // averaged for auto-zero
constexpr uint32_t REPORT_MS        = 200;

// ---- State ----------------------------------------------------------------
float g_zeroAdc = (ADC_MAX + 1) / 2.0f;     // quiescent ADC level (~mid-scale)
float g_emaAmps = 0.0f;                      // smoothed current
float g_peakAmps = 0.0f;                     // max |current| since last report
constexpr float EMA_ALPHA = 0.2f;

float readAvgAdc(int samples) {
  long acc = 0;
  for (int i = 0; i < samples; i++) acc += analogRead(PIN_ISENSE);
  return (float)acc / (float)samples;
}

void doZero() {
  g_zeroAdc = readAvgAdc(ZERO_SAMPLES);
  g_emaAmps = 0.0f;
  g_peakAmps = 0.0f;
  Serial.print(F("zeroed: zeroAdc=")); Serial.print(g_zeroAdc, 1);
  Serial.print(F(" (")); Serial.print(g_zeroAdc * VREF / (ADC_MAX + 1), 3);
  Serial.println(F(" V)"));
}

void printHelp() {
  Serial.println(F("ACHS-7121 test  |  cmds: z = re-zero (no current),  ? = help"));
  Serial.print(F("range +/-10 A, 185 mV/A, ~"));
  Serial.print(COUNTS_PER_AMP, 1); Serial.println(F(" counts/A @ 14-bit"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  analogReadResolution(ADC_BITS);
  pinMode(PIN_ISENSE, INPUT);
  // discard the first few conversions after changing resolution
  for (int i = 0; i < 16; i++) analogRead(PIN_ISENSE);

  Serial.println(F("=== Current sensor test (ACHS-7121) ==="));
  printHelp();
  Serial.println(F("Auto-zeroing - ensure NO current is flowing..."));
  doZero();
}

void loop() {
  // serial commands
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'z' || c == 'Z') doZero();
    else if (c == '?')        printHelp();
  }

  float adc = readAvgAdc(SAMPLES_PER_READ);
  float amps = (adc - g_zeroAdc) / COUNTS_PER_AMP;

  g_emaAmps += EMA_ALPHA * (amps - g_emaAmps);
  if (fabsf(amps) > g_peakAmps) g_peakAmps = fabsf(amps);

  static uint32_t lastReport = 0;
  uint32_t now = millis();
  if (now - lastReport >= REPORT_MS) {
    lastReport = now;
    float volts = adc * VREF / (ADC_MAX + 1);
    Serial.print(F("I="));      Serial.print(g_emaAmps, 3); Serial.print(F(" A"));
    Serial.print(F("  (inst=")); Serial.print(amps, 3);     Serial.print(F(" A"));
    Serial.print(F(", peak="));  Serial.print(g_peakAmps, 3); Serial.print(F(" A)"));
    Serial.print(F("  Vout="));  Serial.print(volts, 3);    Serial.print(F(" V"));
    Serial.print(F("  adc="));   Serial.println(adc, 1);
    g_peakAmps = 0.0f;                       // reset peak each window
  }
}
