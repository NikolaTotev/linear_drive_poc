/*
 * safety.h  -  Endstops + latching e-stop (physical button and software).
 *
 * E-stop latch rules (single push-button):
 *   - First press latches the e-stop.
 *   - While latched, a separate long press (>= ESTOP_LONGPRESS_MS) clears it.
 *   - A single long press from idle only latches (clearing always needs a fresh
 *     press), so you can never accidentally clear with the same press.
 * Software ESTOP/ECLR commands latch/clear an independent software flag; the
 * device is stopped while EITHER source is latched.
 */
#ifndef LD_SAFETY_H
#define LD_SAFETY_H

#include <Arduino.h>

namespace safety {

enum class EstopSource : uint8_t { SW, BTN };

void begin();
void update(uint32_t now);            // call every loop iteration

bool limitMinPressed();               // debounced, true = pressed or wire fault
bool limitMaxPressed();

bool estopLatched();                  // software OR physical

void commandSoftwareEstop(bool on);   // ESTOP (true) / ECLR (false)

// One-shot edge events for the orchestrator to forward as EVT lines.
bool consumeEngaged(EstopSource& src);
bool consumeCleared();

} // namespace safety

#endif // LD_SAFETY_H
