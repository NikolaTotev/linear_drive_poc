/*
 * encoder.h  -  x4 quadrature decoding for the 17E1K-07 incremental encoder.
 *
 * Channels A/B (after the AM26LS32AC line receiver) are read on two
 * interrupt-capable pins. Counting is done in the ISR; the rest of the firmware
 * only reads an atomic snapshot of the count.
 */
#ifndef LD_ENCODER_H
#define LD_ENCODER_H

#include <Arduino.h>

namespace encoder {

// Attach interrupts and initialise. Call once from setup().
void begin();

// Atomic read of the current signed count (forward motion increases it, after
// applying ENC_FORWARD_SIGN).
long count();

// Number of invalid/merged quadrature transitions seen (both channels appeared
// to change between two ISR reads). Should stay ~0 in normal operation; a
// rising value means the encoder is being driven faster than the ISR can
// service (decode/throughput limit), not a position error in itself.
long invalidEdges();

// Set the current count (used to define the zero at the MIN endstop).
void set(long value);

// Convenience: zero the count.
inline void zero() { set(0); }

} // namespace encoder

#endif // LD_ENCODER_H
