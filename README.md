# Linear Drive

Motorised linear actuator with calibration, homing, jogging, move-to-position,
selectable acceleration profiles, and a latching e-stop — controlled from a
Flutter app over WiFi.

```
Flutter app ──WiFi/TCP──► Pico-W (bridge) ──UART──► Arduino R4 ──► DM320T ──► 17E1K-07
```

## Hardware
Arduino UNO R4 Minima · DM320T digital stepper driver · 17E1K-07 stepper with
1000 PPR differential encoder · AM26LS32AC line receiver · 2× D3V endstops ·
latching e-stop button · WS2812 status LED · Raspberry Pi Pico-W · TXS0108E
level shifter.

**Wiring and DIP-switch settings: [`docs/WIRING.md`](docs/WIRING.md).**
**Comms protocol (the contract between all three): [`docs/PROTOCOL.md`](docs/PROTOCOL.md).**

## Repository layout
| Path | What |
|---|---|
| `arduino/linear_drive/` | Arduino firmware (motion, safety, encoder, e-stop, LED, protocol). |
| `arduino/encoder_test/` | Standalone encoder bring-up test (Serial Monitor). |
| `arduino/stepper_test/` | Standalone stepper test, commanded from the Serial Monitor. |
| `arduino/current_sensor_test/` | Standalone ACHS-7121 current-sensor test (Serial Monitor). |
| `arduino/button_test/` | Standalone limit-switch + e-stop button test (Serial Monitor). |
| `arduino/led_test/` | Standalone WS2812 status-LED test (Serial Monitor). |
| `arduino/uart_test/` | Arduino side of the Pico↔Arduino UART comms test. |
| `firmware/` | Pico-W MicroPython WiFi-AP + TCP↔UART bridge. |
| `firmware/tests/uart_test.py` | Pico side of the Pico↔Arduino UART comms test. |
| `mini_bot_gui/` | Flutter control app (package name kept as `ocadi_bot_gui`). |
| `docs/` | Wiring + protocol docs. |
| `claude_info/` | Component datasheets. |
| `firmware/reference_minibot/` | Previous project's firmware, kept for reference. |

## 1. Arduino firmware
- Open `arduino/linear_drive/linear_drive.ino` in the Arduino IDE.
- Boards Manager: install **Arduino UNO R4 Boards** (Renesas RA4M1).
- Library Manager: install **Adafruit NeoPixel** (the only external dependency).
- Edit the **SET ME** constants at the top of `config.h` (`TRAVEL_LENGTH_CM`,
  `MICROSTEP`, and the direction/encoder-sign constants if needed).
- Select *Arduino UNO R4 Minima* and upload. USB serial (115200) prints debug;
  it is independent of the D0/D1 link to the Pico.

## 2. Pico-W firmware
- Flash MicroPython for Pico-W, then copy `firmware/main.py`, `wifi_ap.py`,
  `bridge.py` to the board (Thonny or `mpremote fs cp`).
- On boot it brings up AP **`LinearDrive_AP`** (password `linear12345`,
  192.168.4.1) and bridges TCP :5000 ↔ UART1 (GP8/GP9).

## 3. Flutter app
```
cd mini_bot_gui
flutter pub get
flutter run        # or: flutter run -d windows / -d android
```
Connect your device/PC to the `LinearDrive_AP` WiFi network first, then press
**Connect** in the app (defaults to 192.168.4.1:5000).

## Operating notes
- The drive **homes automatically on boot** (position is unknown at power-up).
- **Calibrate** once after wiring: it sweeps endstop-to-endstop, stores the
  full-travel encoder count in EEPROM, and reports it to the app. Move-to-position
  needs this.
- The e-stop latches on a short press of the physical button (or the app's
  software e-stop) and clears on a long (≥1.5 s) press of the button / the app's
  clear button.
- An **unexpected** endstop hit during a move forces a re-home (surfaced in the
  app); hitting a limit while jogging just blocks that direction.

## Status / verification
- Pico firmware: `python -m py_compile` clean.
- Flutter app: `flutter analyze` clean (no issues).
- Arduino firmware: reviewed by hand (no `arduino-cli` in the build env) — compile
  in the IDE after installing the Adafruit NeoPixel library.
