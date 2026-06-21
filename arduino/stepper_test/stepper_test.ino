/*
 * stepper_test.ino  -  Standalone stepper bring-up test, driven from the
 * Arduino IDE Serial Monitor over USB (no Pico / no app involved).
 *
 * Board: Arduino UNO R4 Minima. Same STEP/DIR/ENA pins and pulse timing as the
 * main firmware, so confirming movement here validates the real driver wiring.
 * If the encoder (D2/D3) and current sensor (A0) are wired, each move also
 * reports the measured encoder counts and the peak supply current.
 *
 * Wiring (see docs/WIRING.md): D7->PUL, plus DIR & ENA (see pin constants
 * below), each via ~220 ohm; OPTO->+5V (common-anode => active-low signals).
 * Encoder via AM26LS32AC: D2<-1Y, D3<-2Y.
 * Current sensor ACHS-7121: VOUT->A0, IP+/IP- in series with the +24V supply.
 *
 * --- Serial Monitor: set 115200 baud and "Newline" (or "Both NL & CR") ---
 * Commands (case-insensitive, one per line):
 *   ?               show this help
 *   en | dis        enable / disable the driver (ENA)
 *   dir f | dir b   set direction forward (toward MAX) / backward (toward MIN)
 *   spd <hz>        set target step frequency in Hz (e.g. "spd 1600")
 *   acc <hz/s>      set acceleration ramp in Hz/s; 0 = instant (e.g. "acc 8000")
 *   move <steps>    move a (signed) number of steps then stop, e.g. "move -3200"
 *   run [f|b]       run continuously until "stop" (optional direction)
 *   stop            stop motion
 *   zero            zero the encoder count
 *   izero           re-zero the current sensor (driver disabled, no load)
 *   st              print status now (incl. current)
 */
#include <Arduino.h>
#include <math.h>

// ---- Pins (match the main firmware config.h) ------------------------------
constexpr uint8_t PIN_STEP = 7;
constexpr uint8_t PIN_DIR  = 9;
constexpr uint8_t PIN_ENA  = 8;
// Common-anode wiring (OPTO->+5V) => active-low control signals.
constexpr uint8_t STEP_ACTIVE_LEVEL = LOW;    // pulse pulls LOW
constexpr uint8_t STEP_IDLE_LEVEL   = HIGH;
constexpr uint8_t DIR_FORWARD_LEVEL = LOW;    // DIR level toward MAX (flip if reversed)
constexpr uint8_t ENA_ENABLED_LEVEL = HIGH;   // opto-off = enabled (common-anode)

// ---- Pulse timing (DM320T: >=7.5 us width; we use 8 us / 16 us min period) -
constexpr uint16_t PULSE_HIGH_US      = 8;
constexpr uint32_t STEP_MIN_PERIOD_US = 16;   // ~62 kHz ceiling

// ---- Defaults -------------------------------------------------------------
constexpr float DEFAULT_HZ    = 800.0f;
constexpr float DEFAULT_ACCEL = 8000.0f;      // Hz/s; 0 = instant
constexpr float START_HZ      = 100.0f;       // ramp floor / "stopped" threshold

// ---- Optional encoder (D2/D3, x4) for measured feedback -------------------
constexpr uint8_t PIN_ENC_A = 2;
constexpr uint8_t PIN_ENC_B = 3;
static_assert(PIN_ENC_A == 2 && PIN_ENC_B == 3,
              "encoder fast read assumes D2/D3 (PORT1 bits 4/5)");
static const int8_t kTable[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
volatile long g_count = 0;
volatile uint8_t g_prevAB = 0;
inline uint8_t readAB() {
  uint16_t p = R_PORT1->PIDR;
  return (uint8_t)((((p >> 4) & 1u) << 1) | ((p >> 5) & 1u));
}
void encIsr() {
  uint8_t curr = readAB();
  g_count += (long)kTable[(g_prevAB << 2) | curr];
  g_prevAB = curr;
}

// ---- Current sensor (ACHS-7121 on A0, ratiometric) ------------------------
constexpr uint8_t  PIN_ISENSE = A0;
constexpr int      ADC_BITS = 14;
constexpr long     ADC_FULL = (1L << ADC_BITS);                 // 16384 counts @ 5 V
constexpr float    ISENSE_V_PER_A = 0.185f;                     // ACHS-7121 sensitivity
constexpr float    ISENSE_COUNTS_PER_AMP = ISENSE_V_PER_A * (float)ADC_FULL / 5.0f; // ~606
constexpr uint32_t CURRENT_SAMPLE_MS = 2;                       // light cadence; keeps loop fast

float g_iZero = ADC_FULL / 2.0f;   // quiescent ADC level (~mid), set by zeroCurrent()
float g_iEma  = 0.0f;              // smoothed current (A)
float g_iPeak = 0.0f;             // peak |current| since the last move/run start

float readCurrentInst() {          // single-sample instantaneous current (A)
  float adc = (float)analogRead(PIN_ISENSE);
  return (adc - g_iZero) / ISENSE_COUNTS_PER_AMP;
}
void zeroCurrent() {               // call with the driver disabled / no load
  long acc = 0;
  for (int i = 0; i < 1000; i++) acc += analogRead(PIN_ISENSE);
  g_iZero = (float)acc / 1000.0f;
  g_iEma = 0.0f; g_iPeak = 0.0f;
  Serial.print(F("current zeroed: zeroAdc=")); Serial.println(g_iZero, 1);
}

// ---- Step engine (non-blocking, with trapezoidal ramp) --------------------
bool  g_enabled  = false;
bool  g_running  = false;
bool  g_forward  = true;
bool  g_stepHigh = false;
long  g_stepsTarget = -1;     // -1 = continuous
long  g_stepsDone   = 0;
float g_setHz   = DEFAULT_HZ;
float g_curHz   = 0.0f;
float g_accel   = DEFAULT_ACCEL;
uint32_t g_lastStepUs = 0, g_pulseStartUs = 0, g_rampUs = 0;

void setEnabled(bool on) {
  g_enabled = on;
  digitalWrite(PIN_ENA, on ? ENA_ENABLED_LEVEL : !ENA_ENABLED_LEVEL);
}
void setDirection(bool fwd) {
  g_forward = fwd;
  digitalWrite(PIN_DIR, fwd ? DIR_FORWARD_LEVEL : !DIR_FORWARD_LEVEL);
  delayMicroseconds(10);                 // DIR setup before stepping (idle context)
}
void stopMotion() { g_running = false; g_curHz = 0.0f; digitalWrite(PIN_STEP, STEP_IDLE_LEVEL); g_stepHigh = false; }

void startMove(long steps) {
  if (!g_enabled) { Serial.println(F("driver disabled - send 'en' first")); return; }
  if (steps == 0) return;
  setDirection(steps > 0);
  g_stepsTarget = labs(steps);
  g_stepsDone = 0;
  g_iPeak = 0.0f;
  g_curHz = START_HZ;
  g_running = true;
  g_lastStepUs = micros();
}
void startRun() {
  if (!g_enabled) { Serial.println(F("driver disabled - send 'en' first")); return; }
  g_stepsTarget = -1;
  g_stepsDone = 0;
  g_iPeak = 0.0f;
  g_curHz = START_HZ;
  g_running = true;
  g_lastStepUs = micros();
}

void engineUpdate() {
  uint32_t nowUs = micros();

  // drop STEP low after the high time
  if (g_stepHigh && (uint32_t)(nowUs - g_pulseStartUs) >= PULSE_HIGH_US) {
    digitalWrite(PIN_STEP, STEP_IDLE_LEVEL); g_stepHigh = false;
  }
  if (!g_running) return;

  // pick the target this instant (decelerate near the end of a finite move)
  float target = g_setHz;
  if (g_stepsTarget > 0 && g_accel > 0.0f) {
    long remaining = g_stepsTarget - g_stepsDone;
    float decelSteps = (g_curHz * g_curHz) / (2.0f * g_accel);
    if ((float)remaining <= decelSteps) target = START_HZ;
  }

  // ramp current frequency toward target (accel-limited), ~1 kHz update
  uint32_t dtUs = nowUs - g_rampUs;
  if (dtUs >= 1000) {
    g_rampUs = nowUs;
    if (g_accel <= 0.0f) {
      g_curHz = target;                  // instant
    } else {
      float step = g_accel * (dtUs / 1e6f);
      if (g_curHz < target) g_curHz = min(g_curHz + step, target);
      else if (g_curHz > target) g_curHz = max(g_curHz - step, target);
    }
    if (g_curHz < START_HZ) g_curHz = START_HZ;
  }

  // emit a step when due
  if (!g_stepHigh && g_curHz > 0.0f) {
    uint32_t period = (uint32_t)(1e6f / g_curHz);
    if (period < STEP_MIN_PERIOD_US) period = STEP_MIN_PERIOD_US;
    if ((uint32_t)(nowUs - g_lastStepUs) >= period) {
      digitalWrite(PIN_STEP, STEP_ACTIVE_LEVEL);
      g_stepHigh = true; g_pulseStartUs = nowUs; g_lastStepUs = nowUs;
      g_stepsDone++;
      if (g_stepsTarget > 0 && g_stepsDone >= g_stepsTarget) {
        g_running = false;
        Serial.print(F("move done: ")); Serial.print(g_stepsDone);
        Serial.print(F(" steps, encoder=")); Serial.print(g_count);
        Serial.print(F(", Ipeak=")); Serial.print(g_iPeak, 3); Serial.println(F(" A"));
      }
    }
  }
}

void printStatus() {
  Serial.print(F("[")); Serial.print(g_enabled ? F("ENABLED") : F("disabled"));
  Serial.print(F(", ")); Serial.print(g_running ? F("RUNNING") : F("idle"));
  Serial.print(F(", dir=")); Serial.print(g_forward ? F("F") : F("B"));
  Serial.print(F(", spd=")); Serial.print(g_setHz, 0); Serial.print(F("Hz"));
  Serial.print(F(", cur=")); Serial.print(g_curHz, 0); Serial.print(F("Hz"));
  Serial.print(F(", acc=")); Serial.print(g_accel, 0);
  Serial.print(F(", steps=")); Serial.print(g_stepsDone);
  Serial.print(F(", enc=")); Serial.print(g_count);
  Serial.print(F(", I=")); Serial.print(g_iEma, 3); Serial.print(F("A"));
  Serial.print(F(", Ipk=")); Serial.print(g_iPeak, 3); Serial.print(F("A"));
  Serial.println(F("]"));
}

void printHelp() {
  Serial.println(F("cmds: ? | en | dis | dir f|b | spd <hz> | acc <hz/s> | "
                   "move <steps> | run [f|b] | stop | zero | izero | st"));
}

// ---- Serial line parsing --------------------------------------------------
char g_line[48];
uint8_t g_len = 0;

void handleLine(char* s) {
  // lower-case the command in place
  for (char* p = s; *p; ++p) *p = (char)tolower(*p);

  char* cmd = strtok(s, " \t");
  if (!cmd) return;
  char* arg = strtok(nullptr, " \t");

  if (!strcmp(cmd, "?") || !strcmp(cmd, "help")) { printHelp(); return; }
  if (!strcmp(cmd, "en"))  { setEnabled(true);  Serial.println(F("enabled")); return; }
  if (!strcmp(cmd, "dis")) { stopMotion(); setEnabled(false); Serial.println(F("disabled")); return; }
  if (!strcmp(cmd, "stop")){ stopMotion(); Serial.println(F("stopped")); return; }
  if (!strcmp(cmd, "zero")){ noInterrupts(); g_count = 0; interrupts(); Serial.println(F("encoder zeroed")); return; }
  if (!strcmp(cmd, "izero")){ zeroCurrent(); return; }
  if (!strcmp(cmd, "st"))  { printStatus(); return; }

  if (!strcmp(cmd, "dir")) {
    if (arg && arg[0] == 'b') setDirection(false);
    else                      setDirection(true);
    Serial.print(F("dir=")); Serial.println(g_forward ? F("F") : F("B"));
    return;
  }
  if (!strcmp(cmd, "spd")) {
    if (arg) g_setHz = max(START_HZ, (float)atof(arg));
    Serial.print(F("spd=")); Serial.print(g_setHz, 0); Serial.println(F("Hz"));
    return;
  }
  if (!strcmp(cmd, "acc")) {
    if (arg) g_accel = max(0.0f, (float)atof(arg));
    Serial.print(F("acc=")); Serial.println(g_accel, 0);
    return;
  }
  if (!strcmp(cmd, "move")) {
    if (arg) startMove(atol(arg));
    else Serial.println(F("usage: move <signed steps>"));
    return;
  }
  if (!strcmp(cmd, "run")) {
    if (arg && arg[0] == 'b') setDirection(false);
    else if (arg && arg[0] == 'f') setDirection(true);
    startRun();
    Serial.println(F("running (send 'stop')"));
    return;
  }
  Serial.print(F("unknown cmd: ")); Serial.println(cmd);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  pinMode(PIN_STEP, OUTPUT); digitalWrite(PIN_STEP, STEP_IDLE_LEVEL);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ENA, OUTPUT);
  setEnabled(false);            // start safe / disabled
  setDirection(true);

  pinMode(PIN_ENC_A, INPUT);
  pinMode(PIN_ENC_B, INPUT);
  g_prevAB = readAB();
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encIsr, CHANGE);

  analogReadResolution(ADC_BITS);
  pinMode(PIN_ISENSE, INPUT);
  for (int i = 0; i < 16; i++) analogRead(PIN_ISENSE);   // discard after res change

  Serial.println(F("=== Stepper test ==="));
  printHelp();
  Serial.println(F("auto-zeroing current sensor (driver disabled, no load)..."));
  zeroCurrent();
  Serial.println(F("driver starts DISABLED - send 'en' to enable."));
}

void loop() {
  // read serial commands (line-buffered)
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (g_len) { g_line[g_len] = '\0'; handleLine(g_line); g_len = 0; }
    } else if (g_len < sizeof(g_line) - 1) {
      g_line[g_len++] = c;
    }
  }

  engineUpdate();

  // sample current periodically (one cheap read; EMA filter + peak hold)
  static uint32_t lastISample = 0;
  if ((uint32_t)(millis() - lastISample) >= CURRENT_SAMPLE_MS) {
    lastISample = millis();
    float a = readCurrentInst();
    g_iEma += 0.2f * (a - g_iEma);
    if (fabsf(a) > g_iPeak) g_iPeak = fabsf(a);
  }

  // periodic status while running
  static uint32_t lastPrint = 0;
  if (g_running && (millis() - lastPrint) >= 500) { lastPrint = millis(); printStatus(); }
}
