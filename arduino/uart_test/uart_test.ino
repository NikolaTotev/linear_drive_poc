/*
 * uart_test.ino  -  UART comms test for the Arduino side of the Linear Drive
 * link. Pairs with firmware/tests/uart_test.py on the Pico.
 *
 * Board: Arduino UNO R4 Minima. Uses Serial1 (D0=RX, D1=TX) through the
 * TXS0108E level shifter (the Pico drives the shifter's OE). The USB `Serial`
 * is independent, so the Serial Monitor stays free for observing/typing.
 *
 * --- USB Serial Monitor: 115200 baud, "Newline" line ending ---
 * Behaviour:
 *   - prints every line received from the Pico as "RX<-PICO: ..."
 *   - echoes each received line back to the Pico as "ARD_ECHO:<line>"
 *     (so the Pico can confirm a full round trip),
 *   - sends "ARD:<counter>" to the Pico once per second on its own,
 *   - anything you type in the monitor is sent to the Pico as a line.
 */
#include <Arduino.h>

constexpr uint32_t USB_BAUD  = 115200;   // USB serial monitor
constexpr uint32_t LINK_BAUD = 9600;     // Serial1 to the Pico (must match firmware)
constexpr uint32_t SEND_INTERVAL_MS = 1000;

char linkBuf[80]; uint8_t linkLen = 0;   // line buffer from Serial1 (Pico)
char usbBuf[80];  uint8_t usbLen = 0;    // line buffer from USB monitor

void setup() {
  Serial.begin(USB_BAUD);
  Serial1.begin(LINK_BAUD);               // D0(RX)/D1(TX) -> shifter -> Pico
  while (!Serial && millis() < 3000) {}
  Serial.println(F("=== Arduino UART test (Serial1 D0/D1) ==="));
  Serial.println(F("Shows lines from the Pico; type a line to send to the Pico."));
}

void loop() {
  // Lines from the Pico -> print to USB, echo back to the Pico.
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\n' || c == '\r') {
      if (linkLen) {
        linkBuf[linkLen] = '\0';
        Serial.print(F("RX<-PICO: ")); Serial.println(linkBuf);
        Serial1.print(F("ARD_ECHO:")); Serial1.println(linkBuf);
        linkLen = 0;
      }
    } else if (linkLen < sizeof(linkBuf) - 1) {
      linkBuf[linkLen++] = c;
    }
  }

  // Lines typed in the USB monitor -> forward to the Pico.
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (usbLen) {
        usbBuf[usbLen] = '\0';
        Serial1.print(usbBuf); Serial1.print('\n');
        Serial.print(F("TX->PICO: ")); Serial.println(usbBuf);
        usbLen = 0;
      }
    } else if (usbLen < sizeof(usbBuf) - 1) {
      usbBuf[usbLen++] = c;
    }
  }

  // Periodic counter to the Pico.
  static uint32_t last = 0; static uint32_t counter = 0;
  if (millis() - last >= SEND_INTERVAL_MS) {
    last = millis();
    Serial1.print(F("ARD:")); Serial1.println(counter);
    counter++;
  }
}
