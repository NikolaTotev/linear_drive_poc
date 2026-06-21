#include "led.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

namespace {
// This pixel is RGB-ordered (red/green came out swapped under NEO_GRB).
// If your colours are wrong, try swapping NEO_RGB <-> NEO_GRB here.
Adafruit_NeoPixel g_px(WS2812_COUNT, PIN_WS2812, NEO_RGB + NEO_KHZ800);
led::Mode g_mode = led::Mode::NOT_HOMED;
uint32_t g_lastAnim = 0;
bool g_blinkOn = true;
uint8_t g_pulse = 0;          // 0..255 ramp for the breathing effect
int8_t  g_pulseDir = +4;

constexpr uint32_t BLINK_MS = 250;
constexpr uint32_t PULSE_MS = 15;

uint32_t g_lastColor = 0xFFFFFFFFu;   // last shown color (force first write)

inline void write(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  if (c == g_lastColor) return;       // no change -> skip show() (and its ~30us interrupts-off)
  g_lastColor = c;
  g_px.setPixelColor(0, g_px.Color(r, g, b));
  g_px.show();
}
} // namespace

namespace led {

void begin() {
  g_px.begin();
  g_px.setBrightness(60);
  write(0, 0, 0);
}

void set(Mode m) {
  if (m == g_mode) return;
  g_mode = m;
  g_blinkOn = true;
  g_pulse = 0;
  g_pulseDir = +4;
}

void update(uint32_t now) {
  switch (g_mode) {
    case Mode::IDLE:  write(0, 255, 0); break;
    case Mode::ESTOP: write(255, 0, 0); break;

    case Mode::BUSY:
      if (now - g_lastAnim >= BLINK_MS) {
        g_lastAnim = now; g_blinkOn = !g_blinkOn;
        write(0, g_blinkOn ? 255 : 0, 0);
      }
      break;

    case Mode::ERROR:
      if (now - g_lastAnim >= BLINK_MS) {
        g_lastAnim = now; g_blinkOn = !g_blinkOn;
        write(g_blinkOn ? 255 : 0, 0, 0);
      }
      break;

    case Mode::NOT_HOMED:
      if (now - g_lastAnim >= PULSE_MS) {
        g_lastAnim = now;
        int v = g_pulse + g_pulseDir;
        if (v >= 255) { v = 255; g_pulseDir = -4; }
        if (v <= 0)   { v = 0;   g_pulseDir = +4; }
        g_pulse = (uint8_t)v;
        write(0, 0, g_pulse);
      }
      break;
  }
}

} // namespace led
