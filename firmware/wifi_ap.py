"""Wi-Fi Access Point bring-up for the Linear Drive bridge."""

import network

# --- Config constants (must match docs/PROTOCOL.md) ---
AP_SSID = "LinearDrive_AP"
AP_PASSWORD = "linear12345"
AP_IP = "192.168.4.1"


def start_ap(ssid=AP_SSID, password=AP_PASSWORD):
    """Start the Wi-Fi Access Point and block until it is active.

    Returns the configured network.WLAN AP interface.
    """
    ap = network.WLAN(network.AP_IF)
    ap.config(ssid=ssid, password=password)
    ap.active(True)

    while not ap.active():
        pass

    print("AP up", ap.ifconfig()[0])
    return ap
