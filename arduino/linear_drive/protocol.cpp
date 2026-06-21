#include "protocol.h"
#include "config.h"
#include "motion.h"
#include "safety.h"

namespace {

constexpr uint8_t LINE_MAX = 64;
char     g_buf[LINE_MAX + 1];
uint8_t  g_len = 0;
uint32_t g_lastStatus = 0;

const char* stateStr(motion::State s) {
  switch (s) {
    case motion::State::IDLE:        return "IDLE";
    case motion::State::HOMING:      return "HOMING";
    case motion::State::CALIBRATING: return "CALIBRATING";
    case motion::State::JOGGING:     return "JOGGING";
    case motion::State::MOVING:      return "MOVING";
    case motion::State::ESTOP:       return "ESTOP";
    case motion::State::ERROR:       return "ERROR";
  }
  return "IDLE";
}
const char* profileStr(motion::Profile p) {
  switch (p) {
    case motion::Profile::LIN:  return "LIN";
    case motion::Profile::TRAP: return "TRAP";
    case motion::Profile::SIN:  return "SIN";
  }
  return "TRAP";
}

void sendEvent(const char* body) {
  LINK.print("EVT:");
  LINK.println(body);
  if (DEBUG) { DEBUG.print("EVT:"); DEBUG.println(body); } // skip if USB not open
}

// ---- command dispatch -----------------------------------------------------
void dispatch(char* line) {
  // split "KEY" or "KEY:args"
  char* args = strchr(line, ':');
  if (args) { *args = '\0'; args++; }
  const char* key = line;

  if (strcmp(key, "PING") == 0)        { protocol::forceStatus(); return; }
  if (strcmp(key, "CAL") == 0)         { motion::startCalibrate(); return; }
  if (strcmp(key, "HOME") == 0)        { motion::startHome(); return; }
  if (strcmp(key, "JSTOP") == 0)       { motion::stopJog(); return; }
  if (strcmp(key, "STOP") == 0)        { motion::stopMotion(); return; }
  if (strcmp(key, "ESTOP") == 0)       { safety::commandSoftwareEstop(true); return; }
  if (strcmp(key, "ECLR") == 0)        { safety::commandSoftwareEstop(false); return; }

  if (strcmp(key, "JOG") == 0 && args) {
    char* d = strtok(args, ",");
    char* s = strtok(nullptr, ",");
    if (d && s) motion::startJog(d[0] == 'F', atoi(s));
    return;
  }
  if (strcmp(key, "MOVE") == 0 && args) {
    char* cm = strtok(args, ",");
    char* s  = strtok(nullptr, ",");
    if (cm && s) motion::startMove(atof(cm), atoi(s));
    return;
  }
  if (strcmp(key, "PROFILE") == 0 && args) {
    if (strcmp(args, "LIN") == 0)  motion::setProfile(motion::Profile::LIN);
    else if (strcmp(args, "SIN") == 0) motion::setProfile(motion::Profile::SIN);
    else motion::setProfile(motion::Profile::TRAP);
    return;
  }
  // unknown -> ignore
}

} // namespace

namespace protocol {

void begin() { g_len = 0; }

void poll() {
  while (LINK.available()) {
    char c = (char)LINK.read();
    if (c == '\n') {
      g_buf[g_len] = '\0';
      // strip trailing '\r'
      if (g_len && g_buf[g_len - 1] == '\r') g_buf[g_len - 1] = '\0';
      if (g_len > 0) dispatch(g_buf);
      g_len = 0;
    } else if (c >= 0x20 && c <= 0x7E) {  // printable ASCII only
      // Ignore non-printable bytes ('\r', nulls, UART line noise) so leading/
      // embedded garbage from a marginal link doesn't corrupt the command.
      if (g_len < LINE_MAX) g_buf[g_len++] = c;
      else g_len = 0;              // overflow -> drop line
    }
  }
}

void pumpEvents() {
  bool fired = false;

  // e-stop edges drive the motion module + EVT lines
  safety::EstopSource src;
  if (safety::consumeEngaged(src)) {
    motion::onEstopEngaged();
    sendEvent(src == safety::EstopSource::BTN ? "ESTOP,BTN" : "ESTOP,SW");
    fired = true;
  }
  if (safety::consumeCleared()) {
    motion::onEstopCleared();
    sendEvent("ECLR");
    fired = true;
  }

  long pulses;
  if (motion::consumeCalDone(pulses)) {
    char b[32]; snprintf(b, sizeof(b), "CAL_DONE,%ld", pulses);
    sendEvent(b); fired = true;
  }
  if (motion::consumeHomed()) { sendEvent("HOMED"); fired = true; }

  const char* which;
  if (motion::consumeLimit(which)) {
    char b[20]; snprintf(b, sizeof(b), "LIMIT,%s", which);
    sendEvent(b); fired = true;
  }
  const char* code;
  if (motion::consumeError(code)) {
    char b[32]; snprintf(b, sizeof(b), "ERR,%s", code);
    sendEvent(b); fired = true;
  }

  if (fired) forceStatus();       // keep the app in sync right after an event
}

void forceStatus() {
  char pos[12];
  dtostrf(motion::posCm(), 0, 2, pos);   // "-1.00" if unknown

  char line[LINE_MAX + 1];
  snprintf(line, sizeof(line), "ST:%s,%s,%d,%d,%s,%ld,%s",
           stateStr(motion::state()),
           pos,
           motion::homed() ? 1 : 0,
           safety::estopLatched() ? 1 : 0,
           profileStr(motion::profile()),
           motion::rangePulses(),
           motion::errCode());
  // Non-blocking: at 9600 baud a full TX buffer would otherwise stall the loop
  // (and jitter the steps). Skip this periodic status if there isn't room.
  size_t need = strlen(line) + 2;  // + "\r\n"
  if (LINK.availableForWrite() >= (int)need) LINK.println(line);
  if (DEBUG && DEBUG.availableForWrite() >= (int)need) DEBUG.println(line);
  g_lastStatus = millis();
}

void sendStatusIfDue(uint32_t now) {
  if ((uint32_t)(now - g_lastStatus) >= STATUS_INTERVAL_MS) forceStatus();
}

} // namespace protocol
