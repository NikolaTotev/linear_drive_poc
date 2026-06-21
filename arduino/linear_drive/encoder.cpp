/*
 * encoder.cpp  -  x4 quadrature decode using a transition lookup table.
 *
 * On every edge of A or B we form a 4-bit index [oldA oldB newA newB] and look
 * up +1 / -1 / 0. Invalid (double) transitions yield 0 and are ignored. This
 * gives full 4x resolution (4000 counts/rev for a 1000 PPR encoder).
 */
#include "encoder.h"
#include "config.h"

namespace {

// Transition table indexed by (prev<<2 | curr), where each 2-bit code is (A<<1|B).
// +1 for forward (mechanically) transitions, -1 for reverse, 0 for none/invalid.
const int8_t kTable[16] = {
   0, -1, +1,  0,
  +1,  0,  0, -1,
  -1,  0,  0, +1,
   0, +1, -1,  0
};

volatile long g_count = 0;
volatile uint8_t g_prev = 0;   // last 2-bit AB state
volatile long g_errors = 0;    // invalid/merged transitions (decode health)

// Fast, atomic both-channel read. digitalRead() on the RA4M1 is slow (~1 us)
// and reads one pin at a time, which lengthens the ISR and can "tear" the A/B
// pair when both change close together (e.g. a geared encoder at speed). Read
// both channels from the PORT1 input register in one access instead.
// D2 = P1_04, D3 = P1_05 -> PORT1 bits 4 and 5.
static_assert(PIN_ENC_A == 2 && PIN_ENC_B == 3,
              "Fast read assumes D2/D3 (PORT1 bits 4/5); update readAB() if you move the encoder pins");

inline uint8_t readAB() {
  uint16_t p = R_PORT1->PIDR;                  // one atomic snapshot of the port
  return (uint8_t)((((p >> 4) & 1u) << 1) | ((p >> 5) & 1u));
}

inline void update() {
  uint8_t curr = readAB();
  uint8_t idx  = (uint8_t)((g_prev << 2) | curr);
  int8_t  delta = kTable[idx];
  if (delta == 0 && curr != g_prev) g_errors++;   // both bits changed = merged/missed edge
  g_count += (long)delta * ENC_FORWARD_SIGN;
  g_prev = curr;
}

void isr() { update(); }

} // namespace

namespace encoder {

void begin() {
  pinMode(PIN_ENC_A, INPUT);   // line receiver already provides clean TTL levels
  pinMode(PIN_ENC_B, INPUT);
  g_prev = readAB();
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), isr, CHANGE);
}

long count() {
  noInterrupts();
  long c = g_count;
  interrupts();
  return c;
}

long invalidEdges() {
  noInterrupts();
  long e = g_errors;
  interrupts();
  return e;
}

void set(long value) {
  noInterrupts();
  g_count = value;
  interrupts();
}

} // namespace encoder
