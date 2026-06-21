/*
 * motion.h  -  Step generation + high-level motion state machine.
 *
 * Everything here is COOPERATIVE / non-blocking: update() is called every loop
 * iteration and never blocks, so e-stop, status and the comms link stay live
 * while the motor runs. Position is measured from the encoder (open-loop steps,
 * encoder-referenced stop).
 */
#ifndef LD_MOTION_H
#define LD_MOTION_H

#include <Arduino.h>

namespace motion {

enum class State : uint8_t {
  IDLE, HOMING, CALIBRATING, JOGGING, MOVING, ESTOP, ERROR
};

enum class Profile : uint8_t { LIN, TRAP, SIN };

void begin();                 // init pins, load EEPROM (range + profile)
void update(uint32_t now);    // call every loop iteration

// --- Commands (orchestrator calls these AFTER e-stop gating) ---------------
void startHome();
void startCalibrate();
void startJog(bool forward, int speedPct);   // also serves as jog keepalive
void stopJog();
void startMove(float cm, int speedPct);
void stopMotion();                           // graceful STOP (no latch)
void setProfile(Profile p);

// --- E-stop hooks ----------------------------------------------------------
void onEstopEngaged();        // immediate halt, enter ESTOP
void onEstopCleared();        // leave ESTOP

// --- Status accessors ------------------------------------------------------
State    state();
float    posCm();             // -1 if position unknown
bool     homed();
long     rangePulses();       // 0 if not calibrated
Profile  profile();
const char* errCode();        // "NONE" or a code

// --- One-shot events (orchestrator forwards as EVT lines) ------------------
bool consumeCalDone(long& pulses);
bool consumeHomed();
bool consumeLimit(const char*& which);   // "MIN" / "MAX"
bool consumeError(const char*& code);

} // namespace motion

#endif // LD_MOTION_H
