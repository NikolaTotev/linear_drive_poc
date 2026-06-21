"""Entrypoint: Pico-W transparent bridge between Flutter app (TCP/WiFi)
and Arduino (UART). See docs/PROTOCOL.md for the frozen contract.
"""

import machine
import socket

from wifi_ap import start_ap, AP_SSID, AP_PASSWORD
from bridge import Bridge, TCP_HOST, TCP_PORT


# --- UART config (must match docs/PROTOCOL.md) ---
UART_ID = 1         # GP8/GP9 are on UART1
UART_BAUD = 9600    # low baud for a marginal level shifter (must match Arduino LINK_BAUD)
UART_TX_PIN = 8
UART_RX_PIN = 9
OE_PIN = 14          # TXS0108E output-enable (active high, referenced to 3V3)


def make_uart():
    return machine.UART(
        UART_ID,
        baudrate=UART_BAUD,
        bits=8,
        parity=None,
        stop=1,
        tx=machine.Pin(UART_TX_PIN),
        rx=machine.Pin(UART_RX_PIN),
    )


def enable_level_shifter():
    """Drive the TXS0108E OE high to enable the translator.

    OE is active-high and referenced to VCCA (3V3). An external pull-down holds
    it low (outputs Hi-Z) during boot/reset until we explicitly enable it here.
    Returns the pin so the caller keeps a reference to it.
    """
    oe = machine.Pin(OE_PIN, machine.Pin.OUT)
    oe.value(1)
    return oe


def make_listen_socket():
    addr_info = socket.getaddrinfo(TCP_HOST, TCP_PORT)[0][-1]
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(addr_info)
    sock.listen(1)
    sock.setblocking(False)
    print("TCP listening on", addr_info)
    return sock


def main():
    print("Starting", AP_SSID)
    start_ap(AP_SSID, AP_PASSWORD)

    uart = make_uart()
    oe = enable_level_shifter()   # keep a reference so the pin stays configured
    print("level shifter enabled (OE/GP%d high)" % OE_PIN)
    listen_sock = make_listen_socket()

    bridge = Bridge(listen_sock, uart)
    bridge.run_forever()


if __name__ == "__main__":
    main()
