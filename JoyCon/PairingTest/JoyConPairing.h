// ---------------------------------------------------------------------------------------
// JoyConPairing.h
//
// Pairing Joy-Cons without editing or re-uploading the sketch.
//
//   - First boot: no Joy-Con recorded in EEPROM, so pairing starts by itself.
//   - Re-pair: hold Minus + Capture (left) or Plus + Home (right) for 5 s on a connected
//     Joy-Con, or reset the Mega three times within 10 s of each other, or (ShadowMD Lite)
//     send JOYPAIR from the web console.
//   - A session lasts at most 60 s. The droid must not move while it runs; the sketch asks
//     active() and stops the motors.
//   - The sketch is told what happens through one callback, so it can play R2 sounds.
//
// Same file at the root of ShadowMD Lite, in JoyCon/BenchTest, JoyCon/PairingTest and in
// the Shadow MD fork. Keep them identical.
// ---------------------------------------------------------------------------------------

#ifndef JOYCON_PAIRING_H
#define JOYCON_PAIRING_H

#include <Arduino.h>

#ifndef JOYCON_COMBO_HOLD_MS
  #define JOYCON_COMBO_HOLD_MS          5000UL
#endif
#ifndef JOYCON_PAIR_TIMEOUT_MS
  #define JOYCON_PAIR_TIMEOUT_MS        60000UL
#endif
#ifndef JOYCON_PAIR_REARM_MS
  #define JOYCON_PAIR_REARM_MS          15000UL
#endif
#ifndef JOYCON_TRIPLE_RESET_WINDOW_MS
  #define JOYCON_TRIPLE_RESET_WINDOW_MS 10000UL
#endif
#ifndef JOYCON_EEPROM_ADDR
  #define JOYCON_EEPROM_ADDR            4000     // 3 bytes, near the end of the Mega's 4096
#endif

// ---- pure logic, no hardware ----------------------------------------------------------

// True once, when a button combination has been held without a break for
// JOYCON_COMBO_HOLD_MS. Releasing it starts again from zero.
struct HoldTimer
{
  void reset() { _holding = false; _fired = false; _start = 0; _now = 0; }

  bool update(bool held, uint32_t now)
  {
    if (!held) { reset(); return false; }
    if (!_holding) { _holding = true; _fired = false; _start = now; }
    _now = now;
    if (!_fired && now - _start >= JOYCON_COMBO_HOLD_MS) { _fired = true; return true; }
    return false;
  }

  // Whole seconds held so far, for a rumble tick each second.
  uint8_t ticks() const { return _holding ? (uint8_t)((_now - _start) / 1000UL) : 0; }

private:
  bool     _holding = false;
  bool     _fired = false;
  uint32_t _start = 0;
  uint32_t _now = 0;
};

// Counts boots that come within JOYCON_TRIPLE_RESET_WINDOW_MS of each other (the sketch
// clears the counter once it has been running that long). The third one asks for pairing.
inline bool jcpTripleReset(uint8_t& counter)
{
  if (counter < 3) counter++;
  if (counter >= 3) { counter = 0; return true; }
  return false;
}

// What is kept in EEPROM: which halves have been paired, and the boot counter.
struct JcpRecord
{
  static const uint8_t MAGIC = 0x4A;
  static const uint8_t LEFT  = 1;
  static const uint8_t RIGHT = 2;

  uint8_t magic;
  uint8_t sides;
  uint8_t boots;

  bool valid() const    { return magic == MAGIC; }
  bool complete() const { return (sides & (LEFT | RIGHT)) == (LEFT | RIGHT); }
};

// A Joy-Con keeps two host records in its flash, at 0x2000 and 0x2026. A new pairing goes
// into 0x2026 and the old one moves to 0x2000. If either points to another host, a Switch
// for instance, pressing a button no longer reconnects to the droid. Each record starts
// with magic, size, two checksum bytes, then the host address big-endian; my_bdaddr in
// the BTD library is little-endian.
inline bool jcpHostRecordsMatch(const uint8_t rec2000[10], const uint8_t rec2026[10],
                                const uint8_t dongle[6])
{
  for (uint8_t i = 0; i < 6; i++)
  {
    if (rec2000[4 + i] != dongle[5 - i]) return false;
    if (rec2026[4 + i] != dongle[5 - i]) return false;
  }
  return true;
}

#endif // JOYCON_PAIRING_H
