#include "motion.h"
#include "config.h"
#include "encoder.h"
#include "safety.h"
#include <EEPROM.h>
#include <math.h>

namespace {

// ===========================================================================
// StepEngine - low-level, non-blocking STEP/DIR pulse generator with a ramped
// frequency so the open-loop stepper never gets an abrupt speed step.
// ===========================================================================
class StepEngine {
public:
  void begin() {
    pinMode(PIN_STEP, OUTPUT); digitalWrite(PIN_STEP, STEP_IDLE_LEVEL);
    pinMode(PIN_DIR, OUTPUT);  digitalWrite(PIN_DIR, DIR_FORWARD);
    pinMode(PIN_ENA, OUTPUT);  digitalWrite(PIN_ENA, ENA_ENABLED_LEVEL);  // enable driver
    _forward = true;
    _lastStepUs = micros();
  }

  void setDirection(bool forward) {
    if (forward == _forward && _dirInit) return;
    _forward = forward; _dirInit = true;
    digitalWrite(PIN_DIR, forward ? DIR_FORWARD : DIR_BACKWARD);
    _dirChangeUs = micros();
    // Guarantee the DM320T's >=5 us direction setup before the next pulse.
    if ((long)(_lastStepUs - _dirChangeUs) < 0) _lastStepUs = _dirChangeUs;
  }
  bool forward() const { return _forward; }

  // Run toward a target frequency (Hz). Engine ramps current -> target.
  void run(float targetHz) {
    _running = true;
    _targetHz = constrainFreq(targetHz);
    if (_curHz < kStartHz) _curHz = kStartHz;   // kick off motion
  }
  void setAccel(float a) { _accel = (a > 0.0f) ? a : MAX_ACCEL_HZ_PER_S; }
  void requestStop() { _targetHz = 0.0f; }      // graceful decel to a halt
  void hardStop() {
    _running = false; _targetHz = 0.0f; _curHz = 0.0f;
    digitalWrite(PIN_STEP, STEP_IDLE_LEVEL); _stepHigh = false;
  }

  bool moving() const { return _running && _curHz > 0.0f; }

  void update(uint32_t /*nowMs*/) {
    uint32_t nowUs = micros();

    // 1) Finish any active pulse (return STEP to idle after PULSE_HIGH_US).
    if (_stepHigh && (uint32_t)(nowUs - _pulseStartUs) >= PULSE_HIGH_US) {
      digitalWrite(PIN_STEP, STEP_IDLE_LEVEL);
      _stepHigh = false;
    }

    // 2) Ramp current frequency toward target (accel-limited).
    uint32_t dtUs = nowUs - _rampUs;
    if (dtUs >= 1000) {                       // update ramp ~1 kHz
      float dt = dtUs / 1e6f;
      _rampUs = nowUs;
      float step = _accel * dt;
      if (_curHz < _targetHz) _curHz = min(_curHz + step, _targetHz);
      else if (_curHz > _targetHz) _curHz = max(_curHz - step, _targetHz);
      if (_running && _targetHz == 0.0f && _curHz <= kStartHz) {
        _running = false; _curHz = 0.0f;      // decel complete
      }
    }

    // 3) Emit a step pulse when due.
    if (_running && _curHz > 0.0f && !_stepHigh) {
      uint32_t period = (uint32_t)(1e6f / _curHz);
      if (period < STEP_MIN_PERIOD_US) period = STEP_MIN_PERIOD_US;
      if ((uint32_t)(nowUs - _lastStepUs) >= period) {
        // honour DIR setup time
        if ((uint32_t)(nowUs - _dirChangeUs) >= 5) {
          digitalWrite(PIN_STEP, STEP_ACTIVE_LEVEL);   // begin pulse (active level)
          _stepHigh = true; _pulseStartUs = nowUs; _lastStepUs = nowUs;
        }
      }
    }
  }

private:
  static constexpr float kStartHz = 60.0f;    // "basically stopped" threshold
  static float constrainFreq(float hz) {
    float maxHz = 1e6f / STEP_MIN_PERIOD_US;
    if (hz > maxHz) hz = maxHz;
    if (hz < 0) hz = 0;
    return hz;
  }
  bool _forward = true, _dirInit = false, _running = false, _stepHigh = false;
  float _curHz = 0.0f, _targetHz = 0.0f;
  float _accel = MAX_ACCEL_HZ_PER_S;
  uint32_t _lastStepUs = 0, _pulseStartUs = 0, _rampUs = 0, _dirChangeUs = 0;
};

// ===========================================================================
// Motion module state
// ===========================================================================
StepEngine eng;

motion::State   g_state   = motion::State::IDLE;
motion::Profile g_profile = motion::Profile::TRAP;
bool g_homed = false;
long g_range = 0;                 // calibrated full-travel encoder counts
const char* g_err = "NONE";

// homing / calibration sub-states
//   H_BACKOFF      : parked on MIN -> move toward MAX until MIN releases
//   H_BACKOFF_MAX  : parked on MAX -> move toward MIN until MAX releases
//   H_SEEK         : seek MIN (zero datum)
enum class Sub : uint8_t { NONE, H_BACKOFF, H_BACKOFF_MAX, H_SEEK, C_SEEK_MIN, C_SEEK_MAX };
Sub g_sub = Sub::NONE;

// Back-off safety: releasing an endstop only needs a small move, so if a
// back-off phase can't clear the switch in this time, something is wrong
// (e.g. direction reversed, stuck switch) -> fault instead of hanging.
uint32_t g_homeBackoffTs = 0;
constexpr uint32_t HOME_BACKOFF_TIMEOUT_MS = 4000;

// jog
bool g_jogFwd = true;
float g_jogHz = 0.0f;
uint32_t g_jogKeepalive = 0;

// move
long  g_moveStart = 0, g_moveTarget = 0;
bool  g_moveFwd = true;
float g_moveMaxHz = 0.0f;
constexpr long MOVE_DEADBAND_COUNTS = 4;

// pending events
bool g_evCalDone = false; long g_evCalPulses = 0;
bool g_evHomed = false;
bool g_evLimit = false; const char* g_evLimitWhich = "MIN";
bool g_evError = false; const char* g_evErrCode = "NONE";

// --- helpers ---------------------------------------------------------------
float pctToFreq(int pct, float lo, float hi) {
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  return lo + (pct / 100.0f) * (hi - lo);
}

void raiseError(const char* code) {
  g_err = code; g_evError = true; g_evErrCode = code;
}

void loadEeprom() {
  uint32_t magic = 0;
  EEPROM.get(EEPROM_ADDR_MAGIC, magic);
  if (magic == EEPROM_MAGIC) {
    EEPROM.get(EEPROM_ADDR_RANGE, g_range);
    uint8_t p = 0; EEPROM.get(EEPROM_ADDR_PROFILE, p);
    if (p <= (uint8_t)motion::Profile::SIN) g_profile = (motion::Profile)p;
    if (g_range < 0) g_range = 0;
  } else {
    g_range = 0;
  }
}
void saveRange(long range) {
  uint32_t magic = EEPROM_MAGIC;
  EEPROM.put(EEPROM_ADDR_MAGIC, magic);
  EEPROM.put(EEPROM_ADDR_RANGE, range);
}
void saveProfile(motion::Profile p) {
  uint32_t magic = EEPROM_MAGIC;
  EEPROM.put(EEPROM_ADDR_MAGIC, magic);
  uint8_t v = (uint8_t)p; EEPROM.put(EEPROM_ADDR_PROFILE, v);
}

// instantaneous target frequency for a move, given fraction f in [0,1]
float moveProfileFreq(float f) {
  if (f < 0) f = 0; if (f > 1) f = 1;
  switch (g_profile) {
    case motion::Profile::LIN:
      return g_moveMaxHz;                       // constant velocity
    case motion::Profile::TRAP: {
      float fa = TRAP_ACCEL_FRAC;
      if (f < fa)        return MOVE_MIN_FREQ + (g_moveMaxHz - MOVE_MIN_FREQ) * (f / fa);
      if (f > 1.0f - fa) return MOVE_MIN_FREQ + (g_moveMaxHz - MOVE_MIN_FREQ) * ((1.0f - f) / fa);
      return g_moveMaxHz;
    }
    case motion::Profile::SIN:                  // smooth single-hump S-curve
      return MOVE_MIN_FREQ + (g_moveMaxHz - MOVE_MIN_FREQ) * sinf((float)M_PI * f);
  }
  return MOVE_MIN_FREQ;
}

void finishToIdle() { g_sub = Sub::NONE; g_state = motion::State::IDLE; }

} // namespace

// ===========================================================================
namespace motion {

void begin() {
  eng.begin();
  loadEeprom();
  g_state = State::IDLE;
}

// --- commands --------------------------------------------------------------
void startHome() {
  if (safety::estopLatched()) return;
  g_err = "NONE";
  eng.setAccel(SEEK_ACCEL_HZ_PER_S);            // fast ramp for the unloaded seek
  g_state = State::HOMING;
  g_homeBackoffTs = millis();
  // If parked on an endstop, back off it first; otherwise seek MIN directly.
  if (safety::limitMaxPressed())      { g_sub = Sub::H_BACKOFF_MAX; eng.setDirection(false); } // toward MIN
  else if (safety::limitMinPressed()) { g_sub = Sub::H_BACKOFF;     eng.setDirection(true);  } // toward MAX
  else                                { g_sub = Sub::H_SEEK;        eng.setDirection(false); } // seek MIN
}

void startCalibrate() {
  if (safety::estopLatched()) return;
  g_err = "NONE";
  eng.setAccel(SEEK_ACCEL_HZ_PER_S);            // fast ramp for the sweep
  g_state = State::CALIBRATING;
  g_sub = Sub::C_SEEK_MIN;
  eng.setDirection(false);                      // seek MIN first
}

void startJog(bool forward, int speedPct) {
  if (safety::estopLatched()) return;
  // Refuse to drive into a pressed limit; opposite direction stays allowed.
  if (forward && safety::limitMaxPressed()) return;
  if (!forward && safety::limitMinPressed()) return;
  if (g_state == State::HOMING || g_state == State::CALIBRATING || g_state == State::MOVING)
    return;                                     // don't interrupt a routine
  g_jogFwd = forward;
  g_jogHz = pctToFreq(speedPct, JOG_MIN_FREQ, JOG_MAX_FREQ);
  g_jogKeepalive = millis();
  eng.setAccel(MAX_ACCEL_HZ_PER_S);             // normal (loaded) accel for jog
  g_state = State::JOGGING;
  eng.setDirection(forward);
}

void stopJog() {
  if (g_state == State::JOGGING) { eng.requestStop(); finishToIdle(); }
}

void startMove(float cm, int speedPct) {
  if (safety::estopLatched()) return;
  if (!g_homed)     { raiseError("NOT_HOMED");     return; }
  if (g_range <= 0) { raiseError("NOT_CALIBRATED"); return; }
  if (cm < 0.0f || cm > TRAVEL_LENGTH_CM) { raiseError("OUT_OF_RANGE"); return; }

  long target = lroundf((cm / TRAVEL_LENGTH_CM) * (float)g_range);
  long cur = encoder::count();
  if (labs(target - cur) <= MOVE_DEADBAND_COUNTS) { return; } // already there

  g_moveStart = cur; g_moveTarget = target;
  g_moveFwd = (target > cur);
  g_moveMaxHz = pctToFreq(speedPct, MOVE_MIN_FREQ, MOVE_MAX_FREQ);
  eng.setAccel(MAX_ACCEL_HZ_PER_S);             // normal (loaded) accel for moves
  g_state = State::MOVING;
  eng.setDirection(g_moveFwd);
}

void stopMotion() {
  if (g_state == State::JOGGING || g_state == State::MOVING) {
    eng.requestStop(); finishToIdle();
  }
}

void setProfile(Profile p) {
  g_profile = p; saveProfile(p);
}

void onEstopEngaged() {
  eng.hardStop();
  g_sub = Sub::NONE;
  g_state = State::ESTOP;
}
void onEstopCleared() {
  if (g_state == State::ESTOP) g_state = State::IDLE;
}

// --- main state machine ----------------------------------------------------
void update(uint32_t now) {
  switch (g_state) {

    case State::ESTOP:
      eng.hardStop();
      break;

    case State::ERROR:
      // residual decel must not drive into a limit
      if ((eng.forward() && safety::limitMaxPressed()) ||
          (!eng.forward() && safety::limitMinPressed())) eng.hardStop();
      break;

    case State::IDLE:
      if ((eng.forward() && safety::limitMaxPressed()) ||
          (!eng.forward() && safety::limitMinPressed())) eng.hardStop();
      break;

    case State::HOMING:
      if (g_sub == Sub::H_BACKOFF_MAX) {            // parked on MAX: move toward MIN until it releases
        eng.setDirection(false);
        eng.run(HOMING_FREQ);
        if (!safety::limitMaxPressed()) { g_sub = Sub::H_SEEK; }  // clear of MAX -> seek MIN (same dir)
        else if ((now - g_homeBackoffTs) > HOME_BACKOFF_TIMEOUT_MS) {
          eng.hardStop(); raiseError("HOME_FAIL"); g_state = State::ERROR; g_sub = Sub::NONE;
        }
      } else if (g_sub == Sub::H_BACKOFF) {         // parked on MIN: move toward MAX until it releases
        eng.setDirection(true);
        eng.run(HOMING_FREQ);
        if (safety::limitMaxPressed() ||            // hit far end (MIN stuck?) -> fault
            (now - g_homeBackoffTs) > HOME_BACKOFF_TIMEOUT_MS) {
          eng.hardStop(); raiseError("HOME_FAIL"); g_state = State::ERROR; g_sub = Sub::NONE;
        } else if (!safety::limitMinPressed()) {
          g_sub = Sub::H_SEEK; eng.setDirection(false);
        }
      } else { // H_SEEK toward MIN
        eng.setDirection(false);
        eng.run(HOMING_FREQ);
        if (safety::limitMaxPressed()) {            // seeking MIN but hit MAX -> direction wrong
          eng.hardStop(); raiseError("HOME_FAIL"); g_state = State::ERROR; g_sub = Sub::NONE;
        } else if (safety::limitMinPressed()) {
          eng.hardStop();
          encoder::zero();
          g_homed = true; g_err = "NONE";
          g_evHomed = true;
          finishToIdle();
        }
      }
      break;

    case State::CALIBRATING:
      if (g_sub == Sub::C_SEEK_MIN) {
        eng.setDirection(false);
        eng.run(CAL_FREQ);
        if (safety::limitMinPressed()) {
          eng.hardStop();
          encoder::zero();                          // zero datum at MIN
          g_sub = Sub::C_SEEK_MAX;
          eng.setDirection(true);
        }
      } else { // C_SEEK_MAX
        eng.setDirection(true);
        eng.run(CAL_FREQ);
        if (safety::limitMaxPressed()) {
          eng.hardStop();
          long range = encoder::count();
          if (range <= 0) {                         // direction/encoder sign wrong
            raiseError("CAL_FAIL"); g_state = State::ERROR; g_sub = Sub::NONE;
          } else {
            g_range = range; saveRange(range);
            g_homed = true; g_err = "NONE";
            g_evCalDone = true; g_evCalPulses = range;
            finishToIdle();
          }
        }
      }
      break;

    case State::JOGGING: {
      // jog keepalive watchdog
      if ((uint32_t)(now - g_jogKeepalive) > JOG_KEEPALIVE_MS) {
        eng.requestStop(); finishToIdle(); break;
      }
      // limit in the jog direction -> stop; that direction stays blocked while
      // the switch is held (startJog refuses it). Opposite direction still ok.
      if ((g_jogFwd && safety::limitMaxPressed()) ||
          (!g_jogFwd && safety::limitMinPressed())) {
        eng.hardStop(); finishToIdle(); break;
      }
      eng.setDirection(g_jogFwd);
      eng.run(g_jogHz);
      break;
    }

    case State::MOVING: {
      long cur = encoder::count();
      bool reached = g_moveFwd ? (cur >= g_moveTarget - MOVE_DEADBAND_COUNTS)
                               : (cur <= g_moveTarget + MOVE_DEADBAND_COUNTS);
      bool nearTarget = labs(cur - g_moveTarget) <= max((long)MOVE_DEADBAND_COUNTS, g_range / 200);

      // Only a limit in our direction of travel matters. A limit we're moving
      // AWAY from (e.g. starting a move while parked on an endstop) is ignored
      // until it releases, so you can always drive back off an endstop.
      bool travelLimit = g_moveFwd ? safety::limitMaxPressed()
                                   : safety::limitMinPressed();
      if (travelLimit) {
        eng.hardStop();
        if (nearTarget) {                           // boundary target reached
          finishToIdle();
        } else {                                    // UNEXPECTED limit -> rehome
          const char* w = g_moveFwd ? "MAX" : "MIN";
          g_evLimit = true; g_evLimitWhich = w;
          g_homed = false; raiseError("UNEXPECTED_LIMIT");
          g_state = State::ERROR; g_sub = Sub::NONE;
        }
        break;
      }

      if (reached) { eng.hardStop(); finishToIdle(); break; }

      float span = (float)(g_moveTarget - g_moveStart);
      float f = (span != 0.0f) ? (float)(cur - g_moveStart) / span : 1.0f;
      eng.setDirection(g_moveFwd);
      eng.run(moveProfileFreq(f));
      break;
    }
  }

  eng.update(now);
}

// --- accessors -------------------------------------------------------------
State   state()       { return g_state; }
bool    homed()       { return g_homed; }
long    rangePulses() { return g_range; }
Profile profile()     { return g_profile; }
const char* errCode() { return g_err; }

float posCm() {
  if (!g_homed || g_range <= 0) return -1.0f;
  return ((float)encoder::count() / (float)g_range) * TRAVEL_LENGTH_CM;
}

// --- events ----------------------------------------------------------------
bool consumeCalDone(long& pulses) {
  if (!g_evCalDone) return false; g_evCalDone = false; pulses = g_evCalPulses; return true;
}
bool consumeHomed() {
  if (!g_evHomed) return false; g_evHomed = false; return true;
}
bool consumeLimit(const char*& which) {
  if (!g_evLimit) return false; g_evLimit = false; which = g_evLimitWhich; return true;
}
bool consumeError(const char*& code) {
  if (!g_evError) return false; g_evError = false; code = g_evErrCode; return true;
}

} // namespace motion
