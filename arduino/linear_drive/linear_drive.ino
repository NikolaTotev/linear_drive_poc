/*
 * linear_drive.ino  -  Firmware for the linear drive controller.
 *
 *   Arduino UNO R4 Minima  ->  DM320T stepper driver  ->  17E1K-07 motor
 *   + 1000PPR differential encoder (via AM26LS32AC)
 *   + 2x D3V endstops, latching e-stop button, WS2812 status LED
 *   + UART link (Serial1 / D0-D1) to a Pico-W that bridges to a Flutter app.
 *
 * The whole program is a single cooperative loop: nothing blocks, so e-stop,
 * limit switches and status reporting stay responsive while the motor runs.
 * See docs/PROTOCOL.md for the command/status contract and config.h for tuning.
 */
#include "config.h"
#include "encoder.h"
#include "safety.h"
#include "led.h"
#include "motion.h"
#include "protocol.h"

static void updateLed() {
  led::Mode m;
  if (safety::estopLatched())                m = led::Mode::ESTOP;
  else if (motion::state() == motion::State::ERROR) m = led::Mode::ERROR;
  else {
    switch (motion::state()) {
      case motion::State::HOMING:
      case motion::State::CALIBRATING:
      case motion::State::JOGGING:
      case motion::State::MOVING:  m = led::Mode::BUSY; break;
      default: m = motion::homed() ? led::Mode::IDLE : led::Mode::NOT_HOMED; break;
    }
  }
  led::set(m);
}

void setup() {
  DEBUG.begin(DEBUG_BAUD);
  LINK.begin(LINK_BAUD);

  encoder::begin();
  safety::begin();
  led::begin();
  motion::begin();
  protocol::begin();

  // Current-sensor analog input (ACHS-7121) - provisioned, not yet read by the
  // control loop; over-current/stall handling is added after bench testing.
  analogReadResolution(ADC_BITS);
  pinMode(PIN_ISENSE, INPUT);

  DEBUG.println("Linear Drive firmware ready");

  // Position is unknown at power-up; home automatically unless an e-stop is held.
  if (AUTO_HOME_ON_BOOT && !safety::estopLatched()) {
    motion::startHome();
  }
}

void loop() {
  uint32_t now = millis();

  protocol::poll();        // ingest commands from the app/Pico
  safety::update(now);     // debounce limits + e-stop latch logic
  motion::update(now);     // run the motion state machine + step engine
  protocol::pumpEvents();  // forward e-stop/motion events, sync status
  updateLed();
  led::update(now);
  protocol::sendStatusIfDue(now);
}
