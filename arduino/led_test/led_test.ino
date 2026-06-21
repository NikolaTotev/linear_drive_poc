/*
 * led_test.ino  -  Standalone WS2812 status-LED test.
 *
 * Board: Arduino UNO R4 Minima. Same pin, brightness, colours and animations
 * as the main firmware (config.h / led.cpp), so this confirms the LED works and
 * the operating states are visually distinct.
 *
 * Requires the Adafruit_NeoPixel library (Library Manager -> "Adafruit NeoPixel").
 *
 * Wiring (see docs/WIRING.md): DIN -> D11, VCC -> 5 V, GND -> GND.
 *
 * By default it auto-cycles through all states (~3 s each), printing the name.
 * --- Serial Monitor: 115200 baud ---
 *   1..5  jump to a state (pauses auto-cycle)   a  resume auto-cycle
 *   w     solid white at full brightness (raw pixel check)
 *   States: 1 NOT_HOMED(blue pulse) 2 IDLE(green) 3 BUSY(green blink)
 *           4 ESTOP(red) 5 ERROR(red blink)
 */
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

constexpr uint8_t  PIN_WS2812   = 11;
constexpr uint16_t WS2812_COUNT = 1;
constexpr uint8_t  BRIGHTNESS   = 60;     // matches firmware
constexpr uint32_t AUTO_MS      = 3000;   // time per state when auto-cycling

constexpr uint32_t BLINK_MS = 250;
constexpr uint32_t PULSE_MS = 15;

enum class Mode : uint8_t { NOT_HOMED, IDLE, BUSY, ESTOP, ERROR, WHITE };
const char* kNames[] = {"NOT_HOMED (blue pulse)", "IDLE (green)", "BUSY (green blink)",
                        "ESTOP (red)", "ERROR (red blink)", "WHITE (full)"};

// This pixel is RGB-ordered (red/green came out swapped under NEO_GRB).
// If your colours are wrong, try swapping NEO_RGB <-> NEO_GRB here.
Adafruit_NeoPixel px(WS2812_COUNT, PIN_WS2812, NEO_RGB + NEO_KHZ800);

Mode g_mode = Mode::NOT_HOMED;
bool g_auto = true;
uint32_t g_lastAuto = 0, g_lastAnim = 0;
bool g_blinkOn = true;
int  g_pulse = 0, g_pulseDir = +4;

void write(uint8_t r, uint8_t g, uint8_t b) {
  px.setPixelColor(0, px.Color(r, g, b));
  px.show();
}

void setMode(Mode m) {
  g_mode = m;
  g_blinkOn = true; g_pulse = 0; g_pulseDir = +4;
  Serial.print(F("state: ")); Serial.println(kNames[(int)m]);
}

void updateAnim(uint32_t now) {
  switch (g_mode) {
    case Mode::IDLE:  write(0, 255, 0); break;
    case Mode::ESTOP: write(255, 0, 0); break;
    case Mode::WHITE: write(255, 255, 255); break;

    case Mode::BUSY:
      if (now - g_lastAnim >= BLINK_MS) { g_lastAnim = now; g_blinkOn = !g_blinkOn; write(0, g_blinkOn ? 255 : 0, 0); }
      break;
    case Mode::ERROR:
      if (now - g_lastAnim >= BLINK_MS) { g_lastAnim = now; g_blinkOn = !g_blinkOn; write(g_blinkOn ? 255 : 0, 0, 0); }
      break;
    case Mode::NOT_HOMED:
      if (now - g_lastAnim >= PULSE_MS) {
        g_lastAnim = now;
        int v = g_pulse + g_pulseDir;
        if (v >= 255) { v = 255; g_pulseDir = -4; }
        if (v <= 0)   { v = 0;   g_pulseDir = +4; }
        g_pulse = v; write(0, 0, (uint8_t)g_pulse);
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  px.begin();
  px.setBrightness(BRIGHTNESS);
  write(0, 0, 0);

  Serial.println(F("=== WS2812 LED test ==="));
  Serial.println(F("keys: 1..5 select state, a auto-cycle, w white"));
  setMode(Mode::NOT_HOMED);
  g_lastAuto = millis();
}

void loop() {
  uint32_t now = millis();

  while (Serial.available()) {
    char c = (char)Serial.read();
    switch (c) {
      case '1': g_auto = false; setMode(Mode::NOT_HOMED); break;
      case '2': g_auto = false; setMode(Mode::IDLE);      break;
      case '3': g_auto = false; setMode(Mode::BUSY);      break;
      case '4': g_auto = false; setMode(Mode::ESTOP);     break;
      case '5': g_auto = false; setMode(Mode::ERROR);     break;
      case 'w': case 'W': g_auto = false; setMode(Mode::WHITE); break;
      case 'a': case 'A': g_auto = true; g_lastAuto = now; Serial.println(F("auto-cycle on")); break;
      default: break;
    }
  }

  if (g_auto && (now - g_lastAuto) >= AUTO_MS) {
    g_lastAuto = now;
    // cycle through the five real states (skip WHITE in auto mode)
    Mode next = (g_mode == Mode::ERROR || g_mode == Mode::WHITE)
                  ? Mode::NOT_HOMED : (Mode)((int)g_mode + 1);
    setMode(next);
  }

  updateAnim(now);
}
