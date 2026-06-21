"""UART comms test for the Pico-W side of the Linear Drive link.

Pairs with arduino/uart_test/uart_test.ino on the Arduino. Exercises the
GP8/GP9 (UART1) <-> D0/D1 link through the TXS0108E (whose OE is driven high here).

Run it directly in Thonny (Run), or `mpremote run firmware/tests/uart_test.py`,
or copy it to the board as main.py temporarily.

What you should see:
  - this Pico REPL prints lines received from the Arduino (its "ARD:n" counter
    and its "ARD_ECHO:PICO:n" echo of our messages -> proves the round trip);
  - the Arduino's USB monitor prints the "PICO:n" lines we send (proves the
    Pico -> Arduino direction).
"""
import machine
import utime

UART_ID = 1                # GP8/GP9 are on UART1
UART_BAUD = 9600    # must match the Arduino side (LINK_BAUD)
UART_TX_PIN = 8
UART_RX_PIN = 9
OE_PIN = 14                # TXS0108E output-enable (active high, ref 3V3)
SEND_INTERVAL_MS = 1000


def main():
    # Enable the level shifter (held Hi-Z by an external pull-down until now).
    oe = machine.Pin(OE_PIN, machine.Pin.OUT)
    oe.value(1)

    uart = machine.UART(
        UART_ID, baudrate=UART_BAUD, bits=8, parity=None, stop=1,
        tx=machine.Pin(UART_TX_PIN), rx=machine.Pin(UART_RX_PIN),
    )
    print("Pico UART test: GP8/GP9 (UART1) @", UART_BAUD, "baud, OE(GP%d)=1" % OE_PIN)

    counter = 0
    last_send = utime.ticks_ms()
    buf = b""

    while True:
        now = utime.ticks_ms()
        if utime.ticks_diff(now, last_send) >= SEND_INTERVAL_MS:
            last_send = now
            msg = "PICO:%d\n" % counter
            uart.write(msg)
            print("TX->ARD:", msg.strip())
            counter += 1

        n = uart.any()
        if n:
            buf += uart.read(n)
            while b"\n" in buf:
                idx = buf.index(b"\n")
                line = buf[:idx].rstrip(b"\r")
                buf = buf[idx + 1:]
                if line:
                    try:
                        print("RX<-ARD:", line.decode())
                    except Exception:
                        print("RX<-ARD (raw):", line)

        utime.sleep_ms(5)


if __name__ == "__main__":
    main()
