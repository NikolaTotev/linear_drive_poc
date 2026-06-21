/*
 * protocol.h  -  ASCII line protocol over the Pico UART link (see PROTOCOL.md).
 *
 * Parses incoming command lines and emits ST:/EVT: lines. All e-stop and motion
 * gating is funnelled through here so the .ino stays a thin orchestrator.
 */
#ifndef LD_PROTOCOL_H
#define LD_PROTOCOL_H

#include <Arduino.h>

namespace protocol {

void begin();
void poll();                       // read LINK, dispatch complete command lines
void pumpEvents();                 // forward motion/safety events as EVT lines
void sendStatusIfDue(uint32_t now);// push ST: at STATUS_INTERVAL_MS
void forceStatus();                // push ST: immediately

} // namespace protocol

#endif // LD_PROTOCOL_H
