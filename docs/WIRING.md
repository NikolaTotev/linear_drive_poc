# Linear Drive — Wiring & Setup

All component facts below come from the datasheets in `claude_info/`. Values
marked **SET ME** must match your physical build and the firmware `config.h`.

## 1. System overview

```
   Flutter app
       │  WiFi (Pico AP "LinearDrive_AP")
       │  TCP 192.168.4.1:5000
   ┌───┴────────┐   UART 9600 8N1     ┌──────────────────────┐
   │  Pico-W    │◄──via TXS0108E─────►│  Arduino UNO R4 Minima│
   │  (3.3 V)   │   GP8/GP9  ⇄ D0/D1  │  (5 V logic)          │
   └────────────┘                     └───────┬──────────────┘
                                  STEP/DIR/ENA │   ▲ encoder A/B (TTL)
                                   (5 V, +Rs)  ▼   │
                                   ┌───────────────┴──────┐
                                   │   DM320T driver       │   ┌─────────────┐
                                   │   (10–30 VDC, 24 V)   │──►│ 17E1K-07     │
                                   └───────────────────────┘   │ stepper +    │
                                        ▲ AM26LS32AC ◄──────────┤ 1000PPR enc  │
                                        │ (diff→TTL)            └─────────────┘
   D3V endstops ×2, e-stop button, WS2812 → Arduino GPIO
```

## 2. Power rails

| Rail | Source | Feeds |
|---|---|---|
| 24 VDC | PSU (size ≥ motor current; 24 V recommended, DM320T range 10–30 V) | DM320T `+Vdc` |
| 5 V | Arduino R4 `5V` pin (USB/VIN regulator) | AM26LS32AC VCC, encoder VCC, level-shifter VccB, WS2812 |
| 3.3 V | Pico-W `3V3` | level-shifter VccA |
| GND | logic-domain common | tie the logic grounds: Arduino GND, Pico GND, encoder EGND, AM26LS32AC GND, level-shifter GND, current-sensor signal GND |

> The DM320T control inputs (PUL/DIR/ENA/OPTO) are **opto-isolated**, so a common
> ground between the 24 V motor supply and the 5 V logic is **not required** for
> the signals. You can either keep the two domains isolated, or tie the 24 V
> return to logic GND at a single point for a shared reference — both are fine;
> just avoid creating ground loops. (The current sensor's conduction path is
> likewise isolated from its signal side.)

## 3. DM320T stepper driver

### Connector P2 — power & motor
| P2 pin | Connect |
|---|---|
| `GND` | 24 V PSU − (and common ground) |
| `+Vdc` | 24 V PSU + |
| `A+` `A-` | 17E1K-07 motor pin 1 (A+) / pin 2 (A-) |
| `B+` `B-` | 17E1K-07 motor pin 3 (B+) / pin 4 (B-) |

> Never plug/unplug P2 with the driver powered — back-EMF can destroy it.

### Connector P1 — control signals (common-anode wiring with a 5 V Arduino)
The P1 terminals are `PUL`, `DIR`, `OPTO`, `ENA`. The common pin `OPTO` ties to
**+5 V**, so the signals are **active-low** (pulling a line low turns its opto
on). The inputs accept 5 V directly (a series resistor is only needed for
12 V/24 V logic — 1 K / 2 K). **However the R4 GPIO is rated 8 mA/pin and the
opto draws ~10 mA, so add a ~220 Ω series resistor on each signal** to stay in
spec.

| P1 pin | Connect |
|---|---|
| `OPTO` | Arduino **+5 V** (common-anode reference) |
| `PUL`  | Arduino **D7** through 220 Ω  (STEP, active-low pulse) |
| `DIR`  | Arduino **D9** through 220 Ω |
| `ENA`  | **Best left unconnected** (= enabled). If wired: Arduino **D8** through 220 Ω. |

The firmware drives the signals **active-low** to match (`STEP_ACTIVE_LEVEL`,
`DIR_FORWARD`, `ENA_ENABLED_LEVEL` in `config.h`). Pulse timing honoured: ≥ 8 µs
active, ≥ 16 µs period, ≥ 5 µs DIR setup (datasheet minimums 7.5 µs / 5 µs,
60 kHz max).

### DIP switches
**Microstep — SW4/5/6** (Steps/rev for a 1.8° motor). Must equal `MICROSTEP` in `config.h`.

| Steps/rev | SW4 | SW5 | SW6 |
|---:|:--:|:--:|:--:|
| 400  | ON  | ON  | ON  |
| 800  | OFF | ON  | ON  |
| **1600** | **ON** | **OFF** | **ON** | ← default in `config.h` |
| 3200 | OFF | OFF | ON  |
| 6400 | ON  | ON  | OFF |
| 12800| OFF | ON  | OFF |
| 4000 | ON  | OFF | OFF |
| 8000 | OFF | OFF | OFF |

**Current — SW1/2/3** (peak / RMS). Motor is rated 2.0 A/phase.

| Peak | RMS | SW1 | SW2 | SW3 |
|---:|---:|:--:|:--:|:--:|
| 0.3 | 0.21 | ON | ON | ON |
| 0.5 | 0.35 | OFF| ON | ON |
| 0.7 | 0.49 | ON | OFF| ON |
| 1.0 | 0.71 | OFF| OFF| ON |
| 1.3 | 0.92 | ON | ON | OFF|
| 1.6 | 1.13 | OFF| ON | OFF|
| 1.9 | 1.34 | ON | OFF| OFF|
| **2.2** | **1.56** | **OFF** | **OFF** | **OFF** | ← recommended (max torque, still ≤ motor rating) |

> The DM320T's 2.2 A ceiling is below the motor's full bipolar-peak demand, so
> running at the 2.2 A setting is safe (it delivers ~1.57 A RMS, under the
> 2.0 A rating). Drop to 1.9 A if you want the motor/driver to run cooler.
> Standstill current auto-drops to 50 % 0.4 s after the last pulse.

## 4. Encoder via AM26LS32AC (differential → TTL)

17E1K-07 encoder: 1000 PPR (4000 counts/rev quadrature), 5 V supply, differential A/B.
Encoder connector pins (from the motor's encoder pin table):
`VCC=2, EGND=3, EA+=1, EA-=13, EB+=11, EB-=12`.

| AM26LS32AC pin | Connect |
|---|---|
| 16 `VCC` | 5 V |
| 8 `GND` | GND |
| 4 `G` | 5 V (enable) |
| 12 `Ḡ` | GND (enable) |
| 2 `1A` / 1 `1B` | encoder `EA+` / `EA-` |
| 3 `1Y` (out) | Arduino **D2** (encoder A) |
| 6 `2A` / 7 `2B` | encoder `EB+` / `EB-` |
| 5 `2Y` (out) | Arduino **D3** (encoder B) |

- Encoder power: `VCC`→5 V, `EGND`→GND.
- Optional but recommended: a 120 Ω resistor across each differential pair
  (EA+/EA−, EB+/EB−) at the receiver for noise immunity on longer cable runs.
- Outputs are TTL and drive the 5 V Arduino inputs directly.

## 5. Endstops (D3V ×2) — fail-safe NC wiring

Each switch: `COM`→GND, `NC`→Arduino input, pin set to `INPUT_PULLUP`.
A healthy closed switch reads **LOW**; pressing it (or a broken wire) reads
**HIGH** → treated as triggered (fail-safe).

| Switch | Arduino pin |
|---|---|
| MIN endstop (home / 0 cm end) | **D4** |
| MAX endstop (far end) | **D5** |

## 6. E-stop button

Single push-button: one terminal → Arduino **D10**, other → GND. Pin is
`INPUT_PULLUP` (pressed = LOW). Latch logic (firmware): first press latches the
e-stop; while latched, a separate **long press (≥ 1.5 s)** clears it.

## 7. WS2812 status LED

`DIN` → Arduino **D11**, `VCC` → 5 V, `GND` → GND. (Add the usual ~330 Ω in
series on DIN and a 1000 µF cap across 5 V/GND if you chain more pixels.)
Colours: blue pulse = needs homing, green = idle, green blink = busy,
red = e-stop, red blink = error.

## 8. Current sensor (ACHS-7121)

Broadcom ACHS-7121: ±10 A, 185 mV/A, 5 V single supply, ratiometric output
(zero current ≈ VDD/2). The conduction path is isolated from the signal side.

| ACHS-7121 pin | Connect |
|---|---|
| `VDD` (8) | 5 V (with 0.1 µF bypass to GND) |
| `GND` (5) | common GND |
| `VOUT` (7) | Arduino **A0** |
| `FILTER` (6) | capacitor to GND (1 nF = 80 kHz; ~47 nF for a quieter motor-current reading) |
| `IP+` (1,2) / `IP-` (3,4) | **in series with the DM320T `+Vdc` supply line** (1&2 tied, 3&4 tied) |

Put the conduction path in the 24 V supply line to the driver so it reads the
DC bus current (good for load/stall detection). Current is measured on **A0** at
14-bit; since the sensor is ratiometric and the R4 ADC reference is the same 5 V
rail, current is computed in ADC counts so VDD drift cancels. The analog input
is provisioned in `config.h` (`PIN_ISENSE`, `ISENSE_COUNTS_PER_AMP`); the
over-current/stall logic is added to the firmware after bench testing (use
`arduino/current_sensor_test/` first).

## 9. UART link via TXS0108E level shifter

| TXS0108E | Connect |
|---|---|
| `VccA` | Pico 3V3 | (low-voltage side) |
| `VccB` | Arduino 5V | (high-voltage side) |
| `OE`   | Pico **GP2** + a **10 kΩ pull-down to GND** |
| `GND`  | common GND |
| `A1` ⇄ `B1` | Pico **GP8 (UART1 TX)** ⇄ Arduino **D0 (RX)** |
| `A2` ⇄ `B2` | Pico **GP9 (UART1 RX)** ⇄ Arduino **D1 (TX)** |

`OE` is active-high and referenced to VCCA (3.3 V). The Pico drives **GP2 high**
once it is up (`enable_level_shifter()` in `firmware/main.py`); the 10 kΩ
pull-down holds the translator in Hi-Z during the Pico's boot/reset so the link
isn't driven before both rails and the firmware are ready (per the TXS0108E
datasheet's power-sequencing note).

Both boards keep their USB ports free for programming/debug — `Serial1`/D0-D1 on
the R4 and `machine.UART(0)` on the Pico are independent of USB.

## 10. Arduino R4 Minima pin summary

| Pin | Net |
|---|---|
| D0 / D1 | Serial1 RX / TX ↔ Pico (via shifter) |
| D2 / D3 | Encoder A / B (interrupts) |
| D4 / D5 | Limit MIN / MAX (INPUT_PULLUP, NC) |
| D7 / D9 / D8 | STEP / DIR / ENA → DM320T (+220 Ω each) |
| D10 | E-stop button (INPUT_PULLUP) |
| D11 | WS2812 DIN |
| A0 | Current sensor VOUT (ACHS-7121) |
| 5V / GND / 3V3 | rails as above |

## 11. Before first power-on (checklist)

1. Set DM320T DIP switches (microstep + current) per §3; make `MICROSTEP` in
   `config.h` match. **SET ME**
2. Measure travel between endstops; set `TRAVEL_LENGTH_CM`. **SET ME**
3. Confirm all grounds common, 220 Ω resistors on STEP/DIR/ENA.
4. First bring-up: run `HOME`/`CAL`. If the carriage drives the wrong way or
   `CAL` returns `CAL_FAIL`/`HOME_FAIL`, flip `DIR_FORWARD`/`DIR_BACKWARD` or
   `ENC_FORWARD_SIGN` in `config.h` (see comments there).
