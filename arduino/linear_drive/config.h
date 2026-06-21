/*
 * config.h  -  Central configuration for the Linear Drive firmware.
 *
 * Every value you may need to tune for your physical build lives here.
 * Items marked  >>> SET ME <<<  depend on your mechanics / DIP switches and
 * MUST be checked before trusting "move to position".
 *
 * Target board: Arduino UNO R4 Minima (Renesas RA4M1, 5 V logic).
 */
#ifndef LD_CONFIG_H
#define LD_CONFIG_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Pin assignments (Arduino UNO R4 Minima)
// ---------------------------------------------------------------------------
// Stepper driver (DM320T P1 connector, COMMON-ANODE wiring: tie OPTO->+5V,
// drive PUL/DIR/ENA from these pins through ~220 ohm series resistors so the
// opto current stays under the R4's 8 mA/pin limit. Common-anode makes the
// signals ACTIVE-LOW (see the polarity constants below). See docs/WIRING.md).
constexpr uint8_t PIN_STEP = 7;   // -> PUL  (pulse, active on RISING edge)
constexpr uint8_t PIN_DIR  = 9;   // -> DIR
constexpr uint8_t PIN_ENA  = 8;   // -> ENA  (enabled = ENA_ENABLED_LEVEL). Optional.

// Incremental encoder (after the AM26LS32AC differential line receiver -> TTL).
// Both pins are hardware-interrupt capable on the RA4M1.
constexpr uint8_t PIN_ENC_A = 2;
constexpr uint8_t PIN_ENC_B = 3;

// D3V limit switches, wired COM->GND, NC->pin, using INPUT_PULLUP.
// Fail-safe: a healthy (closed) switch reads LOW; pressed OR broken wire = HIGH.
constexpr uint8_t PIN_LIMIT_MIN = 4;  // endstop at the 0 cm / home end
constexpr uint8_t PIN_LIMIT_MAX = 5;  // endstop at the far end

// Physical e-stop push-button, wired to GND, INPUT_PULLUP (pressed = LOW).
constexpr uint8_t PIN_ESTOP_BTN = 10;

// WS2812 status LED (single pixel) - requires the Adafruit_NeoPixel library.
constexpr uint8_t PIN_WS2812  = 11;
constexpr uint16_t WS2812_COUNT = 1;

// Current sensor (Broadcom ACHS-7121) analog output. Provisioned here; the
// over-current/stall logic is wired into the firmware after bench testing.
constexpr uint8_t PIN_ISENSE = A0;   // ACHS-7121 VOUT -> A0

// ---------------------------------------------------------------------------
// Serial links
// ---------------------------------------------------------------------------
// Serial1 = hardware UART on D0(RX)/D1(TX) -> level shifter -> Pico-W.
// This is SEPARATE from the USB CDC `Serial`, so USB debug stays available.
#define LINK   Serial1            // command/status link to the Pico
#define DEBUG  Serial             // USB serial, human debug only
constexpr uint32_t LINK_BAUD  = 9600;     // Pico link: low baud for a marginal level shifter
constexpr uint32_t DEBUG_BAUD = 115200;   // USB debug (independent of the link)

// ---------------------------------------------------------------------------
// Mechanics  ( >>> SET ME <<< )
// ---------------------------------------------------------------------------
// Total usable travel between the two endstops, in centimetres. Used to convert
// encoder counts <-> cm together with the measured calibration count.
constexpr float TRAVEL_LENGTH_CM = 21.0f;        // >>> SET ME <<<  measure it

// Motor / driver geometry. MICROSTEP must match the DM320T SW4/5/6 setting.
constexpr long FULL_STEPS_PER_REV = 200;         // 1.8 deg motor
constexpr long MICROSTEP          = 800;        // >>> SET ME <<<  DM320T pulses/rev (DIP)
constexpr long ENCODER_PPR        = 1000;        // 17E1K-07 encoder
constexpr long ENCODER_CPR        = 4000;        // x4 quadrature counts/rev

// Driver signal polarity. We wire COMMON-ANODE (OPTO -> +5V), which makes the
// control signals ACTIVE-LOW: pulling a line LOW turns its opto on.
//   common-anode  (OPTO->+5V): STEP_ACTIVE_LEVEL = LOW
//   common-cathode(OPTO->GND): STEP_ACTIVE_LEVEL = HIGH  (and flip the others)
constexpr uint8_t STEP_ACTIVE_LEVEL = LOW;                              // pulse pulls LOW
constexpr uint8_t STEP_IDLE_LEVEL   = (STEP_ACTIVE_LEVEL == HIGH) ? LOW : HIGH;

// ENA is optional. Easiest is to leave it UNCONNECTED (= always enabled). If you
// do connect it, this is the level that enables the drive; for common-anode the
// opto-off level (HIGH) matches the unconnected "enabled" state.
constexpr uint8_t ENA_ENABLED_LEVEL = HIGH;

// Direction conventions. Flip these if the carriage moves the wrong way or the
// encoder counts down while moving "forward" (toward MAX). Set once at bring-up.
constexpr uint8_t DIR_FORWARD  = HIGH;  // DIR level that drives toward MAX
constexpr uint8_t DIR_BACKWARD = LOW;   // DIR level that drives toward MIN
constexpr int     ENC_FORWARD_SIGN = +1; // +1 if count rises moving forward, else -1

// ---------------------------------------------------------------------------
// Speeds & motion (pulse frequencies in Hz)
// ---------------------------------------------------------------------------
// Hardware floor: DM320T needs >= 7.5 us pulse width; we use 8 us high time and
// enforce a >= 16 us total period => ~62 kHz hard ceiling. Keep working speeds
// well below that so the cooperative main loop never starves stepping.
constexpr uint16_t PULSE_HIGH_US        = 8;     // step HIGH time (>= 7.5 us)
constexpr uint32_t STEP_MIN_PERIOD_US   = 16;    // absolute min period (safety cap)

constexpr float JOG_MIN_FREQ  = 200.0f;    // Hz at slider 0 %
constexpr float JOG_MAX_FREQ  = 12000.0f;  // Hz at slider 100 %   ( >>> tune <<< )
constexpr float MOVE_MIN_FREQ = 8000.0f;    // Hz floor for move profiles
constexpr float MOVE_MAX_FREQ = 18000.0f;  // Hz at slider 100 %   ( >>> tune <<< )
// Endstop seek / calibration sweep speeds (motor step Hz). With the gearbox
// reduction the carriage moves slowly even at these rates, so they can be high;
// the engine ramps up to them (MAX_ACCEL_HZ_PER_S) so there's no stall. Lower
// them if the carriage hits the endstops too hard.
constexpr float HOMING_FREQ   = 18000.0f; // endstop seek
constexpr float CAL_FREQ      = 18000.0f; // calibration sweep

// Acceleration cap applied to every speed change (Hz per second). Prevents the
// open-loop stepper from stalling on abrupt speed steps (jog start, etc.).
constexpr float MAX_ACCEL_HZ_PER_S = 32000.0f;   // >>> tune <<<  (jog / move)

// Higher acceleration used only during homing & calibration (unloaded seek),
// so those routines ramp up quickly. The gearbox lowers reflected inertia, so
// this can be aggressive; lower it if the motor stalls during home/cal.
constexpr float SEEK_ACCEL_HZ_PER_S = 24000.0f;  // >>> tune <<<  (home / cal)

// Trapezoidal profile: fraction of the move spent accelerating (and the same
// decelerating). 0.25 => 25 % ramp up, 50 % cruise, 25 % ramp down.
constexpr float TRAP_ACCEL_FRAC = 0.25f;

// ---------------------------------------------------------------------------
// Current sensing  (ACHS-7121: +/-10 A, 185 mV/A, ratiometric to the 5 V rail)
// ---------------------------------------------------------------------------
constexpr int   ADC_BITS = 14;                  // R4 ADC resolution (set in setup)
constexpr long  ADC_FULL = (1L << ADC_BITS);    // 16384 counts (full scale = 5 V)
constexpr float ISENSE_V_PER_A = 0.185f;        // ACHS-7121 sensitivity
// Ratiometric scale: ADC counts per amp. VDD drift cancels because the sensor
// output and the ADC reference are both the 5 V rail.
//   current_A = (adc - zero) / ISENSE_COUNTS_PER_AMP   (zero = quiescent ~mid-scale)
constexpr float ISENSE_COUNTS_PER_AMP = ISENSE_V_PER_A * (float)ADC_FULL / 5.0f; // ~606

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
constexpr uint32_t STATUS_INTERVAL_MS = 100;   // push ST: at ~10 Hz
constexpr uint32_t JOG_KEEPALIVE_MS   = 250;   // auto-stop jog if no JOG within
constexpr uint32_t ESTOP_LONGPRESS_MS = 1500;  // hold to clear a latched e-stop
constexpr uint32_t SWITCH_DEBOUNCE_MS = 20;    // limit + button debounce

// Auto-run a homing routine at power-up (carriage position is unknown on boot).
constexpr bool AUTO_HOME_ON_BOOT = true;

// ---------------------------------------------------------------------------
// EEPROM layout (RA4M1 emulated EEPROM via the Arduino EEPROM library)
// ---------------------------------------------------------------------------
constexpr int      EEPROM_ADDR_MAGIC = 0;   // uint32 validity marker
constexpr int      EEPROM_ADDR_RANGE = 4;   // long, calibrated full-travel counts
constexpr int      EEPROM_ADDR_PROFILE = 8; // uint8, last profile
constexpr uint32_t EEPROM_MAGIC = 0x4C44'0001UL; // "LD" + version

#endif // LD_CONFIG_H
