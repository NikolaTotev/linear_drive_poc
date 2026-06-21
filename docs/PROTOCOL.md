# Linear Drive — Communication Protocol (v1)

This is the **frozen contract** shared by all three codebases:

```
Flutter app  <--TCP/WiFi-->  Pico-W  <--UART-->  Arduino R4 Minima
```

- The **Pico is a transparent bridge**: every full line it receives from the TCP
  client it forwards verbatim to the Arduino over UART, and every full line it
  receives from the Arduino over UART it forwards verbatim to the TCP client.
- All real motion logic, safety, calibration and state live on the **Arduino**.
- The **Flutter app** only sends commands and renders the state the device pushes.

## Framing

- Every message is a single line of **ASCII text terminated by `\n`** (LF).
  An optional `\r` before `\n` must be tolerated and stripped.
- Format: `KEY` or `KEY:field1,field2,...`
- Keys are uppercase. Fields are comma-separated, no spaces. Floats use `.`.
- Lines are case-sensitive for keys. Unknown lines must be ignored (not fatal).
- Max line length: 64 bytes.

## Transport

- **WiFi:** Pico-W runs as an Access Point.
  - SSID: `LinearDrive_AP`  (configurable constant)
  - Password: `linear12345`   (configurable constant, >= 8 chars)
  - Pico AP IP: `192.168.4.1`
- **TCP:** Pico listens on `192.168.4.1:5000`. Single client at a time.
- **UART (Pico <-> Arduino):** 9600 baud, 8N1 (low baud for reliability through the level shifter).
  - Pico: `UART1` TX=GP8, RX=GP9 (3.3 V) through a TXS0108E level shifter.
  - Arduino: `Serial1` RX=D0, TX=D1 (5 V).
  - Pico TX -> level-shift -> Arduino RX (D0); Arduino TX (D1) -> level-shift -> Pico RX.
  - Pico `GP2` -> TXS0108E `OE` (driven high to enable; 10 kΩ pull-down to GND).

## Commands  (App -> Pico -> Arduino)

| Line | Meaning |
|---|---|
| `CAL` | Start the calibration routine (find both endstops, measure full-travel encoder count, save to EEPROM). |
| `HOME` | Start the homing routine (seek the MIN endstop, set zero). Required before MOVE. |
| `JOG:<D>,<S>` | Jog continuously. `D` = `F` (toward MAX) or `B` (toward MIN). `S` = speed 0–100 (int). Must be **re-sent at least every 250 ms while the button is held** (keepalive). |
| `JSTOP` | Stop jogging immediately (graceful decel). |
| `MOVE:<cm>,<S>` | Move to absolute position `cm` (float, from MIN=0) at speed `S` = 0–100. |
| `STOP` | Graceful stop of any active move/jog (decelerate). Does NOT latch. |
| `PROFILE:<P>` | Set acceleration profile. `P` = `LIN` \| `TRAP` \| `SIN`. |
| `ESTOP` | Engage software e-stop. Latches. Halts motion immediately. |
| `ECLR` | Clear the software e-stop (only succeeds if no physical e-stop active). |
| `PING` | Connectivity check. Device replies with a `ST:` line. |

Notes:
- Speed `S` (0–100 %) is mapped on the Arduino between `JOG_MIN_FREQ` and
  `JOG_MAX_FREQ` step pulse frequencies (named constants).
- While e-stop (software or physical) is latched, all motion commands are
  rejected until cleared; the device keeps pushing `ST:` with `estop=1`.
- `JOG` is rejected in the collision direction after a limit is hit, but allowed
  in the opposite direction (see EVT:LIMIT).

## Status  (Arduino -> Pico -> App), pushed ~10 Hz and after every event

```
ST:<state>,<pos_cm>,<homed>,<estop>,<profile>,<range_pulses>,<err>
```

| Field | Type | Values |
|---|---|---|
| `state` | enum | `IDLE` `HOMING` `CALIBRATING` `JOGGING` `MOVING` `ESTOP` `ERROR` |
| `pos_cm` | float | Current carriage position in cm from MIN. `-1` if unknown (not homed). |
| `homed` | int | `0` not homed, `1` homed. |
| `estop` | int | `0` clear, `1` latched (software or physical). |
| `profile` | enum | `LIN` `TRAP` `SIN` (currently selected). |
| `range_pulses` | int | Full-travel encoder count from calibration; `0` if uncalibrated. |
| `err` | string | `NONE`, or an error code (see events). No commas. |

Example: `ST:IDLE,12.34,1,0,TRAP,48213,NONE`

## Events  (Arduino -> Pico -> App), emitted immediately on occurrence

| Line | Meaning |
|---|---|
| `EVT:CAL_DONE,<pulses>` | Calibration finished; full-travel encoder count = `<pulses>`. |
| `EVT:HOMED` | Homing finished successfully; position is now valid (0 cm at MIN). |
| `EVT:LIMIT,<W>` | **Unexpected** endstop hit during motion. `W` = `MIN` or `MAX`. Motion halts; device requires a re-home. App must force the user to re-home. |
| `EVT:ESTOP,<SRC>` | E-stop engaged. `SRC` = `SW` (software) or `BTN` (physical button). |
| `EVT:ECLR` | E-stop cleared, device ready again. |
| `EVT:ERR,<code>` | Generic error. `code` e.g. `NOT_HOMED`, `OUT_OF_RANGE`, `NOT_CALIBRATED`. |

The expected endstop contacts that occur *during* CAL/HOME are part of the
routine and are NOT reported as `EVT:LIMIT`.

## Operating-state LED (WS2812, on the Arduino)

| Device state | Color |
|---|---|
| Not homed / needs action | Blue pulsing |
| Idle, ready | Green solid |
| Moving / jogging / homing / calibrating | Green blinking |
| E-stop latched | Red solid |
| Error (e.g. unexpected limit) | Red blinking |

## Connection lifecycle

1. App connects TCP to `192.168.4.1:5000`.
2. App sends `PING`; device answers with `ST:`.
3. App renders state; pushes commands; renders pushed `ST:`/`EVT:`.
4. On TCP disconnect the Pico drops the client and waits for a new one. The
   Arduino keeps its state; on reconnect the app re-syncs from the next `ST:`.
