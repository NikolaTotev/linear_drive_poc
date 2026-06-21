/*
 * led.h  -  WS2812 operating-state indicator (single pixel).
 *
 * Requires the Adafruit_NeoPixel library (Library Manager -> "Adafruit
 * NeoPixel"). Colour/animation mapping is defined in docs/PROTOCOL.md.
 */
#ifndef LD_LED_H
#define LD_LED_H

#include <Arduino.h>

namespace led {

enum class Mode : uint8_t {
  NOT_HOMED,   // blue, pulsing   - needs homing/action
  IDLE,        // green, solid     - ready
  BUSY,        // green, blinking  - moving/jogging/homing/calibrating
  ESTOP,       // red, solid       - e-stop latched
  ERROR        // red, blinking    - fault (e.g. unexpected limit)
};

void begin();
void set(Mode m);            // change the displayed mode
void update(uint32_t now);   // call every loop to drive animation

} // namespace led

#endif // LD_LED_H
